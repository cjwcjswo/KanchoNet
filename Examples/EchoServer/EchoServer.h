#pragma once

#include <KanchoNet.h>
#include <iostream>

// 크로스 플랫폼 네트워크 모델 선택
#ifdef KANCHONET_PLATFORM_WINDOWS
    using DefaultNetworkModel = KanchoNet::IOCPModel;
#elif defined(KANCHONET_PLATFORM_LINUX)
    using DefaultNetworkModel = KanchoNet::EpollModel;
#endif

// Echo 서버 - 받은 데이터를 그대로 다시 전송
class EchoServer : public KanchoNet::NetworkEngine<DefaultNetworkModel>
{
public:
    EchoServer() = default;
    virtual ~EchoServer() = default;

protected:
    // 클라이언트 접속
    void OnAccept(KanchoNet::Session* session) override
    {
        std::cout << "[Accept] SessionID: " << session->GetID() 
                  << ", Socket: " << session->GetSocket() << std::endl;
    }

    // 패킷 수신
    void OnReceive(KanchoNet::Session* session, const uint8_t* data, size_t size) override
    {
        std::cout << "[Receive] SessionID: " << session->GetID() 
                  << ", Size: " << size << " bytes" << std::endl;

        // Echo: 받은 데이터를 그대로 다시 전송
        Send(session, data, size);
    }

    // 연결 종료
    void OnDisconnect(KanchoNet::Session* session) override
    {
        std::cout << "[Disconnect] SessionID: " << session->GetID() << std::endl;
    }

    // 에러 발생
    void OnError(KanchoNet::Session* session, KanchoNet::ErrorCode errorCode) override
    {
        std::cout << "[Error] SessionID: " << (session ? session->GetID() : 0)
                  << ", ErrorCode: " << static_cast<int>(errorCode) << std::endl;
    }
};

