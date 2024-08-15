MAKEFLAGS += --no-print-directory -s

ROOT_PATH := $(strip $(patsubst %/, %, $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
export ROOT_PATH

CC := gcc
LD := gcc

SRC := src
OBJ := obj
BIN := bin
INCLUDE := include

EXTERNAL_DIR := $(ROOT_PATH)/external
EXTERNAL_LIBS_DIR := $(ROOT_PATH)/external-libs

CFLAGS := -I$(ROOT_PATH)/$(SRC) -isystem $(EXTERNAL_DIR) -I$(ROOT_PATH)/$(INCLUDE)
LDFLAGS := -Wl,-rpath=$(ROOT_PATH)/$(BIN) -L$(ROOT_PATH)/$(BIN) -lm

GENERATE_ASM := 1

export CC LD SRC OBJ BIN INCLUDE EXTERNAL_DIR EXTERNAL_LIBS_DIR CFLAGS LDFLAGS GENERATE_ASM

OBJ_DIRS := $(patsubst $(SRC)/%, $(OBJ)/%, $(shell find $(SRC)/ -mindepth 1 -type d))
CREATE_DIR_COMMAND := ./dirs.sh


PROJECTS := test doom-style-renderer.so 

.PHONY: all dirs clean external run

all: dirs $(PROJECTS)

# ---------------------- PROJECTS ----------------------

doom-style-renderer.so: 
	@$(MAKE) -C $(SRC)/doom-style-renderer

test: doom-style-renderer.so
	@$(MAKE) -C $(SRC)/test


# ---------------------- UTILITY ----------------------

external:
	@mkdir -p $(EXTERNAL_LIBS_DIR)
	@$(MAKE) -C $(EXTERNAL_DIR)

dirs: 
	@mkdir -p $(BIN) 
	@mkdir -p $(OBJ)
	@$(CREATE_DIR_COMMAND) $(OBJ_DIRS)

clean:
	-@rm -rf $(OBJ)
	-@rm -rf $(BIN)
	-@rm -f ./val.txt

run: bin/test
	./bin/test "./assets/scenes/neighbours.dsrs"

valgrind: 
	@valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=./val.txt \
				 --keep-debuginfo=yes\
         ./bin/test	"./assets/scenes/neighbours.dsrs"

