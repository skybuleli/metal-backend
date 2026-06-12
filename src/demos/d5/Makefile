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

SHOWCASE_PPM := out/advanced_texturing_showcase.ppm
MSAA_COMPARE_PPM := out/advanced_texturing_msaa_compare.ppm

EVIDENCE_SHOWCASE_PPM := $(EVIDENCE_DIR)/P2.7a-d5-showcase.ppm
EVIDENCE_SHOWCASE_PNG := $(EVIDENCE_DIR)/P2.7a-d5-showcase.png
EVIDENCE_MSAA_PPM := $(EVIDENCE_DIR)/P2.7a-d5-msaa-compare.ppm
EVIDENCE_MSAA_PNG := $(EVIDENCE_DIR)/P2.7a-d5-msaa-compare.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.7a-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.7a-meta.json

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
	cp $(SHOWCASE_PPM) $(EVIDENCE_SHOWCASE_PPM)
	cp $(MSAA_COMPARE_PPM) $(EVIDENCE_MSAA_PPM)
	@python3 -c $$'from pathlib import Path\npaths = [(Path("$(EVIDENCE_SHOWCASE_PPM)"), (768, 768)), (Path("$(EVIDENCE_MSAA_PPM)"), (1536, 768))]\nbackground = bytes((8, 13, 20))\nfor ppm_path, expected in paths:\n    data = ppm_path.read_bytes()\n    n1 = data.find(b"\\n")\n    n2 = data.find(b"\\n", n1 + 1)\n    n3 = data.find(b"\\n", n2 + 1)\n    magic = data[:n1].strip()\n    width, height = map(int, data[n1 + 1:n2].decode().strip().split())\n    pixels = data[n3 + 1:]\n    non_background = sum(1 for i in range(0, len(pixels), 3) if pixels[i:i + 3] != background)\n    assert magic == b"P6", f"{ppm_path.name} 魔数不是 P6"\n    assert (width, height) == expected, f"{ppm_path.name} 尺寸错误: {(width, height)} != {expected}"\n    assert non_background > 0, f"{ppm_path.name} 全是背景色"\n    print(f"{ppm_path.name} 校验通过: {width}x{height}, 非背景像素 {non_background}")'
	sips -s format png $(EVIDENCE_SHOWCASE_PPM) --out $(EVIDENCE_SHOWCASE_PNG) >/dev/null 2>&1
	sips -s format png $(EVIDENCE_MSAA_PPM) --out $(EVIDENCE_MSAA_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.7a",\n  "demo": "D5",\n  "scene": "双面板法线贴图对照 + 连续天空盒 Cubemap + 1x/4x MSAA 放大对比",\n  "shader_path": "hand-written MSL (compiled from source)",\n  "artifacts": ["docs/evidence/P2.7a-d5-showcase.ppm", "docs/evidence/P2.7a-d5-showcase.png", "docs/evidence/P2.7a-d5-msaa-compare.ppm", "docs/evidence/P2.7a-d5-msaa-compare.png", "docs/evidence/P2.7a-run.txt"],\n  "notes": "P2.7a 直接把法线贴图开关差异放进同一画面，并对斜边区域做 1x 与 4x MSAA 放大并排证据。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_SHOWCASE_PPM)"
	@echo "  $(EVIDENCE_SHOWCASE_PNG)"
	@echo "  $(EVIDENCE_MSAA_PPM)"
	@echo "  $(EVIDENCE_MSAA_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
