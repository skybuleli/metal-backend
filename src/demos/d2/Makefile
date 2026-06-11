.PHONY: all build run evidence clean

CXX := $(shell xcrun --sdk macosx --find clang++)
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-c99-extensions -Wno-c++20-designator -stdlib=libc++ -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/c++/v1 -I/usr/local/include
FRAMEWORKS := -framework Foundation -framework Metal -framework CoreGraphics

METAL_CPP_DIR ?= $(HOME)/GraphicsExperiments/third_party/metal-cpp
SLANGC ?= slangc
MSC ?= metal-shaderconverter

TARGET := build/d2_textured_quad
SOURCE := src/main.cpp
SHADER := shaders/quad.slang
VERT_DXIL := build/quad_vertex.dxil
FRAG_DXIL := build/quad_fragment.dxil
VERT_METALLIB := build/quad_vertex.metallib
FRAG_METALLIB := build/quad_fragment.metallib
VERT_REFLECT := build/quad_vertex.reflect.json
FRAG_REFLECT := build/quad_fragment.reflect.json
EVIDENCE_DIR := $(abspath ../../../docs/evidence)
EVIDENCE_REPO_DIR := docs/evidence
PPM_OUT := out/textured_quad.ppm
EVIDENCE_PPM := $(EVIDENCE_DIR)/P2.4-d2-textured-quad.ppm
EVIDENCE_PNG := $(EVIDENCE_DIR)/P2.4-d2-textured-quad.png
EVIDENCE_RUN := $(EVIDENCE_DIR)/P2.4-run.txt
EVIDENCE_META := $(EVIDENCE_DIR)/P2.4-meta.json
EVIDENCE_VERT_REFLECT := $(EVIDENCE_DIR)/P2.4-vertex-reflection.json
EVIDENCE_FRAG_REFLECT := $(EVIDENCE_DIR)/P2.4-fragment-reflection.json

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
	@printf '{\n  "task": "P2.4",\n  "demo": "D2",\n  "scene": "Textured Quad offscreen validation",\n  "shader_path": "path-a-metallib",\n  "artifacts": ["%s", "%s", "%s", "%s", "%s", "%s", "%s"],\n  "notes": "Path A 已生成 vertex/fragment metallib，运行期通过 metal_irconverter_runtime 约定绑定顶层参数缓冲，离屏 PPM 已转换为 PNG。"\n}\n' \
		"$(EVIDENCE_REPO_DIR)/P2.4-d2-textured-quad.ppm" "$(EVIDENCE_REPO_DIR)/P2.4-d2-textured-quad.png" "$(EVIDENCE_REPO_DIR)/P2.4-run.txt" "$(EVIDENCE_REPO_DIR)/P2.4-vertex-reflection.json" "$(EVIDENCE_REPO_DIR)/P2.4-fragment-reflection.json" "src/demos/d2/$(VERT_METALLIB)" "src/demos/d2/$(FRAG_METALLIB)" > $(EVIDENCE_META)
	@echo "已生成验证证据:"
	@echo "  $(EVIDENCE_PPM)"
	@echo "  $(EVIDENCE_PNG)"
	@echo "  $(EVIDENCE_RUN)"
	@echo "  $(EVIDENCE_META)"
	@echo "  $(EVIDENCE_VERT_REFLECT)"
	@echo "  $(EVIDENCE_FRAG_REFLECT)"

clean:
	rm -rf build out
