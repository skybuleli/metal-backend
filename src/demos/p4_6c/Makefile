.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -O2
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp

TARGET := build/p4_6c_homebrew_smoke
SOURCE := src/main.mm
OUT_DIR := out
IMAGE_OUT := $(OUT_DIR)/p4_6c_homebrew_smoke.ppm
ATLAS_OUT := $(OUT_DIR)/p4_6c_homebrew_atlas.ppm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_PPM := $(EVIDENCE_DIR)/P4.6.8-p4-6c-homebrew-smoke.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P4.6.8-p4-6c-homebrew-smoke.png
EVIDENCE_ATLAS := $(EVIDENCE_DIR)/P4.6.8-p4-6c-homebrew-atlas.ppm
EVIDENCE_ATLAS_PNG := $(EVIDENCE_DIR)/P4.6.8-p4-6c-homebrew-atlas.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.8-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P4.6.8-meta.json

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
	cp $(IMAGE_OUT) $(EVIDENCE_PPM)
	cp $(ATLAS_OUT) $(EVIDENCE_ATLAS)
	python3 ../../../tools/test_p4_6c_smoke.py --image $(EVIDENCE_PPM) --atlas $(EVIDENCE_ATLAS) --meta $(EVIDENCE_META)
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
