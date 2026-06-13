cmake_minimum_required(VERSION 3.20)
project(libmetal_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── 自动获取 macOS SDK 路径（metal-cpp 头文件依赖 SDK 中的 <cstdint>）──
execute_process(COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE SDKROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(SDKROOT)
    set(CMAKE_OSX_SYSROOT "${SDKROOT}")
    message(STATUS "macOS SDK: ${SDKROOT}")
else()
    message(WARNING "无法自动获取 macOS SDK 路径，编译可能失败")
endif()

# ── C++ 标准库头文件路径（CLT 环境中 -isysroot 不自动包含 <c++/v1>）──
# D1 Makefile 同样采用 -isysroot + -I<SDK>/usr/include/c++/v1 双保险
if(SDKROOT)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${SDKROOT}/usr/include/c++/v1")
    message(STATUS "C++ 头文件: ${SDKROOT}/usr/include/c++/v1")
endif()

# ── metal-cpp 路径（默认从 ~/GraphicsExperiments/third_party/metal-cpp 查找）──
# 可通过 -DMETAL_CPP_DIR=... 覆盖
if(NOT METAL_CPP_DIR)
    set(METAL_CPP_DIR "$ENV{HOME}/GraphicsExperiments/third_party/metal-cpp" CACHE PATH "metal-cpp 头文件路径")
endif()

# ── 公共头文件 ──
set(METAL_BRIDGE_PUBLIC_HEADERS
    include/metal_bridge.h
    include/metal_limits.h
    include/metal_internal.h
)

set(METAL_BRIDGE_SOURCES
    src/MetalDevice.cpp
    src/MetalQueue.cpp
    src/MetalBuffer.cpp
    src/MetalTexture.cpp
    src/ShaderCompiler.cpp
    src/CommandMapper.cpp
    src/Presenter.cpp
    src/MetalSampler.cpp
    src/MetalHeap.cpp
)

add_library(metal_bridge STATIC
    ${METAL_BRIDGE_PUBLIC_HEADERS}
    ${METAL_BRIDGE_SOURCES}
)

target_include_directories(metal_bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── 系统框架：用 find_library 获取框架路径 ──
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

mark_as_advanced(METAL_FRAMEWORK FOUNDATION_FRAMEWORK QUARTZCORE_FRAMEWORK)

target_link_libraries(metal_bridge
    PUBLIC
        "${METAL_FRAMEWORK}"
        "${FOUNDATION_FRAMEWORK}"
        "${QUARTZCORE_FRAMEWORK}"
)

if(SLANG_LIB)
    target_link_libraries(metal_bridge PRIVATE "${SLANG_LIB}")
    target_include_directories(metal_bridge PRIVATE "${SLANG_INCLUDE_DIR}")
    target_compile_definitions(metal_bridge PRIVATE METAL_SLANG_FOUND=1)
else()
    target_compile_definitions(metal_bridge PRIVATE METAL_SLANG_FOUND=0)
endif()

# ── Slang 着色器编译器 ──
find_library(SLANG_LIB slang HINTS /usr/local/lib)
if(SLANG_LIB)
    message(STATUS "Slang 库: ${SLANG_LIB}")
else()
    message(WARNING "未找到 libslang.dylib，P4.2.2 着色器编译将不可用")
endif()

find_path(SLANG_INCLUDE_DIR slang.h HINTS /usr/local/include/slang)
if(SLANG_INCLUDE_DIR)
    message(STATUS "Slang 头文件: ${SLANG_INCLUDE_DIR}")
else()
    message(WARNING "未找到 slang.h，P4.2.2 着色器编译将不可用")
endif()

# ── metal-cpp 私有头文件路径（不对外暴露）──
if(EXISTS "${METAL_CPP_DIR}")
    target_include_directories(metal_bridge
        PRIVATE
            "${METAL_CPP_DIR}"
    )
    target_compile_definitions(metal_bridge PRIVATE METAL_CPP_FOUND=1)
    message(STATUS "metal-cpp 路径: ${METAL_CPP_DIR}")
else()
    message(WARNING "metal-cpp 未找到: ${METAL_CPP_DIR}。")
    message(WARNING "请在 CMake 命令行中指定 -DMETAL_CPP_DIR=...")
    target_compile_definitions(metal_bridge PRIVATE METAL_CPP_FOUND=0)
endif()

# ── Catch2 测试框架 ──
option(BUILD_TESTS "构建单元测试" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Catch2 REQUIRED)

    add_executable(test_device
        tests/test_device.cpp
    )
    target_link_libraries(test_device PRIVATE
        metal_bridge
        Catch2::Catch2WithMain
    )
    target_include_directories(test_device PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_device COMMAND test_device)

    add_executable(test_buffer
        tests/test_buffer.cpp
    )
    target_link_libraries(test_buffer PRIVATE
        metal_bridge
        Catch2::Catch2WithMain
    )
    target_include_directories(test_buffer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_buffer COMMAND test_buffer)

    add_executable(test_texture
        tests/test_texture.cpp
    )
    target_link_libraries(test_texture PRIVATE
        metal_bridge
        Catch2::Catch2WithMain
    )
    target_include_directories(test_texture PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_texture COMMAND test_texture)

    add_executable(test_sampler
        tests/test_sampler.cpp
    )
    target_link_libraries(test_sampler PRIVATE
        metal_bridge
        Catch2::Catch2WithMain
    )
    target_include_directories(test_sampler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_sampler COMMAND test_sampler)

    add_executable(test_heap
        tests/test_heap.cpp
    )
    target_link_libraries(test_heap PRIVATE
        metal_bridge
        Catch2::Catch2WithMain
    )
    target_include_directories(test_heap PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME test_heap COMMAND test_heap)
endif()
