# Contributing to Arabian Shield Linux Core

Thank you for your interest in contributing! This document outlines how to participate in the project effectively.

---

## Code of Conduct

This project is welcoming to all contributors regardless of background. We are especially focused on building a tool that serves Arabic-speaking communities, so respectful, constructive communication is essential.

---

## How to Contribute

### Reporting Bugs

Before filing a bug, please:

1. Search [existing issues](https://github.com/arabian-shield/arabian-shield-linux-core/issues) to avoid duplicates.
2. Run the test suite: `bash scripts/test_render.sh` and include the output.
3. Include your system info: distro, kernel version, Pango version, display server (X11/Wayland).

When filing a bug, use the template:

```
**Describe the bug:**
A clear description of what is broken.

**Arabic text affected:**
Paste the specific Arabic string that renders incorrectly.

**Expected behavior:**
What the text should look like.

**System info:**
- OS: Ubuntu 22.04
- Pango: 1.50.x
- HarfBuzz: 4.x
- Display: X11 / Wayland
- GPU/Driver: Intel / NVIDIA / AMD

**Logs:**
Output of `bash scripts/test_render.sh --verbose`
```

---

### Submitting Pull Requests

1. **Fork** the repository and create a feature branch from `main`:
   ```bash
   git checkout -b fix/kashida-rendering
   ```

2. **Write tests** for any new functionality in `tests/`.

3. **Follow the coding style**:
   - C code: K&R style, 4-space indentation, max 100 columns
   - Python: PEP 8
   - Commit messages: `[component] Short description` (e.g., `[fribidi] Fix paragraph direction detection for mixed scripts`)

4. **Run the test suite before submitting**:
   ```bash
   make test
   ```

5. **Update documentation** in `docs/` if your change affects public API or behavior.

6. Open a pull request targeting the `main` branch. Fill out the PR template completely.

---

### Priority Areas

We especially welcome contributions in these areas:

- **Font coverage**: Testing with additional Arabic fonts (Scheherazade, Cairo, Lateef, etc.)
- **Wayland support**: Testing and fixing issues under Wayland compositors
- **Qt integration**: The Qt wrapper is less mature than the GTK/Pango one
- **Farsi/Persian support**: The `fa_IR` locale has unique shaping needs (ZWNJ handling)
- **Urdu support**: Nastaliq script requires special handling beyond standard Arabic shaping
- **Distribution packaging**: `.deb`, `.rpm`, Arch PKGBUILD contributions welcome
- **Documentation in Arabic**: Translating docs to Arabic is highly valued

---

### Development Environment Setup

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/arabian-shield-linux-core.git
cd arabian-shield-linux-core

# Install all development dependencies (Debian/Ubuntu)
sudo apt-get install -y \
    build-essential valgrind cppcheck clang-format \
    libpango1.0-dev libfribidi-dev libharfbuzz-dev \
    libfontconfig1-dev libfreetype6-dev libgtk-3-dev \
    python3-dev python3-pip python3-gi python3-cairo \
    gtk-3-examples

# Install Python test dependencies
pip3 install -r tests/requirements.txt

# Build in debug mode
make DEBUG=1

# Run all tests
make test

# Run the visual render test
bash scripts/test_render.sh
```

---

### Commit Signing

We encourage (but do not require) GPG-signed commits:

```bash
git config --global user.signingkey YOUR_KEY_ID
git config --global commit.gpgsign true
```

---

## Architecture Notes for Contributors

Read [docs/architecture.md](docs/architecture.md) carefully before modifying core shaping code. Key invariants:

- The `arabic_shield_init()` function must be idempotent — calling it multiple times must be safe.
- The Pango wrapper must not break rendering of non-Arabic scripts.
- All public API functions must be thread-safe after `arabic_shield_init()` completes.
- Memory allocated by library functions must be freed by corresponding `_free()` calls, not by the caller using `free()`.

---

## Licensing

By contributing to this project, you agree that your contributions will be licensed under the **GNU General Public License v3.0**. Do not submit code that is incompatible with GPLv3.
