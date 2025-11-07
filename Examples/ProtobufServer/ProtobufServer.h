#pragma once

#include <KanchoNet.h>
#include <iostream>

// Protobuf 예제 서버
// 실제로 Protobuf를 사용하려면:
// 1. Protobuf 라이브러리 설치
// 2. .proto 파일 정의
// 3. protoc 컴파일러로 .pb.h/.pb.cc 생성
// 4. ParseFromArray() / SerializeToArray() 사용

// 크로스 플랫폼 네트워크 모델 선택
#ifdef KANCHONET_PLATFORM_WINDOWS
    using ProtobufNetworkModel = KanchoNet::IOCPModel;
#elif defined(KANCHONET_PLATFORM_LINUX)
    using ProtobufNetworkModel = KanchoNet::EpollModel;
#endif

class ProtobufServer : public KanchoNet::NetworkEngine<ProtobufNetworkModel>
{
public:
    ProtobufServer() = default;
    virtual ~ProtobufServer() = default;

protected:
    // 클라이언트 접속
    void OnAccept(KanchoNet::Session* session) override
    {
        std::cout << "[Accept] SessionID: " << session->GetID() << std::endl;
    }

    // 패킷 수신
    void OnReceive(KanchoNet::Session* session, const uint8_t* data, size_t size) override
    {
        std::cout << "[Receive] SessionID: " << session->GetID() 
                  << ", Size: " << size << " bytes" << std::endl;

        // Protobuf 사용 예제 (실제 구현은 Proto 파일 필요)
        // 
        // GameMessage message;
        // if (message.ParseFromArray(data, size))
        // {
        //     // 메시지 처리
        //     ProcessGameMessage(session, message);
        //     
        //     // 응답 전송
        //     GameResponse response;
        //     response.set_result(true);
        //     response.set_message("OK");
        //     
        //     KanchoNet::PacketBuffer buffer;
        //     buffer.Resize(response.ByteSizeLong());
        //     response.SerializeToArray(buffer.GetData(), buffer.GetSize());
        //     
        //     Send(session, buffer);
        // }

        // 현재는 단순히 Echo
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

