#pragma once

#include "../Types.h"
#include "../Buffer/RingBuffer.h"
#include "../Buffer/PacketBuffer.h"
#include "../Utils/SpinLock.h"
#include "SessionConfig.h"
#include <memory>
#include <atomic>

namespace KanchoNet
{
    // 클라이언트 세션을 나타내는 클래스
    class Session
    {
    public:
        Session(SessionID id, SocketHandle socket, const SessionConfig& config);
        ~Session();

        // 복사 불가, 이동 가능
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&& other) noexcept;
        Session& operator=(Session&& other) noexcept;

        // 세션 정보
        SessionID GetID() const { return id_; }
        SocketHandle GetSocket() const { return socket_; }
        SessionState GetState() const { return state_.load(std::memory_order_acquire); }
        
        // 상태 관리
        void SetState(SessionState state) { state_.store(state, std::memory_order_release); }
        bool IsConnected() const { return GetState() == SessionState::Connected; }
        bool IsDisconnected() const { return GetState() == SessionState::Disconnected; }

        // 버퍼 접근
        RingBuffer& GetSendBuffer() { return sendBuffer_; }
        RingBuffer& GetRecvBuffer() { return recvBuffer_; }
        const RingBuffer& GetSendBuffer() const { return sendBuffer_; }
        const RingBuffer& GetRecvBuffer() const { return recvBuffer_; }

        // 사용자 데이터 (어플리케이션에서 자유롭게 사용)
        void SetUserData(void* data) { userData_ = data; }
        void* GetUserData() const { return userData_; }
        
        template<typename T>
        void SetUserData(T* data) { userData_ = static_cast<void*>(data); }
        
        template<typename T>
        T* GetUserData() const { return static_cast<T*>(userData_); }

        // 송신 대기 중 플래그 (중복 송신 방지)
        bool IsSending() const { return isSending_.load(std::memory_order_acquire); }
        void SetSending(bool sending) { isSending_.store(sending, std::memory_order_release); }

        // 락 (세션 데이터 동기화용)
        SpinLock& GetLock() { return lock_; }

        // 설정
        const SessionConfig& GetConfig() const { return config_; }

    private:
        SessionID id_;
        SocketHandle socket_;
        std::atomic<SessionState> state_;
        
        RingBuffer sendBuffer_;
        RingBuffer recvBuffer_;
        
        void* userData_;
        std::atomic<bool> isSending_;
        
        SessionConfig config_;
        SpinLock lock_;
    };

} // namespace KanchoNet

