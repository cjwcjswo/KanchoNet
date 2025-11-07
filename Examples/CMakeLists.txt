cmake_minimum_required(VERSION 3.15)

# 예제 서버 빌드 함수
function(add_example_server TARGET_NAME SOURCE_DIR)
    # 소스 파일
    file(GLOB SOURCES "${SOURCE_DIR}/*.cpp")
    file(GLOB HEADERS "${SOURCE_DIR}/*.h")
    
    # 실행 파일 생성
    add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})
    
    # KanchoNet 라이브러리 링크
    target_link_libraries(${TARGET_NAME} PRIVATE KanchoNet)
    
    # Include 디렉토리
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/KanchoNet
    )
    
    # 출력 디렉토리
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    message(STATUS "Added example: ${TARGET_NAME}")
endfunction()

# 예제 서버들 추가
add_example_server(EchoServer ${CMAKE_CURRENT_SOURCE_DIR}/EchoServer)
add_example_server(ChatServer ${CMAKE_CURRENT_SOURCE_DIR}/ChatServer)
add_example_server(ProtobufServer ${CMAKE_CURRENT_SOURCE_DIR}/ProtobufServer)

