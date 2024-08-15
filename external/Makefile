PROJECTS := raylib


.PHONY: all $(PROJECTS)

all: $(PROJECTS)

raylib:
	@echo Building raylib
	@mkdir -p ./build
	@cmake -G Ninja -S ./raylib -B ./build
	@cd ./build && ninja
	@mkdir -p $(EXTERNAL_LIBS_DIR)/raylib
	@mv ./build/raylib/libraylib*.a $(EXTERNAL_LIBS_DIR)/raylib
	@rm -rf ./build
	@echo Built raylib


