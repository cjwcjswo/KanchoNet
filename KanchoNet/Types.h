#pragma once

#include "Platform.h"
#include <cstdint>
#include <memory>

// 플랫폼별 헤더
#ifdef KANCHONET_PLATFORM_WINDOWS
    #include <WinSock2.h>
    #include <Windows.h>
#endif

#ifdef KANCHONET_PLATFORM_LINUX
    // Linux에서는 소켓이 파일 디스크립터(int)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace KanchoNet
{
    // 세션 ID 타입 (플랫폼 독립적)
    using SessionID = uint64_t;

    // 플랫폼별 소켓 핸들 타입
    #ifdef KANCHONET_PLATFORM_WINDOWS
        using SocketHandle = SOCKET;
        using FileHandle = HANDLE;
        constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
        constexpr FileHandle INVALID_FILE_HANDLE = INVALID_HANDLE_VALUE;
    #elif defined(KANCHONET_PLATFORM_LINUX)
        using SocketHandle = int;
        using FileHandle = int;
        constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
        constexpr FileHandle INVALID_FILE_HANDLE = -1;
    #endif

    // 상수 정의
    constexpr SessionID INVALID_SESSION_ID = 0;

    // 기본 버퍼 크기
    constexpr size_t DEFAULT_BUFFER_SIZE = 8192;           // 8KB
    constexpr size_t DEFAULT_SEND_BUFFER_SIZE = 65536;     // 64KB
    constexpr size_t DEFAULT_RECV_BUFFER_SIZE = 65536;     // 64KB

    // 기본 설정값
    constexpr uint16_t DEFAULT_PORT = 9000;
    constexpr uint32_t DEFAULT_MAX_SESSIONS = 10000;
    constexpr uint32_t DEFAULT_BACKLOG = 200;
    constexpr uint32_t DEFAULT_WORKER_THREADS = 0; // 0 = 어플리케이션이 관리

    // 에러 코드
    enum class ErrorCode : int32_t
    {
        None = 0,
        
        // 초기화 관련
        InitializeFailed = 1000,
        AlreadyInitialized = 1001,
        NotInitialized = 1002,
        
        // 소켓 관련
        SocketCreateFailed = 2000,
        SocketBindFailed = 2001,
        SocketListenFailed = 2002,
        SocketAcceptFailed = 2003,
        SocketConnectFailed = 2004,
        SocketCloseFailed = 2005,
        SocketOptionFailed = 2006,
        
        // 네트워크 I/O 관련
        SendFailed = 3000,
        ReceiveFailed = 3001,
        DisconnectFailed = 3002,
        
        // IOCP 관련
        IOCPCreateFailed = 4000,
        IOCPAssociateFailed = 4001,
        IOCPGetQueuedFailed = 4002,
        IOCPPostQueuedFailed = 4003,
        
        // RIO 관련 (Windows)
        RIONotSupported = 5000,
        RIOLoadFailed = 5001,
        RIOCreateCQFailed = 5002,
        RIOCreateRQFailed = 5003,
        RIORegisterBufferFailed = 5004,
        RIODequeueCompletionFailed = 5005,
        
        // epoll 관련 (Linux)
        EpollCreateFailed = 5100,
        EpollCtlFailed = 5101,
        EpollWaitFailed = 5102,
        
        // io_uring 관련 (Linux)
        IOUringNotSupported = 5200,
        IOUringSetupFailed = 5201,
        IOUringSubmitFailed = 5202,
        IOUringWaitFailed = 5203,
        IOUringRegisterFailed = 5204,
        
        // 세션 관련
        SessionNotFound = 6000,
        SessionLimitReached = 6001,
        InvalidSessionState = 6002,
        
        // 버퍼 관련
        BufferOverflow = 7000,
        BufferUnderflow = 7001,
        InvalidBufferSize = 7002,
        
        // 일반 에러
        InvalidParameter = 8000,
        OutOfMemory = 8001,
        OperationAborted = 8002,
        TimeoutError = 8003,
        UnknownError = 9999
    };

    // I/O 오퍼레이션 타입
    enum class IOOperation : uint8_t
    {
        Accept = 0,
        Receive = 1,
        Send = 2,
        Disconnect = 3
    };

    // 세션 상태
    enum class SessionState : uint8_t
    {
        Idle = 0,           // 초기 상태
        Connected = 1,      // 연결됨
        Disconnecting = 2,  // 연결 해제 중
        Disconnected = 3    // 연결 해제됨
    };

    // Forward declarations
    class Session;
    class PacketBuffer;
    class NetworkEngineBase;
    template<typename TNetworkModel>
    class NetworkEngine;

} // namespace KanchoNet

