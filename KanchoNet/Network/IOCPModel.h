#pragma once

#include "../Platform.h"

// IOCP는 Windows 전용
#ifdef KANCHONET_PLATFORM_WINDOWS

#include "../Core/INetworkModel.h"
#include "../Session/SessionManager.h"
#include "../Utils/NonCopyable.h"
#include <WinSock2.h>
#include <Windows.h>
#include <functional>
#include <memory>

namespace KanchoNet
{
    // IOCP (I/O Completion Port) 네트워크 모델
    class IOCPModel : public NonCopyable
    {
    public:
        IOCPModel();
        ~IOCPModel();

        // INetworkModel 인터페이스 구현
        bool Initialize(const EngineConfig& config);
        bool StartListen();
        bool ProcessIO(uint32_t timeoutMs = 0);
        bool Send(Session* session, const PacketBuffer& buffer);
        void Shutdown();

        // 콜백 설정
        void SetAcceptCallback(std::function<void(Session*)> callback);
        void SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback);
        void SetDisconnectCallback(std::function<void(Session*)> callback);
        void SetErrorCallback(std::function<void(Session*, ErrorCode)> callback);

        // 상태 확인
        bool IsInitialized() const { return initialized_; }
        bool IsRunning() const { return running_; }

    private:
        // Overlapped I/O 컨텍스트
        struct OverlappedContext
        {
            OVERLAPPED overlapped;
            IOOperation operation;
            Session* session;
            WSABUF wsaBuf;
            uint8_t buffer[DEFAULT_BUFFER_SIZE];
            SOCKET acceptSocket; // Accept 전용
        };

        // 내부 함수들
        bool CreateIOCP();
        bool RegisterAccept();
        void ProcessAccept(OverlappedContext* context, DWORD bytesTransferred);
        void ProcessReceive(OverlappedContext* context, DWORD bytesTransferred);
        void ProcessSend(OverlappedContext* context, DWORD bytesTransferred);
        void ProcessDisconnect(OverlappedContext* context);
        
        bool PostReceive(Session* session);
        bool PostSend(Session* session);
        
        void CloseSession(Session* session);
        
        OverlappedContext* AllocateContext();
        void DeallocateContext(OverlappedContext* context);

        // 멤버 변수
        bool initialized_;
        bool running_;
        
        EngineConfig config_;
        SOCKET listenSocket_;
        HANDLE iocpHandle_;
        
        std::unique_ptr<SessionManager> sessionManager_;
        
        // 콜백 함수들
        std::function<void(Session*)> onAccept_;
        std::function<void(Session*, const uint8_t*, size_t)> onReceive_;
        std::function<void(Session*)> onDisconnect_;
        std::function<void(Session*, ErrorCode)> onError_;
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

