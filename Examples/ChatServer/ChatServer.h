#pragma once

#include <KanchoNet.h>
#include "ChatProtocol.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>

// 채팅 유저 정보
struct ChatUser
{
    std::string username;
    KanchoNet::Session* session;
};

// 크로스 플랫폼 네트워크 모델 선택 (고성능 모델 우선)
#ifdef KANCHONET_PLATFORM_WINDOWS
    using ChatNetworkModel = KanchoNet::RIOModel;
#elif defined(KANCHONET_PLATFORM_LINUX)
    // liburing이 있으면 IOUring, 없으면 Epoll
    #ifdef KANCHONET_HAS_LIBURING
        using ChatNetworkModel = KanchoNet::IOUringModel;
    #else
        using ChatNetworkModel = KanchoNet::EpollModel;
    #endif
#endif

// 채팅 서버
class ChatServer : public KanchoNet::NetworkEngine<ChatNetworkModel>
{
public:
    // public 멤버변수 (없음)
    
private:
    // private 멤버변수
    std::unordered_map<KanchoNet::SessionID, ChatUser*> mUsers;
    std::mutex mUsersMutex;
    
public:
    // 생성자, 파괴자
    ChatServer() = default;
    virtual ~ChatServer() = default;

protected:
    // protected 함수
    // 클라이언트 접속
    void OnAccept(KanchoNet::Session* session) override
    {
        std::cout << "[Accept] SessionID: " << session->GetID() << std::endl;
        
        // 사용자 데이터 초기화
        session->SetUserData(new ChatUser{ "", session });
    }

    // 패킷 수신
    void OnReceive(KanchoNet::Session* session, const uint8_t* data, size_t size) override
    {
        if (size < sizeof(ChatProtocol::PacketHeader))
        {
            return;
        }

        const ChatProtocol::PacketHeader* header = 
            reinterpret_cast<const ChatProtocol::PacketHeader*>(data);

        switch (header->type)
        {
        case ChatProtocol::PacketType::Login:
            HandleLogin(session, data, size);
            break;

        case ChatProtocol::PacketType::Message:
            HandleMessage(session, data, size);
            break;

        case ChatProtocol::PacketType::Logout:
            HandleLogout(session);
            break;

        default:
            std::cout << "[Warning] Unknown packet type: " 
                     << static_cast<int>(header->type) << std::endl;
            break;
        }
    }

    // 연결 종료
    void OnDisconnect(KanchoNet::Session* session) override
    {
        ChatUser* user = session->GetUserData<ChatUser>();
        if (user)
        {
            if (!user->username.empty())
            {
                std::cout << "[Disconnect] User: " << user->username << std::endl;
                
                std::lock_guard<std::mutex> lock(mUsersMutex);
                mUsers.erase(session->GetID());
            }
            else
            {
                std::cout << "[Disconnect] SessionID: " << session->GetID() << std::endl;
            }

            delete user;
            session->SetUserData<ChatUser>(nullptr);
        }
    }

    // 에러 발생
    void OnError(KanchoNet::Session* session, KanchoNet::ErrorCode errorCode) override
    {
        std::cout << "[Error] SessionID: " << (session ? session->GetID() : 0)
                  << ", ErrorCode: " << static_cast<int>(errorCode) << std::endl;
    }

private:
    void HandleLogin(KanchoNet::Session* session, const uint8_t* data, size_t size)
    {
        const ChatProtocol::LoginPacket* packet = 
            reinterpret_cast<const ChatProtocol::LoginPacket*>(data);

        ChatUser* user = session->GetUserData<ChatUser>();
        if (!user)
        {
            return;
        }

        user->username = packet->username;

        // 사용자 목록에 추가
        {
            std::lock_guard<std::mutex> lock(mUsersMutex);
            mUsers[session->GetID()] = user;
        }

        std::cout << "[Login] User: " << user->username 
                  << ", SessionID: " << session->GetID() << std::endl;

        // 로그인 응답
        ChatProtocol::LoginResponsePacket response = {};
        response.header.size = sizeof(response);
        response.header.type = ChatProtocol::PacketType::LoginResponse;
        response.success = true;
        strcpy_s(response.message, "Welcome to KanchoNet Chat Server!");

        Send(session, &response, sizeof(response));
    }

    void HandleMessage(KanchoNet::Session* session, const uint8_t* data, size_t size)
    {
        const ChatProtocol::MessagePacket* packet = 
            reinterpret_cast<const ChatProtocol::MessagePacket*>(data);

        ChatUser* user = session->GetUserData<ChatUser>();
        if (!user || user->username.empty())
        {
            return;
        }

        std::cout << "[Message] From: " << user->username 
                  << ", Message: " << packet->message << std::endl;

        // 모든 사용자에게 브로드캐스트
        ChatProtocol::MessageBroadcastPacket broadcast = {};
        broadcast.header.size = sizeof(broadcast);
        broadcast.header.type = ChatProtocol::PacketType::MessageBroadcast;
        strcpy_s(broadcast.username, user->username.c_str());
        strcpy_s(broadcast.message, packet->message);

        BroadcastMessage(broadcast);
    }

    void HandleLogout(KanchoNet::Session* session)
    {
        ChatUser* user = session->GetUserData<ChatUser>();
        if (user && !user->username.empty())
        {
            std::cout << "[Logout] User: " << user->username << std::endl;
            
            std::lock_guard<std::mutex> lock(mUsersMutex);
            mUsers.erase(session->GetID());
        }
    }

private:
    // private 함수
    void BroadcastMessage(const ChatProtocol::MessageBroadcastPacket& packet)
    {
        std::lock_guard<std::mutex> lock(mUsersMutex);
        
        for (auto& pair : mUsers)
        {
            Send(pair.second->session, &packet, sizeof(packet));
        }
    }
};

