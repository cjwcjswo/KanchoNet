#include "RingBuffer.h"
#include <algorithm>

namespace KanchoNet
{
    RingBuffer::RingBuffer(size_t capacity)
        : mCapacity(capacity + 1) // +1 for distinguishing full from empty
        , mReadPos(0)
        , mWritePos(0)
    {
        mBuffer.resize(mCapacity);
    }

    RingBuffer::~RingBuffer()
    {
    }

    RingBuffer::RingBuffer(RingBuffer&& other) noexcept
        : mBuffer(std::move(other.mBuffer))
        , mCapacity(other.mCapacity)
        , mReadPos(other.mReadPos)
        , mWritePos(other.mWritePos)
    {
        other.mCapacity = 0;
        other.mReadPos = 0;
        other.mWritePos = 0;
    }

    RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            mBuffer = std::move(other.mBuffer);
            mCapacity = other.mCapacity;
            mReadPos = other.mReadPos;
            mWritePos = other.mWritePos;
            
            other.mCapacity = 0;
            other.mReadPos = 0;
            other.mWritePos = 0;
        }
        return *this;
    }

    size_t RingBuffer::Write(const void* data, size_t size)
    {
        if (data == nullptr || size == 0)
            return 0;

        size_t availableWrite = GetAvailableWrite();
        size_t writeSize = (std::min)(size, availableWrite);
        
        if (writeSize == 0)
            return 0;

        const uint8_t* src = static_cast<const uint8_t*>(data);
        
        // 연속된 공간에 쓰기
        size_t contiguousSize = GetContiguousWriteSize();
        if (writeSize <= contiguousSize)
        {
            std::memcpy(mBuffer.data() + mWritePos, src, writeSize);
            mWritePos = (mWritePos + writeSize) % mCapacity;
        }
        else
        {
            // 두 번에 나눠서 쓰기 (순환)
            std::memcpy(mBuffer.data() + mWritePos, src, contiguousSize);
            std::memcpy(mBuffer.data(), src + contiguousSize, writeSize - contiguousSize);
            mWritePos = writeSize - contiguousSize;
        }

        return writeSize;
    }

    size_t RingBuffer::Read(void* buffer, size_t size)
    {
        size_t readSize = Peek(buffer, size);
        if (readSize > 0)
        {
            mReadPos = (mReadPos + readSize) % mCapacity;
        }
        return readSize;
    }

    size_t RingBuffer::Peek(void* buffer, size_t size) const
    {
        if (buffer == nullptr || size == 0)
            return 0;

        size_t availableRead = GetAvailableRead();
        size_t peekSize = (std::min)(size, availableRead);
        
        if (peekSize == 0)
            return 0;

        uint8_t* dest = static_cast<uint8_t*>(buffer);
        
        // 연속된 공간에서 읽기
        size_t contiguousSize = GetContiguousReadSize();
        if (peekSize <= contiguousSize)
        {
            std::memcpy(dest, mBuffer.data() + mReadPos, peekSize);
        }
        else
        {
            // 두 번에 나눠서 읽기 (순환)
            std::memcpy(dest, mBuffer.data() + mReadPos, contiguousSize);
            std::memcpy(dest + contiguousSize, mBuffer.data(), peekSize - contiguousSize);
        }

        return peekSize;
    }

    size_t RingBuffer::Skip(size_t size)
    {
        size_t availableRead = GetAvailableRead();
        size_t skipSize = (std::min)(size, availableRead);
        
        if (skipSize > 0)
        {
            mReadPos = (mReadPos + skipSize) % mCapacity;
        }
        
        return skipSize;
    }

    size_t RingBuffer::GetAvailableRead() const
    {
        if (mWritePos >= mReadPos)
        {
            return mWritePos - mReadPos;
        }
        else
        {
            return mCapacity - mReadPos + mWritePos;
        }
    }

    size_t RingBuffer::GetAvailableWrite() const
    {
        return mCapacity - 1 - GetAvailableRead();
    }

    void RingBuffer::Clear()
    {
        mReadPos = 0;
        mWritePos = 0;
    }

    size_t RingBuffer::GetContiguousWriteSize() const
    {
        if (mWritePos >= mReadPos)
        {
            size_t toEnd = mCapacity - mWritePos;
            if (mReadPos == 0)
                return toEnd - 1; // 버퍼가 꽉 찼는지 구분하기 위해 -1
            return toEnd;
        }
        else
        {
            return mReadPos - mWritePos - 1;
        }
    }

    size_t RingBuffer::GetContiguousReadSize() const
    {
        if (mWritePos >= mReadPos)
        {
            return mWritePos - mReadPos;
        }
        else
        {
            return mCapacity - mReadPos;
        }
    }

    void RingBuffer::CommitWrite(size_t size)
    {
        size_t availableWrite = GetAvailableWrite();
        size_t commitSize = (std::min)(size, availableWrite);
        mWritePos = (mWritePos + commitSize) % mCapacity;
    }

    void RingBuffer::CommitRead(size_t size)
    {
        size_t availableRead = GetAvailableRead();
        size_t commitSize = (std::min)(size, availableRead);
        mReadPos = (mReadPos + commitSize) % mCapacity;
    }

} // namespace KanchoNet

