#pragma once

#include "../Types.h"

namespace KanchoNet
{
    // 네트워크 엔진 설정 구조체
    struct EngineConfig
    {
        // 네트워크 설정
        uint16_t port = DEFAULT_PORT;                           // 리슨 포트
        uint32_t maxSessions = DEFAULT_MAX_SESSIONS;            // 최대 동시 접속 수
        uint32_t backlog = DEFAULT_BACKLOG;                     // listen() backlog 크기

        // 버퍼 설정
        size_t sendBufferSize = DEFAULT_SEND_BUFFER_SIZE;       // 송신 버퍼 크기
        size_t recvBufferSize = DEFAULT_RECV_BUFFER_SIZE;       // 수신 버퍼 크기
        
        // 소켓 옵션
        bool noDelay = true;                                    // Nagle 알고리즘 비활성화 (true = 비활성화)
        bool keepAlive = true;                                  // TCP Keep-Alive 활성화
        uint32_t keepAliveTime = 7200000;                       // Keep-Alive 시작 시간 (ms, 기본 2시간)
        uint32_t keepAliveInterval = 1000;                      // Keep-Alive 간격 (ms, 기본 1초)
        
        // RIO 전용 설정
        uint32_t rioReceiveBufferCount = 1024;                  // RIO 수신 버퍼 개수
        uint32_t rioSendBufferCount = 1024;                     // RIO 송신 버퍼 개수
        uint32_t rioMaxOutstandingReceive = 100;                // 대기 중인 최대 수신 작업
        uint32_t rioMaxOutstandingSend = 100;                   // 대기 중인 최대 송신 작업
        uint32_t rioCQSize = 2048;                              // Completion Queue 크기

        // 기본 생성자
        EngineConfig() = default;
        
        // 복사/이동 가능
        EngineConfig(const EngineConfig&) = default;
        EngineConfig& operator=(const EngineConfig&) = default;
        EngineConfig(EngineConfig&&) = default;
        EngineConfig& operator=(EngineConfig&&) = default;

        // 설정 검증
        bool Validate() const;
    };

} // namespace KanchoNet

