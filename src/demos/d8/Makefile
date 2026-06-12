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

TARGET := build/d8_complex_showcase
SOURCE := src/main.cpp
SHADER := shaders/particle_update.slang
COMPUTE_DXIL := build/d8_particle_update.dxil
COMPUTE_METALLIB := build/d8_particle_update.metallib
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/complex_showcase.ppm
PERF_OUT := out/complex_showcase_perf.json
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.10-d8-complex-showcase.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.10-d8-complex-showcase.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.10-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.10-meta.json
EVIDENCE_PERF := $(EVIDENCE_DIR)/P2.10-perf.json

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(SLANGC) $(SHADER) -target dxil -entry main -profile cs_6_0 -o $(COMPUTE_DXIL) 2>&1 | grep -v "warning\[E00030\]" || true
	$(MSC) $(COMPUTE_DXIL) -o $(COMPUTE_METALLIB)
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET) $(ARGS)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) $(ARGS) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	cp $(PERF_OUT) $(EVIDENCE_PERF)
	@python3 -c $$'from pathlib import Path\nimport json\nppm_path = Path("$(EVIDENCE_PPM)")\ndata = ppm_path.read_bytes()\nn1 = data.find(b"\\n")\nn2 = data.find(b"\\n", n1 + 1)\nn3 = data.find(b"\\n", n2 + 1)\nmagic = data[:n1].strip()\nwidth, height = map(int, data[n1 + 1:n2].decode().strip().split())\npixels = data[n3 + 1:]\nbackground = bytes((4, 6, 12))\nnon_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != background)\nbright_pixels = sum(1 for i in range(0, len(pixels), 3) if max(pixels[i:i + 3]) >= 160)\nassert magic == b"P6", "D8 证据校验失败：PPM 魔数不是 P6"\nassert (width, height) == (960, 720), f"D8 证据尺寸错误: {(width, height)}"\nassert non_background > 30000, f"D8 证据校验失败：非背景像素过少 ({non_background})"\nassert bright_pixels > 5000, f"D8 证据校验失败：明亮像素过少 ({bright_pixels})"\nprint(f"D8 证据校验通过: {non_background} 非背景像素, {bright_pixels} 明亮像素")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1 || true
	@printf '{\n  "task": "P2.10",\n  "demo": "D8",\n  "scene": "PBR spheres (GGX) + shadow map + procedural skybox + bloom + tone mapping + particles + HUD + free camera",\n  "shader_path": {"compute": "path-a-metallib", "render": "handwritten-msl"},\n  "artifacts": ["docs/evidence/P2.10-d8-complex-showcase.ppm", "docs/evidence/P2.10-d8-complex-showcase.png", "docs/evidence/P2.10-run.txt", "docs/evidence/P2.10-perf.json"],\n  "notes": "7 PBR 材质球 (Gold/Chrome/Copper/Plastic/Ceramic/Blue Metal/Green Metal) + Cook-Torrance GGX + 自由轨道摄像机 + 粒子漩涡 + HUD 覆盖层"\n}\n' > $(EVIDENCE_META)
	@echo "已生成 D8 验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"
	@echo "  $(EVIDENCE_PERF)"

clean:
	rm -rf build out
