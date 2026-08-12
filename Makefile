CC      := gcc

CFLAGS  := -Wall -Wextra -g -MMD -MP
CFLAGS_DEBUG := -fsanitize=address,undefined
CPPFLAGS := -Isrc -Iinclude

LDFLAGS := 
LDFLAGS_DEBUG := -fsanitize=address,undefined

SRC_DIR := src
OBJ_DIR := build/obj
OBJ_DIR_DEBUG := build/release
BIN_DIR := build

PLUGIN_SRC_DIR := plugins
PLUGIN_BUILD_DIR := build/plugins

PLUGIN_SRCS := $(shell find $(PLUGIN_SRC_DIR) -name '*.c')
PLUGINS := $(patsubst $(PLUGIN_SRC_DIR)/%.c,$(PLUGIN_BUILD_DIR)/%.so,$(PLUGIN_SRCS))

TARGET  := nytor
TARGET_DEBUG := nytor_debug

SRCS := $(shell find $(SRC_DIR) -name '*.c')

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_DEBUG)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(OBJS_DEBUG)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(OBJS_DEBUG) -o $@ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(OBJ_DIR_DEBUG)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_DEBUG) -c $< -o $@

plugins: $(PLUGINS)

$(PLUGIN_BUILD_DIR)/%.so: $(PLUGIN_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -fPIC $< -o $@

clean:
	rm -rf $(BIN_DIR) $(TARGET) $(TARGET_DEBUG) debug.ny

-include $(DEPS)
-include $(DEPS_DEBUG)

.PHONY: all clean
