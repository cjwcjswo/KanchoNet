#pragma once

#include "../Types.h"

namespace KanchoNet
{
    // 네트워크 엔진 설정 구조체
    struct EngineConfig
    {
    public:
        // public 멤버변수
        // 네트워크 설정
        uint16_t mPort = DEFAULT_PORT;                           // 리슨 포트
        uint32_t mMaxSessions = DEFAULT_MAX_SESSIONS;            // 최대 동시 접속 수
        uint32_t mBacklog = DEFAULT_BACKLOG;                     // listen() backlog 크기

        // 버퍼 설정
        size_t mSendBufferSize = DEFAULT_SEND_BUFFER_SIZE;       // 송신 버퍼 크기
        size_t mRecvBufferSize = DEFAULT_RECV_BUFFER_SIZE;       // 수신 버퍼 크기
        
        // 소켓 옵션
        bool mNoDelay = true;                                    // Nagle 알고리즘 비활성화 (true = 비활성화)
        bool mKeepAlive = true;                                  // TCP Keep-Alive 활성화
        uint32_t mKeepAliveTime = 7200000;                       // Keep-Alive 시작 시간 (ms, 기본 2시간)
        uint32_t mKeepAliveInterval = 1000;                      // Keep-Alive 간격 (ms, 기본 1초)
        
        // RIO 전용 설정
        uint32_t mRioReceiveBufferCount = 1024;                  // RIO 수신 버퍼 개수
        uint32_t mRioSendBufferCount = 1024;                     // RIO 송신 버퍼 개수
        uint32_t mRioMaxOutstandingReceive = 100;                // 대기 중인 최대 수신 작업
        uint32_t mRioMaxOutstandingSend = 100;                   // 대기 중인 최대 송신 작업
        uint32_t mRioCQSize = 2048;                              // Completion Queue 크기
        
    public:
        // 생성자, 파괴자
        // 기본 생성자
        EngineConfig() = default;
        
        // 복사/이동 가능
        EngineConfig(const EngineConfig&) = default;
        EngineConfig& operator=(const EngineConfig&) = default;
        EngineConfig(EngineConfig&&) = default;
        EngineConfig& operator=(EngineConfig&&) = default;

    public:
        // public 함수
        // 설정 검증
        bool Validate() const;
    };

} // namespace KanchoNet
