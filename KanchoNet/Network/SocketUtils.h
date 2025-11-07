#pragma once

#include "../Platform.h"
#include "../Types.h"
#include "../Core/EngineConfig.h"

#ifdef KANCHONET_PLATFORM_WINDOWS
    #include <WinSock2.h>
    #include <MSWSock.h>
    #include <Windows.h>
#elif defined(KANCHONET_PLATFORM_LINUX)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <cstring>
#endif

namespace KanchoNet
{
    // 소켓 유틸리티 함수들 (플랫폼 독립적 인터페이스)
    class SocketUtils
    {
    public:
        // 네트워크 초기화/종료
        static bool InitializeNetwork();
        static void CleanupNetwork();

        // 소켓 생성
        static SocketHandle CreateTCPSocket();
        
        // 소켓 설정
        static bool SetSocketOption(SocketHandle socket, const EngineConfig& config);
        static bool SetNonBlocking(SocketHandle socket, bool nonBlocking);
        static bool SetReuseAddress(SocketHandle socket, bool reuse);
        static bool SetNoDelay(SocketHandle socket, bool noDelay);
        static bool SetKeepAlive(SocketHandle socket, bool enable, uint32_t time, uint32_t interval);
        static bool SetSendBufferSize(SocketHandle socket, int size);
        static bool SetRecvBufferSize(SocketHandle socket, int size);
        
        // 소켓 바인드/리슨
        static bool BindSocket(SocketHandle socket, uint16_t port);
        static bool ListenSocket(SocketHandle socket, int backlog);
        
        // 소켓 닫기
        static void CloseSocket(SocketHandle socket);
        static void ShutdownSocket(SocketHandle socket);
        
        // 에러 처리
        static int GetLastSocketError();
        static const char* GetSocketErrorString(int errorCode);

    #ifdef KANCHONET_PLATFORM_WINDOWS
        // Windows Extension Functions 로드 (AcceptEx, ConnectEx 등)
        static bool LoadExtensionFunctions(SOCKET socket);
        
        // Windows Extension Functions
        static LPFN_ACCEPTEX GetAcceptEx() { return lpfnAcceptEx_; }
        static LPFN_CONNECTEX GetConnectEx() { return lpfnConnectEx_; }
        static LPFN_DISCONNECTEX GetDisconnectEx() { return lpfnDisconnectEx_; }
        static LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs() { return lpfnGetAcceptExSockaddrs_; }

    private:
        static LPFN_ACCEPTEX lpfnAcceptEx_;
        static LPFN_CONNECTEX lpfnConnectEx_;
        static LPFN_DISCONNECTEX lpfnDisconnectEx_;
        static LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs_;
    #endif
    };

} // namespace KanchoNet

