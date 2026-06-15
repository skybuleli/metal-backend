.PHONY: all help build-demos preview preview-evidence d1 d2 d3 d4 d5 d6 d7 d8 \
	run-d1 run-d2 run-d3 run-d4 run-d4-window run-preview run-d5 run-d6 run-d7 run-d8 \
	evidence evidence-d8 clean

DEMOS = d1 d2 d3 d4 d5 d6 d7 d8 preview p4_6a p4_6b p4_6c p4_6d p4_6e

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
	@$(MAKE) -C d7 build
	@$(MAKE) -C d8 build
	@$(MAKE) -C preview build
	@$(MAKE) -C p4_6a build
	@$(MAKE) -C p4_6b build
	@$(MAKE) -C p4_6c build
	@$(MAKE) -C p4_6d build
	@$(MAKE) -C p4_6e build
	@echo "D1–D8、P4.6a–P4.6e 与预览窗口已全部构建完成。"
	@echo "可用下一步: make -C src/demos run-d8"

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

p4_6a:
	@$(MAKE) -C p4_6a build

p4_6b:
	@$(MAKE) -C p4_6b build

p4_6c:
	@$(MAKE) -C p4_6c build

p4_6d:
	@$(MAKE) -C p4_6d build

p4_6e:
	@$(MAKE) -C p4_6e build

preview-evidence:
	@$(MAKE) -C preview evidence

d7:
	@$(MAKE) -C d7 build

d8:
	@$(MAKE) -C d8 build

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

run-d7:
	@$(MAKE) -C d7 run

run-preview:
	@$(MAKE) -C preview run

run-p4_6a:
	@$(MAKE) -C p4_6a run

run-p4_6b:
	@$(MAKE) -C p4_6b run

run-p4_6c:
	@$(MAKE) -C p4_6c run

run-p4_6d:
	@$(MAKE) -C p4_6d run

run-p4_6e:
	@$(MAKE) -C p4_6e run

run-d8:
	@$(MAKE) -C d8 run

evidence:
	@echo "===== Demo 证据收集 ====="
	@$(MAKE) -C d8 evidence
	@echo "D8 证据已生成；D1–D7 历史证据见 docs/evidence/"

evidence-d8:
	@$(MAKE) -C d8 evidence

clean:
	@for demo in $(DEMOS); do \
		rm -rf "$$demo/build" "$$demo/out"; \
	done
	@echo "已清理 src/demos 下的构建目录占位。"
