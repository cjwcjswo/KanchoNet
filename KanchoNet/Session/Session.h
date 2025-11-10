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
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        SessionID mID;
        SocketHandle mSocket;
        std::atomic<SessionState> mState;
        
        RingBuffer mSendBuffer;
        RingBuffer mRecvBuffer;
        
        void* mUserData;
        std::atomic<bool> mIsSending;
        
        SessionConfig mConfig;
        SpinLock mLock;
        
    public:
        // 생성자, 파괴자
        Session(SessionID id, SocketHandle socket, const SessionConfig& config);
        ~Session();

        // 복사 불가, 이동 가능
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&& other) noexcept;
        Session& operator=(Session&& other) noexcept;
        
    public:
        // public 함수
        // 세션 정보
        SessionID GetID() const { return mID; }
        SocketHandle GetSocket() const { return mSocket; }
        SessionState GetState() const { return mState.load(std::memory_order_acquire); }
        
        // 상태 관리
        void SetState(SessionState state) { mState.store(state, std::memory_order_release); }
        bool IsConnected() const { return GetState() == SessionState::Connected; }
        bool IsDisconnected() const { return GetState() == SessionState::Disconnected; }

        // 버퍼 접근
        RingBuffer& GetSendBuffer() { return mSendBuffer; }
        RingBuffer& GetRecvBuffer() { return mRecvBuffer; }
        const RingBuffer& GetSendBuffer() const { return mSendBuffer; }
        const RingBuffer& GetRecvBuffer() const { return mRecvBuffer; }

        // 사용자 데이터 (어플리케이션에서 자유롭게 사용)
        void SetUserData(void* data) { mUserData = data; }
        void* GetUserData() const { return mUserData; }
        
        template<typename T>
        void SetUserData(T* data) { mUserData = static_cast<void*>(data); }
        
        template<typename T>
        T* GetUserData() const { return static_cast<T*>(mUserData); }

        // 송신 대기 중 플래그 (중복 송신 방지)
        bool IsSending() const { return mIsSending.load(std::memory_order_acquire); }
        void SetSending(bool sending) { mIsSending.store(sending, std::memory_order_release); }

        // 락 (세션 데이터 동기화용)
        SpinLock& GetLock() { return mLock; }

        // 설정
        const SessionConfig& GetConfig() const { return mConfig; }
    };

} // namespace KanchoNet

