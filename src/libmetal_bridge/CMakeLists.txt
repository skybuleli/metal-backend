cmake_minimum_required(VERSION 3.20)
project(libmetal_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── metal-cpp 路径（默认从 ~/GraphicsExperiments/third_party/metal-cpp 查找）──
# 可通过 -DMETAL_CPP_DIR=... 覆盖
if(NOT METAL_CPP_DIR)
    set(METAL_CPP_DIR "$ENV{HOME}/GraphicsExperiments/third_party/metal-cpp" CACHE PATH "metal-cpp 头文件路径")
endif()

# ── 公共头文件 ──
set(METAL_BRIDGE_PUBLIC_HEADERS
    include/metal_bridge.h
    include/metal_limits.h
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

# ── 系统框架：Metal / Foundation / QuartzCore ──
target_link_libraries(metal_bridge
    PUBLIC
        -framework Metal
        -framework Foundation
        -framework QuartzCore
)

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

# ── 测试目标（后续递增）──
option(BUILD_TESTS "构建单元测试" OFF)
if(BUILD_TESTS)
    add_executable(test_device tests/test_device.cpp)
    target_link_libraries(test_device PRIVATE metal_bridge)
    target_include_directories(test_device PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
