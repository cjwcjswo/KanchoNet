#include "PacketBuffer.h"
#include <algorithm>

namespace KanchoNet
{
    PacketBuffer::PacketBuffer()
        : mSize(0)
    {
        mData.reserve(DEFAULT_BUFFER_SIZE);
    }

    PacketBuffer::PacketBuffer(size_t initialCapacity)
        : mSize(0)
    {
        mData.reserve(initialCapacity);
    }

    PacketBuffer::PacketBuffer(const void* data, size_t size)
        : mSize(size)
    {
        mData.resize(size);
        if (data && size > 0)
        {
            std::memcpy(mData.data(), data, size);
        }
    }

    PacketBuffer::PacketBuffer(const PacketBuffer& other)
        : mData(other.mData)
        , mSize(other.mSize)
    {
    }

    PacketBuffer& PacketBuffer::operator=(const PacketBuffer& other)
    {
        if (this != &other)
        {
            mData = other.mData;
            mSize = other.mSize;
        }
        return *this;
    }

    PacketBuffer::PacketBuffer(PacketBuffer&& other) noexcept
        : mData(std::move(other.mData))
        , mSize(other.mSize)
    {
        other.mSize = 0;
    }

    PacketBuffer& PacketBuffer::operator=(PacketBuffer&& other) noexcept
    {
        if (this != &other)
        {
            mData = std::move(other.mData);
            mSize = other.mSize;
            other.mSize = 0;
        }
        return *this;
    }

    PacketBuffer::~PacketBuffer()
    {
    }

    void PacketBuffer::Clear()
    {
        mSize = 0;
        // capacity는 유지하여 재사용 가능하게 함
    }

    void PacketBuffer::Reserve(size_t capacity)
    {
        mData.reserve(capacity);
    }

    void PacketBuffer::Resize(size_t newSize)
    {
        mData.resize(newSize);
        mSize = newSize;
    }

    void PacketBuffer::Append(const void* data, size_t size)
    {
        if (data == nullptr || size == 0)
            return;

        size_t newSize = mSize + size;
        if (newSize > mData.capacity())
        {
            mData.reserve(newSize * 2); // 2배 증가 전략
        }
        
        mData.resize(newSize);
        std::memcpy(mData.data() + mSize, data, size);
        mSize = newSize;
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
            mData.resize(size);
            std::memcpy(mData.data(), data, size);
            mSize = size;
        }
    }

} // namespace KanchoNet

