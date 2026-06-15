.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/p4_6b_tilemap_hud
SOURCE := src/main.mm
OUT_DIR := out
FRAME_A_OUT := $(OUT_DIR)/p4_6b_tilemap_scroll_a.ppm
FRAME_B_OUT := $(OUT_DIR)/p4_6b_tilemap_scroll_b.ppm
ATLAS_OUT := $(OUT_DIR)/p4_6b_tilemap_atlas.ppm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_A_PPM := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-scroll-a.ppm
EVIDENCE_A_PNG := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-scroll-a.png
EVIDENCE_B_PPM := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-scroll-b.ppm
EVIDENCE_B_PNG := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-scroll-b.png
EVIDENCE_ATLAS := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-atlas.ppm
EVIDENCE_ATLAS_PNG := $(EVIDENCE_DIR)/P4.6.7-p4-6b-tilemap-atlas.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.7-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P4.6.7-meta.json

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
	cp $(FRAME_A_OUT) $(EVIDENCE_A_PPM)
	cp $(FRAME_B_OUT) $(EVIDENCE_B_PPM)
	cp $(ATLAS_OUT) $(EVIDENCE_ATLAS)
	@python3 -c $$'from pathlib import Path\nimport json\n\ndef read_ppm(path):\n    data = path.read_bytes()\n    magic, rest = data.split(b"\\n", 1)\n    dims_line, max_line, body = rest.split(b"\\n", 2)\n    assert magic == b"P6", "PPM 魔数错误"\n    assert max_line == b"255", "PPM 最大值错误"\n    width, height = map(int, dims_line.split())\n    return width, height, body\n\naw, ah, a = read_ppm(Path("$(EVIDENCE_A_PPM)"))\nbw, bh, b = read_ppm(Path("$(EVIDENCE_B_PPM)"))\nassert (aw, ah) == (512, 512), f"frame A 尺寸错误: {(aw, ah)}"\nassert (bw, bh) == (512, 512), f"frame B 尺寸错误: {(bw, bh)}"\nassert a != b, "camera scroll 未形成可见差异"\ncenter_a = tuple(a[((ah // 2) * aw + (aw // 2)) * 3:((ah // 2) * aw + (aw // 2)) * 3 + 3])\ncenter_b = tuple(b[((bh // 2) * bw + (bw // 2)) * 3:((bh // 2) * bw + (bw // 2)) * 3 + 3])\ndiff = sum(1 for i in range(0, len(a), 3) if a[i:i + 3] != b[i:i + 3])\nassert diff > 12000, f"scroll 差异过小: {diff}"\npanel = tuple(a[(400 * aw + 40) * 3:(400 * aw + 40) * 3 + 3])\ntext = tuple(a[(428 * aw + 90) * 3:(428 * aw + 90) * 3 + 3])\nassert panel != text, "HUD 文本未形成可见差异"\nmeta = {\n    "task": "P4.6.7",\n    "demo": "P4_6b",\n    "scene": "handwritten tilemap with camera scroll and HUD panel/text",\n    "artifacts": [\n        "docs/evidence/P4.6.7-p4-6b-tilemap-scroll-a.ppm",\n        "docs/evidence/P4.6.7-p4-6b-tilemap-scroll-a.png",\n        "docs/evidence/P4.6.7-p4-6b-tilemap-scroll-b.ppm",\n        "docs/evidence/P4.6.7-p4-6b-tilemap-scroll-b.png",\n        "docs/evidence/P4.6.7-p4-6b-tilemap-atlas.ppm",\n        "docs/evidence/P4.6.7-p4-6b-tilemap-atlas.png",\n        "docs/evidence/P4.6.7-run.txt",\n    ],\n    "notes": "单张 atlas 同时承载 world tiles 与 HUD glyphs；用两帧不同 camera scroll 证明 tilemap 位移与 HUD 覆盖层稳定。",\n    "frame_diff_pixels": diff,\n    "center_a": center_a,\n    "center_b": center_b,\n    "all_passed": True,\n}\nPath("$(EVIDENCE_META)").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\\n", encoding="utf-8")\nprint(f"P4.6.7 证据校验通过: diff={diff}, center_a={center_a}, center_b={center_b}")'
	sips -s format png $(EVIDENCE_A_PPM) --out $(EVIDENCE_A_PNG) >/dev/null 2>&1
	sips -s format png $(EVIDENCE_B_PPM) --out $(EVIDENCE_B_PNG) >/dev/null 2>&1
	sips -s format png $(EVIDENCE_ATLAS) --out $(EVIDENCE_ATLAS_PNG) >/dev/null 2>&1
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_A_PPM)"
	@echo "  $(EVIDENCE_A_PNG)"
	@echo "  $(EVIDENCE_B_PPM)"
	@echo "  $(EVIDENCE_B_PNG)"
	@echo "  $(EVIDENCE_ATLAS)"
	@echo "  $(EVIDENCE_ATLAS_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build $(OUT_DIR)
