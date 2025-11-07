#pragma once

#include "../Types.h"
#include <vector>
#include <cstring>

namespace KanchoNet
{
    // 바이너리 패킷 데이터를 담는 버퍼 클래스
    // Protobuf 등의 직렬화 라이브러리와 함께 사용 가능
    class PacketBuffer
    {
    public:
        // 생성자
        PacketBuffer();
        explicit PacketBuffer(size_t initialCapacity);
        PacketBuffer(const void* data, size_t size);
        
        // 복사/이동
        PacketBuffer(const PacketBuffer& other);
        PacketBuffer& operator=(const PacketBuffer& other);
        PacketBuffer(PacketBuffer&& other) noexcept;
        PacketBuffer& operator=(PacketBuffer&& other) noexcept;
        
        // 소멸자
        ~PacketBuffer();

        // 데이터 접근
        const uint8_t* GetData() const { return data_.data(); }
        uint8_t* GetData() { return data_.data(); }
        size_t GetSize() const { return size_; }
        size_t GetCapacity() const { return data_.capacity(); }
        
        // 데이터 조작
        void Clear();
        void Reserve(size_t capacity);
        void Resize(size_t newSize);
        
        // 데이터 추가
        void Append(const void* data, size_t size);
        void Append(const PacketBuffer& other);
        
        // 데이터 설정 (기존 데이터 덮어쓰기)
        void SetData(const void* data, size_t size);
        
        // 유효성 검사
        bool IsEmpty() const { return size_ == 0; }
        
    private:
        std::vector<uint8_t> data_;
        size_t size_;  // 실제 사용 중인 데이터 크기
    };

} // namespace KanchoNet

