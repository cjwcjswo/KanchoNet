# KanchoNet 게임 서버 엔진

Windows와 Linux 환경에서 동작하는 고성능 크로스 플랫폼 C++ 게임 서버 엔진입니다.

## 주요 특징

- **크로스 플랫폼**: Windows (IOCP/RIO), Linux (epoll/io_uring) 지원
- **다양한 네트워크 모델**: 템플릿 기반으로 컴파일 타임에 선택
  - Windows: IOCP, RIO (Registered I/O)
  - Linux: epoll, io_uring (커널 5.1+)
- **콜백 기반 인터페이스**: OnAccept, OnReceive, OnDisconnect 등 간편한 이벤트 처리
- **바이너리 패킷 처리**: 직접 바이너리 처리 또는 Protobuf 직렬화 지원
- **커스터마이징 가능**: 어플리케이션에서 엔진을 상속하여 확장
- **고성능**: 각 플랫폼에 최적화된 I/O 모델 사용

## 지원 플랫폼

| 플랫폼 | 네트워크 모델 | 요구사항 |
|--------|--------------|---------|
| Windows | IOCP | Windows 10 이상 |
| Windows | RIO | Windows 8 이상 |
| Linux | epoll | 모든 Linux 배포판 |
| Linux | io_uring | Linux Kernel 5.1+, liburing |

## 시스템 요구사항

### Windows
- Windows 10 이상
- Visual Studio 2022
- C++17 이상

### Linux
- GCC 7+ 또는 Clang 5+
- CMake 3.15 이상
- pthread
- (선택) liburing-dev (io_uring 사용 시)

## 빌드 방법

### Windows

```bash
# Visual Studio 사용
# 1. KanchoNet.sln 열기
# 2. 빌드 구성 선택 (Debug/Release, x64)
# 3. 솔루션 빌드

# 또는 명령줄에서
build.bat Release
```

### Linux

```bash
# 의존성 설치 (Ubuntu/Debian)
sudo apt-get install build-essential cmake
sudo apt-get install liburing-dev  # io_uring 사용 시

# 빌드
chmod +x build.sh
./build.sh Release

# 실행
./build/bin/EchoServer
```

## 사용 예제

### 크로스 플랫폼 Echo 서버

```cpp
#include <KanchoNet.h>

// 플랫폼별 네트워크 모델 선택
#ifdef KANCHONET_PLATFORM_WINDOWS
    using DefaultNetworkModel = KanchoNet::IOCPModel;
#elif defined(KANCHONET_PLATFORM_LINUX)
    using DefaultNetworkModel = KanchoNet::EpollModel;
#endif

class MyEchoServer : public KanchoNet::NetworkEngine<DefaultNetworkModel>
{
protected:
    void OnAccept(KanchoNet::Session* session) override
    {
        // 클라이언트 접속 처리
        LOG_INFO("Client connected: %llu", session->GetID());
    }

    void OnReceive(KanchoNet::Session* session, 
                   const uint8_t* data, size_t size) override
    {
        // 받은 데이터를 그대로 다시 전송 (Echo)
        KanchoNet::PacketBuffer buffer(data, size);
        Send(session, buffer);
    }

    void OnDisconnect(KanchoNet::Session* session) override
    {
        // 연결 종료 처리
        LOG_INFO("Client disconnected: %llu", session->GetID());
    }
};

int main()
{
    MyEchoServer server;
    KanchoNet::EngineConfig config;
    config.port = 9000;
    config.maxSessions = 1000;
    config.noDelay = true;    // Nagle 알고리즘 비활성화
    config.keepAlive = true;  // TCP Keep-Alive 활성화
    
    if (!server.Initialize(config))
    {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    if (!server.Start())
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    // 워커 스레드 생성 (멀티스레드)
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i)
    {
        workers.emplace_back([&]() {
            while (server.IsRunning()) {
                server.ProcessIO();
            }
        });
    }
    
    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;
    
    for (auto& t : workers) {
        t.join();
    }
    
    return 0;
}
```

### Windows 전용 RIO 서버

```cpp
#ifdef KANCHONET_PLATFORM_WINDOWS

class MyRIOServer : public KanchoNet::NetworkEngine<KanchoNet::RIOModel>
{
    // ... 동일한 콜백 구현
};

#endif
```

### Linux io_uring 서버

```cpp
#ifdef KANCHONET_PLATFORM_LINUX

class MyIOUringServer : public KanchoNet::NetworkEngine<KanchoNet::IOUringModel>
{
    // ... 동일한 콜백 구현
};

#endif
```

## 프로젝트 구조

```
KanchoNet/
├── Platform.h          # 플랫폼 감지
├── Types.h             # 공통 타입 정의
├── KanchoNet.h         # 통합 헤더
│
├── Core/               # 핵심 엔진
│   ├── NetworkEngine.h
│   ├── INetworkModel.h
│   └── EngineConfig.h
│
├── Network/            # 네트워크 모델
│   ├── IOCPModel.h/cpp      # Windows IOCP
│   ├── RIOModel.h/cpp       # Windows RIO
│   ├── EpollModel.h/cpp     # Linux epoll
│   ├── IOUringModel.h/cpp   # Linux io_uring
│   └── SocketUtils.h/cpp    # 소켓 유틸리티
│
├── Session/            # 세션 관리
│   ├── Session.h/cpp
│   ├── SessionManager.h/cpp
│   └── SessionConfig.h
│
├── Buffer/             # 버퍼 관리
│   ├── PacketBuffer.h/cpp
│   ├── RingBuffer.h/cpp
│   └── BufferPool.h/cpp
│
└── Utils/              # 유틸리티
    ├── NonCopyable.h
    ├── SpinLock.h/cpp
    └── Logger.h/cpp

Examples/
├── EchoServer/         # Echo 서버 예제
├── ChatServer/         # 채팅 서버 예제
└── ProtobufServer/     # Protobuf 통합 예제
```

## 예제 서버

프로젝트에는 3가지 예제 서버가 포함되어 있습니다:

1. **EchoServer**: 받은 데이터를 그대로 전송하는 간단한 예제
   - Windows: IOCP 사용
   - Linux: epoll 사용

2. **ChatServer**: 다중 클라이언트 채팅 서버
   - Windows: RIO 사용
   - Linux: io_uring (또는 epoll) 사용

3. **ProtobufServer**: Google Protobuf 통합 예제
   - 바이너리 위에서 Protobuf 직렬화/역직렬화 방법 제시

## 네트워크 모델 성능 비교

| 모델 | 플랫폼 | 성능 | 사용 시기 |
|------|--------|------|----------|
| IOCP | Windows | 높음 | 일반적인 Windows 서버 |
| RIO | Windows | 매우 높음 | 매우 높은 성능이 필요한 경우 |
| epoll | Linux | 높음 | 일반적인 Linux 서버 |
| io_uring | Linux | 매우 높음 | 커널 5.1+ 환경에서 최고 성능 |

## 설정 옵션

```cpp
KanchoNet::EngineConfig config;
config.port = 9000;                  // 리슨 포트
config.maxSessions = 10000;          // 최대 동시 접속 수
config.backlog = 200;                // 리슨 백로그
config.noDelay = true;               // TCP_NODELAY (Nagle 알고리즘)
config.keepAlive = true;             // TCP Keep-Alive
config.keepAliveTime = 10000;        // Keep-Alive 시간 (ms)
config.keepAliveInterval = 3000;     // Keep-Alive 간격 (ms)
```

## 문서

더 자세한 문서는 [Wiki](../../wiki)를 참조하세요.

## 기여

버그 리포트 및 기능 제안은 [Issues](../../issues)에 등록해주세요.

## 라이선스

MIT License

## 향후 계획

- [ ] macOS 지원 (kqueue)
- [ ] FreeBSD 지원 (kqueue)
- [ ] 성능 벤치마크 도구
- [ ] 더 많은 예제 서버
- [ ] 상세한 문서화

