.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/p4_6a_sprite_quad
SOURCE := src/main.mm
OUT_DIR := out
PPM_OUT := $(OUT_DIR)/p4_6a_sprite_quad.ppm
ATLAS_OUT := $(OUT_DIR)/p4_6a_sprite_atlas.ppm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_PPM := $(EVIDENCE_DIR)/P4.6.6-p4-6a-sprite-quad.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P4.6.6-p4-6a-sprite-quad.png
EVIDENCE_ATLAS := $(EVIDENCE_DIR)/P4.6.6-p4-6a-sprite-atlas.ppm
EVIDENCE_ATLAS_PNG := $(EVIDENCE_DIR)/P4.6.6-p4-6a-sprite-atlas.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.6-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P4.6.6-meta.json

all: build

build:
	@mkdir -p build $(OUT_DIR)
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
	cp $(ATLAS_OUT) $(EVIDENCE_ATLAS)
	@python3 -c 'from pathlib import Path; import json; from collections import Counter; img = Path("$(EVIDENCE_PPM)").read_bytes(); header = img.split(b"\n", 3); assert header[0] == b"P6", "PPM 魔数错误"; dims = header[1].split(); w, h = map(int, dims); assert (w, h) == (512, 512), f"尺寸错误: {(w, h)}"; body = header[3]; corner = tuple(body[:3]); center_offset = ((h // 2) * w + (w // 2)) * 3; center = tuple(body[center_offset:center_offset + 3]); assert corner != center, "中心像素未与角落拉开差异，alpha blend 可能未生效"; meta = {"task": "P4.6.6", "demo": "P4_6a", "scene": "handwritten sprite atlas quad with alpha blend", "artifacts": ["docs/evidence/P4.6.6-p4-6a-sprite-quad.ppm", "docs/evidence/P4.6.6-p4-6a-sprite-quad.png", "docs/evidence/P4.6.6-p4-6a-sprite-atlas.ppm", "docs/evidence/P4.6.6-p4-6a-sprite-atlas.png", "docs/evidence/P4.6.6-run.txt"], "notes": "单张 sprite atlas 驱动背景 quad + 前景 sprite quad，前景使用 sourceAlpha/oneMinusSourceAlpha 混合。", "all_passed": True}; Path("$(EVIDENCE_META)").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"); print(f"P4.6.6 证据校验通过: corner={corner}, center={center}")'
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	sips -s format png $(EVIDENCE_ATLAS) --out $(EVIDENCE_ATLAS_PNG) >/dev/null 2>&1
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_ATLAS)"
	@echo "  $(EVIDENCE_ATLAS_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build $(OUT_DIR)

