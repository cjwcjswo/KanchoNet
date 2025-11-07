#pragma once

#include "../Platform.h"

// RIO는 Windows 전용
#ifdef KANCHONET_PLATFORM_WINDOWS

#include "../Core/INetworkModel.h"
#include "../Session/SessionManager.h"
#include "../Utils/NonCopyable.h"
#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>

namespace KanchoNet
{
    // RIO (Registered I/O) 네트워크 모델
    // Windows 8 이상에서 지원되는 고성능 I/O 모델
    class RIOModel : public NonCopyable
    {
    public:
        RIOModel();
        ~RIOModel();

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
        bool IsRIOSupported() const;

    private:
        // RIO 버퍼 정보
        struct RIOBufferInfo
        {
            RIO_BUFFERID bufferID;
            uint8_t* buffer;
            size_t bufferSize;
            size_t bufferCount;
        };

        // RIO 컨텍스트
        struct RIOContext
        {
            IOOperation operation;
            Session* session;
            RIO_BUF rioBuf;
        };

        // 내부 함수들
        bool LoadRIOFunctions();
        bool CreateRIOResources();
        bool RegisterBuffer(RIOBufferInfo& bufferInfo, size_t bufferSize, size_t bufferCount);
        void DeregisterBuffer(RIOBufferInfo& bufferInfo);
        
        bool PostAccept();
        bool PostReceive(Session* session);
        bool PostSend(Session* session);
        
        void ProcessCompletions();
        void ProcessAcceptCompletion(RIOContext* context);
        void ProcessReceiveCompletion(RIOContext* context, ULONG bytesTransferred);
        void ProcessSendCompletion(RIOContext* context, ULONG bytesTransferred);
        
        void CloseSession(Session* session);
        
        RIOContext* AllocateContext();
        void DeallocateContext(RIOContext* context);

        // 멤버 변수
        bool initialized_;
        bool running_;
        
        EngineConfig config_;
        SOCKET listenSocket_;
        
        // RIO Extension Functions
        RIO_EXTENSION_FUNCTION_TABLE rioFunctions_;
        RIO_CQ completionQueue_;
        OVERLAPPED overlapped_;
        
        // RIO 버퍼
        RIOBufferInfo recvBufferInfo_;
        RIOBufferInfo sendBufferInfo_;
        
        std::unique_ptr<SessionManager> sessionManager_;
        
        // 콜백 함수들
        std::function<void(Session*)> onAccept_;
        std::function<void(Session*, const uint8_t*, size_t)> onReceive_;
        std::function<void(Session*)> onDisconnect_;
        std::function<void(Session*, ErrorCode)> onError_;
        
        // RIO 지원 여부
        static bool rioSupportChecked_;
        static bool rioSupported_;
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

