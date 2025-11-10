#include "Logger.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <chrono>

namespace KanchoNet
{
    Logger& Logger::GetInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger()
        : mCurrentLevel(LogLevel::Info)
    {
    }

    void Logger::Log(LogLevel level, const char* format, ...)
    {
        if (level < mCurrentLevel)
            return;

        std::lock_guard<std::mutex> lock(mMutex);

        // 타임스탬프
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        struct tm timeInfo;
        localtime_s(&timeInfo, &timeT);

        char timeBuffer[64];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeInfo);

        // 로그 레벨
        const char* levelStr = GetLogLevelString(level);

        // 출력
        printf("[%s] [%s] ", timeBuffer, levelStr);

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        printf("\n");
        fflush(stdout);
    }

    void Logger::LogDebug(const char* format, ...)
    {
        if (LogLevel::Debug < mCurrentLevel)
            return;

        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Log(LogLevel::Debug, "%s", buffer);
        
        va_end(args);
    }

    void Logger::LogInfo(const char* format, ...)
    {
        if (LogLevel::Info < mCurrentLevel)
            return;

        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Log(LogLevel::Info, "%s", buffer);
        
        va_end(args);
    }

    void Logger::LogWarning(const char* format, ...)
    {
        if (LogLevel::Warning < mCurrentLevel)
            return;

        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Log(LogLevel::Warning, "%s", buffer);
        
        va_end(args);
    }

    void Logger::LogError(const char* format, ...)
    {
        if (LogLevel::Error < mCurrentLevel)
            return;

        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Log(LogLevel::Error, "%s", buffer);
        
        va_end(args);
    }

    void Logger::LogCritical(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Log(LogLevel::Critical, "%s", buffer);
        
        va_end(args);
    }

    void Logger::LogErrorCode(ErrorCode errorCode, const char* context)
    {
        if (context)
        {
            LogError("ErrorCode: %s, Context: %s", GetErrorCodeString(errorCode), context);
        }
        else
        {
            LogError("ErrorCode: %s", GetErrorCodeString(errorCode));
        }
    }

    const char* Logger::GetLogLevelString(LogLevel level) const
    {
        switch (level)
        {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARNING";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN";
        }
    }

    const char* Logger::GetErrorCodeString(ErrorCode errorCode) const
    {
        switch (errorCode)
        {
        case ErrorCode::None:                       return "None";
        case ErrorCode::InitializeFailed:           return "InitializeFailed";
        case ErrorCode::AlreadyInitialized:         return "AlreadyInitialized";
        case ErrorCode::NotInitialized:             return "NotInitialized";
        case ErrorCode::SocketCreateFailed:         return "SocketCreateFailed";
        case ErrorCode::SocketBindFailed:           return "SocketBindFailed";
        case ErrorCode::SocketListenFailed:         return "SocketListenFailed";
        case ErrorCode::SocketAcceptFailed:         return "SocketAcceptFailed";
        case ErrorCode::SocketConnectFailed:        return "SocketConnectFailed";
        case ErrorCode::SocketCloseFailed:          return "SocketCloseFailed";
        case ErrorCode::SocketOptionFailed:         return "SocketOptionFailed";
        case ErrorCode::SendFailed:                 return "SendFailed";
        case ErrorCode::ReceiveFailed:              return "ReceiveFailed";
        case ErrorCode::DisconnectFailed:           return "DisconnectFailed";
        case ErrorCode::IOCPCreateFailed:           return "IOCPCreateFailed";
        case ErrorCode::IOCPAssociateFailed:        return "IOCPAssociateFailed";
        case ErrorCode::IOCPGetQueuedFailed:        return "IOCPGetQueuedFailed";
        case ErrorCode::IOCPPostQueuedFailed:       return "IOCPPostQueuedFailed";
        case ErrorCode::RIONotSupported:            return "RIONotSupported";
        case ErrorCode::RIOLoadFailed:              return "RIOLoadFailed";
        case ErrorCode::RIOCreateCQFailed:          return "RIOCreateCQFailed";
        case ErrorCode::RIOCreateRQFailed:          return "RIOCreateRQFailed";
        case ErrorCode::RIORegisterBufferFailed:    return "RIORegisterBufferFailed";
        case ErrorCode::RIODequeueCompletionFailed: return "RIODequeueCompletionFailed";
        case ErrorCode::SessionNotFound:            return "SessionNotFound";
        case ErrorCode::SessionLimitReached:        return "SessionLimitReached";
        case ErrorCode::InvalidSessionState:        return "InvalidSessionState";
        case ErrorCode::BufferOverflow:             return "BufferOverflow";
        case ErrorCode::BufferUnderflow:            return "BufferUnderflow";
        case ErrorCode::InvalidBufferSize:          return "InvalidBufferSize";
        case ErrorCode::InvalidParameter:           return "InvalidParameter";
        case ErrorCode::OutOfMemory:                return "OutOfMemory";
        case ErrorCode::OperationAborted:           return "OperationAborted";
        case ErrorCode::TimeoutError:               return "TimeoutError";
        case ErrorCode::UnknownError:               return "UnknownError";
        default:                                    return "UnknownErrorCode";
        }
    }

} // namespace KanchoNet

