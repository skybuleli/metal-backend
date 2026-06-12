.PHONY: all build run evidence clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-c++20-designator -Wno-c99-extensions -stdlib=libc++ -fobjc-arc -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics -framework AppKit -framework QuartzCore

TARGET := build/demos_preview
SOURCE := src/main.mm
OUTPUT_PPM := out/preview_grid.ppm
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.6a-window.txt
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.6a-preview-grid.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.6a-preview-grid.png
EVIDENCE_META := $(EVIDENCE_DIR)/P2.6a-meta.json

D1_PPM := ../d1/out/triangle.ppm
D2_PPM := ../d2/out/textured_quad.ppm
D3_PPM := ../d3/out/multi_texture.ppm
D4_PPM := ../d4/out/basic_lighting.ppm

all: build

build:
	@mkdir -p build out
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

$(D1_PPM):
	@$(MAKE) -C ../d1 run

$(D2_PPM):
	@$(MAKE) -C ../d2 run

$(D3_PPM):
	@$(MAKE) -C ../d3 run

$(D4_PPM):
	@$(MAKE) -C ../d4 run

run: build $(D1_PPM) $(D2_PPM) $(D3_PPM) $(D4_PPM)
	./$(TARGET) $(D1_PPM) $(D2_PPM) $(D3_PPM) $(D4_PPM)

evidence: build $(D1_PPM) $(D2_PPM) $(D3_PPM) $(D4_PPM)
	@mkdir -p $(EVIDENCE_DIR)
	./$(TARGET) --auto-close-seconds 2 --export-ppm $(OUTPUT_PPM) $(D1_PPM) $(D2_PPM) $(D3_PPM) $(D4_PPM) > $(EVIDENCE_RUN)
	cp $(OUTPUT_PPM) $(EVIDENCE_PPM)
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null
	@printf '{\n  "task": "P2.6a",\n  "demo": "PreviewWindow",\n  "scene": "NSWindow + CAMetalLayer local preview for D1-D4",\n  "shader_path": "handwritten-msl-preview",\n  "artifacts": ["docs/evidence/P2.6a-window.txt", "docs/evidence/P2.6a-preview-grid.ppm", "docs/evidence/P2.6a-preview-grid.png"],\n  "notes": "窗口预览用于本地体验与调试，离屏证据链仍由各 Demo 自身维护。"\n}\n' > $(EVIDENCE_META)
	@echo "已生成窗口预览证据:"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_META)"

clean:
	rm -rf build out
