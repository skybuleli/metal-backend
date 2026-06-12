.PHONY: all help build-demos preview preview-evidence d1 d2 d3 d4 d5 d6 d7 d8 \
	run-d1 run-d2 run-d3 run-d4 run-d4-window run-preview run-d5 run-d6 run-d7 run-d8 clean

DEMOS = d1 d2 d3 d4 d5 d6 d7 d8 preview

all: build-demos

help:
	@echo "Demo 入口:"
	@echo "  make -C src/demos build-demos"
	@echo "  make -C src/demos d1"
	@echo "  make -C src/demos run-d1"

build-demos:
	@echo "Phase 2 Demo 统一入口"
	@$(MAKE) -C d1 build
	@$(MAKE) -C d2 build
	@$(MAKE) -C d3 build
	@$(MAKE) -C d4 build
	@$(MAKE) -C d4 build-window
	@$(MAKE) -C d5 build
	@$(MAKE) -C d6 build
	@$(MAKE) -C preview build
	@echo "D1、D2、D3、D4（含实时窗口版）、D5、D6 与预览窗口已构建；其余 D7-D8 构建目标尚未逐个实现。"
	@echo "可用下一步: make -C src/demos run-d6"

d1:
	@$(MAKE) -C d1 build

d2:
	@$(MAKE) -C d2 build

d3:
	@$(MAKE) -C d3 build

d4:
	@$(MAKE) -C d4 build

d5:
	@$(MAKE) -C d5 build

d6:
	@$(MAKE) -C d6 build

preview:
	@$(MAKE) -C preview build

preview-evidence:
	@$(MAKE) -C preview evidence

d7 d8:
	@echo "[$@] 目录已就位，具体构建逻辑待对应 P2 任务实现。"
	@echo "[$@] 预期证据格式见 docs/p2-demo-evidence.md"

run-d1:
	@$(MAKE) -C d1 run

run-d2:
	@$(MAKE) -C d2 run

run-d3:
	@$(MAKE) -C d3 run

run-d4:
	@$(MAKE) -C d4 run

run-d4-window:
	@$(MAKE) -C d4 run-window

run-d5:
	@$(MAKE) -C d5 run

run-d6:
	@$(MAKE) -C d6 run

run-preview:
	@$(MAKE) -C preview run

run-d7 run-d8:
	@demo_name=$${@#run-}; \
	echo "[$$demo_name] 当前尚未实现运行入口。"; \
	echo "[$$demo_name] 完成后应产出截图/帧缓冲证据和构建日志。"

clean:
	@for demo in $(DEMOS); do \
		rm -rf "$$demo/build" "$$demo/out"; \
	done
	@echo "已清理 src/demos 下的构建目录占位。"
