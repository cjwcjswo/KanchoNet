#pragma once

#include "../Types.h"
#include "../Utils/NonCopyable.h"
#include "PacketBuffer.h"
#include <memory>
#include <vector>
#include <mutex>

namespace KanchoNet
{
    // 버퍼 풀 (객체 재사용을 통한 메모리 할당 최적화)
    class BufferPool : public NonCopyable
    {
    public:
        explicit BufferPool(size_t bufferSize, size_t initialCount = 100);
        ~BufferPool();

        // 버퍼 할당
        std::unique_ptr<PacketBuffer> Allocate();
        
        // 버퍼 반환 (재사용을 위해 풀에 반환)
        void Deallocate(std::unique_ptr<PacketBuffer> buffer);
        
        // 풀 상태
        size_t GetPoolSize() const;
        size_t GetBufferSize() const { return bufferSize_; }
        size_t GetTotalAllocated() const { return totalAllocated_; }
        
        // 풀 비우기
        void Clear();

    private:
        size_t bufferSize_;
        size_t totalAllocated_;
        std::vector<std::unique_ptr<PacketBuffer>> pool_;
        mutable std::mutex mutex_;
    };

} // namespace KanchoNet

