#include "ChatServer.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

int main()
{
    std::cout << "==================================" << std::endl;
    #ifdef KANCHONET_PLATFORM_WINDOWS
        std::cout << "  KanchoNet Chat Server (RIO)" << std::endl;
    #elif defined(KANCHONET_PLATFORM_LINUX)
        #ifdef KANCHONET_HAS_LIBURING
            std::cout << "  KanchoNet Chat Server (io_uring)" << std::endl;
        #else
            std::cout << "  KanchoNet Chat Server (epoll)" << std::endl;
        #endif
    #endif
    std::cout << "  Platform: " << KANCHONET_PLATFORM_NAME << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;

    // 로그 레벨 설정
    KanchoNet::Logger::GetInstance().SetLogLevel(KanchoNet::LogLevel::Info);

    // Chat 서버 생성
    ChatServer server;

    // 서버 설정
    KanchoNet::EngineConfig config;
    config.mPort = 9001;
    config.mMaxSessions = 1000;
    config.mBacklog = 200;
    config.mNoDelay = true;
    config.mKeepAlive = true;

    // 초기화
    std::cout << "Initializing Chat Server..." << std::endl;
    if (!server.Initialize(config))
    {
        std::cerr << "Failed to initialize server" << std::endl;
        #ifdef KANCHONET_PLATFORM_WINDOWS
            std::cerr << "Note: RIO requires Windows 8 or later" << std::endl;
        #elif defined(KANCHONET_PLATFORM_LINUX)
            #ifdef KANCHONET_HAS_LIBURING
                std::cerr << "Note: io_uring requires Linux kernel 5.1+" << std::endl;
            #endif
        #endif
        return 1;
    }

    // 시작
    if (!server.Start())
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Chat Server started on port " << config.mPort << std::endl;
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

