cmake_minimum_required(VERSION 3.16)

project(server_project VERSION 1.0.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 빌드 타입 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 컴파일러 플래그
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 크로스컴파일 설정 옵션
option(CROSS_COMPILE_AARCH64 "Enable cross compilation for aarch64" OFF)

if(CROSS_COMPILE_AARCH64)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    
    # 크로스컴파일 툴체인 확인
    find_program(AARCH64_GCC aarch64-linux-gnu-gcc)
    find_program(AARCH64_GXX aarch64-linux-gnu-g++)
    
    if(NOT AARCH64_GCC OR NOT AARCH64_GXX)
        message(FATAL_ERROR "aarch64 cross compilation toolchain not found. Please install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu")
    endif()
    
    set(CMAKE_C_COMPILER ${AARCH64_GCC})
    set(CMAKE_CXX_COMPILER ${AARCH64_GXX})
    
    # 링커와 다른 도구들도 설정
    set(CMAKE_LINKER aarch64-linux-gnu-ld)
    set(CMAKE_AR aarch64-linux-gnu-ar)
    set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
    
    # 시스템 루트 경로 설정
    set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    
    # pkg-config 경로 설정
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
    set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig")
    
    message(STATUS "Cross compiling for aarch64")
    message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER}")
else()
    message(STATUS "Native compilation for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# 컴파일러 플래그
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# pkg-config 사용
find_package(PkgConfig REQUIRED)

# 필요한 패키지들
pkg_check_modules(GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBCURL REQUIRED libcurl)

# OpenSSL 찾기
# find_package(OpenSSL REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)

# 추가 라이브러리들
# find_library(SQLITECPP_LIB NAMES SQLiteCpp sqlitecpp)
find_library(SQLITECPP_LIB NAMES SQLiteCpp sqlitecpp
             HINTS /usr/aarch64-linux-gnu/lib /usr/lib/aarch64-linux-gnu)
find_library(SQLITE3_LIB NAMES sqlite3)
# find_library(SODIUM_LIB NAMES sodium)
pkg_check_modules(SODIUM REQUIRED libsodium)

# 라이브러리 확인 및 대안 처리
if(NOT SQLITECPP_LIB)
    message(WARNING "SQLiteCpp library not found, trying to find alternative names...")
    find_library(SQLITECPP_LIB NAMES libSQLiteCpp.a libsqlitecpp.a SQLiteCpp.a sqlitecpp.a)
    if(NOT SQLITECPP_LIB)
        # pkg-config로 시도
        pkg_check_modules(SQLITECPP sqlite3cpp)
        if(SQLITECPP_FOUND)
            set(SQLITECPP_LIB ${SQLITECPP_LIBRARIES})
        else()
            message(FATAL_ERROR "SQLiteCpp library not found. Please install libsqlitecpp-dev")
        endif()
    endif()
endif()

if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "sqlite3 library not found. Please install libsqlite3-dev")
endif()


# 인클루드 디렉토리
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/otp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/otp/cotp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/otp/QR-Code-generator
    ${GSTREAMER_RTSP_SERVER_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# OTP 소스 파일들
file(GLOB OTP_SOURCES "src/otp/*.cpp")
set(OTP_DEPS_SOURCES "src/otp/QR-Code-generator/qrcodegen.cpp")

# 메인 서버 실행파일
add_executable(server
    server.cpp
    src/rtsp_server.cpp
    src/tcp_server.cpp
    src/request_handlers.cpp
    src/utils.cpp
    src/db_management.cpp
    src/metadata_parser.cpp
    src/hash.cpp
    src/ssl.cpp
    src/curl_camera.cpp
    src/config_manager.cpp
    ${OTP_SOURCES}
    ${OTP_DEPS_SOURCES}
)

# 메인 서버 링크 라이브러리
target_link_libraries(server
    ${GSTREAMER_RTSP_SERVER_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLITECPP_LIB}
    ${SQLITE3_LIB}
    ${SODIUM_LIB}
    sodium
    pthread
)

# 메인 서버 컴파일 정의
target_compile_options(server PRIVATE
    ${GSTREAMER_RTSP_SERVER_CFLAGS_OTHER}
    ${GSTREAMER_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
)

# 메타데이터 제어 실행파일
add_executable(control
    src/metadata/main_control.cpp
    src/metadata/board_control.cpp
    src/config_manager.cpp
)

# 메타데이터 제어 링크 라이브러리
target_link_libraries(control
    ${GSTREAMER_RTSP_SERVER_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${SQLITECPP_LIB}
    ${SQLITE3_LIB}
)

target_compile_options(control PRIVATE
    ${GSTREAMER_RTSP_SERVER_CFLAGS_OTHER}
    ${GSTREAMER_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
)

# 설치 규칙
install(TARGETS server control
    RUNTIME DESTINATION bin
)

# 클린 타겟 (이미 CMake에 내장되어 있음)
# make clean 또는 cmake --build . --target clean 사용 가능

# 사용법 출력
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Server Project Build Configuration")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Required packages (Ubuntu/Debian):")
message(STATUS "  sudo apt update")
message(STATUS "  sudo apt install -y \\")
message(STATUS "    build-essential cmake pkg-config \\")
message(STATUS "    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \\")
message(STATUS "    libgstreamer-rtsp-server-1.0-dev \\")
message(STATUS "    libglib2.0-dev libcurl4-openssl-dev \\")
message(STATUS "    libssl-dev libsqlite3-dev \\")
message(STATUS "    libsodium-dev libsqlitecpp-dev")
message(STATUS "")
message(STATUS "For cross compilation (aarch64):")
message(STATUS "  sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu")
message(STATUS "")
message(STATUS "Build instructions:")
message(STATUS "  Native build:")
message(STATUS "    mkdir -p build && cd build")
message(STATUS "    cmake ..")
message(STATUS "    make -j\$(nproc)")
message(STATUS "")
message(STATUS "  Cross compile for aarch64:")
message(STATUS "    mkdir -p build && cd build")
message(STATUS "    cmake -DCROSS_COMPILE_AARCH64=ON ..")
message(STATUS "    make -j\$(nproc)")
message(STATUS "")
message(STATUS "Output executables:")
message(STATUS "  - server: Main TCP/RTSP server")
message(STATUS "  - control: Metadata control server")
message(STATUS "========================================")
message(STATUS "")
