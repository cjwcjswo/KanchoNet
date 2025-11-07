#include "PacketBuffer.h"
#include <algorithm>

namespace KanchoNet
{
    PacketBuffer::PacketBuffer()
        : size_(0)
    {
        data_.reserve(DEFAULT_BUFFER_SIZE);
    }

    PacketBuffer::PacketBuffer(size_t initialCapacity)
        : size_(0)
    {
        data_.reserve(initialCapacity);
    }

    PacketBuffer::PacketBuffer(const void* data, size_t size)
        : size_(size)
    {
        data_.resize(size);
        if (data && size > 0)
        {
            std::memcpy(data_.data(), data, size);
        }
    }

    PacketBuffer::PacketBuffer(const PacketBuffer& other)
        : data_(other.data_)
        , size_(other.size_)
    {
    }

    PacketBuffer& PacketBuffer::operator=(const PacketBuffer& other)
    {
        if (this != &other)
        {
            data_ = other.data_;
            size_ = other.size_;
        }
        return *this;
    }

    PacketBuffer::PacketBuffer(PacketBuffer&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)
    {
        other.size_ = 0;
    }

    PacketBuffer& PacketBuffer::operator=(PacketBuffer&& other) noexcept
    {
        if (this != &other)
        {
            data_ = std::move(other.data_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    PacketBuffer::~PacketBuffer()
    {
    }

    void PacketBuffer::Clear()
    {
        size_ = 0;
        // capacity는 유지하여 재사용 가능하게 함
    }

    void PacketBuffer::Reserve(size_t capacity)
    {
        data_.reserve(capacity);
    }

    void PacketBuffer::Resize(size_t newSize)
    {
        data_.resize(newSize);
        size_ = newSize;
    }

    void PacketBuffer::Append(const void* data, size_t size)
    {
        if (data == nullptr || size == 0)
            return;

        size_t newSize = size_ + size;
        if (newSize > data_.capacity())
        {
            data_.reserve(newSize * 2); // 2배 증가 전략
        }
        
        data_.resize(newSize);
        std::memcpy(data_.data() + size_, data, size);
        size_ = newSize;
    }

    void PacketBuffer::Append(const PacketBuffer& other)
    {
        if (other.IsEmpty())
            return;
            
        Append(other.GetData(), other.GetSize());
    }

    void PacketBuffer::SetData(const void* data, size_t size)
    {
        Clear();
        if (data && size > 0)
        {
            data_.resize(size);
            std::memcpy(data_.data(), data, size);
            size_ = size;
        }
    }

} // namespace KanchoNet

