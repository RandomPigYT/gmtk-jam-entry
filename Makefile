MAKEFLAGS += --no-print-directory -s

ROOT_PATH := $(strip $(patsubst %/, %, $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
export ROOT_PATH

PLATFORM := linux

ifeq ($(PLATFORM), linux)

	CC := gcc
	LD := gcc

else ifeq ($(PLATFORM), web)

	CC := emcc
	LD := emcc
	
endif

SRC := src
OBJ := obj
BIN := bin
WASM := wasm
INCLUDE := include

EXTERNAL_DIR := $(ROOT_PATH)/external
EXTERNAL_LIBS_DIR := $(ROOT_PATH)/external-libs

CFLAGS := -I$(ROOT_PATH)/$(SRC) -isystem $(EXTERNAL_DIR) -I$(ROOT_PATH)/$(INCLUDE)

ifeq ($(PLATFORM), linux)

	LDFLAGS := -Wl,-rpath=$(ROOT_PATH)/$(BIN) -L$(ROOT_PATH)/$(BIN) -L$(ROOT_PATH)/$(EXTERNAL_LIBS_DIR)/raylib -lraylib -lm

else

	LDFLAGS := -Wl,-rpath=$(ROOT_PATH)/$(BIN) -L$(ROOT_PATH)/$(BIN) -L$(ROOT_PATH) -l:./libraylib.a -lm

endif

GENERATE_ASM := 1

export PLATFORM CC LD SRC OBJ BIN WASM INCLUDE EXTERNAL_DIR EXTERNAL_LIBS_DIR CFLAGS LDFLAGS GENERATE_ASM

OBJ_DIRS := $(patsubst $(SRC)/%, $(OBJ)/%, $(shell find $(SRC)/ -mindepth 1 -type d))
CREATE_DIR_COMMAND := ./dirs.sh

ifeq ($(PLATFORM), linux)

	PROJECTS := game plugin

else ifeq ($(PLATFORM), web)

	PROJECTS := game
	
endif


.PHONY: all dirs clean external run 
.PHONY: $(PROJECTS)

all: dirs $(PROJECTS)

# ---------------------- PROJECTS ----------------------

game: $(if $(findstring $(PLATFORM),linux),$(filter-out game,$(PROJECTS))) 
ifeq ($(PLATFORM), linux)

	@$(MAKE) -C $(SRC)/game

else ifeq ($(PLATFORM), web)

	@cp -r assets $(WASM)
	@$(MAKE) -C $(SRC)

endif

plugin:
	@$(MAKE) -C $(SRC)/plugin

# ---------------------- UTILITY ----------------------

external:
	@mkdir -p $(EXTERNAL_LIBS_DIR)
	@$(MAKE) -C $(EXTERNAL_DIR)

dirs: 
	@mkdir -p $(BIN) 
	@mkdir -p $(OBJ)
	@mkdir -p $(WASM)
	@$(CREATE_DIR_COMMAND) $(OBJ_DIRS)

clean:
	-@rm -rf $(OBJ)
	-@rm -rf $(BIN)
	-@rm -rf $(WASM)
	-@rm -f ./val.txt

run: bin/game
	./bin/game

valgrind: 
	@valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=./val.txt \
				 --keep-debuginfo=yes\
         ./bin/test	"./assets/scenes/neighbours.dsrs"

