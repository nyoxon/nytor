PLUGINS := $(patsubst $(PLUGIN_SRC_DIR)/%.c,$(PLUGIN_BUILD_DIR)/%.so,$(PLUGIN_SRCS))

TARGET  := nytor