.PHONY: all help verify build-bridge build-demos evidence test clean release

all: verify build-demos

help:
	@echo "可用目标:"
	@echo "  make verify       # 验证工具链"
	@echo "  make build-bridge # 构建 src/libmetal_bridge 占位工程"
	@echo "  make build-demos  # 进入 src/demos 统一入口"
	@echo "  make evidence     # 收集 Demo 证据"
	@echo "  make test         # 显示测试阶段入口状态"
	@echo "  make clean        # 清理构建目录"

verify:
	@bash tools/tools-verify.sh

build-bridge:
	@cmake -S src/libmetal_bridge -B src/libmetal_bridge/build
	@cmake --build src/libmetal_bridge/build

build-demos:
	@$(MAKE) -C src/demos build-demos

evidence:
	@$(MAKE) -C src/demos evidence

test:
	@echo "Phase 6 测试入口尚未实现；当前测试说明见 src/tests/README.md"

clean:
	@rm -rf src/libmetal_bridge/build build/
	@$(MAKE) -C src/demos clean

release:
	@$(MAKE) build-bridge
	@$(MAKE) build-demos
	@echo "Release 入口已执行；当前仓库仍处于 Phase 2 前期。"
