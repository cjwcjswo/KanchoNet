#include "EpollModel.h"

#ifdef KANCHONET_PLATFORM_LINUX

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <unistd.h>
#include <cstring>

namespace KanchoNet
{
    EpollModel::EpollModel()
        : mInitialized(false)
        , mRunning(false)
        , mListenSocket(INVALID_SOCKET_HANDLE)
        , mEpollFd(-1)
    {
    }

    EpollModel::~EpollModel()
    {
        Shutdown();
    }

    bool EpollModel::Initialize(const EngineConfig& config)
    {
        if (mInitialized)
        {
            LOG_ERROR("EpollModel already initialized");
            return false;
        }

        mConfig = config;

        // 네트워크 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // epoll 인스턴스 생성
        mEpollFd = epoll_create1(0);
        if (mEpollFd < 0)
        {
            LOG_ERROR("Failed to create epoll. Error: %d", SocketUtils::GetLastSocketError());
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 리슨 소켓 생성
        mListenSocket = SocketUtils::CreateTCPSocket();
        if (mListenSocket == INVALID_SOCKET_HANDLE)
        {
            close(mEpollFd);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 옵션 설정
        SocketUtils::SetSocketOption(mListenSocket, mConfig);
        SocketUtils::SetNonBlocking(mListenSocket, true);

        // 소켓 바인드
        if (!SocketUtils::BindSocket(mListenSocket, mConfig.mPort))
        {
            SocketUtils::CloseSocket(mListenSocket);
            close(mEpollFd);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        mSessionManager = std::make_unique<SessionManager>(mConfig.mMaxSessions);

        mInitialized = true;
        LOG_INFO("EpollModel initialized successfully. Port: %u", mConfig.mPort);
        
        return true;
    }

    bool EpollModel::StartListen()
    {
        if (!mInitialized)
        {
            LOG_ERROR("EpollModel not initialized");
            return false;
        }

        if (mRunning)
        {
            LOG_WARNING("EpollModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(mListenSocket, mConfig.mBacklog))
        {
            return false;
        }

        // epoll에 리슨 소켓 등록 (EPOLLIN: 읽기 이벤트, EPOLLET: Edge-Triggered)
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = nullptr; // 리슨 소켓은 nullptr로 표시
        ev.data.fd = mListenSocket;

        if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mListenSocket, &ev) < 0)
        {
            LOG_ERROR("Failed to add listen socket to epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        mRunning = true;
        LOG_INFO("EpollModel started listening");
        
        return true;
    }

    bool EpollModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!mRunning)
        {
            return false;
        }

        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(mEpollFd, events, MAX_EVENTS, timeoutMs);

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
            if (ev.data.fd == mListenSocket)
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
        if (!mInitialized)
        {
            return;
        }

        mRunning = false;

        // 세션 정리
        if (mSessionManager)
        {
            mSessionManager->ForEachSession([this](Session* session) {
                CloseSession(session);
            });
            mSessionManager->Clear();
        }

        mSocketToSession.clear();

        // 리슨 소켓 닫기
        if (mListenSocket != INVALID_SOCKET_HANDLE)
        {
            SocketUtils::CloseSocket(mListenSocket);
            mListenSocket = INVALID_SOCKET_HANDLE;
        }

        // epoll 닫기
        if (mEpollFd >= 0)
        {
            close(mEpollFd);
            mEpollFd = -1;
        }

        // 네트워크 정리
        SocketUtils::CleanupNetwork();

        mInitialized = false;
        LOG_INFO("EpollModel shutdown completed");
    }

    void EpollModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        mOnAccept = callback;
    }

    void EpollModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        mOnReceive = callback;
    }

    void EpollModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        mOnDisconnect = callback;
    }

    void EpollModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        mOnError = callback;
    }

    void EpollModel::ProcessAccept()
    {
        // Edge-Triggered 모드에서는 모든 연결을 처리해야 함
        while (true)
        {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            
            SocketHandle clientSocket = accept(mListenSocket, 
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
            Session* session = mSessionManager->AddSession(clientSocket, sessionConfig);
            if (!session)
            {
                LOG_WARNING("Failed to add session. Session limit reached.");
                close(clientSocket);
                continue;
            }

            session->SetState(SessionState::Connected);
            mSocketToSession[clientSocket] = session;

            // epoll에 클라이언트 소켓 등록
            if (!RegisterSocket(clientSocket, session, EPOLLIN | EPOLLET))
            {
                CloseSession(session);
                continue;
            }

            // Accept 콜백 호출
            if (mOnAccept)
            {
                mOnAccept(session);
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
                                     mReceiveBuffer, 
                                     sizeof(mReceiveBuffer), 
                                     0);

            if (bytesRead > 0)
            {
                // 데이터 수신 성공
                if (mOnReceive)
                {
                    mOnReceive(session, mReceiveBuffer, bytesRead);
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
        if (mOnDisconnect)
        {
            mOnDisconnect(session);
        }

        LOG_DEBUG("Client disconnected. SessionID: %llu", session->GetID());

        // 소켓 제거
        SocketHandle socket = session->GetSocket();
        mSocketToSession.erase(socket);
        UnregisterSocket(socket);
        SocketUtils::CloseSocket(socket);

        // 세션 제거
        mSessionManager->RemoveSession(session->GetID());
    }

    bool EpollModel::RegisterSocket(SocketHandle socket, Session* session, uint32_t events)
    {
        struct epoll_event ev;
        ev.events = events;
        ev.data.ptr = session;
        ev.data.fd = socket;

        if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, socket, &ev) < 0)
        {
            LOG_ERROR("Failed to add socket to epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        return true;
    }

    bool EpollModel::ModifySocket(SocketHandle socket, uint32_t events)
    {
        auto it = mSocketToSession.find(socket);
        if (it == mSocketToSession.end())
        {
            return false;
        }

        struct epoll_event ev;
        ev.events = events;
        ev.data.ptr = it->second;
        ev.data.fd = socket;

        if (epoll_ctl(mEpollFd, EPOLL_CTL_MOD, socket, &ev) < 0)
        {
            LOG_ERROR("Failed to modify socket in epoll. Error: %d", 
                     SocketUtils::GetLastSocketError());
            return false;
        }

        return true;
    }

    bool EpollModel::UnregisterSocket(SocketHandle socket)
    {
        if (epoll_ctl(mEpollFd, EPOLL_CTL_DEL, socket, nullptr) < 0)
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
        mSocketToSession.erase(socket);
        UnregisterSocket(socket);
        SocketUtils::ShutdownSocket(socket);
        SocketUtils::CloseSocket(socket);
    }

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

