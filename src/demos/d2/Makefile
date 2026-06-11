.PHONY: all build run clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
SLANGC ?= slangc
MSC ?= metal-shaderconverter

TARGET := build/d2_textured_quad
SOURCE := src/main.cpp
SHADER := shaders/quad.slang
VERT_DXIL := build/quad_vertex.dxil
FRAG_DXIL := build/quad_fragment.dxil
VERT_METALLIB := build/quad_vertex.metallib
FRAG_METALLIB := build/quad_fragment.metallib

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(SLANGC) $(SHADER) -target dxil -entry vertexMain -stage vertex -profile sm_6_0 -o $(VERT_DXIL)
	$(SLANGC) $(SHADER) -target dxil -entry fragmentMain -stage fragment -profile ps_6_0 -o $(FRAG_DXIL)
	$(MSC) $(VERT_DXIL) -o $(VERT_METALLIB)
	$(MSC) $(FRAG_DXIL) -o $(FRAG_METALLIB)
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET)

clean:
	rm -rf build out
