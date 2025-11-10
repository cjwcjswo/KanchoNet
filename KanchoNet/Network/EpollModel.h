#pragma once

#include "../Platform.h"

// epoll은 Linux 전용
#ifdef KANCHONET_PLATFORM_LINUX

#include "../Core/INetworkModel.h"
#include "../Session/SessionManager.h"
#include "../Utils/NonCopyable.h"
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <unordered_map>

namespace KanchoNet
{
    // epoll 네트워크 모델 (Linux)
    class EpollModel : public INetworkModel, public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        bool mInitialized;
        bool mRunning;
        
        EngineConfig mConfig;
        SocketHandle mListenSocket;
        int mEpollFd;
        
        std::unique_ptr<SessionManager> mSessionManager;
        std::unordered_map<SocketHandle, Session*> mSocketToSession;
        
        // 콜백 함수들
        std::function<void(Session*)> mOnAccept;
        std::function<void(Session*, const uint8_t*, size_t)> mOnReceive;
        std::function<void(Session*)> mOnDisconnect;
        std::function<void(Session*, ErrorCode)> mOnError;
        
        // 버퍼
        static constexpr size_t MAX_EVENTS = 128;
        uint8_t mReceiveBuffer[DEFAULT_BUFFER_SIZE];
        
    public:
        // 생성자, 파괴자
        EpollModel();
        ~EpollModel();
        
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
        // epoll 이벤트 처리
        void ProcessAccept();
        void ProcessReceive(Session* session);
        void ProcessSend(Session* session);
        void ProcessDisconnect(Session* session);
        
        // 소켓 등록/제거
        bool RegisterSocket(SocketHandle socket, Session* session, uint32_t events);
        bool ModifySocket(SocketHandle socket, uint32_t events);
        bool UnregisterSocket(SocketHandle socket);
        
        // 세션 관리
        void CloseSession(Session* session);
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

