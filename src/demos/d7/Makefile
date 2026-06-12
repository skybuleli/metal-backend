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

TARGET := build/d7_gpu_driven
SOURCE := src/main.cpp
SHADER := shaders/particle_update.slang
COMPUTE_DXIL := build/d7_particle_update.dxil
COMPUTE_METALLIB := build/d7_particle_update.metallib
COMPUTE_REFLECT := build/d7_particle_update.reflect.json
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/gpu_driven.ppm
PERF_OUT := out/gpu_driven_perf.json
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.9-d7-gpu-driven.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.9-d7-gpu-driven.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.9-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.9-meta.json
EVIDENCE_PERF := $(EVIDENCE_DIR)/P2.9-perf.json
EVIDENCE_COMPUTE_REFLECT := $(EVIDENCE_DIR)/P2.9-compute-reflection.json

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(SLANGC) $(SHADER) -target dxil -entry main -stage compute -profile cs_6_0 -o $(COMPUTE_DXIL)
	$(MSC) $(COMPUTE_DXIL) -o $(COMPUTE_METALLIB) --output-reflection-file $(COMPUTE_REFLECT)
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET) $(ARGS)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) $(ARGS) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	cp $(PERF_OUT) $(EVIDENCE_PERF)
	cp $(COMPUTE_REFLECT) $(EVIDENCE_COMPUTE_REFLECT)
	@python3 -c $$'from pathlib import Path\nimport json\nppm_path = Path("$(EVIDENCE_PPM)")\nperf_path = Path("$(EVIDENCE_PERF)")\ndata = ppm_path.read_bytes()\nn1 = data.find(b"\\n")\nn2 = data.find(b"\\n", n1 + 1)\nn3 = data.find(b"\\n", n2 + 1)\nmagic = data[:n1].strip()\nwidth, height = map(int, data[n1 + 1:n2].decode().strip().split())\npixels = data[n3 + 1:]\nbackground = bytes((6, 10, 18))\nnon_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != background)\nbright_pixels = sum(1 for i in range(0, len(pixels), 3) if max(pixels[i:i + 3]) >= 180)\nassert magic == b"P6", "D7 证据校验失败：PPM 魔数不是 P6"\nassert (width, height) == (960, 720), f"D7 证据尺寸错误: {(width, height)}"\nassert non_background > 20000, f"D7 证据校验失败：非背景像素过少 ({non_background})"\nassert bright_pixels > 2000, f"D7 证据校验失败：高亮像素过少 ({bright_pixels})"\nperf = json.loads(perf_path.read_text(encoding="utf-8"))\navg_fps = perf["avg_fps"]\nassert avg_fps > 0.0, "D7 性能数据无效：avg_fps <= 0"\nassert perf["max_instance_count"] > perf["min_instance_count"], "D7 性能数据无效：indirect instanceCount 没有动态变化"\nprint(f"D7 证据校验通过：{width}x{height}, 非背景像素 {non_background}, 高亮像素 {bright_pixels}, 平均 FPS {avg_fps:.2f}")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.9",\n  "demo": "D7",\n  "scene": "Path A compute 更新粒子 + GPU 生成 indirect draw 参数 + instanced quad 渲染",\n  "shader_path": "compute: Path A (Slang -> DXIL -> MSC -> metallib), render: hand-written MSL",\n  "artifacts": ["docs/evidence/P2.9-d7-gpu-driven.ppm", "docs/evidence/P2.9-d7-gpu-driven.png", "docs/evidence/P2.9-run.txt", "docs/evidence/P2.9-perf.json", "docs/evidence/P2.9-compute-reflection.json"],\n  "notes": "CPU 每帧只提交一次 compute dispatch 和一次 indirect draw。粒子数量与 indirect instanceCount 由 GPU 侧缓冲驱动。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"
	@echo "  $(EVIDENCE_PERF)"
	@echo "  $(EVIDENCE_COMPUTE_REFLECT)"

clean:
	rm -rf build out
