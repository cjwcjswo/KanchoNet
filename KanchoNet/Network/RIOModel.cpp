#include "RIOModel.h"

#ifdef KANCHONET_PLATFORM_WINDOWS

#include "SocketUtils.h"
#include "../Utils/Logger.h"
#include <WS2tcpip.h>

namespace KanchoNet
{
    bool RIOModel::rioSupportChecked_ = false;
    bool RIOModel::rioSupported_ = false;

    RIOModel::RIOModel()
        : initialized_(false)
        , running_(false)
        , listenSocket_(INVALID_SOCKET)
        , completionQueue_(RIO_INVALID_CQ)
    {
        ZeroMemory(&rioFunctions_, sizeof(rioFunctions_));
        ZeroMemory(&overlapped_, sizeof(overlapped_));
        ZeroMemory(&recvBufferInfo_, sizeof(recvBufferInfo_));
        ZeroMemory(&sendBufferInfo_, sizeof(sendBufferInfo_));
    }

    RIOModel::~RIOModel()
    {
        Shutdown();
    }

    bool RIOModel::Initialize(const EngineConfig& config)
    {
        if (initialized_)
        {
            LOG_ERROR("RIOModel already initialized");
            return false;
        }

        config_ = config;

        // RIO 지원 여부 확인
        if (!IsRIOSupported())
        {
            LOG_ERROR("RIO is not supported on this system");
            if (onError_)
            {
                onError_(nullptr, ErrorCode::RIONotSupported);
            }
            return false;
        }

        // Winsock 초기화
        if (!SocketUtils::InitializeNetwork())
        {
            return false;
        }

        // 리슨 소켓 생성
        listenSocket_ = SocketUtils::CreateTCPSocket();
        if (listenSocket_ == INVALID_SOCKET)
        {
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 옵션 설정
        SocketUtils::SetSocketOption(listenSocket_, config_);

        // RIO Functions 로드
        if (!LoadRIOFunctions())
        {
            SocketUtils::CloseSocket(listenSocket_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // RIO 리소스 생성
        if (!CreateRIOResources())
        {
            SocketUtils::CloseSocket(listenSocket_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 소켓 바인드
        if (!SocketUtils::BindSocket(listenSocket_, config_.port))
        {
            DeregisterBuffer(recvBufferInfo_);
            DeregisterBuffer(sendBufferInfo_);
            SocketUtils::CloseSocket(listenSocket_);
            SocketUtils::CleanupNetwork();
            return false;
        }

        // 세션 매니저 생성
        sessionManager_ = std::make_unique<SessionManager>(config_.maxSessions);

        initialized_ = true;
        LOG_INFO("RIOModel initialized successfully. Port: %u", config_.port);
        
        return true;
    }

    bool RIOModel::StartListen()
    {
        if (!initialized_)
        {
            LOG_ERROR("RIOModel not initialized");
            return false;
        }

        if (running_)
        {
            LOG_WARNING("RIOModel already running");
            return true;
        }

        // 리슨 시작
        if (!SocketUtils::ListenSocket(listenSocket_, config_.backlog))
        {
            return false;
        }

        running_ = true;
        LOG_INFO("RIOModel started listening");
        
        // Accept는 일반 소켓 API 사용 (RIO는 연결된 소켓에만 사용)
        // 별도 스레드나 비동기 Accept 구현 필요
        LOG_WARNING("RIOModel: Accept handling needs to be implemented in application thread");
        
        return true;
    }

    bool RIOModel::ProcessIO(uint32_t timeoutMs)
    {
        if (!running_)
        {
            return false;
        }

        // Completion Queue에서 완료된 I/O 처리
        ULONG numResults = rioFunctions_.RIODequeueCompletion(completionQueue_, nullptr, 0);
        
        if (numResults == RIO_CORRUPT_CQ)
        {
            LOG_ERROR("RIO Completion Queue is corrupted");
            return false;
        }

        if (numResults == 0)
        {
            // 대기
            DWORD waitResult = WaitForSingleObject((HANDLE)overlapped_.hEvent, timeoutMs);
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

        // RIO 리소스 정리
        if (completionQueue_ != RIO_INVALID_CQ)
        {
            rioFunctions_.RIOCloseCompletionQueue(completionQueue_);
            completionQueue_ = RIO_INVALID_CQ;
        }

        DeregisterBuffer(recvBufferInfo_);
        DeregisterBuffer(sendBufferInfo_);

        // 리슨 소켓 닫기
        if (listenSocket_ != INVALID_SOCKET)
        {
            SocketUtils::CloseSocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }

        // Winsock 정리
        SocketUtils::CleanupNetwork();

        initialized_ = false;
        LOG_INFO("RIOModel shutdown completed");
    }

    void RIOModel::SetAcceptCallback(std::function<void(Session*)> callback)
    {
        onAccept_ = callback;
    }

    void RIOModel::SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    {
        onReceive_ = callback;
    }

    void RIOModel::SetDisconnectCallback(std::function<void(Session*)> callback)
    {
        onDisconnect_ = callback;
    }

    void RIOModel::SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)
    {
        onError_ = callback;
    }

    bool RIOModel::IsRIOSupported() const
    {
        if (rioSupportChecked_)
        {
            return rioSupported_;
        }

        // Windows 버전 확인 (Windows 8 이상)
        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        osvi.dwMajorVersion = 6;
        osvi.dwMinorVersion = 2; // Windows 8

        DWORDLONG conditionMask = 0;
        VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

        rioSupported_ = VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) != FALSE;
        rioSupportChecked_ = true;

        return rioSupported_;
    }

    bool RIOModel::LoadRIOFunctions()
    {
        GUID functionTableId = WSAID_MULTIPLE_RIO;
        DWORD bytes = 0;

        int result = WSAIoctl(
            listenSocket_,
            SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
            &functionTableId,
            sizeof(functionTableId),
            &rioFunctions_,
            sizeof(rioFunctions_),
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
        overlappedEntry.lpOverlapped = &overlapped_;
        overlapped_.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        RIO_NOTIFICATION_COMPLETION completionType = {};
        completionType.Type = RIO_EVENT_COMPLETION;
        completionType.Event.EventHandle = overlapped_.hEvent;
        completionType.Event.NotifyReset = TRUE;

        completionQueue_ = rioFunctions_.RIOCreateCompletionQueue(
            config_.rioCQSize,
            &completionType
        );

        if (completionQueue_ == RIO_INVALID_CQ)
        {
            LOG_ERROR("Failed to create RIO Completion Queue. Error: %d", GetLastError());
            return false;
        }

        // 버퍼 등록
        if (!RegisterBuffer(recvBufferInfo_, config_.recvBufferSize, config_.rioReceiveBufferCount))
        {
            LOG_ERROR("Failed to register receive buffer");
            return false;
        }

        if (!RegisterBuffer(sendBufferInfo_, config_.sendBufferSize, config_.rioSendBufferCount))
        {
            LOG_ERROR("Failed to register send buffer");
            DeregisterBuffer(recvBufferInfo_);
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

        bufferInfo.bufferID = rioFunctions_.RIORegisterBuffer(
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
            rioFunctions_.RIODeregisterBuffer(bufferInfo.bufferID);
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
        ULONG numResults = rioFunctions_.RIODequeueCompletion(
            completionQueue_,
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
            rioFunctions_.RIONotify(completionQueue_);
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
        if (onReceive_ && bytesTransferred > 0)
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

