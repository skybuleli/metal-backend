.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -fobjc-arc -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics -framework AppKit -framework QuartzCore

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
BRIDGE_DIR := $(abspath ../../libmetal_bridge)
BRIDGE_BUILD_DIR := $(BRIDGE_DIR)/build

TARGET := build/p4_6d_window_smoke
SOURCE := src/main.mm
OUT_DIR := out
FRAME_OUT := $(OUT_DIR)/p4_6d_window_smoke.ppm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_PPM := $(EVIDENCE_DIR)/P4.6.10-window-smoke.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P4.6.10-window-smoke.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.10-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P4.6.10-meta.json

all: build

build:
	@mkdir -p build $(OUT_DIR)
	@if [ ! -f "$(METAL_CPP_DIR)/QuartzCore/QuartzCore.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 metal-cpp 根目录。"; \
		exit 1; \
	fi
	@if [ ! -f "$(BRIDGE_BUILD_DIR)/libmetal_bridge.dylib" ]; then \
		$(MAKE) -C $(BRIDGE_DIR)/build metal_bridge; \
	fi
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" -I"$(BRIDGE_DIR)/include" $(SOURCE) -o $(TARGET) $(FRAMEWORKS) -L"$(BRIDGE_BUILD_DIR)" -lmetal_bridge -Wl,-rpath,"$(BRIDGE_BUILD_DIR)"

run: build
	./$(TARGET)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) --auto-close-seconds 2 --export-ppm $(FRAME_OUT) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(FRAME_OUT) $(EVIDENCE_PPM)
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	@printf '{\n  "task": "P4.6.10",\n  "demo": "p4_6d_window_smoke",\n  "scene": "NSWindow + CAMetalLayer + metal_presenter_present_texture",\n  "frames": "CPU generated 2D smoke frame uploaded through bridge texture",\n  "artifacts": ["docs/evidence/P4.6.10-run.txt", "docs/evidence/P4.6.10-window-smoke.ppm", "docs/evidence/P4.6.10-window-smoke.png"],\n  "notes": "该任务用于验证窗口呈现链路，不再依赖离屏终帧作为主证据。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成窗口 smoke 证据:"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build $(OUT_DIR)
