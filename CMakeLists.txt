cmake_minimum_required(VERSION 3.15)
project(KanchoNet VERSION 1.0.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows에서는 Visual Studio 솔루션 사용
if(WIN32)
    message(FATAL_ERROR "Windows에서는 Visual Studio 솔루션(KanchoNet.sln)을 사용하세요.")
endif()

# 빌드 타입 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")

# 컴파일러 플래그
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 하위 디렉토리 추가
add_subdirectory(KanchoNet)
add_subdirectory(Examples)

# 설치 설정
install(DIRECTORY KanchoNet/
    DESTINATION include/KanchoNet
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS KanchoNet
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

