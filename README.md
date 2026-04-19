# Arabian Shield Linux Core

<div dir="rtl">

**الدرع العربي - طبقة وسيطة لمعالجة النصوص العربية على لينكس**

</div>

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/arabian-shield/arabian-shield-linux-core)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)](https://github.com/arabian-shield/arabian-shield-linux-core)

---

## The Problem

Arabic font rendering and Right-to-Left (RTL) text shaping on Linux is fundamentally broken compared to Windows and macOS. Developers, designers, and Arabic-speaking users constantly face:

- **Ligature failures** — Arabic letters rendered as isolated glyphs instead of connected forms
- **Baseline misalignment** — Arabic text floats above or below the Latin baseline in mixed text
- **Broken Kashida (تشكيل)** — Diacritical marks displaced or dropped entirely
- **Wrong glyph selection** — Initial/medial/final forms rendered incorrectly
- **Bidirectional confusion** — Numbers embedded in Arabic text render in wrong order
- **Font fallback chaos** — System falls back to a Latin font with no Arabic support, rendering boxes (□□□)
- **HarfBuzz misconfiguration** — Shaping engine not invoked correctly at the application level

Arabian Shield Linux Core is a foundational OS-level middleware and configuration layer that **fixes these problems at the root**, so every application on the system benefits automatically.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│              Applications (GTK/Qt/X11)           │
├─────────────────────────────────────────────────┤
│          Arabian Shield Middleware Layer          │
│  ┌─────────────┐  ┌──────────┐  ┌────────────┐  │
│  │ Pango Wrapper│  │ FriBidi  │  │ Baseline   │  │
│  │ (shaping)   │  │ Wrapper  │  │ Adjuster   │  │
│  └─────────────┘  └──────────┘  └────────────┘  │
├─────────────────────────────────────────────────┤
│        HarfBuzz  │  FreeType  │  Fontconfig      │
├─────────────────────────────────────────────────┤
│              Linux Kernel / Display Server        │
└─────────────────────────────────────────────────┘
```

See [docs/architecture.md](docs/architecture.md) for a full technical breakdown.

---

## Features

- **Correct Arabic shaping** via a Pango/HarfBuzz wrapper that enforces proper script itemization
- **RTL paragraph isolation** using Unicode Bidirectional Algorithm (UAX #9) via FriBidi
- **Baseline normalization** for mixed Arabic/Latin text
- **Drop-in Fontconfig rules** that establish correct Arabic font priority chains
- **Locale enforcement** for `ar_*` and `fa_*` locales
- **Kashida (كشيدة) support** configuration for justification
- **Tested with**: GTK 3/4, Qt 5/6, Pango, Cairo, SDL2, Electron (via Chromium's Skia)

---

## Quick Start

### Requirements

- GCC >= 9 or Clang >= 10
- `libpango-1.0-dev` (>= 1.44)
- `libfribidi-dev` (>= 1.0)
- `libharfbuzz-dev` (>= 2.6)
- `libfontconfig-dev`
- `libfreetype-dev`
- Python 3.8+ (for tests)

### Install Dependencies (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libpango1.0-dev \
    libfribidi-dev \
    libharfbuzz-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libgtk-3-dev \
    python3-gi \
    python3-cairo
```

### Build and Install

```bash
git clone https://github.com/arabian-shield/arabian-shield-linux-core.git
cd arabian-shield-linux-core

# Download recommended Arabic fonts first
bash fonts/download_fonts.sh

# Build
make

# Install system-wide (requires root)
sudo make install

# Or install for current user only
make install-user
```

### Verify Installation

```bash
bash scripts/test_render.sh
```

---

## Configuration

Arabian Shield installs configuration files to:

| File | Destination | Purpose |
|------|-------------|---------|
| `config/fonts.conf` | `/etc/fonts/conf.d/99-arabic-shield.conf` | Font priority and substitution rules |
| `config/locale.conf` | `/etc/locale.conf` (merged) | Arabic locale settings |
| `config/99-arabic-shield.conf` | `/etc/X11/Xresources.d/` | X11 resource overrides |

To customize, edit files in `config/` before running `make install`.

---

## Usage in Your Application

### C / GTK

```c
#include <arabic_shield.h>

int main(void) {
    arabic_shield_init(ARABIC_SHIELD_FLAG_AUTO_BIDI | ARABIC_SHIELD_FLAG_FIX_BASELINE);
    // ... your application code ...
    arabic_shield_cleanup();
    return 0;
}
```

Link with: `pkg-config --libs arabic-shield`

See [examples/simple_gtk_app.c](examples/simple_gtk_app.c) for a complete GTK example.

### C++ / Qt

```cpp
#include <arabic_shield.h>

int main(int argc, char *argv[]) {
    arabic_shield_init(ARABIC_SHIELD_FLAG_AUTO_BIDI);
    QApplication app(argc, argv);
    // ...
}
```

See [examples/simple_qt_app.cpp](examples/simple_qt_app.cpp) for a complete Qt example.

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/architecture.md](docs/architecture.md) | System architecture and design decisions |
| [docs/building.md](docs/building.md) | Detailed build instructions for all distros |
| [docs/arabic-typography-guide.md](docs/arabic-typography-guide.md) | Arabic typography concepts for developers |
| [docs/api_reference.md](docs/api_reference.md) | Full C API reference |

---

## Contributing

We welcome contributions! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests.

---

## License

Arabian Shield Linux Core is licensed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE) for the full text.

---

## Acknowledgments

This project stands on the shoulders of:
- [HarfBuzz](https://harfbuzz.github.io/) — The text shaping engine
- [FriBidi](https://fribidi.org/) — The Free Implementation of the Unicode BiDi Algorithm
- [Pango](https://pango.gnome.org/) — Internationalized text layout
- [Noto Fonts](https://fonts.google.com/noto) — Google's universal font family
- [Amiri Font](https://www.amirifont.org/) — A classical Arabic typeface
