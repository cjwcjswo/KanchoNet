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
    class RIOModel : public INetworkModel, public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 함수 - 구조체 정의 (멤버 변수 선언 전에 정의 필요)
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
        
        // private 멤버변수
        bool mInitialized;
        bool mRunning;
        
        EngineConfig mConfig;
        SOCKET mListenSocket;
        
        // RIO Extension Functions
        RIO_EXTENSION_FUNCTION_TABLE mRioFunctions;
        RIO_CQ mCompletionQueue;
        OVERLAPPED mOverlapped;
        
        // RIO 버퍼
        RIOBufferInfo mRecvBufferInfo;
        RIOBufferInfo mSendBufferInfo;
        
        std::unique_ptr<SessionManager> mSessionManager;
        
        // 콜백 함수들
        std::function<void(Session*)> mOnAccept;
        std::function<void(Session*, const uint8_t*, size_t)> mOnReceive;
        std::function<void(Session*)> mOnDisconnect;
        std::function<void(Session*, ErrorCode)> mOnError;
        
        // RIO 지원 여부
        static bool mRioSupportChecked;
        static bool mRioSupported;
        
    public:
        // 생성자, 파괴자
        RIOModel();
        ~RIOModel();
        
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
        bool IsRIOSupported() const;

    private:
        // private 함수
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
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

