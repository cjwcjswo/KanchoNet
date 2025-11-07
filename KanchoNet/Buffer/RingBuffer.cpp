#include "RingBuffer.h"
#include <algorithm>

namespace KanchoNet
{
    RingBuffer::RingBuffer(size_t capacity)
        : capacity_(capacity + 1) // +1 for distinguishing full from empty
        , readPos_(0)
        , writePos_(0)
    {
        buffer_.resize(capacity_);
    }

    RingBuffer::~RingBuffer()
    {
    }

    RingBuffer::RingBuffer(RingBuffer&& other) noexcept
        : buffer_(std::move(other.buffer_))
        , capacity_(other.capacity_)
        , readPos_(other.readPos_)
        , writePos_(other.writePos_)
    {
        other.capacity_ = 0;
        other.readPos_ = 0;
        other.writePos_ = 0;
    }

    RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            buffer_ = std::move(other.buffer_);
            capacity_ = other.capacity_;
            readPos_ = other.readPos_;
            writePos_ = other.writePos_;
            
            other.capacity_ = 0;
            other.readPos_ = 0;
            other.writePos_ = 0;
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
            std::memcpy(buffer_.data() + writePos_, src, writeSize);
            writePos_ = (writePos_ + writeSize) % capacity_;
        }
        else
        {
            // 두 번에 나눠서 쓰기 (순환)
            std::memcpy(buffer_.data() + writePos_, src, contiguousSize);
            std::memcpy(buffer_.data(), src + contiguousSize, writeSize - contiguousSize);
            writePos_ = writeSize - contiguousSize;
        }

        return writeSize;
    }

    size_t RingBuffer::Read(void* buffer, size_t size)
    {
        size_t readSize = Peek(buffer, size);
        if (readSize > 0)
        {
            readPos_ = (readPos_ + readSize) % capacity_;
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
            std::memcpy(dest, buffer_.data() + readPos_, peekSize);
        }
        else
        {
            // 두 번에 나눠서 읽기 (순환)
            std::memcpy(dest, buffer_.data() + readPos_, contiguousSize);
            std::memcpy(dest + contiguousSize, buffer_.data(), peekSize - contiguousSize);
        }

        return peekSize;
    }

    size_t RingBuffer::Skip(size_t size)
    {
        size_t availableRead = GetAvailableRead();
        size_t skipSize = (std::min)(size, availableRead);
        
        if (skipSize > 0)
        {
            readPos_ = (readPos_ + skipSize) % capacity_;
        }
        
        return skipSize;
    }

    size_t RingBuffer::GetAvailableRead() const
    {
        if (writePos_ >= readPos_)
        {
            return writePos_ - readPos_;
        }
        else
        {
            return capacity_ - readPos_ + writePos_;
        }
    }

    size_t RingBuffer::GetAvailableWrite() const
    {
        return capacity_ - 1 - GetAvailableRead();
    }

    void RingBuffer::Clear()
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    size_t RingBuffer::GetContiguousWriteSize() const
    {
        if (writePos_ >= readPos_)
        {
            size_t toEnd = capacity_ - writePos_;
            if (readPos_ == 0)
                return toEnd - 1; // 버퍼가 꽉 찼는지 구분하기 위해 -1
            return toEnd;
        }
        else
        {
            return readPos_ - writePos_ - 1;
        }
    }

    size_t RingBuffer::GetContiguousReadSize() const
    {
        if (writePos_ >= readPos_)
        {
            return writePos_ - readPos_;
        }
        else
        {
            return capacity_ - readPos_;
        }
    }

    void RingBuffer::CommitWrite(size_t size)
    {
        size_t availableWrite = GetAvailableWrite();
        size_t commitSize = (std::min)(size, availableWrite);
        writePos_ = (writePos_ + commitSize) % capacity_;
    }

    void RingBuffer::CommitRead(size_t size)
    {
        size_t availableRead = GetAvailableRead();
        size_t commitSize = (std::min)(size, availableRead);
        readPos_ = (readPos_ + commitSize) % capacity_;
    }

} // namespace KanchoNet

