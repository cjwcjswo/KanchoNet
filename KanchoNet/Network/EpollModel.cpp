#include "EpollModel.h"

#ifdef KANCHONET_PLATFORM_LINUX

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <unistd.h>
#include <cstring>

namespace KanchoNet
{
    EpollModel::EpollModel()
        : initialized_(false)
        , running_(false)
        , listenSocket_(INVALID_SOCKET_HANDLE)
        , epollFd_(-1)
    {
    }

    EpollModel::~EpollModel()
    {
        Shutdown();
    }

    bool EpollModel::Initialize(const EngineConfig& config)
    {
        if (initialized_)
        {
            LOG_ERROR("EpollModel already initialized");
            return false;
        }

        config_ = config;

        // 네트워크 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // epoll 인스턴스 생성
        epollFd_ = epoll_create1(0);
        if (epollFd_ < 0)
        {
            LOG_ERROR("Failed to create epoll. Error: %d", SocketUtils::GetLastSocketError());
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 리슨 소켓 생성
        listenSocket_ = SocketUtils::CreateTCPSocket();
        if (listenSocket_ == INVALID_SOCKET_HANDLE)
        {
            close(epollFd_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 옵션 설정
        SocketUtils::SetSocketOption(listenSocket_, config_);
        SocketUtils::SetNonBlocking(listenSocket_, true);

        // 소켓 바인드
        if (!SocketUtils::BindSocket(listenSocket_, config_.port))
        {
            SocketUtils::CloseSocket(listenSocket_);
            close(epollFd_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        sessionManager_ = std::make_unique<SessionManager>(config_.maxSessions);

        initialized_ = true;
        LOG_INFO("EpollModel initialized successfully. Port: %u", config_.port);
        
        return true;
    }

    bool EpollModel::StartListen()
    {
        if (!initialized_)
        {
            LOG_ERROR("EpollModel not initialized");
            return false;
        }

        if (running_)
        {
            LOG_WARNING("EpollModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(listenSocket_, config_.backlog))
        {
            return false;
        }

        // epoll에 리슨 소켓 등록 (EPOLLIN: 읽기 이벤트, EPOLLET: Edge-Triggered)
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = nullptr; // 리슨 소켓은 nullptr로 표시
        ev.data.fd = listenSocket_;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenSocket_, &ev) < 0)
        {
            LOG_ERROR("Failed to add listen socket to epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        running_ = true;
        LOG_INFO("EpollModel started listening");
        
        return true;
    }

    bool EpollModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!running_)
        {
            return false;
        }

        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, timeoutMs);

        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return true; // 인터럽트는 에러가 아님
            }
            LOG_ERROR("epoll_wait failed. Error: %d", SocketUtils::GetLastSocketError());
            return false;
        }

        // 이벤트 처리
        for (int i = 0; i < nfds; ++i)
        {
            struct epoll_event& ev = events[i];

            // 리슨 소켓 이벤트
            if (ev.data.fd == listenSocket_)
            {
                ProcessAccept();
                continue;
            }

            // 클라이언트 소켓 이벤트
            Session* session = static_cast<Session*>(ev.data.ptr);
            if (!session)
            {
                continue;
            }

            // 에러 또는 연결 종료
            if (ev.events & (EPOLLERR | EPOLLHUP))
            {
                ProcessDisconnect(session);
                continue;
            }

            // 읽기 이벤트
            if (ev.events & EPOLLIN)
            {
                ProcessReceive(session);
            }

            // 쓰기 이벤트
            if (ev.events & EPOLLOUT)
            {
                ProcessSend(session);
            }
        }

        return true;
    }

    bool EpollModel::Send(Session* session, const PacketBuffer& buffer)
    {
        if (!session || buffer.IsEmpty())
        {
            return false;
        }

        SpinLockGuard lock(session->GetLock());
        
        // 송신 버퍼에 데이터 추가
        size_t written = session->GetSendBuffer().Write(buffer.GetData(), buffer.GetSize());
        if (written < buffer.GetSize())
        {
            LOG_WARNING("Send buffer overflow. SessionID: %llu", session->GetID());
            return false;
        }

        // 소켓을 쓰기 가능 이벤트로 등록
        if (!session->IsSending())
        {
            session->SetSending(true);
            ModifySocket(session->GetSocket(), EPOLLIN | EPOLLOUT | EPOLLET);
        }

        return true;
    }

    void EpollModel::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        running_ = false;

        // 세션 정리
        if (sessionManager_)
        {
            sessionManager_->ForEachSession([this](Session* session) {
                CloseSession(session);
            });
            sessionManager_->Clear();
        }

        socketToSession_.clear();

        // 리슨 소켓 닫기
        if (listenSocket_ != INVALID_SOCKET_HANDLE)
        {
            SocketUtils::CloseSocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET_HANDLE;
        }

        // epoll 닫기
        if (epollFd_ >= 0)
        {
            close(epollFd_);
            epollFd_ = -1;
        }

        // 네트워크 정리
        SocketUtils::CleanupNetwork();

        initialized_ = false;
        LOG_INFO("EpollModel shutdown completed");
    }

    void EpollModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        onAccept_ = callback;
    }

    void EpollModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        onReceive_ = callback;
    }

    void EpollModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        onDisconnect_ = callback;
    }

    void EpollModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        onError_ = callback;
    }

    void EpollModel::ProcessAccept()
    {
        // Edge-Triggered 모드에서는 모든 연결을 처리해야 함
        while (true)
        {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            
            SocketHandle clientSocket = accept(listenSocket_, 
                                               (struct sockaddr*)&clientAddr, 
                                               &addrLen);
            
            if (clientSocket < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 더 이상 받을 연결이 없음
                    break;
                }
                LOG_ERROR("accept failed. Error: %d", SocketUtils::GetLastSocketError());
                break;
            }

            // Non-blocking 설정
            SocketUtils::SetNonBlocking(clientSocket, true);

            // 세션 생성
            SessionConfig sessionConfig;
            Session* session = sessionManager_->AddSession(clientSocket, sessionConfig);
            if (!session)
            {
                LOG_WARNING("Failed to add session. Session limit reached.");
                close(clientSocket);
                continue;
            }

            session->SetState(SessionState::Connected);
            socketToSession_[clientSocket] = session;

            // epoll에 클라이언트 소켓 등록
            if (!RegisterSocket(clientSocket, session, EPOLLIN | EPOLLET))
            {
                CloseSession(session);
                continue;
            }

            // Accept 콜백 호출
            if (onAccept_)
            {
                onAccept_(session);
            }

            LOG_DEBUG("Client accepted. SessionID: %llu", session->GetID());
        }
    }

    void EpollModel::ProcessReceive(Session* session)
    {
        if (!session || !session->IsConnected())
        {
            return;
        }

        // Edge-Triggered 모드에서는 버퍼가 빌 때까지 읽어야 함
        while (true)
        {
            ssize_t bytesRead = recv(session->GetSocket(), 
                                     receiveBuffer_, 
                                     sizeof(receiveBuffer_), 
                                     0);

            if (bytesRead > 0)
            {
                // 데이터 수신 성공
                if (onReceive_)
                {
                    onReceive_(session, receiveBuffer_, bytesRead);
                }
            }
            else if (bytesRead == 0)
            {
                // 연결 종료
                ProcessDisconnect(session);
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 더 이상 읽을 데이터가 없음
                    break;
                }
                
                // 에러
                LOG_ERROR("recv failed. SessionID: %llu, Error: %d", 
                         session->GetID(), SocketUtils::GetLastSocketError());
                ProcessDisconnect(session);
                break;
            }
        }
    }

    void EpollModel::ProcessSend(Session* session)
    {
        if (!session)
        {
            return;
        }

        SpinLockGuard lock(session->GetLock());

        // Edge-Triggered 모드에서는 버퍼가 빌 때까지 쓰기
        while (true)
        {
            size_t availableData = session->GetSendBuffer().GetAvailableRead();
            if (availableData == 0)
            {
                // 더 이상 보낼 데이터가 없음
                session->SetSending(false);
                // EPOLLOUT 제거
                ModifySocket(session->GetSocket(), EPOLLIN | EPOLLET);
                break;
            }

            // 버퍼에서 데이터 읽기
            uint8_t sendBuffer[DEFAULT_BUFFER_SIZE];
            size_t dataSize = session->GetSendBuffer().Peek(sendBuffer, sizeof(sendBuffer));

            ssize_t bytesSent = send(session->GetSocket(), sendBuffer, dataSize, MSG_NOSIGNAL);

            if (bytesSent > 0)
            {
                // 송신 성공
                session->GetSendBuffer().Skip(bytesSent);
            }
            else if (bytesSent == 0)
            {
                // 연결 종료
                ProcessDisconnect(session);
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 더 이상 쓸 수 없음, 나중에 EPOLLOUT 이벤트로 재시도
                    break;
                }
                
                // 에러
                LOG_ERROR("send failed. SessionID: %llu, Error: %d", 
                         session->GetID(), SocketUtils::GetLastSocketError());
                ProcessDisconnect(session);
                break;
            }
        }
    }

    void EpollModel::ProcessDisconnect(Session* session)
    {
        if (!session)
        {
            return;
        }

        if (session->IsDisconnected())
        {
            return;
        }

        session->SetState(SessionState::Disconnected);

        // Disconnect 콜백 호출
        if (onDisconnect_)
        {
            onDisconnect_(session);
        }

        LOG_DEBUG("Client disconnected. SessionID: %llu", session->GetID());

        // 소켓 제거
        SocketHandle socket = session->GetSocket();
        socketToSession_.erase(socket);
        UnregisterSocket(socket);
        SocketUtils::CloseSocket(socket);

        // 세션 제거
        sessionManager_->RemoveSession(session->GetID());
    }

    bool EpollModel::RegisterSocket(SocketHandle socket, Session* session, uint32_t events)
    {
        struct epoll_event ev;
        ev.events = events;
        ev.data.ptr = session;
        ev.data.fd = socket;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, socket, &ev) < 0)
        {
            LOG_ERROR("Failed to add socket to epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        return true;
    }

    bool EpollModel::ModifySocket(SocketHandle socket, uint32_t events)
    {
        auto it = socketToSession_.find(socket);
        if (it == socketToSession_.end())
        {
            return false;
        }

        struct epoll_event ev;
        ev.events = events;
        ev.data.ptr = it->second;
        ev.data.fd = socket;

        if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, socket, &ev) < 0)
        {
            LOG_ERROR("Failed to modify socket in epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        return true;
    }

    bool EpollModel::UnregisterSocket(SocketHandle socket)
    {
        if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, socket, nullptr) < 0)
        {
            LOG_ERROR("Failed to remove socket from epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        return true;
    }

    void EpollModel::CloseSession(Session* session)
    {
        if (!session)
        {
            return;
        }

        SocketHandle socket = session->GetSocket();
        socketToSession_.erase(socket);
        UnregisterSocket(socket);
        SocketUtils::ShutdownSocket(socket);
        SocketUtils::CloseSocket(socket);
    }

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

