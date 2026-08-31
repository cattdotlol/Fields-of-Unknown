NAME   := game
SRCDIR := src
BUILD  := build

SRCS := $(shell find $(SRCDIR) -name '*.c')
OBJS := $(SRCS:$(SRCDIR)/%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

# Overridable so the compatibility build can link a static raylib built
# against an older glibc. Defaults to whatever is installed locally.
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib)
RAYLIB_LIBS   ?= $(shell pkg-config --libs raylib)
SYS_LIBS      ?= -lGL -lm

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -I$(SRCDIR) -MMD -MP $(RAYLIB_CFLAGS)
LDLIBS := $(RAYLIB_LIBS) $(SYS_LIBS)

debug:   CFLAGS += -g -O0
debug:   $(BUILD)/$(NAME)

# Needs the sanitizer runtimes: sudo dnf install libasan libubsan
asan:    CFLAGS  += -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
asan:    LDFLAGS += -fsanitize=address,undefined
asan:    $(BUILD)/$(NAME)

release: CFLAGS  += -O2 -DNDEBUG
# $ORIGIN/lib lets a bundled raylib win over the system one, so the
# machine running this does not need raylib installed. Falls back to the
# system copy when that directory is absent.
release: LDFLAGS += -Wl,-rpath,'$$ORIGIN/lib' -s
release: $(BUILD)/$(NAME)

$(BUILD)/$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

run: debug
	./$(BUILD)/$(NAME)

# Everything but main.c, plus the test files.
TEST_SRCS := $(filter-out $(SRCDIR)/main.c,$(SRCS)) $(wildcard tests/*.c)

test:
	@mkdir -p $(BUILD)/tests
	$(CC) $(CFLAGS) -g -O0 -o $(BUILD)/tests/run $(TEST_SRCS) $(LDLIBS)
	@./$(BUILD)/tests/run

VERSION ?= 0.1.0
SRCPKG  := fields-of-unknown-$(VERSION)-src
PKG     := fields-of-unknown-$(VERSION)-linux-x86_64
DIST    := dist

# A folder a friend can unzip and run. The game itself has no data files;
# the only thing bundled is raylib.
dist: clean release
	@rm -rf $(DIST)/$(PKG) $(DIST)/$(PKG).zip
	@mkdir -p $(DIST)/$(PKG)/lib
	@cp $(BUILD)/$(NAME) $(DIST)/$(PKG)/
	@cp -L "$$(ldd $(BUILD)/$(NAME) | awk '/libraylib/{print $$3}')" $(DIST)/$(PKG)/lib/
	@cp -r assets $(DIST)/$(PKG)/
	@printf '#!/bin/sh\ncd "$$(dirname "$$0")" || exit 1\nexec ./$(NAME) "$$@"\n' > $(DIST)/$(PKG)/run.sh
	@chmod +x $(DIST)/$(PKG)/run.sh
	@printf 'FIELDS OF UNKNOWN\n\nRun ./run.sh\n\nKeep assets/ next to the game.\n' > $(DIST)/$(PKG)/README.txt
	@cd $(DIST) && zip -qr $(PKG).zip $(PKG)
	@echo
	@echo "  $(DIST)/$(PKG).zip  ($$(du -h $(DIST)/$(PKG).zip | cut -f1))"
	@echo "  send that; they unzip and run ./run.sh"
	@echo

# Source package. Ship this rather than a binary when the target machine
# is not the build machine - see packaging/README.src.txt.
dist-src:
	@rm -rf $(DIST)/$(SRCPKG) $(DIST)/$(SRCPKG).zip
	@mkdir -p $(DIST)/$(SRCPKG)
	@cp -r src tests assets Makefile README.md $(DIST)/$(SRCPKG)/
	@cd $(DIST) && zip -qr $(SRCPKG).zip $(SRCPKG)
	@echo
	@echo "  $(DIST)/$(SRCPKG).zip  ($$(du -h $(DIST)/$(SRCPKG).zip | cut -f1))"
	@echo "  they unzip it and run ./packaging/build.sh"
	@echo

clean:
	rm -rf $(BUILD) $(DIST)

-include $(DEPS)

.PHONY: debug asan release run test dist dist-src clean
