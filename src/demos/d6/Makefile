.PHONY: all build run evidence evidence-compare clean

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
COMPARE_PATHA_PPM := $(EVIDENCE_DIR)/P2.8b-d6-patha.ppm
COMPARE_PATHA_PNG := $(EVIDENCE_DIR)/P2.8b-d6-patha.png
COMPARE_PATHA_RUN := $(EVIDENCE_DIR)/P2.8b-patha-run.txt
COMPARE_LEGACY_PPM := $(EVIDENCE_DIR)/P2.8b-d6-legacy-msl.ppm
COMPARE_LEGACY_PNG := $(EVIDENCE_DIR)/P2.8b-d6-legacy-msl.png
COMPARE_LEGACY_RUN := $(EVIDENCE_DIR)/P2.8b-legacy-run.txt
COMPARE_DIFF_PPM := $(EVIDENCE_DIR)/P2.8b-d6-diff-heatmap.ppm
COMPARE_DIFF_PNG := $(EVIDENCE_DIR)/P2.8b-d6-diff-heatmap.png
COMPARE_REPORT := $(EVIDENCE_DIR)/P2.8b-compare.txt
COMPARE_META := $(EVIDENCE_DIR)/P2.8b-meta.json

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

evidence-compare: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) 2>&1 | tee $(COMPARE_PATHA_RUN)
	cp $(PPM_OUT) $(COMPARE_PATHA_PPM)
	@set -o pipefail; ./$(TARGET) --legacy-msl 2>&1 | tee $(COMPARE_LEGACY_RUN)
	cp $(PPM_OUT) $(COMPARE_LEGACY_PPM)
	@python3 -c $$'from pathlib import Path\nimport json\n\npatha_path = Path("$(COMPARE_PATHA_PPM)")\nlegacy_path = Path("$(COMPARE_LEGACY_PPM)")\ndiff_path = Path("$(COMPARE_DIFF_PPM)")\nreport_path = Path("$(COMPARE_REPORT)")\nmeta_path = Path("$(COMPARE_META)")\n\ndef read_ppm(path: Path):\n    data = path.read_bytes()\n    n1 = data.find(b\"\\n\")\n    n2 = data.find(b\"\\n\", n1 + 1)\n    n3 = data.find(b\"\\n\", n2 + 1)\n    magic = data[:n1].strip()\n    width, height = map(int, data[n1 + 1:n2].decode().strip().split())\n    pixels = data[n3 + 1:]\n    if magic != b\"P6\":\n        raise SystemExit(f\"{path} 不是 P6 PPM\")\n    return width, height, pixels\n\nwidth_a, height_a, pixels_a = read_ppm(patha_path)\nwidth_b, height_b, pixels_b = read_ppm(legacy_path)\nif (width_a, height_a) != (width_b, height_b):\n    raise SystemExit(\"P2.8b 对照失败：两张图尺寸不一致\")\n\npixel_count = width_a * height_a\nsum_abs = [0, 0, 0]\nmax_abs = [0, 0, 0]\nmismatch_pixels = 0\nsum_sq = 0\nheatmap = bytearray()\nfor i in range(0, len(pixels_a), 3):\n    dr = abs(pixels_a[i + 0] - pixels_b[i + 0])\n    dg = abs(pixels_a[i + 1] - pixels_b[i + 1])\n    db = abs(pixels_a[i + 2] - pixels_b[i + 2])\n    sum_abs[0] += dr\n    sum_abs[1] += dg\n    sum_abs[2] += db\n    max_abs[0] = max(max_abs[0], dr)\n    max_abs[1] = max(max_abs[1], dg)\n    max_abs[2] = max(max_abs[2], db)\n    sum_sq += dr * dr + dg * dg + db * db\n    if dr or dg or db:\n        mismatch_pixels += 1\n    heatmap.extend((min(dr * 8, 255), min(dg * 8, 255), min(db * 8, 255)))\n\nrmse = (sum_sq / (pixel_count * 3)) ** 0.5\nmean_abs = [value / pixel_count for value in sum_abs]\nmismatch_ratio = mismatch_pixels / pixel_count\nsummary = {\n    \"task\": \"P2.8b\",\n    \"width\": width_a,\n    \"height\": height_a,\n    \"mismatch_pixels\": mismatch_pixels,\n    \"mismatch_ratio\": mismatch_ratio,\n    \"mean_abs_diff\": mean_abs,\n    \"max_abs_diff\": max_abs,\n    \"rmse\": rmse,\n}\n\ndiff_path.write_bytes(f\"P6\\n{width_a} {height_a}\\n255\\n\".encode() + bytes(heatmap))\nreport_path.write_text(\n    \"P2.8b 双路径对照\\n\"\n    f\"尺寸: {width_a}x{height_a}\\n\"\n    f\"不一致像素: {mismatch_pixels}/{pixel_count} ({mismatch_ratio:.6%})\\n\"\n    f\"平均绝对差: R={mean_abs[0]:.6f}, G={mean_abs[1]:.6f}, B={mean_abs[2]:.6f}\\n\"\n    f\"最大绝对差: R={max_abs[0]}, G={max_abs[1]}, B={max_abs[2]}\\n\"\n    f\"RMSE: {rmse:.6f}\\n\",\n    encoding=\"utf-8\",\n)\nmeta_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + \"\\n\", encoding=\"utf-8\")'
	sips -s format png $(COMPARE_PATHA_PPM) --out $(COMPARE_PATHA_PNG) >/dev/null 2>&1
	sips -s format png $(COMPARE_LEGACY_PPM) --out $(COMPARE_LEGACY_PNG) >/dev/null 2>&1
	sips -s format png $(COMPARE_DIFF_PPM) --out $(COMPARE_DIFF_PNG) >/dev/null 2>&1
	@echo "已生成双路径对照证据:"
	@echo "  $(COMPARE_PATHA_PNG)"
	@echo "  $(COMPARE_LEGACY_PNG)"
	@echo "  $(COMPARE_DIFF_PNG)"
	@echo "  $(COMPARE_REPORT)"
	@echo "  $(COMPARE_META)"

clean:
	rm -rf build out
