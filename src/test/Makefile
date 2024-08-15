PROJ_SRC := $(ROOT_PATH)/$(SRC)/test
PROJ_OBJ := $(ROOT_PATH)/$(OBJ)/test
PROJ_BIN := $(ROOT_PATH)/$(BIN)
PROJ_INCLUDE := $(ROOT_PATH)/$(INCLUDE)

CFLAGS += -Wall -Wextra -ggdb3 -std=gnu23 -I$(EXTERNAL_DIR)/SDL/include -O3
LDFLAGS += -Wl,-rpath=$(EXTERNAL_LIBS_DIR)/SDL -L$(EXTERNAL_LIBS_DIR)/SDL -lSDL3 -ldoom-style-renderer

TARGET := $(PROJ_BIN)/test

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
	@echo building $(TARGET)
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
