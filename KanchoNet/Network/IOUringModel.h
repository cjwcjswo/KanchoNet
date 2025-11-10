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
    class IOUringModel : public INetworkModel, public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        bool mInitialized;
        bool mRunning;
        
        EngineConfig mConfig;
        SocketHandle mListenSocket;
        
        struct io_uring mRing;
        bool mRingInitialized;
        
        std::unique_ptr<SessionManager> mSessionManager;
        std::unordered_map<SocketHandle, Session*> mSocketToSession;
        
        // 콜백 함수들
        std::function<void(Session*)> mOnAccept;
        std::function<void(Session*, const uint8_t*, size_t)> mOnReceive;
        std::function<void(Session*)> mOnDisconnect;
        std::function<void(Session*, ErrorCode)> mOnError;
        
        // io_uring 지원 여부
        static bool mIOUringSupportChecked;
        static bool mIOUringSupported;
        
    public:
        // 생성자, 파괴자
        IOUringModel();
        ~IOUringModel();
        
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
        static bool IsIOUringSupported();

    private:
        // private 함수
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
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

