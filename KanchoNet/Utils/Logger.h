#pragma once

#include "../Types.h"
#include <string>
#include <mutex>

namespace KanchoNet
{
    // 로그 레벨
    enum class LogLevel : uint8_t
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };

    // 간단한 로거 (디버그 및 기본 로깅용)
    class Logger
    {
    public:
        static Logger& GetInstance();

        // 로그 레벨 설정
        void SetLogLevel(LogLevel level) { currentLevel_ = level; }
        LogLevel GetLogLevel() const { return currentLevel_; }

        // 로그 출력
        void Log(LogLevel level, const char* format, ...);
        void LogDebug(const char* format, ...);
        void LogInfo(const char* format, ...);
        void LogWarning(const char* format, ...);
        void LogError(const char* format, ...);
        void LogCritical(const char* format, ...);

        // 에러 코드 로그
        void LogErrorCode(ErrorCode errorCode, const char* context = nullptr);

    private:
        Logger();
        ~Logger() = default;

        // 복사/이동 불가
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        const char* GetLogLevelString(LogLevel level) const;
        const char* GetErrorCodeString(ErrorCode errorCode) const;

        LogLevel currentLevel_;
        std::mutex mutex_;
    };

    // 편의 매크로
    #define LOG_DEBUG(format, ...) KanchoNet::Logger::GetInstance().LogDebug(format, ##__VA_ARGS__)
    #define LOG_INFO(format, ...) KanchoNet::Logger::GetInstance().LogInfo(format, ##__VA_ARGS__)
    #define LOG_WARNING(format, ...) KanchoNet::Logger::GetInstance().LogWarning(format, ##__VA_ARGS__)
    #define LOG_ERROR(format, ...) KanchoNet::Logger::GetInstance().LogError(format, ##__VA_ARGS__)
    #define LOG_CRITICAL(format, ...) KanchoNet::Logger::GetInstance().LogCritical(format, ##__VA_ARGS__)

} // namespace KanchoNet

