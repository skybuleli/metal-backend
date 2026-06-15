.PHONY: all build run evidence clean

SHELL := /bin/bash

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -fobjc-arc -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics -framework AppKit -framework QuartzCore

TARGET := build/p4_6e_bridge_triangle
SOURCE := src/main.mm
OUT_DIR := out
FRAME_OUT := $(OUT_DIR)/p4_6e_bridge_triangle.ppm
FRAME_JSON := $(OUT_DIR)/p4_6e_bridge_triangle.json
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_PPM := $(EVIDENCE_DIR)/P4.6.11-bridge-triangle.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P4.6.11-bridge-triangle.png
EVIDENCE_JSON := $(EVIDENCE_DIR)/P4.6.11-meta.json
EVIDENCE_RUN := $(EVIDENCE_DIR)/P4.6.11-run.txt

all: build

build:
	@mkdir -p build $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -I"$(abspath ../../libmetal_bridge/include)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS) -L"$(abspath ../../libmetal_bridge/build)" -lmetal_bridge -Wl,-rpath,"$(abspath ../../libmetal_bridge/build)"

run: build
	./$(TARGET)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	@set -o pipefail; ./$(TARGET) --auto-close-seconds 2 --export-ppm $(FRAME_OUT) --export-json $(FRAME_JSON) 2>&1 | tee $(EVIDENCE_RUN)
	cp $(FRAME_OUT) $(EVIDENCE_PPM)
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null 2>&1
	cp $(FRAME_JSON) $(EVIDENCE_JSON)
	@echo "已生成真实 draw path 证据:"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_JSON)"

clean:
	rm -rf build $(OUT_DIR)
