cmake_minimum_required(VERSION 3.20)
project(libmetal_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# P3.1a 先固化模块边界与编译单元列表，后续按模块补实现。
set(METAL_BRIDGE_PUBLIC_HEADERS
    include/metal_bridge.h
)

set(METAL_BRIDGE_SOURCES
    src/MetalDevice.cpp
    src/MetalQueue.cpp
    src/MetalBuffer.cpp
    src/MetalTexture.cpp
    src/ShaderCompiler.cpp
    src/CommandMapper.cpp
    src/Presenter.cpp
)

add_library(metal_bridge STATIC
    ${METAL_BRIDGE_PUBLIC_HEADERS}
    ${METAL_BRIDGE_SOURCES}
)

target_include_directories(metal_bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# TODO: Phase 4
# - 接入 metal-cpp / macOS SDK / Foundation / Metal / QuartzCore
# - 根据模块拆分补 .mm 或桥接实现文件
# - 增加 tests/test_device.cpp 等测试目标
