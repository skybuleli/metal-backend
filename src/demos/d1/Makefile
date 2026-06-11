.PHONY: all build run clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
TARGET := build/d1_triangle
SOURCE := src/main.cpp

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET)

clean:
	rm -rf build out
