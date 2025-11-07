#pragma once

#include "../Types.h"
#include "../Core/EngineConfig.h"
#include "../Session/Session.h"
#include "../Buffer/PacketBuffer.h"

namespace KanchoNet
{
    // 네트워크 모델 인터페이스 (IOCP와 RIO의 공통 인터페이스)
    // 템플릿 파라미터로 사용되므로 순수 가상 함수가 아닌 개념(Concept) 형태
    // 
    // 요구사항:
    // - bool Initialize(const EngineConfig& config)
    // - bool StartListen()
    // - bool ProcessIO(uint32_t timeoutMs = 0)
    // - bool Send(Session* session, const PacketBuffer& buffer)
    // - void Shutdown()
    // - void SetAcceptCallback(std::function<void(Session*)> callback)
    // - void SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback)
    // - void SetDisconnectCallback(std::function<void(Session*)> callback)
    // - void SetErrorCallback(std::function<void(Session*, ErrorCode)> callback)

    // 네트워크 모델의 베이스 (인터페이스 정의용, 실제 사용되지 않음)
    class INetworkModel
    {
    public:
        virtual ~INetworkModel() = default;

        // 초기화
        virtual bool Initialize(const EngineConfig& config) = 0;
        
        // 리슨 시작
        virtual bool StartListen() = 0;
        
        // I/O 처리 (어플리케이션 스레드에서 호출)
        // timeoutMs: 대기 시간 (0 = 즉시 반환, INFINITE = 무한 대기)
        // 반환값: 성공 여부
        virtual bool ProcessIO(uint32_t timeoutMs = 0) = 0;
        
        // 패킷 전송
        virtual bool Send(Session* session, const PacketBuffer& buffer) = 0;
        
        // 종료
        virtual void Shutdown() = 0;

        // 콜백 설정
        virtual void SetAcceptCallback(std::function<void(Session*)> callback) = 0;
        virtual void SetReceiveCallback(std::function<void(Session*, const uint8_t*, size_t)> callback) = 0;
        virtual void SetDisconnectCallback(std::function<void(Session*)> callback) = 0;
        virtual void SetErrorCallback(std::function<void(Session*, ErrorCode)> callback) = 0;
    };

} // namespace KanchoNet

