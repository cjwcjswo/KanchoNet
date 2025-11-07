#include "BufferPool.h"

namespace KanchoNet
{
    BufferPool::BufferPool(size_t bufferSize, size_t initialCount)
        : bufferSize_(bufferSize)
        , totalAllocated_(0)
    {
        pool_.reserve(initialCount);
        for (size_t i = 0; i < initialCount; ++i)
        {
            pool_.push_back(std::make_unique<PacketBuffer>(bufferSize_));
        }
    }

    BufferPool::~BufferPool()
    {
        Clear();
    }

    std::unique_ptr<PacketBuffer> BufferPool::Allocate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!pool_.empty())
        {
            auto buffer = std::move(pool_.back());
            pool_.pop_back();
            buffer->Clear(); // 재사용을 위해 초기화
            return buffer;
        }
        
        // 풀이 비어있으면 새로 할당
        ++totalAllocated_;
        return std::make_unique<PacketBuffer>(bufferSize_);
    }

    void BufferPool::Deallocate(std::unique_ptr<PacketBuffer> buffer)
    {
        if (!buffer)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        // 풀 크기 제한 (너무 많이 쌓이는 것 방지)
        constexpr size_t MAX_POOL_SIZE = 1000;
        if (pool_.size() < MAX_POOL_SIZE)
        {
            buffer->Clear();
            pool_.push_back(std::move(buffer));
        }
        // else: 버퍼를 폐기 (unique_ptr이 자동으로 삭제)
    }

    size_t BufferPool::GetPoolSize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    void BufferPool::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
        totalAllocated_ = 0;
    }

} // namespace KanchoNet

