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
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        size_t mBufferSize;
        size_t mTotalAllocated;
        std::vector<std::unique_ptr<PacketBuffer>> mPool;
        mutable std::mutex mMutex;
        
    public:
        // 생성자, 파괴자
        explicit BufferPool(size_t bufferSize, size_t initialCount = 100);
        ~BufferPool();
        
    public:
        // public 함수
        // 버퍼 할당
        std::unique_ptr<PacketBuffer> Allocate();
        
        // 버퍼 반환 (재사용을 위해 풀에 반환)
        void Deallocate(std::unique_ptr<PacketBuffer> buffer);
        
        // 풀 상태
        size_t GetPoolSize() const;
        size_t GetBufferSize() const { return mBufferSize; }
        size_t GetTotalAllocated() const { return mTotalAllocated; }
        
        // 풀 비우기
        void Clear();
    };

} // namespace KanchoNet

