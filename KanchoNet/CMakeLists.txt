cmake_minimum_required(VERSION 3.15)

# KanchoNet 라이브러리 소스 파일
set(KANCHONET_SOURCES
    # Core
    Core/EngineConfig.cpp
    Core/NetworkEngine.cpp
    
    # Session
    Session/Session.cpp
    Session/SessionManager.cpp
    Session/SessionConfig.cpp
    
    # Buffer
    Buffer/PacketBuffer.cpp
    Buffer/RingBuffer.cpp
    Buffer/BufferPool.cpp
    
    # Utils
    Utils/SpinLock.cpp
    Utils/Logger.cpp
    
    # Network - 공통
    Network/SocketUtils.cpp
)

# Linux 전용 소스
if(UNIX AND NOT APPLE)
    list(APPEND KANCHONET_SOURCES
        Network/EpollModel.cpp
        Network/IOUringModel.cpp
    )
endif()

# KanchoNet 라이브러리 생성
add_library(KanchoNet STATIC ${KANCHONET_SOURCES})

# Include 디렉토리
target_include_directories(KanchoNet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Linux 전용 설정
if(UNIX AND NOT APPLE)
    # pthread 링크
    find_package(Threads REQUIRED)
    target_link_libraries(KanchoNet PUBLIC Threads::Threads)
    
    # liburing 찾기 (선택적)
    find_library(LIBURING_LIBRARY uring)
    if(LIBURING_LIBRARY)
        message(STATUS "Found liburing: ${LIBURING_LIBRARY}")
        target_link_libraries(KanchoNet PUBLIC ${LIBURING_LIBRARY})
        target_compile_definitions(KanchoNet PUBLIC KANCHONET_HAS_LIBURING)
    else()
        message(WARNING "liburing not found. IOUringModel will not be available.")
        message(WARNING "Install with: sudo apt-get install liburing-dev (Ubuntu/Debian)")
    endif()
endif()

# 컴파일 옵션
target_compile_options(KanchoNet PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# 라이브러리 출력 디렉토리
set_target_properties(KanchoNet PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

