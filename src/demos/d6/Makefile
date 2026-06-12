.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/d6_advanced_lighting
SOURCE := src/main.cpp
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/advanced_lighting.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.8-d6-advanced-lighting.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.8-d6-advanced-lighting.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.8-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.8-meta.json

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
	@python3 -c $$'from pathlib import Path\nppm_path = Path("$(EVIDENCE_PPM)")\ndata = ppm_path.read_bytes()\nn1 = data.find(b"\\n")\nn2 = data.find(b"\\n", n1 + 1)\nn3 = data.find(b"\\n", n2 + 1)\nmagic = data[:n1].strip()\nwidth, height = map(int, data[n1 + 1:n2].decode().strip().split())\npixels = data[n3 + 1:]\nassert magic == b"P6", "D6 证据校验失败：PPM 魔数不是 P6"\nassert (width, height) == (768, 768), f"D6 证据尺寸错误: {(width, height)}"\nnon_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != bytes((12, 18, 28)))\nbright_pixels = sum(1 for i in range(0, len(pixels), 3) if max(pixels[i:i + 3]) >= 230)\nassert non_background > 0, "D6 证据校验失败：整张图只有背景色"\nassert bright_pixels > 2000, f"D6 证据校验失败：高亮像素不足 ({bright_pixels})"\nprint(f"D6 证据校验通过：{width}x{height}, 非背景像素 {non_background}, 高亮像素 {bright_pixels}")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.8",\n  "demo": "D6",\n  "scene": "方向光阴影 + HDR 主场景 + Bloom 提取/模糊 + Tone Mapping 合成",\n  "shader_path": "hand-written MSL (compiled from source)",\n  "artifacts": ["docs/evidence/P2.8-d6-advanced-lighting.ppm", "docs/evidence/P2.8-d6-advanced-lighting.png", "docs/evidence/P2.8-run.txt"],\n  "notes": "D6 采用四段真实 pass：shadow depth、HDR scene、bright extract + blur、tone map composite。场景包含地面、多个投影体和发光立方体，用于同时验证阴影与 bloom。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
