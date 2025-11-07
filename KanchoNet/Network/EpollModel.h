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
    class EpollModel : public NonCopyable
    {
    public:
        EpollModel();
        ~EpollModel();

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

        // 멤버 변수
        bool initialized_;
        bool running_;
        
        EngineConfig config_;
        SocketHandle listenSocket_;
        int epollFd_;
        
        std::unique_ptr<SessionManager> sessionManager_;
        std::unordered_map<SocketHandle, Session*> socketToSession_;
        
        // 콜백 함수들
        std::function<void(Session*)> onAccept_;
        std::function<void(Session*, const uint8_t*, size_t)> onReceive_;
        std::function<void(Session*)> onDisconnect_;
        std::function<void(Session*, ErrorCode)> onError_;
        
        // 버퍼
        static constexpr size_t MAX_EVENTS = 128;
        uint8_t receiveBuffer_[DEFAULT_BUFFER_SIZE];
    };

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

