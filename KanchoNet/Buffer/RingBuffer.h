#pragma once

#include "../Types.h"
#include "../Utils/NonCopyable.h"
#include <vector>
#include <cstring>

namespace KanchoNet
{
    // 순환 버퍼 (Circular Buffer)
    // 송수신 버퍼로 사용되며, 연속된 메모리 공간에서 효율적인 데이터 관리
    class RingBuffer : public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        std::vector<uint8_t> mBuffer;
        size_t mCapacity;
        size_t mReadPos;
        size_t mWritePos;
        
    public:
        // 생성자, 파괴자
        explicit RingBuffer(size_t capacity);
        ~RingBuffer();

        // 이동 생성자/대입 연산자
        RingBuffer(RingBuffer&& other) noexcept;
        RingBuffer& operator=(RingBuffer&& other) noexcept;
        
    public:
        // public 함수
        // 데이터 쓰기
        size_t Write(const void* data, size_t size);
        
        // 데이터 읽기
        size_t Read(void* buffer, size_t size);
        
        // 데이터 피크 (읽기 위치 이동 없이 데이터 확인)
        size_t Peek(void* buffer, size_t size) const;
        
        // 읽기 위치 이동 (실제 읽지 않고 건너뛰기)
        size_t Skip(size_t size);
        
        // 버퍼 상태
        size_t GetCapacity() const { return mCapacity; }
        size_t GetAvailableRead() const;  // 읽을 수 있는 데이터 크기
        size_t GetAvailableWrite() const; // 쓸 수 있는 여유 공간
        bool IsEmpty() const { return mReadPos == mWritePos; }
        bool IsFull() const { return GetAvailableWrite() == 0; }
        
        // 버퍼 초기화
        void Clear();
        
        // 직접 메모리 접근 (고급 사용)
        uint8_t* GetWritePtr() { return mBuffer.data() + mWritePos; }
        const uint8_t* GetReadPtr() const { return mBuffer.data() + mReadPos; }
        size_t GetContiguousWriteSize() const; // 연속된 쓰기 가능 크기
        size_t GetContiguousReadSize() const;  // 연속된 읽기 가능 크기
        void CommitWrite(size_t size);         // 쓰기 완료 알림
        void CommitRead(size_t size);          // 읽기 완료 알림
    };

} // namespace KanchoNet

