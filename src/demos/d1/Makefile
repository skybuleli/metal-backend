.PHONY: all build run evidence clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
TARGET := build/d1_triangle
SOURCE := src/main.cpp
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_REPO_DIR := docs/evidence
PPM_OUT := out/triangle.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.2-d1-triangle.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.2-d1-triangle.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.2-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.2-meta.json

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

evidence: run
	@mkdir -p $(EVIDENCE_DIR)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null
	@{ \
		echo "D1 运行验证"; \
		echo "任务: P2.2"; \
		echo "设备: Apple M1"; \
		echo "帧缓冲: $(EVIDENCE_REPO_DIR)/P2.2-d1-triangle.ppm"; \
		echo "PNG 预览: $(EVIDENCE_REPO_DIR)/P2.2-d1-triangle.png"; \
		echo "可执行文件: src/demos/d1/$(TARGET)"; \
		echo "说明: 当前为离屏渲染验证，PNG 由 PPM 自动转换而来。"; \
	} > $(EVIDENCE_RUN)
	@printf '{\n  "task": "P2.2",\n  "demo": "D1",\n  "scene": "Hello Triangle offscreen validation",\n  "shader_path": "handwritten-msl",\n  "artifacts": ["%s", "%s", "%s"],\n  "notes": "离屏 PPM 已转换为 PNG 供人工验证。"\n}\n' \
		"$(EVIDENCE_REPO_DIR)/P2.2-d1-triangle.ppm" "$(EVIDENCE_REPO_DIR)/P2.2-d1-triangle.png" "src/demos/d1/$(TARGET)" > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
