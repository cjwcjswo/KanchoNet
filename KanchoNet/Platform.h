#pragma once

// KanchoNet 플랫폼 감지 및 정의

// 플랫폼 자동 감지
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define KANCHONET_PLATFORM_WINDOWS
    #define KANCHONET_PLATFORM_NAME "Windows"
#elif defined(__linux__)
    #define KANCHONET_PLATFORM_LINUX
    #define KANCHONET_PLATFORM_NAME "Linux"
#elif defined(__APPLE__) || defined(__MACH__)
    #define KANCHONET_PLATFORM_MACOS
    #define KANCHONET_PLATFORM_NAME "macOS"
#elif defined(__FreeBSD__)
    #define KANCHONET_PLATFORM_FREEBSD
    #define KANCHONET_PLATFORM_NAME "FreeBSD"
#else
    #define KANCHONET_PLATFORM_UNKNOWN
    #define KANCHONET_PLATFORM_NAME "Unknown"
#endif

// 지원하는 플랫폼 확인
#if !defined(KANCHONET_PLATFORM_WINDOWS) && !defined(KANCHONET_PLATFORM_LINUX)
    #error "Unsupported platform! KanchoNet currently supports Windows and Linux only."
    #error "Detected platform: " KANCHONET_PLATFORM_NAME
    #error "If you are on macOS or FreeBSD, kqueue support is planned for future releases."
#endif

// 컴파일러 감지
#if defined(_MSC_VER)
    #define KANCHONET_COMPILER_MSVC
    #define KANCHONET_COMPILER_NAME "MSVC"
    #define KANCHONET_COMPILER_VERSION _MSC_VER
#elif defined(__GNUC__) || defined(__GNUG__)
    #define KANCHONET_COMPILER_GCC
    #define KANCHONET_COMPILER_NAME "GCC"
    #define KANCHONET_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__clang__)
    #define KANCHONET_COMPILER_CLANG
    #define KANCHONET_COMPILER_NAME "Clang"
    #define KANCHONET_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#else
    #define KANCHONET_COMPILER_UNKNOWN
    #define KANCHONET_COMPILER_NAME "Unknown"
#endif

// 아키텍처 감지
#if defined(__x86_64__) || defined(_M_X64)
    #define KANCHONET_ARCH_X64
    #define KANCHONET_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
    #define KANCHONET_ARCH_X86
    #define KANCHONET_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define KANCHONET_ARCH_ARM64
    #define KANCHONET_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
    #define KANCHONET_ARCH_ARM
    #define KANCHONET_ARCH_NAME "ARM"
#else
    #define KANCHONET_ARCH_UNKNOWN
    #define KANCHONET_ARCH_NAME "Unknown"
#endif

// 플랫폼별 헤더 포함
#ifdef KANCHONET_PLATFORM_WINDOWS
    // Windows 전용 헤더는 필요한 곳에서 개별적으로 포함
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#ifdef KANCHONET_PLATFORM_LINUX
    // Linux 전용 헤더는 필요한 곳에서 개별적으로 포함
#endif

// 디버그 모드 감지
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
    #define KANCHONET_DEBUG
#else
    #define KANCHONET_RELEASE
#endif

// 경고 메시지 매크로
#ifdef KANCHONET_COMPILER_MSVC
    #define KANCHONET_WARNING(msg) __pragma(message("KanchoNet Warning: " msg))
#elif defined(KANCHONET_COMPILER_GCC) || defined(KANCHONET_COMPILER_CLANG)
    #define KANCHONET_WARNING(msg) _Pragma(#msg)
#else
    #define KANCHONET_WARNING(msg)
#endif

// 플랫폼 정보 출력 (컴파일 시)
#ifdef KANCHONET_DEBUG
    #pragma message("KanchoNet Compile Info:")
    #pragma message("  Platform: " KANCHONET_PLATFORM_NAME)
    #pragma message("  Compiler: " KANCHONET_COMPILER_NAME)
    #pragma message("  Architecture: " KANCHONET_ARCH_NAME)
#endif

