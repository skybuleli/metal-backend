.PHONY: all verify build-bridge build-demos test clean release

    all: verify build-bridge build-demos

    verify:
    	@bash tools/tools-verify.sh

    build-bridge:
    	@cd libmetal_bridge && cmake -B build && cmake --build build

    build-demos:
    	@for d in demos/d*/; do $(MAKE) -C "$$d" 2>/dev/null || true; done

    test:
    	@cd tests && pytest -v

    clean:
    	@rm -rf libmetal_bridge/build build/
    	@for d in demos/d*/; do $(MAKE) -C "$$d" clean 2>/dev/null || true; done

    release:
    	@$(MAKE) build-bridge build-demos
    	@echo "Release build complete."
    