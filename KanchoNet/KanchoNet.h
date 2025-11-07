#pragma once

// KanchoNet 게임 서버 엔진
// 통합 헤더 파일 - 어플리케이션에서 이 헤더만 포함하면 됨

// 플랫폼 감지
#include "Platform.h"

// 공통 타입
#include "Types.h"

// 핵심 엔진
#include "Core/NetworkEngine.h"
#include "Core/INetworkModel.h"
#include "Core/EngineConfig.h"

// 플랫폼별 네트워크 모델
#ifdef KANCHONET_PLATFORM_WINDOWS
    #include "Network/IOCPModel.h"
    #include "Network/RIOModel.h"
#elif defined(KANCHONET_PLATFORM_LINUX)
    #include "Network/EpollModel.h"
    #include "Network/IOUringModel.h"
#endif

// 세션 관리
#include "Session/Session.h"
#include "Session/SessionManager.h"
#include "Session/SessionConfig.h"

// 버퍼 관리
#include "Buffer/PacketBuffer.h"
#include "Buffer/RingBuffer.h"
#include "Buffer/BufferPool.h"

// 유틸리티
#include "Utils/NonCopyable.h"
#include "Utils/SpinLock.h"
#include "Utils/Logger.h"

// 네임스페이스 사용 예제:
// using namespace KanchoNet;
// 
// Windows:
// class MyGameServer : public NetworkEngine<IOCPModel> { ... };
// 
// Linux:
// class MyGameServer : public NetworkEngine<EpollModel> { ... };
//
// 크로스 플랫폼:
// #ifdef KANCHONET_PLATFORM_WINDOWS
//     using DefaultNetworkModel = IOCPModel;
// #elif defined(KANCHONET_PLATFORM_LINUX)
//     using DefaultNetworkModel = EpollModel;
// #endif
// class MyGameServer : public NetworkEngine<DefaultNetworkModel> { ... };

