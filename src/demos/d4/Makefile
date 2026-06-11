.PHONY: all build run evidence clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
SLANGC ?= slangc
MSC ?= metal-shaderconverter

TARGET := build/d4_basic_lighting
SOURCE := src/main.cpp
SHADER := shaders/lighting.slang
VERT_DXIL := build/lighting_vertex.dxil
FRAG_DXIL := build/lighting_fragment.dxil
VERT_METALLIB := build/lighting_vertex.metallib
FRAG_METALLIB := build/lighting_fragment.metallib
VERT_REFLECT := build/lighting_vertex.reflect.json
FRAG_REFLECT := build/lighting_fragment.reflect.json
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_REPO_DIR := docs/evidence
PPM_OUT := out/basic_lighting.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.6-d4-basic-lighting.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.6-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.6-meta.json
EVIDENCE_VERT_REFLECT := $(EVIDENCE_DIR)/P2.6-vertex-reflection.json
EVIDENCE_FRAG_REFLECT := $(EVIDENCE_DIR)/P2.6-fragment-reflection.json

all: build

build:
	@mkdir -p build out
	@if [ ! -f "$(METAL_CPP_DIR)/Metal/Metal.hpp" ]; then \
		echo "未找到 metal-cpp: $(METAL_CPP_DIR)"; \
		echo "请设置 METAL_CPP_DIR 指向 Metal.hpp 所在目录。"; \
		exit 1; \
	fi
	$(SLANGC) $(SHADER) -target dxil -entry vertexMain -stage vertex -profile sm_6_0 -o $(VERT_DXIL)
	$(SLANGC) $(SHADER) -target dxil -entry fragmentMain -stage fragment -profile ps_6_0 -o $(FRAG_DXIL)
	$(MSC) $(VERT_DXIL) -o $(VERT_METALLIB) --output-reflection-file $(VERT_REFLECT)
	$(MSC) $(FRAG_DXIL) -o $(FRAG_METALLIB) --output-reflection-file $(FRAG_REFLECT)
	$(CXX) $(CXXFLAGS) -I"$(METAL_CPP_DIR)" $(SOURCE) -o $(TARGET) $(FRAMEWORKS)

run: build
	./$(TARGET)

evidence: build
	@mkdir -p $(EVIDENCE_DIR)
	./$(TARGET) > $(EVIDENCE_RUN)
	cp $(PPM_OUT) $(EVIDENCE_PPM)
	cp $(VERT_REFLECT) $(EVIDENCE_VERT_REFLECT)
	cp $(FRAG_REFLECT) $(EVIDENCE_FRAG_REFLECT)
	sips -s format png $(EVIDENCE_PPM) --out $(EVIDENCE_PNG) >/dev/null
	@printf '{\n  "task": "P2.6",\n  "demo": "D4",\n  "scene": "Basic Phong lighting with uniform buffer, depth testing, and 3D cube",\n  "shader_path": "path-a-metallib",\n  "artifacts": ["%s", "%s", "%s", "%s", "%s", "%s", "%s"],\n  "notes": "uniform buffer 通过 MSC IRDescriptorTableSetBuffer 绑定，启用深度测试渲染 3D 立方体。"\n}\n' \
		"$(EVIDENCE_REPO_DIR)/P2.6-d4-basic-lighting.ppm" "$(EVIDENCE_REPO_DIR)/P2.6-d4-basic-lighting.png" "$(EVIDENCE_REPO_DIR)/P2.6-run.txt" "$(EVIDENCE_REPO_DIR)/P2.6-vertex-reflection.json" "$(EVIDENCE_REPO_DIR)/P2.6-fragment-reflection.json" "src/demos/d4/$(VERT_METALLIB)" "src/demos/d4/$(FRAG_METALLIB)" > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"
	@echo "  $(EVIDENCE_VERT_REFLECT)"
	@echo "  $(EVIDENCE_FRAG_REFLECT)"

clean:
	rm -rf build out
