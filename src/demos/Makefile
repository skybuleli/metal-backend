.PHONY: all help build-demos d1 d2 d3 d4 d5 d6 d7 d8 \
	run-d1 run-d2 run-d3 run-d4 run-d5 run-d6 run-d7 run-d8 clean

DEMOS = d1 d2 d3 d4 d5 d6 d7 d8

all: build-demos

help:
	@echo "Demo 入口:"
	@echo "  make -C src/demos build-demos"
	@echo "  make -C src/demos d1"
	@echo "  make -C src/demos run-d1"

build-demos:
	@echo "Phase 2 Demo 统一入口"
	@echo "当前仅完成 P2.0 规格收口；D1-D8 构建目标尚未逐个实现。"
	@echo "可用下一步: make -C src/demos d1"

d1 d2 d3 d4 d5 d6 d7 d8:
	@echo "[$@] 目录已就位，具体构建逻辑待对应 P2 任务实现。"
	@echo "[$@] 预期证据格式见 docs/p2-demo-evidence.md"

run-d1 run-d2 run-d3 run-d4 run-d5 run-d6 run-d7 run-d8:
	@demo_name=$${@#run-}; \
	echo "[$$demo_name] 当前尚未实现运行入口。"; \
	echo "[$$demo_name] 完成后应产出截图/帧缓冲证据和构建日志。"

clean:
	@for demo in $(DEMOS); do \
		rm -rf "$$demo/build" "$$demo/out"; \
	done
	@echo "已清理 src/demos 下的构建目录占位。"
