#include "IOCPModel.h"

#ifdef KANCHONET_PLATFORM_WINDOWS

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <WS2tcpip.h>

namespace KanchoNet
{
    IOCPModel::IOCPModel()
        : initialized_(false)
        , running_(false)
        , listenSocket_(INVALID_SOCKET)
        , iocpHandle_(nullptr)
    {
    }

    IOCPModel::~IOCPModel()
    {
        Shutdown();
    }

    bool IOCPModel::Initialize(const EngineConfig& config)
    {
        if (initialized_)
        {
            LOG_ERROR("IOCPModel already initialized");
            return false;
        }

        config_ = config;

        // Winsock 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // IOCP 생성
        if (!CreateIOCP())
        {
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 리슨 소켓 생성
        listenSocket_ = SocketUtils::CreateTCPSocket();
        if (listenSocket_ == INVALID_SOCKET)
        {
            CloseHandle(iocpHandle_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 옵션 설정
        SocketUtils::SetSocketOption(listenSocket_, config_);

        // Extension Functions 로드
        if (!SocketUtils::LoadExtensionFunctions(listenSocket_))
        {
            SocketUtils::CloseSocket(listenSocket_);
            CloseHandle(iocpHandle_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 바인드
        if (!SocketUtils::BindSocket(listenSocket_, config_.port))
        {
            SocketUtils::CloseSocket(listenSocket_);
            CloseHandle(iocpHandle_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        sessionManager_ = std::make_unique<SessionManager>(config_.maxSessions);

        initialized_ = true;
        LOG_INFO("IOCPModel initialized successfully. Port: %u", config_.port);
        
        return true;
    }

    bool IOCPModel::StartListen()
    {
        if (!initialized_)
        {
            LOG_ERROR("IOCPModel not initialized");
            return false;
        }

        if (running_)
        {
            LOG_WARNING("IOCPModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(listenSocket_, config_.backlog))
        {
            return false;
        }

        // IOCP에 리슨 소켓 등록
        HANDLE result = CreateIoCompletionPort((HANDLE)listenSocket_, iocpHandle_, 
                                               (ULONG_PTR)nullptr, 0);
        if (result == nullptr)
        {
            LOG_ERROR("Failed to associate listen socket with IOCP. Error: %d", 
                     GetLastError());
            return false;
        }

        // Accept 시작
        if (!RegisterAccept())
        {
            return false;
        }

        running_ = true;
        LOG_INFO("IOCPModel started listening");
        
        return true;
    }

    bool IOCPModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!running_)
        {
            return false;
        }

        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL result = GetQueuedCompletionStatus(
            iocpHandle_,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            timeoutMs
        );

        // 타임아웃
        if (overlapped == nullptr)
        {
            if (GetLastError() == WAIT_TIMEOUT)
            {
                return true; // 타임아웃은 에러가 아님
            }
            return false;
        }

        OverlappedContext* context = reinterpret_cast<OverlappedContext*>(overlapped);

        // 에러 처리
        if (!result || bytesTransferred == 0)
        {
            if (context->operation != IOOperation::Accept)
            {
                // 연결 종료
                ProcessDisconnect(context);
            }
            DeallocateContext(context);
            return true;
        }

        // I/O 오퍼레이션 처리
        switch (context->operation)
        {
        case IOOperation::Accept:
            ProcessAccept(context, bytesTransferred);
            break;

        case IOOperation::Receive:
            ProcessReceive(context, bytesTransferred);
            break;

        case IOOperation::Send:
            ProcessSend(context, bytesTransferred);
            break;

        case IOOperation::Disconnect:
            ProcessDisconnect(context);
            break;

        default:
            LOG_WARNING("Unknown I/O operation: %d", (int)context->operation);
            break;
        }

        DeallocateContext(context);
        return true;
    }

    bool IOCPModel::Send(Session* session, const PacketBuffer& buffer)
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

        // 이미 송신 중이면 큐에만 추가
        if (session->IsSending())
        {
            return true;
        }

        // 송신 시작
        return PostSend(session);
    }

    void IOCPModel::Shutdown()
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

        // 리슨 소켓 닫기
        if (listenSocket_ != INVALID_SOCKET)
        {
            SocketUtils::CloseSocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }

        // IOCP 핸들 닫기
        if (iocpHandle_ != nullptr)
        {
            CloseHandle(iocpHandle_);
            iocpHandle_ = nullptr;
        }

        // Winsock 정리
        SocketUtils::CleanupNetwork();

        initialized_ = false;
        LOG_INFO("IOCPModel shutdown completed");
    }

    void IOCPModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        onAccept_ = callback;
    }

    void IOCPModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        onReceive_ = callback;
    }

    void IOCPModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        onDisconnect_ = callback;
    }

    void IOCPModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        onError_ = callback;
    }

    bool IOCPModel::CreateIOCP()
    {
        iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (iocpHandle_ == nullptr)
        {
            LOG_ERROR("Failed to create IOCP. Error: %d", GetLastError());
            return false;
        }

        LOG_INFO("IOCP created successfully");
        return true;
    }

    bool IOCPModel::RegisterAccept()
    {
        OverlappedContext* context = AllocateContext();
        context->operation = IOOperation::Accept;
        context->session = nullptr;

        // Accept용 소켓 생성
        context->acceptSocket = SocketUtils::CreateTCPSocket();
        if (context->acceptSocket == INVALID_SOCKET)
        {
            DeallocateContext(context);
            return false;
        }

        DWORD bytes = 0;
        BOOL result = SocketUtils::GetAcceptEx()(
            listenSocket_,
            context->acceptSocket,
            context->buffer,
            0, // 데이터를 기다리지 않음
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            &bytes,
            &context->overlapped
        );

        if (!result && WSAGetLastError() != ERROR_IO_PENDING)
        {
            LOG_ERROR("AcceptEx failed. Error: %d", WSAGetLastError());
            SocketUtils::CloseSocket(context->acceptSocket);
            DeallocateContext(context);
            return false;
        }

        return true;
    }

    void IOCPModel::ProcessAccept(OverlappedContext* context, DWORD bytesTransferred)
    {
        // 다음 Accept 등록
        RegisterAccept();

        // 세션 생성
        SessionConfig sessionConfig;
        Session* session = sessionManager_->AddSession(context->acceptSocket, sessionConfig);
        if (!session)
        {
            LOG_WARNING("Failed to add session. Session limit reached.");
            SocketUtils::CloseSocket(context->acceptSocket);
            return;
        }

        session->SetState(SessionState::Connected);

        // IOCP에 클라이언트 소켓 등록
        HANDLE result = CreateIoCompletionPort(
            (HANDLE)context->acceptSocket,
            iocpHandle_,
            (ULONG_PTR)session,
            0
        );

        if (result == nullptr)
        {
            LOG_ERROR("Failed to associate client socket with IOCP. Error: %d", GetLastError());
            CloseSession(session);
            return;
        }

        // 수신 시작
        if (!PostReceive(session))
        {
            CloseSession(session);
            return;
        }

        // Accept 콜백 호출
        if (onAccept_)
        {
            onAccept_(session);
        }

        LOG_DEBUG("Client accepted. SessionID: %llu", session->GetID());
    }

    void IOCPModel::ProcessReceive(OverlappedContext* context, DWORD bytesTransferred)
    {
        Session* session = context->session;
        if (!session || !session->IsConnected())
        {
            return;
        }

        // Receive 콜백 호출
        if (onReceive_)
        {
            onReceive_(session, context->buffer, bytesTransferred);
        }

        // 다음 수신 등록
        PostReceive(session);
    }

    void IOCPModel::ProcessSend(OverlappedContext* context, DWORD bytesTransferred)
    {
        Session* session = context->session;
        if (!session)
        {
            return;
        }

        SpinLockGuard lock(session->GetLock());

        // 송신 버퍼에서 데이터 제거
        session->GetSendBuffer().Skip(bytesTransferred);

        // 남은 데이터가 있으면 계속 송신
        if (session->GetSendBuffer().GetAvailableRead() > 0)
        {
            PostSend(session);
        }
        else
        {
            session->SetSending(false);
        }
    }

    void IOCPModel::ProcessDisconnect(OverlappedContext* context)
    {
        Session* session = context->session;
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

        // 세션 제거
        sessionManager_->RemoveSession(session->GetID());
    }

    bool IOCPModel::PostReceive(Session* session)
    {
        OverlappedContext* context = AllocateContext();
        context->operation = IOOperation::Receive;
        context->session = session;
        context->wsaBuf.buf = (char*)context->buffer;
        context->wsaBuf.len = sizeof(context->buffer);

        DWORD flags = 0;
        DWORD bytes = 0;

        int result = WSARecv(
            session->GetSocket(),
            &context->wsaBuf,
            1,
            &bytes,
            &flags,
            &context->overlapped,
            nullptr
        );

        if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
        {
            LOG_ERROR("WSARecv failed. SessionID: %llu, Error: %d", 
                     session->GetID(), WSAGetLastError());
            DeallocateContext(context);
            return false;
        }

        return true;
    }

    bool IOCPModel::PostSend(Session* session)
    {
        session->SetSending(true);

        OverlappedContext* context = AllocateContext();
        context->operation = IOOperation::Send;
        context->session = session;

        // 송신 버퍼에서 데이터 읽기
        size_t dataSize = session->GetSendBuffer().Peek(context->buffer, sizeof(context->buffer));
        if (dataSize == 0)
        {
            session->SetSending(false);
            DeallocateContext(context);
            return true;
        }

        context->wsaBuf.buf = (char*)context->buffer;
        context->wsaBuf.len = static_cast<ULONG>(dataSize);

        DWORD bytes = 0;

        int result = WSASend(
            session->GetSocket(),
            &context->wsaBuf,
            1,
            &bytes,
            0,
            &context->overlapped,
            nullptr
        );

        if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
        {
            LOG_ERROR("WSASend failed. SessionID: %llu, Error: %d", 
                     session->GetID(), WSAGetLastError());
            session->SetSending(false);
            DeallocateContext(context);
            return false;
        }

        return true;
    }

    void IOCPModel::CloseSession(Session* session)
    {
        if (!session)
        {
            return;
        }

        SocketUtils::ShutdownSocket(session->GetSocket());
        SocketUtils::CloseSocket(session->GetSocket());
    }

    IOCPModel::OverlappedContext* IOCPModel::AllocateContext()
    {
        OverlappedContext* context = new OverlappedContext();
        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        context->operation = IOOperation::Receive;
        context->session = nullptr;
        context->acceptSocket = INVALID_SOCKET;
        return context;
    }

    void IOCPModel::DeallocateContext(OverlappedContext* context)
    {
        if (context)
        {
            delete context;
        }
    }

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

