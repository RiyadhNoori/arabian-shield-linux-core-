# Building Arabian Shield Linux Core

This document covers building on all major Linux distributions, cross-compilation, and debug builds.

---

## Quick Build (Debian/Ubuntu)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libpango1.0-dev \
    libfribidi-dev \
    libharfbuzz-dev \
    libfontconfig1-dev \
    libfreetype6-dev

# Build
git clone https://github.com/arabian-shield/arabian-shield-linux-core.git
cd arabian-shield-linux-core
make

# Install system-wide
sudo make install
```

---

## Distribution-Specific Instructions

### Ubuntu 20.04 / 22.04 / 24.04

```bash
sudo apt-get install -y \
    build-essential pkg-config \
    libpango1.0-dev libfribidi-dev libharfbuzz-dev \
    libfontconfig1-dev libfreetype6-dev \
    libgtk-3-dev python3-gi python3-cairo \
    gir1.2-pango-1.0 gir1.2-pangocairo-1.0

make && sudo make install
```

### Fedora / RHEL 8+ / CentOS Stream

```bash
sudo dnf install -y \
    gcc make pkgconfig \
    pango-devel fribidi-devel harfbuzz-devel \
    fontconfig-devel freetype-devel \
    gtk3-devel python3-gobject python3-cairo

make && sudo make install
```

### Arch Linux / Manjaro

```bash
sudo pacman -S --needed \
    base-devel pango fribidi harfbuzz \
    fontconfig freetype2 gtk3 \
    python-gobject python-cairo

make && sudo make install
```

### openSUSE Tumbleweed / Leap

```bash
sudo zypper install -y \
    gcc make pkgconf \
    pango-devel fribidi-devel harfbuzz-devel \
    fontconfig-devel freetype-devel \
    gtk3-devel python3-gobject python3-cairo

make && sudo make install
```

### Alpine Linux

```bash
sudo apk add \
    build-base pkgconf \
    pango-dev fribidi-dev harfbuzz-dev \
    fontconfig-dev freetype-dev

make && sudo make install
```

---

## Build Options

### Debug Build

Enables AddressSanitizer, UBSanitizer, and debug logging:

```bash
make DEBUG=1
```

Run with:
```bash
ASAN_OPTIONS=detect_leaks=1 ./tests/test_arabic_render_bin
```

### Custom Install Prefix

```bash
make install PREFIX=/opt/arabic-shield
# Sets library to /opt/arabic-shield/lib/
# Headers to /opt/arabic-shield/include/arabic_shield/
```

### User-Local Install (No Root Required)

```bash
make install-user
# Installs to ~/.local/lib/ and ~/.local/include/
```

Add to your shell profile:
```bash
export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### Build with GTK Examples

```bash
make examples
# Builds examples/simple_gtk_app and examples/simple_qt_app
```

### Static Library Only

```bash
make static
# Produces libarabic_shield.a without shared library
```

---

## Linking Against Arabian Shield

### Using pkg-config (recommended)

```bash
gcc myapp.c $(pkg-config --cflags --libs arabic-shield) -o myapp
```

### Manual Linking

```bash
gcc myapp.c \
    -I/usr/local/include/arabic_shield \
    -L/usr/local/lib \
    -larabic_shield \
    $(pkg-config --libs pango fribidi harfbuzz fontconfig freetype2) \
    -o myapp
```

### With Pango integration enabled

```bash
gcc myapp.c \
    -DARABIC_SHIELD_ENABLE_PANGO \
    $(pkg-config --cflags --libs arabic-shield pango pangocairo) \
    -o myapp
```

---

## Running the Test Suite

```bash
# All tests
make test

# Python tests only
python3 -m pytest tests/test_arabic_render.py -v

# Render verification script
bash scripts/test_render.sh --verbose

# Visual baseline test (opens in browser)
xdg-open tests/test_baseline.html

# GTK UI test
python3 -c "
import gi; gi.require_version('Gtk','3.0')
from gi.repository import Gtk
b = Gtk.Builder(); b.add_from_file('tests/test_rtl_ui.glade')
w = b.get_object('main_window'); w.connect('destroy', Gtk.main_quit)
w.show_all(); Gtk.main()
"
```

---

## Troubleshooting Build Errors

### `pkg-config: command not found`

```bash
# Debian/Ubuntu
sudo apt-get install pkg-config
# Fedora
sudo dnf install pkgconf
# Arch
sudo pacman -S pkgconf
```

### `pango/pango.h: No such file or directory`

Install Pango development headers:
```bash
# Debian/Ubuntu
sudo apt-get install libpango1.0-dev
# Fedora
sudo dnf install pango-devel
```

### `fribidi.h: No such file or directory`

```bash
sudo apt-get install libfribidi-dev   # Debian/Ubuntu
sudo dnf install fribidi-devel        # Fedora
```

### Linker error: `-larabic_shield` not found

The library is not in the linker path. Either:
1. Run `sudo ldconfig` after `sudo make install`
2. Or set `LD_LIBRARY_PATH`:
   ```bash
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```

### `FT_SFNT_OS2` undeclared

Ensure you have FreeType headers with sfnt tables support:
```bash
sudo apt-get install libfreetype6-dev
```
Also make sure you're including `ft2build.h` before `FT_SFNT_TABLES_H`.

---

## Cross-Compilation

### ARM64 (e.g., Raspberry Pi, Pinebook)

```bash
sudo apt-get install gcc-aarch64-linux-gnu
export CC=aarch64-linux-gnu-gcc
export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
make CC=$CC
```

### RISC-V

```bash
sudo apt-get install gcc-riscv64-linux-gnu
export CC=riscv64-linux-gnu-gcc
make CC=$CC
```

---

## Distribution Packaging

### Creating a .deb Package

```bash
# Install packaging tools
sudo apt-get install devscripts debhelper

# Generate tarball
make dist

# Build .deb (from arabian-shield-linux-core/ directory)
dpkg-buildpackage -us -uc -b
```

### Creating an .rpm Package

```bash
sudo dnf install rpm-build
make dist
rpmbuild -tb dist/arabian-shield-linux-core-0.1.0.tar.gz
```
