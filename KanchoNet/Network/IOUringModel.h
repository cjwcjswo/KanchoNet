#pragma once

#include "../Platform.h"

// io_uring은 Linux 전용 (커널 5.1+)
#ifdef KANCHONET_PLATFORM_LINUX

#include "../Core/INetworkModel.h"
#include "../Session/SessionManager.h"
#include "../Utils/NonCopyable.h"
#include <liburing.h>
#include <functional>
#include <memory>
#include <unordered_map>

namespace KanchoNet
{
    // io_uring 네트워크 모델 (Linux, Kernel 5.1+)
    class IOUringModel : public NonCopyable
    {
    public:
        IOUringModel();
        ~IOUringModel();

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
        static bool IsIOUringSupported();

    private:
        // io_uring 컨텍스트
        struct IOUringContext
        {
            IOOperation operation;
            Session* session;
            uint8_t* buffer;
            size_t bufferSize;
        };

        // 내부 함수들
        bool CreateIOUring();
        bool SubmitAccept();
        bool SubmitReceive(Session* session);
        bool SubmitSend(Session* session);
        
        void ProcessCompletion(struct io_uring_cqe* cqe);
        void ProcessAcceptCompletion(IOUringContext* ctx, int result);
        void ProcessReceiveCompletion(IOUringContext* ctx, int result);
        void ProcessSendCompletion(IOUringContext* ctx, int result);
        
        void CloseSession(Session* session);
        
        IOUringContext* AllocateContext();
        void DeallocateContext(IOUringContext* ctx);

        // 멤버 변수
        bool initialized_;
        bool running_;
        
        EngineConfig config_;
        SocketHandle listenSocket_;
        
        struct io_uring ring_;
        bool ringInitialized_;
        
        std::unique_ptr<SessionManager> sessionManager_;
        std::unordered_map<SocketHandle, Session*> socketToSession_;
        
        // 콜백 함수들
        std::function<void(Session*)> onAccept_;
        std::function<void(Session*, const uint8_t*, size_t)> onReceive_;
        std::function<void(Session*)> onDisconnect_;
        std::function<void(Session*, ErrorCode)> onError_;
        
        // io_uring 지원 여부
        static bool iouringSupportChecked_;
        static bool iouringSupported_;
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

