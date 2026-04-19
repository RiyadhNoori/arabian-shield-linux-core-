# Arabian Shield Linux Core — Makefile
# Requires: gcc, pkg-config, libpango, libfribidi, libharfbuzz, libfontconfig

CC          := gcc
CXX         := g++
AR          := ar
PKG_CONFIG  := pkg-config

# --- Versioning ---
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# --- Directories ---
SRCDIR      := src
CONFIGDIR   := config
SCRIPTSDIR  := scripts
TESTSDIR    := tests
PREFIX      ?= /usr/local
SYSCONFDIR  ?= /etc
LIBDIR      := $(PREFIX)/lib
INCLUDEDIR  := $(PREFIX)/include/arabic_shield
FONTCFGDIR  := $(SYSCONFDIR)/fonts/conf.d

# --- pkg-config dependencies ---
DEPS        := pango fribidi harfbuzz fontconfig freetype2
CFLAGS_PKG  := $(shell $(PKG_CONFIG) --cflags $(DEPS))
LIBS_PKG    := $(shell $(PKG_CONFIG) --libs $(DEPS))

# --- Compiler flags ---
CFLAGS_BASE := -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 \
               -Wconversion -Wshadow -fPIC \
               -DARABIC_SHIELD_VERSION=\"$(VERSION)\"
CFLAGS_REL  := -O2 -DNDEBUG
CFLAGS_DBG  := -g3 -O0 -DDEBUG -fsanitize=address,undefined

ifdef DEBUG
    EXTRA_CFLAGS := $(CFLAGS_DBG)
else
    EXTRA_CFLAGS := $(CFLAGS_REL)
endif

CFLAGS      := $(CFLAGS_BASE) $(EXTRA_CFLAGS) $(CFLAGS_PKG) -I$(SRCDIR)
LDFLAGS     := $(LIBS_PKG) -ldl -lpthread

# --- Sources ---
LIB_SRCS    := $(SRCDIR)/arabic_shield.c \
               $(SRCDIR)/pango_wrapper.c \
               $(SRCDIR)/fribidi_wrapper.c \
               $(SRCDIR)/baseline_adjuster.c

LIB_OBJS    := $(LIB_SRCS:.c=.o)

SHARED_LIB  := libarabic_shield.so.$(VERSION)
SHARED_LINK := libarabic_shield.so
STATIC_LIB  := libarabic_shield.a

# --- Targets ---
.PHONY: all clean install install-user uninstall test check \
        shared static examples lint valgrind dist

all: shared static

shared: $(SHARED_LIB)

static: $(STATIC_LIB)

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared -Wl,-soname,libarabic_shield.so.$(VERSION_MAJOR) \
	      -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_LIB) $(SHARED_LINK)

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Examples ---
examples: shared
	$(CC) $(CFLAGS) examples/simple_gtk_app.c \
	      -o examples/simple_gtk_app \
	      -L. -larabic_shield \
	      $(shell $(PKG_CONFIG) --cflags --libs gtk+-3.0) $(LDFLAGS)
	$(CXX) $(CFLAGS) examples/simple_qt_app.cpp \
	      -o examples/simple_qt_app \
	      -L. -larabic_shield \
	      $(shell $(PKG_CONFIG) --cflags --libs Qt5Widgets 2>/dev/null || echo "") $(LDFLAGS) || \
	      echo "[WARN] Qt5 not found — skipping simple_qt_app"

# --- Install (system-wide) ---
install: all
	@echo "Installing Arabian Shield Linux Core $(VERSION)..."
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(FONTCFGDIR)
	install -m 755 $(SHARED_LIB) $(DESTDIR)$(LIBDIR)/
	ln -sf $(SHARED_LIB) $(DESTDIR)$(LIBDIR)/$(SHARED_LINK)
	ldconfig $(DESTDIR)$(LIBDIR) 2>/dev/null || true
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(SRCDIR)/arabic_shield.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(CONFIGDIR)/fonts.conf \
	               $(DESTDIR)$(FONTCFGDIR)/99-arabic-shield.conf
	install -m 644 $(CONFIGDIR)/99-arabic-shield.conf \
	               $(DESTDIR)$(SYSCONFDIR)/X11/Xresources.d/99-arabic-shield.conf \
	               2>/dev/null || true
	@bash $(SCRIPTSDIR)/install.sh --post-install
	@echo "Installation complete. Run 'fc-cache -fv' to refresh font cache."

# --- Install (user-local) ---
install-user: all
	@echo "Installing for current user..."
	install -d $(HOME)/.local/lib
	install -d $(HOME)/.local/include/arabic_shield
	install -d $(HOME)/.config/fontconfig/conf.d
	install -m 755 $(SHARED_LIB) $(HOME)/.local/lib/
	ln -sf $(SHARED_LIB) $(HOME)/.local/lib/$(SHARED_LINK)
	install -m 644 $(STATIC_LIB) $(HOME)/.local/lib/
	install -m 644 $(SRCDIR)/arabic_shield.h \
	               $(HOME)/.local/include/arabic_shield/
	install -m 644 $(CONFIGDIR)/fonts.conf \
	               $(HOME)/.config/fontconfig/conf.d/99-arabic-shield.conf
	@echo "User installation complete."
	@echo "Add to your shell profile: export LD_LIBRARY_PATH=\$$HOME/.local/lib:\$$LD_LIBRARY_PATH"

# --- Uninstall ---
uninstall:
	@bash $(SCRIPTSDIR)/uninstall.sh

# --- Tests ---
test: check

check: shared
	@echo "Running C unit tests..."
	$(CC) $(CFLAGS) -DARABIC_SHIELD_TESTING \
	      $(LIB_SRCS) tests/test_arabic_render.c \
	      -o tests/test_arabic_render_bin $(LDFLAGS)
	./tests/test_arabic_render_bin
	@echo "Running Python tests..."
	python3 -m pytest $(TESTSDIR)/test_arabic_render.py -v
	@echo "Running render script..."
	bash $(SCRIPTSDIR)/test_render.sh

# --- Static analysis ---
lint:
	cppcheck --enable=all --std=c11 --suppress=missingIncludeSystem \
	         -I$(SRCDIR) $(LIB_SRCS)
	clang-format --dry-run --Werror $(LIB_SRCS) $(SRCDIR)/arabic_shield.h

# --- Memory check ---
valgrind: shared
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
	         ./tests/test_arabic_render_bin

# --- Distribution tarball ---
dist: clean
	mkdir -p dist/arabian-shield-linux-core-$(VERSION)
	cp -r $(SRCDIR) $(CONFIGDIR) $(SCRIPTSDIR) $(TESTSDIR) \
	      $(DOCS) $(EXAMPLES) fonts \
	      README.md LICENSE CONTRIBUTING.md Makefile \
	      dist/arabian-shield-linux-core-$(VERSION)/
	tar -czvf dist/arabian-shield-linux-core-$(VERSION).tar.gz \
	    -C dist arabian-shield-linux-core-$(VERSION)
	@echo "Distribution tarball: dist/arabian-shield-linux-core-$(VERSION).tar.gz"

# --- Clean ---
clean:
	rm -f $(LIB_OBJS) $(SHARED_LIB) $(SHARED_LINK) $(STATIC_LIB)
	rm -f examples/simple_gtk_app examples/simple_qt_app
	rm -f tests/test_arabic_render_bin tests/*.png tests/*.ppm
	rm -rf dist/
