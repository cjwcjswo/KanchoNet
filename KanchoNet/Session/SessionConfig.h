#pragma once

#include "../Types.h"

namespace KanchoNet
{
    // 세션별 설정 구조체
    struct SessionConfig
    {
        // 타임아웃 설정
        uint32_t receiveTimeoutMs = 60000;      // 수신 타임아웃 (기본 60초, 0 = 무한)
        uint32_t sendTimeoutMs = 30000;         // 송신 타임아웃 (기본 30초, 0 = 무한)
        
        // 버퍼 설정
        size_t maxPacketSize = 1024 * 1024;     // 최대 패킷 크기 (기본 1MB)
        
        // 기본 생성자
        SessionConfig() = default;
        
        // 복사/이동 가능
        SessionConfig(const SessionConfig&) = default;
        SessionConfig& operator=(const SessionConfig&) = default;
        SessionConfig(SessionConfig&&) = default;
        SessionConfig& operator=(SessionConfig&&) = default;
    };

} // namespace KanchoNet

