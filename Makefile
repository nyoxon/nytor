VERSION = 1.1.0
PACKAGE = nyt
PACKAGE_DIR = package
DEB_PATH = $(PACKAGE)_$(VERSION)_amd64.deb

CC      := gcc

CFLAGS  := -O2 -Wall -Wextra -Wpedantic -g -MMD -MP
CFLAGS += -DNYTOR_VERSION=\"$(VERSION)\" -std=c17 -D_GNU_SOURCE 
CFLAGS_DEBUG := -fsanitize=address,undefined -g
CPPFLAGS := -Isrc -Iinclude

LDFLAGS := -lm
LDFLAGS_DEBUG := -fsanitize=address,undefined

SRC_DIR := src
OBJ_DIR := build/obj
OBJ_DIR_DEBUG := build/release
BIN_DIR := build

PLUGIN_SRC_DIR := plugins
PLUGIN_BUILD_DIR := build/plugins

PLUGIN_SRCS := $(shell find $(PLUGIN_SRC_DIR) -name '*.c')
PLUGINS := $(patsubst $(PLUGIN_SRC_DIR)/%.c,$(PLUGIN_BUILD_DIR)/%.so,$(PLUGIN_SRCS))

TARGET  := nyt
TARGET_DEBUG := nyt_debug

SRCS := $(shell find $(SRC_DIR) -name '*.c')

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_DEBUG)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
DATADIR = $(PREFIX)/share/nytor

DEB_PREFIX ?= /usr
DEB_MANDIR = $(DEB_PREFIX)/share/man
DEB_BINDIR = $(DEB_PREFIX)/bin
DEB_DATADIR = $(DEB_PREFIX)/share/nytor

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

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

	install -Dm644 config/config.ny $(DESTDIR)$(DATADIR)/config.ny

	install -d $(DESTDIR)$(DATADIR)/themes
	install -m644 themes/* $(DESTDIR)$(DATADIR)/themes/

	install -d $(DESTDIR)$(DATADIR)/plugins/languages
	cp -r plugins/shared/* $(DESTDIR)$(DATADIR)/plugins/languages/

	install -Dm644 manual/nyt.1 $(DESTDIR)$(MANDIR)/man1/nyt.1

uninstall:
	rm -rf $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DATADIR)
	rm -rf $(DESTDIR)$(MANDIR)/man1/nyt.1

deb: $(TARGET)
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(PACKAGE_DIR)/DEBIAN

	cp packaging/DEBIAN/control $(PACKAGE_DIR)/DEBIAN/control

	install -Dm755 $(TARGET) $(PWD)/$(PACKAGE_DIR)$(DEB_BINDIR)/$(TARGET)

	install -Dm644 config/config.ny $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/config.ny

	install -d $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/themes
	install -m644 themes/* $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/themes/

	install -d $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins/languages
	cp -r plugins/shared/* $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins/languages/

	install -Dm644 manual/nyt.1 $(PWD)/$(PACKAGE_DIR)$(DEB_MANDIR)/man1/nyt.1

	dpkg-deb --build --root-owner-group $(PACKAGE_DIR) $(PACKAGE)_$(VERSION)_amd64.deb

clean:
	rm -rf $(BIN_DIR) $(TARGET) $(TARGET_DEBUG) $(PACKAGE_DIR) $(DEB_PATH) debug.ny

-include $(DEPS)
-include $(DEPS_DEBUG)

.PHONY: all clean