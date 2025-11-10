#include "IOUringModel.h"

#ifdef KANCHONET_PLATFORM_LINUX

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <unistd.h>
#include <cstring>

namespace KanchoNet
{
    bool IOUringModel::mIOUringSupportChecked = false;
    bool IOUringModel::mIOUringSupported = false;

    IOUringModel::IOUringModel()
        : mInitialized(false)
        , mRunning(false)
        , mListenSocket(INVALID_SOCKET_HANDLE)
        , mRingInitialized(false)
    {
        memset(&mRing, 0, sizeof(mRing));
    }

    IOUringModel::~IOUringModel()
    {
        Shutdown();
    }

    bool IOUringModel::Initialize(const EngineConfig& config)
    {
        if (mInitialized)
        {
            LOG_ERROR("IOUringModel already initialized");
            return false;
        }

        // io_uring 지원 여부 확인
        if (!IsIOUringSupported())
        {
            LOG_ERROR("io_uring is not supported on this system (requires Linux kernel 5.1+)");
            if (mOnError)
            {
                mOnError(nullptr, ErrorCode::IOUringNotSupported);
            }
            return false;
        }

        mConfig = config;

        // 네트워크 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // io_uring 생성
        if (!CreateIOUring())
        {
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 리슨 소켓 생성
        mListenSocket = SocketUtils::CreateTCPSocket();
        if (mListenSocket == INVALID_SOCKET_HANDLE)
        {
            io_uring_queue_exit(&mRing);
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
            io_uring_queue_exit(&mRing);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        mSessionManager = std::make_unique<SessionManager>(mConfig.mMaxSessions);

        mInitialized = true;
        LOG_INFO("IOUringModel initialized successfully. Port: %u", mConfig.mPort);
        
        return true;
    }

    bool IOUringModel::StartListen()
    {
        if (!mInitialized)
        {
            LOG_ERROR("IOUringModel not initialized");
            return false;
        }

        if (mRunning)
        {
            LOG_WARNING("IOUringModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(mListenSocket, mConfig.mBacklog))
        {
            return false;
        }

        // Accept 요청 제출
        if (!SubmitAccept())
        {
            return false;
        }

        mRunning = true;
        LOG_INFO("IOUringModel started listening");
        
        return true;
    }

    bool IOUringModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!mRunning)
        {
            return false;
        }

        struct __kernel_timespec ts;
        ts.tv_sec = timeoutMs / 1000;
        ts.tv_nsec = (timeoutMs % 1000) * 1000000;

        struct io_uring_cqe* cqe;
        int ret;

        if (timeoutMs > 0)
        {
            ret = io_uring_wait_cqe_timeout(&mRing, &cqe, &ts);
        }
        else
        {
            ret = io_uring_peek_cqe(&mRing, &cqe);
        }

        if (ret < 0)
        {
            if (ret == -ETIME || ret == -EAGAIN)
            {
                return true; // 타임아웃은 에러가 아님
            }
            LOG_ERROR("io_uring_wait_cqe failed. Error: %d", -ret);
            return false;
        }

        // 완료된 이벤트들 처리
        unsigned head;
        unsigned count = 0;
        
        io_uring_for_each_cqe(&mRing, head, cqe)
        {
            ++count;
            ProcessCompletion(cqe);
            io_uring_cqe_seen(&mRing, cqe);
        }

        if (count > 0)
        {
            io_uring_cq_advance(&mRing, count);
        }

        return true;
    }

    bool IOUringModel::Send(Session* session, const PacketBuffer& buffer)
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
        return SubmitSend(session);
    }

    void IOUringModel::Shutdown()
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

        // io_uring 정리
        if (mRingInitialized)
        {
            io_uring_queue_exit(&mRing);
            mRingInitialized = false;
        }

        // 네트워크 정리
        SocketUtils::CleanupNetwork();

        mInitialized = false;
        LOG_INFO("IOUringModel shutdown completed");
    }

    void IOUringModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        mOnAccept = callback;
    }

    void IOUringModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        mOnReceive = callback;
    }

    void IOUringModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        mOnDisconnect = callback;
    }

    void IOUringModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        mOnError = callback;
    }

    bool IOUringModel::IsIOUringSupported()
    {
        if (mIOUringSupportChecked)
        {
            return mIOUringSupported;
        }

        // io_uring 지원 여부 확인
        struct io_uring testRing;
        int ret = io_uring_queue_init(8, &testRing, 0);
        
        if (ret == 0)
        {
            io_uring_queue_exit(&testRing);
            mIOUringSupported = true;
        }
        else
        {
            mIOUringSupported = false;
        }

        mIOUringSupportChecked = true;
        return mIOUringSupported;
    }

    bool IOUringModel::CreateIOUring()
    {
        // io_uring 초기화 (queue depth: 256)
        int ret = io_uring_queue_init(256, &mRing, 0);
        if (ret < 0)
        {
            LOG_ERROR("Failed to initialize io_uring. Error: %d", -ret);
            return false;
        }

        mRingInitialized = true;
        LOG_INFO("io_uring initialized successfully");
        return true;
    }

    bool IOUringModel::SubmitAccept()
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&mRing);
        if (!sqe)
        {
            LOG_ERROR("Failed to get SQE for accept");
            return false;
        }

        IOUringContext* ctx = AllocateContext();
        ctx->operation = IOOperation::Accept;
        ctx->session = nullptr;
        ctx->buffer = nullptr;
        ctx->bufferSize = 0;

        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        io_uring_prep_accept(sqe, mListenSocket, (struct sockaddr*)&clientAddr, &addrLen, 0);
        io_uring_sqe_set_data(sqe, ctx);

        int ret = io_uring_submit(&mRing);
        if (ret < 0)
        {
            LOG_ERROR("Failed to submit accept. Error: %d", -ret);
            DeallocateContext(ctx);
            return false;
        }

        return true;
    }

    bool IOUringModel::SubmitReceive(Session* session)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&mRing);
        if (!sqe)
        {
            LOG_ERROR("Failed to get SQE for receive");
            return false;
        }

        IOUringContext* ctx = AllocateContext();
        ctx->operation = IOOperation::Receive;
        ctx->session = session;
        ctx->buffer = new uint8_t[DEFAULT_BUFFER_SIZE];
        ctx->bufferSize = DEFAULT_BUFFER_SIZE;

        io_uring_prep_recv(sqe, session->GetSocket(), ctx->buffer, ctx->bufferSize, 0);
        io_uring_sqe_set_data(sqe, ctx);

        int ret = io_uring_submit(&mRing);
        if (ret < 0)
        {
            LOG_ERROR("Failed to submit receive. Error: %d", -ret);
            DeallocateContext(ctx);
            return false;
        }

        return true;
    }

    bool IOUringModel::SubmitSend(Session* session)
    {
        session->SetSending(true);

        SpinLockGuard lock(session->GetLock());

        size_t dataSize = session->GetSendBuffer().GetAvailableRead();
        if (dataSize == 0)
        {
            session->SetSending(false);
            return true;
        }

        struct io_uring_sqe* sqe = io_uring_get_sqe(&mRing);
        if (!sqe)
        {
            LOG_ERROR("Failed to get SQE for send");
            session->SetSending(false);
            return false;
        }

        IOUringContext* ctx = AllocateContext();
        ctx->operation = IOOperation::Send;
        ctx->session = session;
        ctx->bufferSize = (dataSize > DEFAULT_BUFFER_SIZE) ? DEFAULT_BUFFER_SIZE : dataSize;
        ctx->buffer = new uint8_t[ctx->bufferSize];

        session->GetSendBuffer().Peek(ctx->buffer, ctx->bufferSize);

        io_uring_prep_send(sqe, session->GetSocket(), ctx->buffer, ctx->bufferSize, 0);
        io_uring_sqe_set_data(sqe, ctx);

        int ret = io_uring_submit(&mRing);
        if (ret < 0)
        {
            LOG_ERROR("Failed to submit send. Error: %d", -ret);
            session->SetSending(false);
            DeallocateContext(ctx);
            return false;
        }

        return true;
    }

    void IOUringModel::ProcessCompletion(struct io_uring_cqe* cqe)
    {
        IOUringContext* ctx = static_cast<IOUringContext*>(io_uring_cqe_get_data(cqe));
        if (!ctx)
        {
            return;
        }

        int result = cqe->res;

        switch (ctx->operation)
        {
        case IOOperation::Accept:
            ProcessAcceptCompletion(ctx, result);
            break;

        case IOOperation::Receive:
            ProcessReceiveCompletion(ctx, result);
            break;

        case IOOperation::Send:
            ProcessSendCompletion(ctx, result);
            break;

        default:
            LOG_WARNING("Unknown I/O operation: %d", (int)ctx->operation);
            break;
        }

        DeallocateContext(ctx);
    }

    void IOUringModel::ProcessAcceptCompletion(IOUringContext* ctx, int result)
    {
        // 다음 Accept 등록
        SubmitAccept();

        if (result < 0)
        {
            LOG_ERROR("Accept failed. Error: %d", -result);
            return;
        }

        SocketHandle clientSocket = result;

        // Non-blocking 설정
        SocketUtils::SetNonBlocking(clientSocket, true);

        // 세션 생성
        SessionConfig sessionConfig;
        Session* session = mSessionManager->AddSession(clientSocket, sessionConfig);
        if (!session)
        {
            LOG_WARNING("Failed to add session. Session limit reached.");
            close(clientSocket);
            return;
        }

        session->SetState(SessionState::Connected);
        mSocketToSession[clientSocket] = session;

        // 수신 시작
        if (!SubmitReceive(session))
        {
            CloseSession(session);
            return;
        }

        // Accept 콜백 호출
        if (mOnAccept)
        {
            mOnAccept(session);
        }

        LOG_DEBUG("Client accepted. SessionID: %llu", session->GetID());
    }

    void IOUringModel::ProcessReceiveCompletion(IOUringContext* ctx, int result)
    {
        Session* session = ctx->session;
        if (!session || !session->IsConnected())
        {
            return;
        }

        if (result > 0)
        {
            // 데이터 수신 성공
            if (mOnReceive)
            {
                mOnReceive(session, ctx->buffer, result);
            }

            // 다음 수신 등록
            SubmitReceive(session);
        }
        else if (result == 0)
        {
            // 연결 종료
            ProcessDisconnect(session);
        }
        else
        {
            // 에러
            LOG_ERROR("Receive failed. SessionID: %llu, Error: %d", 
                     session->GetID(), -result);
            ProcessDisconnect(session);
        }
    }

    void IOUringModel::ProcessSendCompletion(IOUringContext* ctx, int result)
    {
        Session* session = ctx->session;
        if (!session)
        {
            return;
        }

        SpinLockGuard lock(session->GetLock());

        if (result > 0)
        {
            // 송신 성공
            session->GetSendBuffer().Skip(result);

            // 남은 데이터가 있으면 계속 송신
            if (session->GetSendBuffer().GetAvailableRead() > 0)
            {
                SubmitSend(session);
            }
            else
            {
                session->SetSending(false);
            }
        }
        else
        {
            // 에러
            session->SetSending(false);
            LOG_ERROR("Send failed. SessionID: %llu, Error: %d", 
                     session->GetID(), -result);
            ProcessDisconnect(session);
        }
    }

    void IOUringModel::ProcessDisconnect(Session* session)
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
        SocketUtils::CloseSocket(socket);

        // 세션 제거
        mSessionManager->RemoveSession(session->GetID());
    }

    void IOUringModel::CloseSession(Session* session)
    {
        if (!session)
        {
            return;
        }

        SocketHandle socket = session->GetSocket();
        mSocketToSession.erase(socket);
        SocketUtils::ShutdownSocket(socket);
        SocketUtils::CloseSocket(socket);
    }

    IOUringModel::IOUringContext* IOUringModel::AllocateContext()
    {
        IOUringContext* ctx = new IOUringContext();
        ctx->operation = IOOperation::Receive;
        ctx->session = nullptr;
        ctx->buffer = nullptr;
        ctx->bufferSize = 0;
        return ctx;
    }

    void IOUringModel::DeallocateContext(IOUringContext* ctx)
    {
        if (ctx)
        {
            if (ctx->buffer)
            {
                delete[] ctx->buffer;
            }
            delete ctx;
        }
    }

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_LINUX

