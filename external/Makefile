PROJECTS := sdl raylib


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


sdl: ./SDL
	@echo Building SDL
	@mkdir -p ./build
	@cmake -G Ninja -S ./SDL -B ./build
	@cd ./build && ninja
	@mkdir -p $(EXTERNAL_LIBS_DIR)/SDL
	@mv ./build/libSDL*.so* $(EXTERNAL_LIBS_DIR)/SDL
	@rm -rf ./build
	@echo Built SDL
	


