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
    class IOCPModel : public INetworkModel, public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        bool mInitialized;
        bool mRunning;
        
        EngineConfig mConfig;
        SOCKET mListenSocket;
        HANDLE mIocpHandle;
        
        std::unique_ptr<SessionManager> mSessionManager;
        
        // 콜백 함수들
        std::function<void(Session*)> mOnAccept;
        std::function<void(Session*, const uint8_t*, size_t)> mOnReceive;
        std::function<void(Session*)> mOnDisconnect;
        std::function<void(Session*, ErrorCode)> mOnError;
        
    public:
        // 생성자, 파괴자
        IOCPModel();
        ~IOCPModel();
        
    public:
        // public 함수
        // INetworkModel 인터페이스 구현
        bool Initialize(const EngineConfig& config) override;
        bool StartListen() override;
        bool ProcessIO(uint32_t timeoutMs = 0) override;
        bool Send(Session* session, const PacketBuffer& buffer) override;
        void Shutdown() override;

        // 콜백 설정
        void SetAcceptCallback(std::function<void(Session*)> callback) override;
        void SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback) override;
        void SetDisconnectCallback(std::function<void(Session*)> callback) override;
        void SetErrorCallback(std::function<void(Session*, ErrorCode)> callback) override;

        // 상태 확인
        bool IsInitialized() const { return mInitialized; }
        bool IsRunning() const { return mRunning; }

    private:
        // private 함수
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
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

