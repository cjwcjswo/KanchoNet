#include "RIOModel.h"

#ifdef KANCHONET_PLATFORM_WINDOWS

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <WS2tcpip.h>

namespace KanchoNet
{
    bool RIOModel::mRioSupportChecked = false;
    bool RIOModel::mRioSupported = false;

    RIOModel::RIOModel()
        : mInitialized(false)
        , mRunning(false)
        , mListenSocket(INVALID_SOCKET)
        , mCompletionQueue(RIO_INVALID_CQ)
    {
        ZeroMemory(&mRioFunctions, sizeof(mRioFunctions));
        ZeroMemory(&mOverlapped, sizeof(mOverlapped));
        ZeroMemory(&mRecvBufferInfo, sizeof(mRecvBufferInfo));
        ZeroMemory(&mSendBufferInfo, sizeof(mSendBufferInfo));
    }

    RIOModel::~RIOModel()
    {
        Shutdown();
    }

    bool RIOModel::Initialize(const EngineConfig& config)
    {
        if (mInitialized)
        {
            LOG_ERROR("RIOModel already initialized");
            return false;
        }

        mConfig = config;

        // RIO 지원 여부 확인
        if (!IsRIOSupported())
        {
            LOG_ERROR("RIO is not supported on this system");
            if (mOnError)
            {
                mOnError(nullptr, ErrorCode::RIONotSupported);
            }
            return false;
        }

        // Winsock 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // 리슨 소켓 생성
        mListenSocket = SocketUtils::CreateTCPSocket();
        if (mListenSocket == INVALID_SOCKET)
        {
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 옵션 설정
        SocketUtils::SetSocketOption(mListenSocket, mConfig);

        // RIO Functions 로드
        if (!LoadRIOFunctions())
        {
            SocketUtils::CloseSocket(mListenSocket);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // RIO 리소스 생성
        if (!CreateRIOResources())
        {
            SocketUtils::CloseSocket(mListenSocket);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 바인드
        if (!SocketUtils::BindSocket(mListenSocket, mConfig.mPort))
        {
            DeregisterBuffer(mRecvBufferInfo);
            DeregisterBuffer(mSendBufferInfo);
            SocketUtils::CloseSocket(mListenSocket);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        mSessionManager = std::make_unique<SessionManager>(mConfig.mMaxSessions);

        mInitialized = true;
        LOG_INFO("RIOModel initialized successfully. Port: %u", mConfig.mPort);
        
        return true;
    }

    bool RIOModel::StartListen()
    {
        if (!mInitialized)
        {
            LOG_ERROR("RIOModel not initialized");
            return false;
        }

        if (mRunning)
        {
            LOG_WARNING("RIOModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(mListenSocket, mConfig.mBacklog))
        {
            return false;
        }

        mRunning = true;
        LOG_INFO("RIOModel started listening");
        
        // Accept는 일반 소켓 API 사용 (RIO는 연결된 소켓에만 사용)
        // 별도 스레드나 비동기 Accept 구현 필요
        LOG_WARNING("RIOModel: Accept handling needs to be implemented in application thread");
        
        return true;
    }

    bool RIOModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!mRunning)
        {
            return false;
        }

        // Completion Queue에서 완료된 I/O 처리
        ULONG numResults = mRioFunctions.RIODequeueCompletion(mCompletionQueue, nullptr, 0);
        
        if (numResults == RIO_CORRUPT_CQ)
        {
            LOG_ERROR("RIO Completion Queue is corrupted");
            return false;
        }

        if (numResults == 0)
        {
            // 대기
            DWORD waitResult = WaitForSingleObject((HANDLE)mOverlapped.hEvent, timeoutMs);
            if (waitResult == WAIT_TIMEOUT)
            {
                return true; // 타임아웃은 에러가 아님
            }
            
            if (waitResult != WAIT_OBJECT_0)
            {
                return false;
            }
        }

        ProcessCompletions();
        return true;
    }

    bool RIOModel::Send(Session* session, const PacketBuffer& buffer)
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

    void RIOModel::Shutdown()
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

        // RIO 리소스 정리
        if (mCompletionQueue != RIO_INVALID_CQ)
        {
            mRioFunctions.RIOCloseCompletionQueue(mCompletionQueue);
            mCompletionQueue = RIO_INVALID_CQ;
        }

        DeregisterBuffer(mRecvBufferInfo);
        DeregisterBuffer(mSendBufferInfo);

        // 리슨 소켓 닫기
        if (mListenSocket != INVALID_SOCKET)
        {
            SocketUtils::CloseSocket(mListenSocket);
            mListenSocket = INVALID_SOCKET;
        }

        // Winsock 정리
        SocketUtils::CleanupNetwork();

        mInitialized = false;
        LOG_INFO("RIOModel shutdown completed");
    }

    void RIOModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        mOnAccept = callback;
    }

    void RIOModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        mOnReceive = callback;
    }

    void RIOModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        mOnDisconnect = callback;
    }

    void RIOModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        mOnError = callback;
    }

    bool RIOModel::IsRIOSupported() const
    {
        if (mRioSupportChecked)
        {
            return mRioSupported;
        }

        // Windows 버전 확인 (Windows 8 이상)
        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        osvi.dwMajorVersion = 6;
        osvi.dwMinorVersion = 2; // Windows 8

        DWORDLONG conditionMask = 0;
        VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

        mRioSupported = VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) != FALSE;
        mRioSupportChecked = true;

        return mRioSupported;
    }

    bool RIOModel::LoadRIOFunctions()
    {
        GUID functionTableId = WSAID_MULTIPLE_RIO;
        DWORD bytes = 0;

        int result = WSAIoctl(
            mListenSocket,
            SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
            &functionTableId,
            sizeof(functionTableId),
            &mRioFunctions,
            sizeof(mRioFunctions),
            &bytes,
            nullptr,
            nullptr
        );

        if (result == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to load RIO functions. Error: %d", WSAGetLastError());
            return false;
        }

        LOG_INFO("RIO functions loaded successfully");
        return true;
    }

    bool RIOModel::CreateRIOResources()
    {
        // Completion Queue 생성
        OVERLAPPED_ENTRY overlappedEntry = {};
        overlappedEntry.lpOverlapped = &mOverlapped;
        mOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        RIO_NOTIFICATION_COMPLETION completionType = {};
        completionType.Type = RIO_EVENT_COMPLETION;
        completionType.Event.EventHandle = mOverlapped.hEvent;
        completionType.Event.NotifyReset = TRUE;

        mCompletionQueue = mRioFunctions.RIOCreateCompletionQueue(
            mConfig.mRioCQSize,
            &completionType
        );

        if (mCompletionQueue == RIO_INVALID_CQ)
        {
            LOG_ERROR("Failed to create RIO Completion Queue. Error: %d", GetLastError());
            return false;
        }

        // 버퍼 등록
        if (!RegisterBuffer(mRecvBufferInfo, mConfig.mRecvBufferSize, mConfig.mRioReceiveBufferCount))
        {
            LOG_ERROR("Failed to register receive buffer");
            return false;
        }

        if (!RegisterBuffer(mSendBufferInfo, mConfig.mSendBufferSize, mConfig.mRioSendBufferCount))
        {
            LOG_ERROR("Failed to register send buffer");
            DeregisterBuffer(mRecvBufferInfo);
            return false;
        }

        LOG_INFO("RIO resources created successfully");
        return true;
    }

    bool RIOModel::RegisterBuffer(RIOBufferInfo& bufferInfo, size_t bufferSize, size_t bufferCount)
    {
        size_t totalSize = bufferSize * bufferCount;
        bufferInfo.buffer = new uint8_t[totalSize];
        bufferInfo.bufferSize = bufferSize;
        bufferInfo.bufferCount = bufferCount;

        bufferInfo.bufferID = mRioFunctions.RIORegisterBuffer(
            (PCHAR)bufferInfo.buffer,
            static_cast<DWORD>(totalSize)
        );

        if (bufferInfo.bufferID == RIO_INVALID_BUFFERID)
        {
            LOG_ERROR("RIORegisterBuffer failed. Error: %d", GetLastError());
            delete[] bufferInfo.buffer;
            bufferInfo.buffer = nullptr;
            return false;
        }

        return true;
    }

    void RIOModel::DeregisterBuffer(RIOBufferInfo& bufferInfo)
    {
        if (bufferInfo.bufferID != RIO_INVALID_BUFFERID)
        {
            mRioFunctions.RIODeregisterBuffer(bufferInfo.bufferID);
            bufferInfo.bufferID = RIO_INVALID_BUFFERID;
        }

        if (bufferInfo.buffer)
        {
            delete[] bufferInfo.buffer;
            bufferInfo.buffer = nullptr;
        }
    }

    bool RIOModel::PostReceive(Session* session)
    {
        // RIO를 사용한 수신
        // 실제 구현에서는 RIO Request Queue를 생성하고 RIOReceive를 호출해야 함
        LOG_WARNING("RIOModel::PostReceive - Not fully implemented");
        return true;
    }

    bool RIOModel::PostSend(Session* session)
    {
        // RIO를 사용한 송신
        // 실제 구현에서는 RIO Request Queue를 생성하고 RIOSend를 호출해야 함
        LOG_WARNING("RIOModel::PostSend - Not fully implemented");
        return true;
    }

    void RIOModel::ProcessCompletions()
    {
        // Completion Queue에서 결과 처리
        std::vector<RIORESULT> results(128);
        ULONG numResults = mRioFunctions.RIODequeueCompletion(
            mCompletionQueue,
            results.data(),
            static_cast<ULONG>(results.size())
        );

        if (numResults == RIO_CORRUPT_CQ)
        {
            LOG_ERROR("RIO Completion Queue is corrupted");
            return;
        }

        for (ULONG i = 0; i < numResults; ++i)
        {
            RIOContext* context = reinterpret_cast<RIOContext*>(results[i].RequestContext);
            
            switch (context->operation)
            {
            case IOOperation::Receive:
                ProcessReceiveCompletion(context, results[i].BytesTransferred);
                break;

            case IOOperation::Send:
                ProcessSendCompletion(context, results[i].BytesTransferred);
                break;

            default:
                LOG_WARNING("Unknown RIO operation: %d", (int)context->operation);
                break;
            }

            DeallocateContext(context);
        }

        // Notify 재설정
        if (numResults > 0)
        {
            mRioFunctions.RIONotify(mCompletionQueue);
        }
    }

    void RIOModel::ProcessAcceptCompletion(RIOContext* context)
    {
        // Accept는 일반 소켓 API 사용
        LOG_WARNING("RIOModel::ProcessAcceptCompletion - Not used");
    }

    void RIOModel::ProcessReceiveCompletion(RIOContext* context, ULONG bytesTransferred)
    {
        Session* session = context->session;
        if (!session || !session->IsConnected())
        {
            return;
        }

        // Receive 콜백 호출
        if (mOnReceive && bytesTransferred > 0)
        {
            // 실제로는 rioBuf에서 데이터를 읽어야 함
            LOG_WARNING("RIOModel::ProcessReceiveCompletion - Data handling not implemented");
        }

        // 다음 수신 등록
        PostReceive(session);
    }

    void RIOModel::ProcessSendCompletion(RIOContext* context, ULONG bytesTransferred)
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

    void RIOModel::CloseSession(Session* session)
    {
        if (!session)
        {
            return;
        }

        SocketUtils::ShutdownSocket(session->GetSocket());
        SocketUtils::CloseSocket(session->GetSocket());
    }

    RIOModel::RIOContext* RIOModel::AllocateContext()
    {
        RIOContext* context = new RIOContext();
        context->operation = IOOperation::Receive;
        context->session = nullptr;
        ZeroMemory(&context->rioBuf, sizeof(RIO_BUF));
        return context;
    }

    void RIOModel::DeallocateContext(RIOContext* context)
    {
        if (context)
        {
            delete context;
        }
    }

} // namespace KanchoNet

#endif // KANCHONET_PLATFORM_WINDOWS

