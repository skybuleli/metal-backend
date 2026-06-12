.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/d5_advanced_texturing
SOURCE := src/main.cpp
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/advanced_texturing.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.7-d5-advanced-texturing.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.7-d5-advanced-texturing.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.7-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.7-meta.json

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
	@python3 -c 'from pathlib import Path; ppm_path = Path("$(EVIDENCE_PPM)"); data = ppm_path.read_bytes(); n1 = data.find(b"\n"); n2 = data.find(b"\n", n1 + 1); n3 = data.find(b"\n", n2 + 1); magic = data[:n1].strip(); pixels = data[n3 + 1:]; background = bytes((15, 20, 31)); non_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != background); assert magic == b"P6", "D5 证据校验失败：PPM 魔数不是 P6"; assert non_background > 0, "D5 证据校验失败：整张图只有背景色"; print(f"D5 证据校验通过：{non_background} 个像素不是背景色")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.7",\n  "demo": "D5",\n  "scene": "法线贴图平面 + 程序化 Cubemap Skybox + 4x MSAA resolve",\n  "shader_path": "hand-written MSL (compiled from source)",\n  "artifacts": ["docs/evidence/P2.7-d5-advanced-texturing.ppm", "docs/evidence/P2.7-d5-advanced-texturing.png", "docs/evidence/P2.7-run.txt"],\n  "notes": "D5 第一版聚焦法线贴图、Skybox/Cubemap 与 4x MSAA 子集。RTT 留待后续更复杂后处理链路再引入。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
