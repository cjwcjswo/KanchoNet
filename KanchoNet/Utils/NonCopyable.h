#pragma once

namespace KanchoNet
{
    // 복사 생성자와 대입 연산자를 삭제하는 베이스 클래스
    // 상속받아 사용하면 해당 클래스의 복사가 금지됨
    class NonCopyable
    {
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;

    public:
        // 복사 생성자 삭제
        NonCopyable(const NonCopyable&) = delete;
        
        // 복사 대입 연산자 삭제
        NonCopyable& operator=(const NonCopyable&) = delete;
        
        // 이동 생성자와 이동 대입 연산자는 허용
        NonCopyable(NonCopyable&&) = default;
        NonCopyable& operator=(NonCopyable&&) = default;
    };

} // namespace KanchoNet

