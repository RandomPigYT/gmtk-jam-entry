# This makefile will only run when compiling for the web

PROJ_SRC := $(ROOT_PATH)/$(SRC)
PROJ_OBJ := $(ROOT_PATH)/$(OBJ)
PROJ_BIN := $(ROOT_PATH)/$(BIN)
PROJ_WASM := $(ROOT_PATH)/$(WASM)
PROJ_INCLUDE := $(ROOT_PATH)/$(INCLUDE)

CFLAGS += --target=wasm32 -Wall -Wextra -ggdb3 -std=gnu23 -O3 -DPLATFORM_WEB -DNDEBUG
LDFLAGS += -nostdlib -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined
TARGET := $(PROJ_WASM)/game.wasm

SRCS := $(shell find $(PROJ_SRC) -type  f -name "*.c")
OBJS := $(patsubst $(PROJ_SRC)/%.c, $(PROJ_OBJ)/%.o, $(SRCS))
DEPS := $(patsubst $(PROJ_SRC)/%.c, $(PROJ_OBJ)/%.d, $(SRCS))

ifeq ($(GENERATE_ASM), 1)
	ASMS := $(patsubst $(PROJ_SRC)/%.c, $(PROJ_OBJ)/%.s, $(SRCS))
endif

INCLUDES := $(shell find $(PROJ_INCLUDE) -type f -name "*.h")
INCLUDES += $(shell find $(PROJ_SRC) -type f -name "*.h")
INCLUDES += $(shell find $(ROOT_PATH)/$(SRC)/util -type f -name "*.h")

all: $(TARGET)

$(TARGET): $(OBJS) $(ASMS)
	@echo
	@echo building $(TARGET) in src
	@$(LD) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@echo built $(TARGET)

-include $(DEPS)

$(PROJ_OBJ)/%.o: $(PROJ_SRC)/%.c
	@echo building $@
	@$(CC) $(CFLAGS) -MD -MP -c $< -o $@
	@echo built $@
	@echo

$(PROJ_OBJ)/%.s: $(PROJ_SRC)/%.c
	@echo Generating assembly $@
	@$(CC) $(CFLAGS) -S $< -o $@
	@echo Generated $@
	@echo
