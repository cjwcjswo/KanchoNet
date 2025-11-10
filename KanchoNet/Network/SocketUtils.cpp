#include "SocketUtils.h"
#include "../Utils/Logger.h"

#ifdef KANCHONET_PLATFORM_WINDOWS
    #include <WS2tcpip.h>
    #include <MSTCPIP.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "mswsock.lib")
    
    // Windows TCP Keep-Alive 구조체 정의 (MSTCPIP.h에 있지만 명시적으로 정의)
    #ifndef SIO_KEEPALIVE_VALS
        #define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR, 4)
    #endif
    
    #ifndef TCP_KEEPALIVE
        struct tcp_keepalive {
            ULONG onoff;
            ULONG keepalivetime;
            ULONG keepaliveinterval;
        };
    #endif
#elif defined(KANCHONET_PLATFORM_LINUX)
    #include <sys/ioctl.h>
    #include <netinet/tcp.h>
#endif

namespace KanchoNet
{
    #ifdef KANCHONET_PLATFORM_WINDOWS
    // Windows Static 멤버 변수 초기화
    LPFN_ACCEPTEX SocketUtils::mLpfnAcceptEx = nullptr;
    LPFN_CONNECTEX SocketUtils::mLpfnConnectEx = nullptr;
    LPFN_DISCONNECTEX SocketUtils::mLpfnDisconnectEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS SocketUtils::mLpfnGetAcceptExSockaddrs = nullptr;
    #endif

    bool SocketUtils::InitializeNetwork()
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0)
            {
                LOG_ERROR("WSAStartup failed. Error: %d", result);
                return false;
            }
            LOG_INFO("Winsock initialized. Version: %d.%d", 
                     LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
            return true;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            // Linux는 별도 초기화 불필요
            LOG_INFO("Network initialized (Linux)");
            return true;
        #endif
    }

    void SocketUtils::CleanupNetwork()
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            WSACleanup();
            LOG_INFO("Winsock cleaned up");
        #elif defined(KANCHONET_PLATFORM_LINUX)
            LOG_INFO("Network cleaned up (Linux)");
        #endif
    }

    SocketHandle SocketUtils::CreateTCPSocket()
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
                                     nullptr, 0, WSA_FLAG_OVERLAPPED);
            if (sock == INVALID_SOCKET)
            {
                LOG_ERROR("Failed to create socket. Error: %d", GetLastSocketError());
                return INVALID_SOCKET_HANDLE;
            }
            return sock;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock < 0)
            {
                LOG_ERROR("Failed to create socket. Error: %d", GetLastSocketError());
                return INVALID_SOCKET_HANDLE;
            }
            return sock;
        #endif
    }

    bool SocketUtils::SetSocketOption(SocketHandle socket, const EngineConfig& config)
    {
        // Nagle 알고리즘 설정
        if (!SetNoDelay(socket, config.mNoDelay))
        {
            LOG_WARNING("Failed to set NoDelay option");
        }

        // Keep-Alive 설정
        if (!SetKeepAlive(socket, config.mKeepAlive, config.mKeepAliveTime, config.mKeepAliveInterval))
        {
            LOG_WARNING("Failed to set KeepAlive option");
        }

        // 송수신 버퍼 크기 설정
        if (!SetSendBufferSize(socket, static_cast<int>(config.mSendBufferSize)))
        {
            LOG_WARNING("Failed to set send buffer size");
        }

        if (!SetRecvBufferSize(socket, static_cast<int>(config.mRecvBufferSize)))
        {
            LOG_WARNING("Failed to set recv buffer size");
        }

        // 주소 재사용 설정
        if (!SetReuseAddress(socket, true))
        {
            LOG_WARNING("Failed to set ReuseAddress option");
        }

        return true;
    }

    bool SocketUtils::SetNonBlocking(SocketHandle socket, bool nonBlocking)
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            u_long mode = nonBlocking ? 1 : 0;
            int result = ioctlsocket(socket, FIONBIO, &mode);
            return result == 0;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int flags = fcntl(socket, F_GETFL, 0);
            if (flags < 0) return false;
            
            if (nonBlocking)
                flags |= O_NONBLOCK;
            else
                flags &= ~O_NONBLOCK;
                
            return fcntl(socket, F_SETFL, flags) == 0;
        #endif
    }

    bool SocketUtils::SetReuseAddress(SocketHandle socket, bool reuse)
    {
        int optval = reuse ? 1 : 0;
        #ifdef KANCHONET_PLATFORM_WINDOWS
            int result = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, 
                                    (const char*)&optval, sizeof(optval));
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int result = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, 
                                    &optval, sizeof(optval));
        #endif
        return result == 0;
    }

    bool SocketUtils::SetNoDelay(SocketHandle socket, bool noDelay)
    {
        int optval = noDelay ? 1 : 0;
        #ifdef KANCHONET_PLATFORM_WINDOWS
            int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, 
                                    (const char*)&optval, sizeof(optval));
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, 
                                    &optval, sizeof(optval));
        #endif
        return result == 0;
    }

    bool SocketUtils::SetKeepAlive(SocketHandle socket, bool enable, uint32_t time, uint32_t interval)
    {
        int optval = enable ? 1 : 0;
        
        #ifdef KANCHONET_PLATFORM_WINDOWS
            int result = setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, 
                                    (const char*)&optval, sizeof(optval));
            
            if (result != 0 || !enable)
                return result == 0;

            // TCP Keep-Alive 상세 설정 (Windows 전용)
            tcp_keepalive keepalive;
            keepalive.onoff = 1;
            keepalive.keepalivetime = time;
            keepalive.keepaliveinterval = interval;

            DWORD bytesReturned;
            result = WSAIoctl(socket, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
                             nullptr, 0, &bytesReturned, nullptr, nullptr);
            return result == 0;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int result = setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, 
                                    &optval, sizeof(optval));
            
            if (result != 0 || !enable)
                return result == 0;

            // TCP Keep-Alive 상세 설정 (Linux)
            int keepIdle = time / 1000; // ms to seconds
            int keepInterval = interval / 1000;
            int keepCount = 5;
            
            setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
            setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
            setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));
            
            return true;
        #endif
    }

    bool SocketUtils::SetSendBufferSize(SocketHandle socket, int size)
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            int result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, 
                                    (const char*)&size, sizeof(size));
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, 
                                    &size, sizeof(size));
        #endif
        return result == 0;
    }

    bool SocketUtils::SetRecvBufferSize(SocketHandle socket, int size)
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            int result = setsockopt(socket, SOL_SOCKET, SO_RCVBUF, 
                                    (const char*)&size, sizeof(size));
        #elif defined(KANCHONET_PLATFORM_LINUX)
            int result = setsockopt(socket, SOL_SOCKET, SO_RCVBUF, 
                                    &size, sizeof(size));
        #endif
        return result == 0;
    }

    bool SocketUtils::BindSocket(SocketHandle socket, uint16_t port)
    {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int result = bind(socket, (sockaddr*)&addr, sizeof(addr));
        
        #ifdef KANCHONET_PLATFORM_WINDOWS
            if (result == SOCKET_ERROR)
        #elif defined(KANCHONET_PLATFORM_LINUX)
            if (result < 0)
        #endif
        {
            LOG_ERROR("bind failed. Port: %u, Error: %d", port, GetLastSocketError());
            return false;
        }

        LOG_INFO("Socket bound to port %u", port);
        return true;
    }

    bool SocketUtils::ListenSocket(SocketHandle socket, int backlog)
    {
        int result = listen(socket, backlog);
        
        #ifdef KANCHONET_PLATFORM_WINDOWS
            if (result == SOCKET_ERROR)
        #elif defined(KANCHONET_PLATFORM_LINUX)
            if (result < 0)
        #endif
        {
            LOG_ERROR("listen failed. Backlog: %d, Error: %d", backlog, GetLastSocketError());
            return false;
        }

        LOG_INFO("Socket listening. Backlog: %d", backlog);
        return true;
    }

    void SocketUtils::CloseSocket(SocketHandle socket)
    {
        if (socket != INVALID_SOCKET_HANDLE)
        {
            #ifdef KANCHONET_PLATFORM_WINDOWS
                closesocket(socket);
            #elif defined(KANCHONET_PLATFORM_LINUX)
                close(socket);
            #endif
        }
    }

    void SocketUtils::ShutdownSocket(SocketHandle socket)
    {
        if (socket != INVALID_SOCKET_HANDLE)
        {
            #ifdef KANCHONET_PLATFORM_WINDOWS
                shutdown(socket, SD_BOTH);
            #elif defined(KANCHONET_PLATFORM_LINUX)
                shutdown(socket, SHUT_RDWR);
            #endif
        }
    }

    int SocketUtils::GetLastSocketError()
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            return WSAGetLastError();
        #elif defined(KANCHONET_PLATFORM_LINUX)
            return errno;
        #endif
    }

    const char* SocketUtils::GetSocketErrorString(int errorCode)
    {
        #ifdef KANCHONET_PLATFORM_WINDOWS
            static char buffer[256];
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          nullptr, errorCode, 0, buffer, sizeof(buffer), nullptr);
            return buffer;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            return strerror(errorCode);
        #endif
    }
    
    #ifdef KANCHONET_PLATFORM_WINDOWS

    bool SocketUtils::LoadExtensionFunctions(SOCKET socket)
    {
        GUID guidAcceptEx = WSAID_ACCEPTEX;
        GUID guidConnectEx = WSAID_CONNECTEX;
        GUID guidDisconnectEx = WSAID_DISCONNECTEX;
        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

        DWORD bytes;

        // AcceptEx
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidAcceptEx, sizeof(guidAcceptEx),
                    &mLpfnAcceptEx, sizeof(mLpfnAcceptEx),
                    &bytes, nullptr, nullptr) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to load AcceptEx. Error: %d", GetLastSocketError());
            return false;
        }

        // ConnectEx
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidConnectEx, sizeof(guidConnectEx),
                    &mLpfnConnectEx, sizeof(mLpfnConnectEx),
                    &bytes, nullptr, nullptr) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to load ConnectEx. Error: %d", GetLastSocketError());
            return false;
        }

        // DisconnectEx
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidDisconnectEx, sizeof(guidDisconnectEx),
                    &mLpfnDisconnectEx, sizeof(mLpfnDisconnectEx),
                    &bytes, nullptr, nullptr) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to load DisconnectEx. Error: %d", GetLastSocketError());
            return false;
        }

        // GetAcceptExSockaddrs
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
                    &mLpfnGetAcceptExSockaddrs, sizeof(mLpfnGetAcceptExSockaddrs),
                    &bytes, nullptr, nullptr) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to load GetAcceptExSockaddrs. Error: %d", GetLastSocketError());
            return false;
        }

        LOG_INFO("Socket extension functions loaded successfully");
        return true;
    }
    #endif // KANCHONET_PLATFORM_WINDOWS

} // namespace KanchoNet

