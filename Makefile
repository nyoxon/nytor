VERSION = 1.2.1
PACKAGE = nyt
PACKAGE_DIR = package
DEB_PATH = $(PACKAGE)_$(VERSION)_amd64.deb

CC      ?= cc


# normal executable
CFLAGS  := -O2 -Wall -Wextra -Wpedantic -MMD -MP
CFLAGS += -DNYTOR_VERSION=\"$(VERSION)\" -std=c17


# GDB nerds
CFLAGS_DEBUG := -O0 -Wall -Wextra -Wpedantic -MMD -MP 
CFLAGS_DEBUG += -g3
CFLAGS_DEBUG += -DNYTOR_VERSION=\"$(VERSION)\" -std=c17


# no time for GDB
CFLAGS_SANITIZE := -O0 -Wall -Wextra -Wpedantic -MMD -MP
CFLAGS_SANITIZE += -fsanitize=address,undefined
CFLAGS_SANITIZE += -DNYTOR_VERSION=\"$(VERSION)\" -std=c17

# glibc hides a few declarations under strict C
# modes unless feature-test
# macros are enabled. FreeBSD exposes the
# interfaces used here without this.
ifeq ($(shell uname -s),Linux)
CFLAGS += -D_GNU_SOURCE
CFLAGS_DEBUG += -D_GNU_SOURCE
CFLAGS_SANITIZE += -D_GNU_SOURCE
endif


CPPFLAGS := -Isrc -Iinclude

LDFLAGS := -lm -ldl
ifeq ($(shell uname -s),FreeBSD)
LDFLAGS += -lutil
endif
LDFLAGS_SANITIZE := -fsanitize=address,undefined

SRC_DIR := src
OBJ_DIR := build/obj
OBJ_DIR_DEBUG := build/debug
OBJ_DIR_SANITIZE := build/sanitize
BIN_DIR := build

PLUGIN_SRC_DIR := plugins/lexers
PLUGIN_BUILD_DIR := build/plugins/lexers

PLUGIN_SRCS := $(shell find $(PLUGIN_SRC_DIR) -name '*.c')
PLUGINS := $(patsubst $(PLUGIN_SRC_DIR)/%.c,$(PLUGIN_BUILD_DIR)/%.so,$(PLUGIN_SRCS))

TARGET  := nyt
TARGET_DEBUG := nyt_debug
TARGET_SANITIZE := nyt_sanitize
TARGET_TEST := build/test_runner
TARGET_TEST_SANITIZE := build/test_runner_sanitize
TARGET_TEST_INTEGRATION := build/nyt_integration

SRCS := $(shell find $(SRC_DIR) -name '*.c')
TEST_SRCS := $(filter-out $(SRC_DIR)/main.c,$(SRCS)) $(shell find tests -name '*.c' 2>/dev/null)

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_DEBUG)/%.o,$(SRCS))
OBJS_SANITIZE := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_SANITIZE)/%.o,$(SRCS))

DEPS := $(OBJS:.o=.d)
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)
DEPS_SANITIZE := $(OBJS_SANITIZE:.o=.d)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
DATADIR = $(PREFIX)/share/nytor

CPPFLAGS += -DNYTOR_DATADIR=\"$(DATADIR)\"

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
	$(CC) $(CPPFLAGS) $(OBJS_DEBUG) -o $@ $(LDFLAGS)

$(OBJ_DIR_DEBUG)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS_DEBUG) -c $< -o $@

sanitize: $(TARGET_SANITIZE)

$(TARGET_SANITIZE): $(OBJS_SANITIZE)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(OBJS_SANITIZE) -o $@ $(LDFLAGS) $(LDFLAGS_SANITIZE)

$(OBJ_DIR_SANITIZE)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS_SANITIZE) -c $< -o $@

test: $(TARGET_TEST)
	./$(TARGET_TEST)

$(TARGET_TEST): $(TEST_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SRCS) -o $@ $(LDFLAGS)

test-sanitize: $(TARGET_TEST_SANITIZE)
	./$(TARGET_TEST_SANITIZE)

$(TARGET_TEST_SANITIZE): $(TEST_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_SANITIZE) $(TEST_SRCS) -o $@ $(LDFLAGS) $(LDFLAGS_SANITIZE)

test-integration: $(TARGET_TEST_INTEGRATION)
	python3 tests/integration_editor.py ./$(TARGET_TEST_INTEGRATION)

$(TARGET_TEST_INTEGRATION): $(SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

test-all: test test-integration

plugins: $(PLUGINS)

$(PLUGIN_BUILD_DIR)/%.so: $(PLUGIN_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -fPIC $< -o $@

install: $(TARGET) $(PLUGINS)
	strip $(TARGET)
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR) \
		$(DESTDIR)$(DATADIR)/themes \
		$(DESTDIR)$(DATADIR)/plugins \
		$(DESTDIR)$(DATADIR)/plugins/languages \
		$(DESTDIR)$(MANDIR)/man1
	install -m755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

	install -m644 config/config.ny $(DESTDIR)$(DATADIR)/config.ny

	install -m644 themes/* $(DESTDIR)$(DATADIR)/themes/
	install -m644 plugins/*.lua $(DESTDIR)$(DATADIR)/plugins/

	for plugin in $(PLUGINS); do \
		name=$$(basename "$$(dirname "$$plugin")"); \
		install -m755 "$$plugin" \
			"$(DESTDIR)$(DATADIR)/plugins/languages/$$name.so"; \
	done

	install -m644 manual/nyt.1 $(DESTDIR)$(MANDIR)/man1/nyt.1

uninstall:
	rm -rf $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DATADIR)
	rm -rf $(DESTDIR)$(MANDIR)/man1/nyt.1

deb: sync-version $(TARGET)
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(PACKAGE_DIR)/DEBIAN

	cp packaging/DEBIAN/control $(PACKAGE_DIR)/DEBIAN/control

	install -Dm755 $(TARGET) $(PWD)/$(PACKAGE_DIR)$(DEB_BINDIR)/$(TARGET)

	install -Dm644 config/config.ny $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/config.ny

	install -d $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/themes
	install -m644 themes/* $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/themes/
	install -d $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins
	install -m644 plugins/*.lua $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins/

	install -d $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins/languages
	cp -r plugins/shared/* $(PWD)/$(PACKAGE_DIR)$(DEB_DATADIR)/plugins/languages/

	install -Dm644 manual/nyt.1 $(PWD)/$(PACKAGE_DIR)$(DEB_MANDIR)/man1/nyt.1

	dpkg-deb --build --root-owner-group $(PACKAGE_DIR) $(PACKAGE)_$(VERSION)_amd64.deb

clean:
	rm -rf $(BIN_DIR) $(TARGET) $(TARGET_DEBUG) $(TARGET_SANITIZE) $(PACKAGE_DIR) $(DEB_PATH) debug.ny

# Keep release metadata derived from VERSION above.
sync-version:
	sh scripts/sync_version.sh

check-version:
	sh scripts/sync_version.sh --check

freebsd-pkg: sync-version
	sh scripts/package_freebsd.sh

-include $(DEPS)
-include $(DEPS_DEBUG)
-include $(DEPS_SANITIZE)

.PHONY: all clean sync-version check-version freebsd-pkg test test-sanitize \
	test-integration test-all
