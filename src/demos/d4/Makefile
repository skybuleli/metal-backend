.PHONY: all build build-window run run-window evidence evidence-window clean
.PHONY: evidence-diagnostic

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics
CXXFLAGS_WINDOW := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -fobjc-arc -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -O2
FRAMEWORKS_WINDOW := -framework Foundation -framework Metal -framework CoreGraphics -framework AppKit -framework QuartzCore

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/d4_basic_lighting
SOURCE := src/main.cpp
WINDOW_TARGET := build/d4_basic_lighting_window
WINDOW_SOURCE := src/window_main.mm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)

PPM_OUT := out/basic_lighting.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.6-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.6-meta.json
WINDOW_PPM_OUT := out/basic_lighting_window.ppm
WINDOW_EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.6b-d4-rotating-window.ppm
WINDOW_EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.6b-d4-rotating-window.png
WINDOW_EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.6b-window.txt
WINDOW_EVIDENCE_META := $(EVIDENCE_DIR)/P2.6b-meta.json

DIAG_BUNDLE_DIR := $(EVIDENCE_DIR)/P4.6.5-diagnostic-bundle
DIAG_EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.5-render-diagnostic.log
DIAG_EVIDENCE_META := $(EVIDENCE_DIR)/P4.6.5-meta.json

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

build-window:
	@mkdir -p build out
	$(CXX) $(CXXFLAGS_WINDOW) $(WINDOW_SOURCE) -o $(WINDOW_TARGET) $(FRAMEWORKS_WINDOW)

run: build
	./$(TARGET)

run-window: build-window
	./$(WINDOW_TARGET)

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

evidence-window: build-window
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(WINDOW_TARGET) --auto-close-seconds 3 --export-ppm $(WINDOW_PPM_OUT) 2>&1 | tee $(WINDOW_EVIDENCE_RUN)
	cp $(WINDOW_PPM_OUT) $(WINDOW_EVIDENCE_PPM)
	sips -s format png $(WINDOW_EVIDENCE_PPM) --out $(WINDOW_EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P2.6b",\n  "demo": "D4Window",\n  "scene": "实时旋转立方体窗口预览，验证 3D 变换、深度测试与 Phong 光照在本地窗口中持续生效",\n  "shader_path": "hand-written MSL (compiled from source)",\n  "artifacts": ["docs/evidence/P2.6b-window.txt", "docs/evidence/P2.6b-d4-rotating-window.ppm", "docs/evidence/P2.6b-d4-rotating-window.png"],\n  "notes": "窗口版与离屏版共用同一套立方体数据、矩阵计算和光照着色器；当前仍为单样本渲染，抗锯齿放到 P2.7 处理。"\n}\n' > $(WINDOW_EVIDENCE_META)
	@echo "已生成窗口版验证证据:"
	@echo "  $(WINDOW_EVIDENCE_PPM)"
	@echo "  $(WINDOW_EVIDENCE_PNG)"
	@echo "  $(WINDOW_EVIDENCE_RUN)"
	@echo "  $(WINDOW_EVIDENCE_META)"

evidence-diagnostic: build-window
	@mkdir -p $(EVIDENCE_DIR)
	@rm -rf $(DIAG_BUNDLE_DIR)
	@set -o pipefail; ./$(WINDOW_TARGET) --auto-close-seconds 1 --export-ppm $(WINDOW_PPM_OUT) --diagnostic-bundle $(DIAG_BUNDLE_DIR) 2>&1 | tee $(DIAG_EVIDENCE_RUN)
	@python3 -c 'from pathlib import Path; import json; bundle = Path("$(DIAG_BUNDLE_DIR)"); required = ["render_target_dump.ppm", "presented_frame.ppm", "draw_order.log", "state_snapshot.json", "manifest.json"]; missing = [name for name in required if not (bundle / name).exists()]; assert not missing, f"缺少诊断证据文件: {missing}"; meta = { "task": "P4.6.5", "demo": "D4Window", "scene": "RenderTarget dump + draw order log + state snapshot + present before/after capture", "artifacts": [str(bundle / name) for name in required] + ["docs/evidence/P4.6.5-render-diagnostic.log"], "notes": "诊断包使用同一帧中的 window pass 与 capture pass 生成。render_target_dump 作为呈现前抓帧，presented_frame 作为呈现后抓帧。", "all_passed": True }; Path("$(DIAG_EVIDENCE_META)").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"); print("P4.6.5 诊断证据包已生成")'
	@echo "已生成渲染诊断证据:"
	@echo "  $(DIAG_BUNDLE_DIR)"
	@echo "  $(DIAG_EVIDENCE_RUN)"
	@echo "  $(DIAG_EVIDENCE_META)"

clean:
	rm -rf build out
