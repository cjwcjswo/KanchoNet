#pragma once

#include <atomic>

namespace KanchoNet
{
    // 스핀락 (Spin Lock)
    // 짧은 시간 동안의 동기화에 적합 (뮤텍스보다 오버헤드가 적음)
    class SpinLock
    {
    public:
        SpinLock() : flag_(false) {}
        ~SpinLock() = default;

        // 복사/이동 불가
        SpinLock(const SpinLock&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;
        SpinLock(SpinLock&&) = delete;
        SpinLock& operator=(SpinLock&&) = delete;

        // 락 획득
        void lock()
        {
            while (flag_.exchange(true, std::memory_order_acquire))
            {
                // 스핀 대기
                while (flag_.load(std::memory_order_relaxed))
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
            flag_.store(false, std::memory_order_release);
        }

        // 락 시도 (블로킹하지 않음)
        bool try_lock()
        {
            return !flag_.exchange(true, std::memory_order_acquire);
        }

    private:
        std::atomic<bool> flag_;
    };

    // RAII 스타일 락 가드
    class SpinLockGuard
    {
    public:
        explicit SpinLockGuard(SpinLock& lock) : lock_(lock)
        {
            lock_.lock();
        }

        ~SpinLockGuard()
        {
            lock_.unlock();
        }

        // 복사/이동 불가
        SpinLockGuard(const SpinLockGuard&) = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;

    private:
        SpinLock& lock_;
    };

} // namespace KanchoNet

