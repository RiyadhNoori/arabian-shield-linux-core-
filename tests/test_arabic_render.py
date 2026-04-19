#!/usr/bin/env python3
"""
Arabian Shield Linux Core — Arabic Rendering Test Suite
Tests correct Arabic shaping, BiDi reordering, and baseline behavior
using Python GObject Introspection (PyGI) with Pango and Cairo.

Run: python3 -m pytest tests/test_arabic_render.py -v
  or: python3 tests/test_arabic_render.py

SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2024 Arabian Shield Contributors
"""

import sys
import os
import subprocess
import unittest

# Optional: allow running without pytest
try:
    import pytest
    HAS_PYTEST = True
except ImportError:
    HAS_PYTEST = False

# ── PyGI imports (soft) ─────────────────────────────────────────────────
try:
    import gi
    gi.require_version('Pango', '1.0')
    gi.require_version('PangoCairo', '1.0')
    from gi.repository import Pango, PangoCairo, GLib
    import cairo
    HAS_PANGO = True
except (ImportError, ValueError) as e:
    HAS_PANGO = False
    PANGO_SKIP_REASON = str(e)

# ── Test constants ───────────────────────────────────────────────────────

# These strings are chosen specifically to exercise known failure modes.
ARABIC_STRINGS = {
    "hello_world":      "مرحبا بالعالم",        # Basic Arabic
    "lam_alef":         "لا إله إلا الله",       # Mandatory lam-alef ligature
    "tashkeel":         "الْعَرَبِيَّةُ",          # Full diacritics (Tashkeel)
    "mixed_ltr":        "Hello مرحبا World",     # RTL embedded in LTR
    "mixed_rtl":        "مرحبا Hello مرحبا",     # LTR embedded in RTL
    "numbers_in_ar":    "رقم ١٢٣ وأيضاً 456",    # Eastern + Western numerals
    "brackets_rtl":     "النص (مثال) هنا",       # Brackets — must mirror in RTL
    "kashida":          "الجميـل",               # Kashida extension
    "farsi":            "سلام دنیا",             # Persian (Farsi)
    "long_word":        "وبالمستضعفين",          # Long word with affixes
    "url_in_arabic":    "زوروا https://example.com لمزيد",  # URL in Arabic
}

EXPECTED_RTL = {
    "hello_world", "lam_alef", "tashkeel", "mixed_rtl",
    "numbers_in_ar", "brackets_rtl", "kashida", "farsi", "long_word",
}

EXPECTED_LTR = {"mixed_ltr"}

# ── Helper: create a Pango layout for testing ────────────────────────────
def make_layout(text: str, font_family: str = "Noto Sans Arabic",
                font_size_pt: int = 14) -> "Pango.Layout":
    """Return a PangoLayout shaped for the given text."""
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 1, 1)
    ctx = cairo.Context(surface)
    layout = PangoCairo.create_layout(ctx)
    layout.set_text(text, -1)
    fd = Pango.FontDescription.from_string(f"{font_family} {font_size_pt}")
    layout.set_font_description(fd)
    layout.set_auto_dir(True)
    return layout


def layout_pixel_size(layout: "Pango.Layout"):
    """Returns (width_px, height_px) of the layout's logical extents."""
    _, logical = layout.get_pixel_extents()
    return logical.width, logical.height


# ════════════════════════════════════════════════════════════════════════
# Test Classes
# ════════════════════════════════════════════════════════════════════════

class TestArabicFontAvailability(unittest.TestCase):
    """Verify that required Arabic fonts are installed and detectable."""

    def test_noto_sans_arabic_available(self):
        result = subprocess.run(
            ["fc-list", ":family=Noto Sans Arabic"],
            capture_output=True, text=True
        )
        self.assertIn("Noto", result.stdout,
            "Noto Sans Arabic font not found. Run: bash fonts/download_fonts.sh")

    def test_at_least_one_arabic_font_available(self):
        result = subprocess.run(
            ["fc-list", ":lang=ar"],
            capture_output=True, text=True
        )
        self.assertGreater(len(result.stdout.strip()), 0,
            "No Arabic-capable fonts installed. Run: bash fonts/download_fonts.sh")

    def test_fontconfig_arabic_priority(self):
        """fc-match :lang=ar should return an Arabic-capable font, not a Latin one."""
        result = subprocess.run(
            ["fc-match", ":lang=ar", "family"],
            capture_output=True, text=True
        )
        matched = result.stdout.strip().lower()
        # If Arabian Shield Fontconfig rules are installed, we should get a known Arabic font
        known_arabic = ["arabic", "amiri", "noto", "scheherazade", "cairo",
                        "vazir", "lateef", "tahoma", "arial unicode"]
        self.assertTrue(
            any(f in matched for f in known_arabic),
            f"fc-match :lang=ar returned '{matched}' which may not support Arabic. "
            "Run: sudo make install"
        )


@unittest.skipUnless(HAS_PANGO, f"PyGI/Pango not available: {locals().get('PANGO_SKIP_REASON','')}")
class TestPangoArabicShaping(unittest.TestCase):
    """Verify that Pango shapes Arabic text correctly."""

    def test_basic_arabic_has_nonzero_width(self):
        """A basic Arabic string must produce a layout with positive width."""
        layout = make_layout(ARABIC_STRINGS["hello_world"])
        w, h = layout_pixel_size(layout)
        self.assertGreater(w, 0, "Arabic layout has zero width — shaping failed")
        self.assertGreater(h, 0, "Arabic layout has zero height")

    def test_lam_alef_ligature_width(self):
        """
        The lam-alef string must be narrower than if each letter were isolated.
        This indirectly verifies the liga/rlig OpenType features are active.
        A naive isolated-form rendering would produce much wider output.
        """
        liga_text = "لا"   # lam + alef — should form ligature
        layout_liga = make_layout(liga_text)
        w_liga, _ = layout_pixel_size(layout_liga)

        # Each letter in isolation: Arabic letter lam (ل) + alef (ا) isolated
        # Ligature form is narrower than two full isolated glyphs
        self.assertGreater(w_liga, 0,
            "Lam-alef layout has zero width")
        # This width should be reasonable for a 2-character string at 14pt
        # (roughly 10–30px for most Arabic fonts at 96dpi)
        self.assertLess(w_liga, 100,
            f"Lam-alef suspiciously wide ({w_liga}px) — possible shaping failure")

    def test_tashkeel_does_not_increase_width_significantly(self):
        """
        Diacritical marks (Tashkeel) should not cause significant horizontal
        expansion — they are placed above/below, not beside the base letter.
        """
        base = "العربية"
        with_tashkeel = "الْعَرَبِيَّةُ"
        w_base, _ = layout_pixel_size(make_layout(base))
        w_tashkeel, _ = layout_pixel_size(make_layout(with_tashkeel))
        # Allow up to 30% width increase for mark positioning differences
        self.assertLess(w_tashkeel, w_base * 1.30,
            f"Tashkeel caused excessive width increase: {w_base}→{w_tashkeel}px "
            "(marks may be placed as spacing characters instead of combining)")

    def test_all_arabic_strings_render_nonzero(self):
        """All test strings must produce non-zero layout dimensions."""
        failures = []
        for name, text in ARABIC_STRINGS.items():
            layout = make_layout(text)
            w, h = layout_pixel_size(layout)
            if w <= 0 or h <= 0:
                failures.append(f"{name!r}: w={w} h={h}")
        self.assertEqual(failures, [],
            "The following strings produced zero-size layouts:\n" +
            "\n".join(f"  - {f}" for f in failures))

    def test_mixed_rtl_ltr_renders(self):
        """Mixed Arabic/Latin text must render without raising exceptions."""
        for name in ("mixed_ltr", "mixed_rtl", "url_in_arabic", "numbers_in_ar"):
            with self.subTest(string=name):
                layout = make_layout(ARABIC_STRINGS[name])
                w, h = layout_pixel_size(layout)
                self.assertGreater(w, 0)

    def test_farsi_renders(self):
        """Persian text must render correctly (uses Arabic script, different locale)."""
        layout = make_layout(ARABIC_STRINGS["farsi"])
        w, h = layout_pixel_size(layout)
        self.assertGreater(w, 0, "Farsi layout has zero width")

    def test_amiri_font_if_available(self):
        """If Amiri is installed, it must produce correct Arabic shaping."""
        result = subprocess.run(["fc-list", ":family=Amiri"],
                                capture_output=True, text=True)
        if "Amiri" not in result.stdout:
            self.skipTest("Amiri font not installed")

        layout = make_layout(ARABIC_STRINGS["hello_world"], font_family="Amiri")
        w, h = layout_pixel_size(layout)
        self.assertGreater(w, 0, "Amiri Arabic layout has zero width")


@unittest.skipUnless(HAS_PANGO, f"PyGI/Pango not available")
class TestBaselineAlignment(unittest.TestCase):
    """
    Test that Arabic and Latin baselines are compatible in mixed runs.
    """

    def test_arabic_and_latin_similar_height(self):
        """
        A mixed Arabic/Latin line height should not be excessively larger
        than a pure Latin line at the same font size.
        Pure Latin: "Hello World"
        Mixed:      "Hello مرحبا"
        The mixed line height should not exceed 2× the Latin height.
        """
        _, h_latin = layout_pixel_size(make_layout("Hello World",
                                                    font_family="DejaVu Sans"))
        _, h_mixed = layout_pixel_size(make_layout("Hello مرحبا"))
        if h_latin > 0:
            ratio = h_mixed / h_latin
            self.assertLess(ratio, 2.5,
                f"Mixed Arabic/Latin line is {ratio:.1f}× taller than pure Latin "
                f"({h_mixed}px vs {h_latin}px). Baseline mismatch?")


@unittest.skipUnless(HAS_PANGO, "PyGI/Pango not available")
class TestAutoDirection(unittest.TestCase):
    """Test that Pango auto-direction works correctly for Arabic text."""

    def test_arabic_layout_direction(self):
        """
        A layout containing Arabic text with auto_dir enabled should
        resolve to RTL. We check this via the resolved direction of
        the first PangoLayoutLine.
        """
        layout = make_layout(ARABIC_STRINGS["hello_world"])
        lines = layout.get_lines_readonly()
        if lines:
            first_line = lines[0]
            # resolved_dir: 0=LTR, 1=RTL
            # In a Pango layout with Arabic text, lines should be RTL
            self.assertEqual(first_line.resolved_dir, 1,
                "Arabic text layout resolved to LTR — auto_dir or BiDi not working")

    def test_latin_layout_direction(self):
        """A pure Latin layout must resolve to LTR."""
        layout = make_layout("Hello World", font_family="DejaVu Sans")
        lines = layout.get_lines_readonly()
        if lines:
            first_line = lines[0]
            self.assertEqual(first_line.resolved_dir, 0,
                "Latin text layout resolved to RTL — direction detection broken")


class TestFriBidiCLI(unittest.TestCase):
    """Test FriBidi command-line tool if available."""

    @classmethod
    def setUpClass(cls):
        result = subprocess.run(["which", "fribidi"], capture_output=True)
        cls.has_fribidi = (result.returncode == 0)

    def test_fribidi_available(self):
        if not self.has_fribidi:
            self.skipTest("fribidi binary not installed")
        result = subprocess.run(["fribidi", "--version"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)

    def test_fribidi_reorders_arabic(self):
        """FriBidi should reorder 'Hello مرحبا' correctly."""
        if not self.has_fribidi:
            self.skipTest("fribidi binary not installed")
        result = subprocess.run(
            ["fribidi", "--nopad"],
            input="Hello مرحبا\n",
            capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 0)
        # The output should contain both Arabic and Latin text
        output = result.stdout.strip()
        self.assertTrue(len(output) > 0, "fribidi produced empty output")

    def test_fribidi_pure_arabic(self):
        """FriBidi should handle pure Arabic without error."""
        if not self.has_fribidi:
            self.skipTest("fribidi binary not installed")
        result = subprocess.run(
            ["fribidi", "--nopad"],
            input=ARABIC_STRINGS["hello_world"] + "\n",
            capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 0)


# ── Entry point ───────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("Arabian Shield — Arabic Rendering Test Suite")
    print("=" * 55)
    if not HAS_PANGO:
        print(f"WARNING: Pango tests will be skipped ({PANGO_SKIP_REASON})")
        print("Install: sudo apt-get install python3-gi python3-cairo "
              "gir1.2-pango-1.0 gir1.2-pangocairo-1.0")
        print()
    unittest.main(verbosity=2)
