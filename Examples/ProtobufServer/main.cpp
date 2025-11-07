#include "ProtobufServer.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

int main()
{
    std::cout << "==================================" << std::endl;
    #ifdef KANCHONET_PLATFORM_WINDOWS
        std::cout << " KanchoNet Protobuf Server (IOCP)" << std::endl;
    #elif defined(KANCHONET_PLATFORM_LINUX)
        std::cout << " KanchoNet Protobuf Server (epoll)" << std::endl;
    #endif
    std::cout << "  Platform: " << KANCHONET_PLATFORM_NAME << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;

    std::cout << "NOTE: This is a Protobuf integration example." << std::endl;
    std::cout << "To use Protobuf, you need to:" << std::endl;
    std::cout << "1. Install Google Protobuf library" << std::endl;
    std::cout << "2. Define .proto files" << std::endl;
    std::cout << "3. Compile with protoc" << std::endl;
    std::cout << "4. Use ParseFromArray() and SerializeToArray()" << std::endl;
    std::cout << std::endl;

    // 로그 레벨 설정
    KanchoNet::Logger::GetInstance().SetLogLevel(KanchoNet::LogLevel::Info);

    // Protobuf 서버 생성
    ProtobufServer server;

    // 서버 설정
    KanchoNet::EngineConfig config;
    config.port = 9002;
    config.maxSessions = 1000;
    config.backlog = 200;
    config.noDelay = true;
    config.keepAlive = true;

    // 초기화
    if (!server.Initialize(config))
    {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    // 시작
    if (!server.Start())
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Protobuf Server started on port " << config.port << std::endl;
    std::cout << "Press 'q' + Enter to quit" << std::endl;
    std::cout << std::endl;

    // 워커 스레드 생성 (4개)
    std::atomic<bool> running(true);
    std::vector<std::thread> workers;
    
    for (int i = 0; i < 4; ++i)
    {
        workers.emplace_back([&server, &running, i]() {
            std::cout << "[Worker " << i << "] Started" << std::endl;
            
            while (running.load())
            {
                // I/O 처리 (100ms 타임아웃)
                server.ProcessIO(100);
            }
            
            std::cout << "[Worker " << i << "] Stopped" << std::endl;
        });
    }

    // 사용자 입력 대기
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input == "q" || input == "Q")
        {
            break;
        }
    }

    // 종료
    std::cout << std::endl;
    std::cout << "Shutting down server..." << std::endl;
    
    running.store(false);
    
    // 워커 스레드 종료 대기
    for (auto& worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    server.Stop();

    std::cout << "Server stopped." << std::endl;
    return 0;
}

