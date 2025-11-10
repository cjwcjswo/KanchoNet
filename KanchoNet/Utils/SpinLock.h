#pragma once

#include <atomic>

namespace KanchoNet
{
    // 스핀락 (Spin Lock)
    // 짧은 시간 동안의 동기화에 적합 (뮤텍스보다 오버헤드가 적음)
    class SpinLock
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        std::atomic<bool> mFlag;
        
    public:
        // 생성자, 파괴자
        SpinLock() : mFlag(false) {}
        ~SpinLock() = default;

        // 복사/이동 불가
        SpinLock(const SpinLock&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;
        SpinLock(SpinLock&&) = delete;
        SpinLock& operator=(SpinLock&&) = delete;
        
    public:
        // public 함수
        // 락 획득
        void lock()
        {
            while (mFlag.exchange(true, std::memory_order_acquire))
            {
                // 스핀 대기
                while (mFlag.load(std::memory_order_relaxed))
                {
                    // CPU에게 힌트를 주어 성능 향상
                    #if defined(_MSC_VER)
                    _mm_pause();
                    #elif defined(__GNUC__) || defined(__clang__)
                    __builtin_ia32_pause();
                    #endif
                }
            }
        }

        // 락 해제
        void unlock()
        {
            mFlag.store(false, std::memory_order_release);
        }

        // 락 시도 (블로킹하지 않음)
        bool try_lock()
        {
            return !mFlag.exchange(true, std::memory_order_acquire);
        }
    };

    // RAII 스타일 락 가드
    class SpinLockGuard
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        SpinLock& mLock;
        
    public:
        // 생성자, 파괴자
        explicit SpinLockGuard(SpinLock& lock) : mLock(lock)
        {
            mLock.lock();
        }

        ~SpinLockGuard()
        {
            mLock.unlock();
        }

        // 복사/이동 불가
        SpinLockGuard(const SpinLockGuard&) = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;
    };

} // namespace KanchoNet

