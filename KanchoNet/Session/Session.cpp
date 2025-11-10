#include "Session.h"

namespace KanchoNet
{
    Session::Session(SessionID id, SocketHandle socket, const SessionConfig& config)
        : mID(id)
        , mSocket(socket)
        , mState(SessionState::Idle)
        , mSendBuffer(config.mMaxPacketSize * 2)  // 송신 버퍼
        , mRecvBuffer(config.mMaxPacketSize * 2)  // 수신 버퍼
        , mUserData(nullptr)
        , mIsSending(false)
        , mConfig(config)
    {
    }

    Session::~Session()
    {
    }

    Session::Session(Session&& other) noexcept
        : mID(other.mID)
        , mSocket(other.mSocket)
        , mState(other.mState.load())
        , mSendBuffer(std::move(other.mSendBuffer))
        , mRecvBuffer(std::move(other.mRecvBuffer))
        , mUserData(other.mUserData)
        , mIsSending(other.mIsSending.load())
        , mConfig(other.mConfig)
    {
        other.mID = INVALID_SESSION_ID;
        other.mSocket = INVALID_SOCKET_HANDLE;
        other.mUserData = nullptr;
    }

    Session& Session::operator=(Session&& other) noexcept
    {
        if (this != &other)
        {
            mID = other.mID;
            mSocket = other.mSocket;
            mState.store(other.mState.load());
            mSendBuffer = std::move(other.mSendBuffer);
            mRecvBuffer = std::move(other.mRecvBuffer);
            mUserData = other.mUserData;
            mIsSending.store(other.mIsSending.load());
            mConfig = other.mConfig;

            other.mID = INVALID_SESSION_ID;
            other.mSocket = INVALID_SOCKET_HANDLE;
            other.mUserData = nullptr;
        }
        return *this;
    }

} // namespace KanchoNet

