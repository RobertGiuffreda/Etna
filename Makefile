CC := clang

LIB := lib

INCFLAGS  = -Iengine/src/
INCFLAGS += -Iextern/glfw/include
INCFLAGS += -Iextern/cglm/include
INCFLAGS += -Iextern/SPIRV-Reflect/
INCFLAGS += -Iextern/stb_image/
INCFLAGS += -Iextern/cgltf/
INCFLAGS += -Iextern/Nuklear/
INCFLAGS += -I$(VULKAN_SDK)/include/

CFLAGS := -std=c17 -g -O0 -Wall -Wextra -Wpedantic

LNKFLAGS  = -luser32 -lvulkan-1 -lglfw.$(LIB) -L$(VULKAN_SDK)/lib

EXTENSION := .exe

BUILD_DIR := bin
SRC_DIRS := engine/src/

SRC := $(shell find $(SRC_DIRS) -name "*.c")
OBJ := $(SRC:%.c=./$(BUILD_DIR)/%.o)

run: $(BUILD_DIR) $(OBJ)
	find ./$(BUILD_DIR) -name *.

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c:
	mkdir -p $(dir $@)
