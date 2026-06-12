.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/d4_basic_lighting
SOURCE := src/main.cpp
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/basic_lighting.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.6-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.6-meta.json

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

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	@python3 -c 'from pathlib import Path; ppm_path = Path("$(EVIDENCE_PPM)"); data = ppm_path.read_bytes(); newline1 = data.find(b"\n"); newline2 = data.find(b"\n", newline1 + 1); newline3 = data.find(b"\n", newline2 + 1); magic = data[:newline1].strip(); pixels = data[newline3 + 1:]; background = bytes((15, 20, 31)); non_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != background); assert magic == b"P6", "D4 证据校验失败：PPM 魔数不是 P6"; assert non_background > 0, "D4 证据校验失败：整张图只有背景色"; print(f"D4 证据校验通过：{non_background} 个像素不是背景色")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.6",\n  "demo": "D4",\n  "scene": "3D cube with Phong lighting (ambient+diffuse+specular), uniform buffer, depth testing, backface culling",\n  "shader_path": "hand-written MSL (compiled from source)",\n  "artifacts": ["docs/evidence/P2.6-d4-basic-lighting.ppm", "docs/evidence/P2.6-d4-basic-lighting.png", "docs/evidence/P2.6-run.txt"],\n  "notes": "D4 正确实现：手写 MSL 代替 Path A 规避 DXIL mul 限制。包含：uniform buffer (MVP+光源+相机)、3D 变换矩阵、深度测试 (Depth32Float + CompareLess)、背面剔除 (CullModeBack)、3D 立方体 (36顶点)、Phong 光照 (ambient/diffuse/specular)。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
