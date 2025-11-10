#pragma once

#include "../Types.h"
#include "INetworkModel.h"
#include "EngineConfig.h"
#include "../Session/Session.h"
#include "../Buffer/PacketBuffer.h"
#include "../Utils/NonCopyable.h"
#include <memory>
#include <atomic>

namespace KanchoNet
{
    // 네트워크 엔진 템플릿 클래스
    // TNetworkModel: IOCPModel 또는 RIOModel
    template<typename TNetworkModel>
    class NetworkEngine : public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        std::atomic<bool> mInitialized;
        std::atomic<bool> mRunning;
        
        EngineConfig mConfig;
        std::unique_ptr<TNetworkModel> mNetworkModel;
        
    public:
        // 생성자, 파괴자
        NetworkEngine();
        virtual ~NetworkEngine();
        
    public:
        // public 함수
        // 초기화 및 시작
        bool Initialize(const EngineConfig& config);
        bool Start();
        void Stop();

        // I/O 처리 (어플리케이션 스레드에서 호출)
        bool ProcessIO(uint32_t timeoutMs = 0);

        // 패킷 전송
        bool Send(Session* session, const PacketBuffer& buffer);
        bool Send(Session* session, const void* data, size_t size);

        // 세션 검색
        Session* GetSession(SessionID sessionID);
        
        // 전체 세션 브로드캐스트
        void Broadcast(const PacketBuffer& buffer);
        
        // 상태 확인
        bool IsInitialized() const { return mInitialized; }
        bool IsRunning() const { return mRunning; }

        // 설정 정보
        const EngineConfig& GetConfig() const { return mConfig; }

    protected:
        // 어플리케이션에서 오버라이드할 콜백 함수들
        virtual void OnAccept(Session* session) {}
        virtual void OnReceive(Session* session, const uint8_t* data, size_t size) {}
        virtual void OnDisconnect(Session* session) {}
        virtual void OnError(Session* session, ErrorCode errorCode) {}

    private:
        // private 함수
        // 내부 콜백 핸들러들
        void HandleAccept(Session* session);
        void HandleReceive(Session* session, const uint8_t* data, size_t size);
        void HandleDisconnect(Session* session);
        void HandleError(Session* session, ErrorCode errorCode);
    };

    // 템플릿 구현 (헤더에 포함)
    template<typename TNetworkModel>
    NetworkEngine<TNetworkModel>::NetworkEngine()
        : mInitialized(false)
        , mRunning(false)
    {
        mNetworkModel = std::make_unique<TNetworkModel>();
    }

    template<typename TNetworkModel>
    NetworkEngine<TNetworkModel>::~NetworkEngine()
    {
        Stop();
    }

    template<typename TNetworkModel>
    bool NetworkEngine<TNetworkModel>::Initialize(const EngineConfig& config)
    {
        if (mInitialized)
        {
            return false;
        }

        if (!config.Validate())
        {
            return false;
        }

        mConfig = config;

        // 네트워크 모델에 콜백 설정
        mNetworkModel->SetAcceptCallback([this](Session* session) {
            HandleAccept(session);
        });

        mNetworkModel->SetReceiveCallback([this](Session* session, const uint8_t* data, size_t size) {
            HandleReceive(session, data, size);
        });

        mNetworkModel->SetDisconnectCallback([this](Session* session) {
            HandleDisconnect(session);
        });

        mNetworkModel->SetErrorCallback([this](Session* session, ErrorCode errorCode) {
            HandleError(session, errorCode);
        });

        // 네트워크 모델 초기화
        if (!mNetworkModel->Initialize(mConfig))
        {
            return false;
        }

        mInitialized = true;
        return true;
    }

    template<typename TNetworkModel>
    bool NetworkEngine<TNetworkModel>::Start()
    {
        if (!mInitialized || mRunning)
        {
            return false;
        }

        if (!mNetworkModel->StartListen())
        {
            return false;
        }

        mRunning = true;
        return true;
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::Stop()
    {
        if (!mRunning)
        {
            return;
        }

        mRunning = false;
        
        if (mNetworkModel)
        {
            mNetworkModel->Shutdown();
        }

        mInitialized = false;
    }

    template<typename TNetworkModel>
    bool NetworkEngine<TNetworkModel>::ProcessIO(uint32_t timeoutMs)
    {
        if (!mRunning)
        {
            return false;
        }

        return mNetworkModel->ProcessIO(timeoutMs);
    }

    template<typename TNetworkModel>
    bool NetworkEngine<TNetworkModel>::Send(Session* session, const PacketBuffer& buffer)
    {
        if (!mRunning || !session)
        {
            return false;
        }

        return mNetworkModel->Send(session, buffer);
    }

    template<typename TNetworkModel>
    bool NetworkEngine<TNetworkModel>::Send(Session* session, const void* data, size_t size)
    {
        if (!mRunning || !session || !data || size == 0)
        {
            return false;
        }

        PacketBuffer buffer(data, size);
        return mNetworkModel->Send(session, buffer);
    }

    template<typename TNetworkModel>
    Session* NetworkEngine<TNetworkModel>::GetSession(SessionID sessionID)
    {
        // 네트워크 모델에서 세션 매니저를 통해 검색
        // 실제로는 mNetworkModel이 SessionManager를 노출하거나
        // 별도의 세션 관리 인터페이스가 필요
        // 현재 구현에서는 간단히 nullptr 반환
        return nullptr;
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::Broadcast(const PacketBuffer& buffer)
    {
        // 모든 세션에 브로드캐스트
        // 실제로는 세션 매니저를 통해 모든 세션을 순회하며 전송
        // 현재 구조에서는 구현 제한
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::HandleAccept(Session* session)
    {
        OnAccept(session);
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::HandleReceive(Session* session, const uint8_t* data, size_t size)
    {
        OnReceive(session, data, size);
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::HandleDisconnect(Session* session)
    {
        OnDisconnect(session);
    }

    template<typename TNetworkModel>
    void NetworkEngine<TNetworkModel>::HandleError(Session* session, ErrorCode errorCode)
    {
        OnError(session, errorCode);
    }

} // namespace KanchoNet

