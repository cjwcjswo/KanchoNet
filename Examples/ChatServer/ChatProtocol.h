#pragma once

#include <cstdint>
#include <string>
#include <cstring>

// 간단한 채팅 프로토콜 정의
namespace ChatProtocol
{
    // 패킷 타입
    enum class PacketType : uint16_t
    {
        Login = 1,
        LoginResponse = 2,
        Message = 3,
        MessageBroadcast = 4,
        Logout = 5
    };

    // 패킷 헤더
    struct PacketHeader
    {
        uint16_t size;          // 전체 패킷 크기 (헤더 포함)
        PacketType type;        // 패킷 타입
    };

    // 로그인 요청
    struct LoginPacket
    {
        PacketHeader header;
        char username[32];
    };

    // 로그인 응답
    struct LoginResponsePacket
    {
        PacketHeader header;
        bool success;
        char message[64];
    };

    // 메시지
    struct MessagePacket
    {
        PacketHeader header;
        char message[256];
    };

    // 메시지 브로드캐스트
    struct MessageBroadcastPacket
    {
        PacketHeader header;
        char username[32];
        char message[256];
    };

    // 로그아웃
    struct LogoutPacket
    {
        PacketHeader header;
    };

} // namespace ChatProtocol

