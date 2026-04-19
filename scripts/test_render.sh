#!/usr/bin/env bash
# Arabian Shield Linux Core — Rendering Verification Script
# Tests that Arabic text renders correctly on the current system.
#
# Usage: bash test_render.sh [--verbose] [--output-dir /tmp]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

VERBOSE=false
OUTPUT_DIR="${TMPDIR:-/tmp}/arabic-shield-test-$$"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
pass()  { echo -e "  ${GREEN}✓${NC} $*"; }
fail()  { echo -e "  ${RED}✗${NC} $*"; FAILURES=$((FAILURES+1)); }
warn()  { echo -e "  ${YELLOW}!${NC} $*"; }
info()  { echo -e "  ${BLUE}→${NC} $*"; }

FAILURES=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose)    VERBOSE=true;      shift ;;
        --output-dir) OUTPUT_DIR="$2";   shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

mkdir -p "$OUTPUT_DIR"

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Arabian Shield Linux Core — Rendering Test Suite"
echo "═══════════════════════════════════════════════════════════"
echo ""

# ── Test 1: Required libraries ──────────────────────────────────────────
echo "▶ Test 1: Required Libraries"

check_lib() {
    local lib="$1"
    if pkg-config --exists "$lib" 2>/dev/null; then
        local ver
        ver=$(pkg-config --modversion "$lib" 2>/dev/null || echo "unknown")
        pass "$lib ($ver)"
    else
        fail "$lib — NOT FOUND (install $(echo "$lib" | sed 's/libpango-1.0/libpango1.0-dev/; s/libfribidi/libfribidi-dev/; s/libharfbuzz/libharfbuzz-dev/; s/libfontconfig/libfontconfig1-dev/')-dev)"
    fi
}

check_lib libpango-1.0
check_lib libfribidi
check_lib libharfbuzz
check_lib libfontconfig
check_lib freetype2
echo ""

# ── Test 2: Arabic fonts ─────────────────────────────────────────────────
echo "▶ Test 2: Arabic Font Availability"

check_font() {
    local family="$1"
    local required="${2:-optional}"
    if fc-list :family="$family" | grep -q .; then
        pass "$family"
    else
        if [[ "$required" == "required" ]]; then
            fail "$family — NOT FOUND (run: bash fonts/download_fonts.sh)"
        else
            warn "$family — not found (optional)"
        fi
    fi
}

check_font "Noto Sans Arabic"   "required"
check_font "Noto Naskh Arabic"  "recommended"
check_font "Amiri"              "recommended"
check_font "Scheherazade New"   "optional"
check_font "Cairo"              "optional"
echo ""

# ── Test 3: Fontconfig BiDi marks ───────────────────────────────────────
echo "▶ Test 3: Fontconfig Arabic Rules"

FC_CONF="/etc/fonts/conf.d/99-arabic-shield.conf"
if [[ -f "$FC_CONF" ]]; then
    pass "Fontconfig override installed: $FC_CONF"
else
    warn "Fontconfig override not installed (run: sudo make install)"
fi

# Check that fc-match resolves Arabic correctly
if command -v fc-match &>/dev/null; then
    MATCH=$(fc-match ":lang=ar" family 2>/dev/null | head -1)
    if echo "$MATCH" | grep -qi "arabic\|amiri\|noto\|scheherazade\|cairo\|vazir"; then
        pass "fc-match :lang=ar → $MATCH"
    else
        warn "fc-match :lang=ar → $MATCH (may not be an Arabic-capable font)"
    fi
fi
echo ""

# ── Test 4: Python GTK rendering test ───────────────────────────────────
echo "▶ Test 4: Python/GI Arabic Rendering"

if python3 -c "import gi; gi.require_version('Pango', '1.0'); from gi.repository import Pango" 2>/dev/null; then
    pass "python3-gi with Pango available"

    # Run the Python rendering test (offscreen, no display required)
    if python3 - <<'PYEOF' 2>"$OUTPUT_DIR/pango_test.log"; then
import gi
gi.require_version('Pango', '1.0')
gi.require_version('PangoCairo', '1.0')
from gi.repository import Pango, PangoCairo
import cairo

TEST_STRINGS = [
    ("Arabic basic",      "مرحبا بالعالم"),           # Hello World
    ("Arabic with nums",  "رقم 123 هنا"),              # Number in Arabic
    ("Lam-Alef ligature", "لا إله إلا الله"),          # Required ligature
    ("Mixed RTL/LTR",     "Hello مرحبا World"),        # Mixed
    ("Tashkeel",          "العَرَبِيَّةُ"),              # With diacritics
]

errors = []
for name, text in TEST_STRINGS:
    try:
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 800, 60)
        ctx = cairo.Context(surface)
        layout = PangoCairo.create_layout(ctx)
        layout.set_text(text, -1)
        # Set an Arabic font
        fd = Pango.FontDescription.from_string("Noto Sans Arabic 16")
        layout.set_font_description(fd)
        layout.set_auto_dir(True)
        # Get the extents — if shaping is broken, width will be wrong
        ink, logical = layout.get_pixel_extents()
        if logical.width <= 0:
            errors.append(f"{name}: zero width (shaping failed?)")
        else:
            print(f"  OK: {name} → {logical.width}×{logical.height}px")
    except Exception as e:
        errors.append(f"{name}: {e}")

if errors:
    for e in errors:
        print(f"  ERROR: {e}", file=__import__('sys').stderr)
    raise SystemExit(1)
PYEOF
        pass "Pango Arabic shaping test passed"
    else
        fail "Pango Arabic shaping test failed — see $OUTPUT_DIR/pango_test.log"
        if $VERBOSE; then cat "$OUTPUT_DIR/pango_test.log"; fi
    fi
else
    warn "python3-gi not available — skipping Pango render test"
    info "Install: sudo apt-get install python3-gi python3-cairo gir1.2-pango-1.0"
fi
echo ""

# ── Test 5: Locale ───────────────────────────────────────────────────────
echo "▶ Test 5: Locale Configuration"

if locale -a 2>/dev/null | grep -q "ar_.*UTF-8"; then
    pass "Arabic UTF-8 locale available"
else
    warn "No Arabic UTF-8 locale installed"
    info "Generate with: sudo locale-gen ar_EG.UTF-8 && sudo update-locale"
fi

# Check LANG or LC_ALL
CURRENT_LANG="${LANG:-${LC_ALL:-unset}}"
if echo "$CURRENT_LANG" | grep -q "ar_"; then
    pass "LANG is set to Arabic: $CURRENT_LANG"
else
    warn "LANG is not Arabic: $CURRENT_LANG (this is fine for mixed-locale systems)"
fi
echo ""

# ── Test 6: Bidirectional text tool ─────────────────────────────────────
echo "▶ Test 6: FriBidi Command-Line"

if command -v fribidi &>/dev/null; then
    # Test: "Hello مرحبا" should reorder correctly
    RESULT=$(echo "Hello مرحبا" | fribidi --nopad 2>/dev/null || echo "failed")
    if [[ "$RESULT" != "failed" ]]; then
        pass "fribidi CLI works: \"$RESULT\""
    else
        fail "fribidi CLI failed"
    fi
else
    warn "fribidi binary not found — install: sudo apt-get install fribidi"
fi
echo ""

# ── Summary ──────────────────────────────────────────────────────────────
echo "═══════════════════════════════════════════════════════════"
if [[ $FAILURES -eq 0 ]]; then
    echo -e "  ${GREEN}All tests passed!${NC} Arabian Shield is correctly configured."
else
    echo -e "  ${RED}${FAILURES} test(s) failed.${NC} See above for details."
    echo "  Run 'sudo make install' and 'bash fonts/download_fonts.sh' to fix common issues."
fi
echo "  Test output directory: $OUTPUT_DIR"
echo "═══════════════════════════════════════════════════════════"
echo ""

exit $FAILURES
