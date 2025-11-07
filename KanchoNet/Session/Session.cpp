#include "Session.h"

namespace KanchoNet
{
    Session::Session(SessionID id, SocketHandle socket, const SessionConfig& config)
        : id_(id)
        , socket_(socket)
        , state_(SessionState::Idle)
        , sendBuffer_(config.maxPacketSize * 2)  // 송신 버퍼
        , recvBuffer_(config.maxPacketSize * 2)  // 수신 버퍼
        , userData_(nullptr)
        , isSending_(false)
        , config_(config)
    {
    }

    Session::~Session()
    {
    }

    Session::Session(Session&& other) noexcept
        : id_(other.id_)
        , socket_(other.socket_)
        , state_(other.state_.load())
        , sendBuffer_(std::move(other.sendBuffer_))
        , recvBuffer_(std::move(other.recvBuffer_))
        , userData_(other.userData_)
        , isSending_(other.isSending_.load())
        , config_(other.config_)
    {
        other.id_ = INVALID_SESSION_ID;
        other.socket_ = INVALID_SOCKET_HANDLE;
        other.userData_ = nullptr;
    }

    Session& Session::operator=(Session&& other) noexcept
    {
        if (this != &other)
        {
            id_ = other.id_;
            socket_ = other.socket_;
            state_.store(other.state_.load());
            sendBuffer_ = std::move(other.sendBuffer_);
            recvBuffer_ = std::move(other.recvBuffer_);
            userData_ = other.userData_;
            isSending_.store(other.isSending_.load());
            config_ = other.config_;

            other.id_ = INVALID_SESSION_ID;
            other.socket_ = INVALID_SOCKET_HANDLE;
            other.userData_ = nullptr;
        }
        return *this;
    }

} // namespace KanchoNet

