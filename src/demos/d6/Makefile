.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics
SLANGC ?= slangc
MSC ?= metal-shaderconverter

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
ARGS ?=

TARGET := build/d6_advanced_lighting
SOURCE := src/main.cpp
SHADER := shaders/advanced_lighting.slang
SHADOW_DXIL := build/d6_shadow_vertex.dxil
SCENE_VERT_DXIL := build/d6_scene_vertex.dxil
SCENE_FRAG_DXIL := build/d6_scene_fragment.dxil
SHADOW_METALLIB := build/d6_shadow_vertex.metallib
SCENE_VERT_METALLIB := build/d6_scene_vertex.metallib
SCENE_FRAG_METALLIB := build/d6_scene_fragment.metallib
SHADOW_REFLECT := build/d6_shadow_vertex.reflect.json
SCENE_VERT_REFLECT := build/d6_scene_vertex.reflect.json
SCENE_FRAG_REFLECT := build/d6_scene_fragment.reflect.json
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/advanced_lighting.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.8a-d6-patha-advanced-lighting.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.8a-d6-patha-advanced-lighting.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.8a-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.8a-meta.json
EVIDENCE_SHADOW_REFLECT := $(EVIDENCE_DIR)/P2.8a-shadow-vertex-reflection.json
EVIDENCE_SCENE_VERT_REFLECT := $(EVIDENCE_DIR)/P2.8a-scene-vertex-reflection.json
EVIDENCE_SCENE_FRAG_REFLECT := $(EVIDENCE_DIR)/P2.8a-scene-fragment-reflection.json

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(SLANGC) $(SHADER) -target dxil -entry shadowVertex -stage vertex -profile sm_6_0 -o $(SHADOW_DXIL)
	$(SLANGC) $(SHADER) -target dxil -entry sceneVertex -stage vertex -profile sm_6_0 -o $(SCENE_VERT_DXIL)
	$(SLANGC) $(SHADER) -target dxil -entry sceneFragment -stage fragment -profile ps_6_0 -o $(SCENE_FRAG_DXIL)
	$(MSC) $(SHADOW_DXIL) -o $(SHADOW_METALLIB) --output-reflection-file $(SHADOW_REFLECT)
	$(MSC) $(SCENE_VERT_DXIL) -o $(SCENE_VERT_METALLIB) --output-reflection-file $(SCENE_VERT_REFLECT)
	$(MSC) $(SCENE_FRAG_DXIL) -o $(SCENE_FRAG_METALLIB) --output-reflection-file $(SCENE_FRAG_REFLECT)
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET) $(ARGS)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	cp $(SHADOW_REFLECT) $(EVIDENCE_SHADOW_REFLECT)
	cp $(SCENE_VERT_REFLECT) $(EVIDENCE_SCENE_VERT_REFLECT)
	cp $(SCENE_FRAG_REFLECT) $(EVIDENCE_SCENE_FRAG_REFLECT)
	@python3 -c $$'from pathlib import Path\nppm_path = Path("$(EVIDENCE_PPM)")\ndata = ppm_path.read_bytes()\nn1 = data.find(b"\\n")\nn2 = data.find(b"\\n", n1 + 1)\nn3 = data.find(b"\\n", n2 + 1)\nmagic = data[:n1].strip()\nwidth, height = map(int, data[n1 + 1:n2].decode().strip().split())\npixels = data[n3 + 1:]\nassert magic == b"P6", "D6 证据校验失败：PPM 魔数不是 P6"\nassert (width, height) == (768, 768), f"D6 证据尺寸错误: {(width, height)}"\nnon_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != bytes((12, 18, 28)))\nbright_pixels = sum(1 for i in range(0, len(pixels), 3) if max(pixels[i:i + 3]) >= 230)\nassert non_background > 0, "D6 证据校验失败：整张图只有背景色"\nassert bright_pixels > 2000, f"D6 证据校验失败：高亮像素不足 ({bright_pixels})"\nprint(f"D6 证据校验通过：{width}x{height}, 非背景像素 {non_background}, 高亮像素 {bright_pixels}")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.8a",\n  "demo": "D6",\n  "scene": "方向光阴影 + HDR 主场景 + Bloom 提取/模糊 + Tone Mapping 合成",\n  "shader_path": "Path A for shadow/scene (Slang -> DXIL -> MSC -> metallib), hand-written MSL for post-process",\n  "artifacts": ["docs/evidence/P2.8a-d6-patha-advanced-lighting.ppm", "docs/evidence/P2.8a-d6-patha-advanced-lighting.png", "docs/evidence/P2.8a-run.txt", "docs/evidence/P2.8a-shadow-vertex-reflection.json", "docs/evidence/P2.8a-scene-vertex-reflection.json", "docs/evidence/P2.8a-scene-fragment-reflection.json"],\n  "notes": "D6 的 shadow pass 与 HDR scene pass 已切换到 Path A。Bloom 和 tone mapping 仍保留手写 MSL，作为 P2.8b 双路径对照的稳定基线。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"
	@echo "  $(EVIDENCE_SHADOW_REFLECT)"
	@echo "  $(EVIDENCE_SCENE_VERT_REFLECT)"
	@echo "  $(EVIDENCE_SCENE_FRAG_REFLECT)"

clean:
	rm -rf build out
