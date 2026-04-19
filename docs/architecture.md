# Architecture — Arabian Shield Linux Core

## Overview

Arabian Shield is a **layered middleware library** that intercepts the text
rendering pipeline at multiple points to correct Arabic shaping and RTL layout
issues endemic to Linux systems.

```
┌───────────────────────────────────────────────────────────────────┐
│                        Application Layer                           │
│              (GTK, Qt, SDL2, Electron, terminal emulators)         │
├───────────────────────────────────────────────────────────────────┤
│                   Arabian Shield Middleware                         │
│                                                                    │
│  ┌─────────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  Pango Wrapper  │  │ FriBidi      │  │  Baseline Adjuster   │  │
│  │  (HarfBuzz OT)  │  │ Wrapper      │  │  (FreeType metrics)  │  │
│  │  pango_wrapper.c│  │ fribidi_     │  │  baseline_adjuster.c │  │
│  │                 │  │ wrapper.c    │  │                      │  │
│  └────────┬────────┘  └──────┬───────┘  └──────────┬───────────┘  │
│           │                  │                      │              │
│  ┌────────▼──────────────────▼──────────────────────▼───────────┐  │
│  │                    arabic_shield.c                             │  │
│  │             Core init, font probing, BiDi reorder             │  │
│  └───────────────────────────────────────────────────────────────┘  │
├───────────────────────────────────────────────────────────────────┤
│  System Libraries: HarfBuzz │ FriBidi │ Pango │ FreeType           │
├───────────────────────────────────────────────────────────────────┤
│  Fontconfig (config/fonts.conf → /etc/fonts/conf.d/)               │
├───────────────────────────────────────────────────────────────────┤
│                    Linux Kernel / Display Server                    │
└───────────────────────────────────────────────────────────────────┘
```

---

## Why Arabic Rendering Breaks on Linux

Arabic rendering requires a coordinated pipeline of five distinct operations.
A failure at any stage produces incorrect output.

### Stage 1: Font Selection (Fontconfig)

**Problem:** The default Fontconfig configuration on most Linux distros does not
include a proper Arabic font fallback chain. When an application requests a font
that doesn't cover Arabic (e.g., DejaVu Sans), Fontconfig may return a Latin
font, causing Arabic codepoints to render as replacement boxes (□□□) or fall
through to an unrelated font with wrong metrics.

**Arabian Shield fix:** `config/fonts.conf` installs as `99-arabic-shield.conf`
(highest priority in `/etc/fonts/conf.d/`) and establishes:
- Explicit Arabic font priority: Noto Sans Arabic → Amiri → Scheherazade → Cairo
- Blocks Latin fonts (DejaVu Sans, Liberation Sans) from Arabic codepoint fallback
- Persian/Farsi: Vazirmatn first, then Noto Sans Arabic
- Urdu: Noto Nastaliq Urdu first (Nastaliq script handling)

### Stage 2: Script Itemization (Pango)

**Problem:** Pango's automatic script detection algorithm may misclassify
Arabic characters when they appear adjacent to other scripts, especially
with digits, punctuation, or URL strings embedded in Arabic text. A misclassified
run gets shaped as Latin, producing isolated Arabic letter forms.

**Arabian Shield fix:** `src/pango_wrapper.c` provides
`arabic_shield_configure_pango_context()` which:
- Explicitly sets `PangoLanguage` to `"ar"` on the context
- Sets `PANGO_DIRECTION_RTL` as the base direction
- Configures `PANGO_GRAVITY_AUTO` for correct line orientation

### Stage 3: OpenType Shaping (HarfBuzz)

**Problem:** Arabic requires mandatory OpenType Layout features for correct
glyph selection. Some Pango installations omit features like `calt`
(contextual alternates, needed for glyph variants) or `mark`/`mkmk` (mark
positioning, needed for Tashkeel/diacritics).

The required feature tags for Arabic are:
| Tag | Feature | Why mandatory |
|-----|---------|---------------|
| `init` | Initial forms | First letter in a word |
| `medi` | Medial forms | Middle letters in a word |
| `fina` | Final forms | Last letter in a word |
| `isol` | Isolated forms | Single-letter words |
| `liga` | Standard ligatures | Basic ligatures (lam-alef) |
| `rlig` | Required ligatures | Cannot be disabled per OT spec |
| `calt` | Contextual alternates | Kashida, Quranic variants |
| `mark` | Mark positioning | Tashkeel placement |
| `mkmk` | Mark-to-mark | Stacked diacritics |

**Arabian Shield fix:** `src/pango_wrapper.c` defines `arabic_features[]` with
all required tags explicitly enabled. These are passed to HarfBuzz via the
shaping pipeline.

### Stage 4: Bidirectional Algorithm (FriBidi)

**Problem:** The Unicode Bidirectional Algorithm (UAX #9) is complex and
applications often apply it incorrectly:
- Per-widget instead of per-paragraph (wrong when one widget contains multiple paragraphs)
- Without the mirroring step (brackets render backwards)
- With incorrect base paragraph direction detection

**Arabian Shield fix:** `src/fribidi_wrapper.c` provides:
- `arabic_shield_bidi_reorder()`: Full UAX #9 reordering via FriBidi
- `arabic_shield_apply_mirroring()`: Correct bracket/parenthesis mirroring
- `arabic_shield_wrap_rtl_marks()`: RLE/PDF wrapping for clipboard compatibility
- `arabic_shield_dump_bidi_analysis()`: Debugging tool

### Stage 5: Baseline Normalization (FreeType)

**Problem:** Arabic and Latin fonts have different baseline geometry conventions.
When both scripts appear on the same line (e.g., an Arabic sentence with an
embedded English word), the Arabic glyphs often float above or below the Latin
baseline. This is because font designers choose different descender depths and
ascender heights.

**Arabian Shield fix:** `src/baseline_adjuster.c` uses FreeType to:
1. Load both fonts and read their OS/2 table metrics
2. Compute the delta between their descender depths
3. Return the offset as a pixel value
4. Apply the offset as a `PangoAttrRise` attribute on Arabic runs

---

## Configuration Layer

### Fontconfig (`config/fonts.conf`)

Installed to `/etc/fonts/conf.d/99-arabic-shield.conf`. The `99-` prefix means
this file is processed last, overriding all distro defaults. Key rules:

- **Font priority aliases**: Arabic `sans-serif` → Noto Sans Arabic, Arabic
  `serif` → Amiri, etc.
- **Charset exclusion**: Removes Arabic codepoints from DejaVu Sans charset,
  forcing fallthrough to a proper Arabic font.
- **Rendering hints**: `hintslight` + antialiasing for Arabic glyphs (heavy
  hinting distorts Arabic's organic curves).
- **Size-specific rules**: At <12px, switch from Amiri to Noto for legibility.

### X11 Resources (`config/99-arabic-shield.conf`)

Installed to `/etc/X11/Xresources.d/`. Configures Xft rendering at the X11
level:
- `Xft.hintstyle: hintslight` — prevents over-sharpening
- `Xft.rgba: rgb` — correct subpixel order for most LCD panels
- `Gtk.font-name: Noto Sans Arabic 10` — default UI font

### Locale (`config/locale.conf`)

Not installed as a replacement — merged with the system locale. Sets:
- `GTK_IM_MODULE=ibus` for Arabic text input
- `PANGO_BACKEND=harfbuzz` to force HarfBuzz shaping
- `BIDI_CONTROL=ON` for applications that check this environment variable

---

## Thread Safety

- `arabic_shield_init()`: NOT thread-safe. Call from main thread before spawning workers.
- All other `arabic_shield_*` functions: Thread-safe after `init()` completes.
- `arabic_shield_bidi_reorder()`: Uses per-call stack allocations; safe to call concurrently.

---

## Memory Model

All strings returned by `arabic_shield_*` functions are heap-allocated and
must be freed with `arabic_shield_free()`. Do not use `free()` directly, as
future versions may use a custom allocator.

---

## Design Decisions

**Why a C library, not a Python module?**
The library needs to be linkable into GTK/Qt applications written in C and C++.
Python bindings can be added later via ctypes or a dedicated pybind11 wrapper.

**Why not patch Pango/HarfBuzz upstream?**
Some of these issues are already fixed in newer Pango versions, but the fixes
haven't reached stable distro releases. Arabian Shield provides the fix NOW,
at the application level, without requiring users to upgrade their system.
Upstream patches where appropriate are welcome.

**Why FriBidi instead of ICU BiDi?**
FriBidi is smaller, has no ICU dependencies, and is already a required
dependency of Pango. Adding ICU would double the dependency tree.
