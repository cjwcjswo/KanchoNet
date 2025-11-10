#include "BufferPool.h"

namespace KanchoNet
{
    BufferPool::BufferPool(size_t bufferSize, size_t initialCount)
        : mBufferSize(bufferSize)
        , mTotalAllocated(0)
    {
        mPool.reserve(initialCount);
        for (size_t i = 0; i < initialCount; ++i)
        {
            mPool.push_back(std::make_unique<PacketBuffer>(mBufferSize));
        }
    }

    BufferPool::~BufferPool()
    {
        Clear();
    }

    std::unique_ptr<PacketBuffer> BufferPool::Allocate()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        
        if (!mPool.empty())
        {
            auto buffer = std::move(mPool.back());
            mPool.pop_back();
            buffer->Clear(); // 재사용을 위해 초기화
            return buffer;
        }
        
        // 풀이 비어있으면 새로 할당
        ++mTotalAllocated;
        return std::make_unique<PacketBuffer>(mBufferSize);
    }

    void BufferPool::Deallocate(std::unique_ptr<PacketBuffer> buffer)
    {
        if (!buffer)
            return;

        std::lock_guard<std::mutex> lock(mMutex);
        
        // 풀 크기 제한 (너무 많이 쌓이는 것 방지)
        constexpr size_t MAX_POOL_SIZE = 1000;
        if (mPool.size() < MAX_POOL_SIZE)
        {
            buffer->Clear();
            mPool.push_back(std::move(buffer));
        }
        // else: 버퍼를 폐기 (unique_ptr이 자동으로 삭제)
    }

    size_t BufferPool::GetPoolSize() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mPool.size();
    }

    void BufferPool::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPool.clear();
        mTotalAllocated = 0;
    }

} // namespace KanchoNet

