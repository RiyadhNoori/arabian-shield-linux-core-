# Arabic Typography Guide for Linux Developers

A practical reference for developers building applications that handle Arabic text on Linux.

---

## Arabic Script Fundamentals

### The Alphabet

Arabic has 28 letters. Every letter is **connected** — Arabic has no equivalent of printed Latin uppercase vs lowercase; instead, every letter has up to four contextual forms:

| Form | When used | Example |
|------|-----------|---------|
| **Isolated** (منفصل) | Letter stands alone | ب |
| **Initial** (أول) | At the start of a word | بـ |
| **Medial** (وسط) | In the middle of a word | ـبـ |
| **Final** (آخر) | At the end of a word | ـب |

**Why this matters for developers:** If your text shaping engine doesn't invoke the
`init`, `medi`, `fina`, and `isol` OpenType features, Arabic letters will all
render in isolated form — the text looks like a list of disconnected glyphs rather
than flowing script. This is one of the most common Arabic rendering bugs on Linux.

### Non-Joining Letters

Six letters only connect to the right (they have initial and isolated forms but
no medial form): ا، د، ذ، ر، ز، و (alef, dal, dhal, ra, zay, waw).
A word must break connection after these letters.

### Required Ligatures (الحروف المركبة الإلزامية)

Some letter combinations **must** be rendered as merged glyphs. The most critical:

| Combination | Result | Codepoints |
|-------------|--------|------------|
| lam + alef | لا | U+0644 + U+0627 |
| lam + alef with hamza above | لأ | U+0644 + U+0623 |
| lam + alef with hamza below | لإ | U+0644 + U+0625 |
| lam + alef with madda | لآ | U+0644 + U+0622 |

The `rlig` (required ligatures) OpenType feature enables these. It cannot be
disabled per the OpenType specification.

---

## Diacritics (التشكيل — Tashkeel)

Arabic diacritics are **combining characters** placed above or below base letters.
They indicate vowelization (short vowels are not written in normal text).

| Mark | Unicode | Name | Placed |
|------|---------|------|--------|
| َ | U+064E | Fatha (a) | Above |
| ِ | U+0650 | Kasra (i) | Below |
| ُ | U+064F | Damma (u) | Above |
| ْ | U+0652 | Sukun (no vowel) | Above |
| ّ | U+0651 | Shadda (gemination) | Above |
| ً | U+064B | Tanwin fatha | Above |
| ٍ | U+064D | Tanwin kasra | Below |
| ٌ | U+064C | Tanwin damma | Above |

**Developer implications:**
- The `mark` OT feature places marks relative to their base glyphs.
- The `mkmk` OT feature handles stacked marks (e.g., shadda + fatha: **ّ** + **َ**).
- If these features are not enabled, marks will be displaced or appear at the
  wrong position — a common GTK bug on unpatched systems.
- String length counting: `len("ب")` = 1 but `len("بَ")` = 2 (base + diacritic).
  Unicode-aware code must handle this correctly.

---

## Kashida (الكشيدة)

Kashida is a horizontal stroke that extends words for justification. Unlike
Latin word-spacing which adds spaces between words, Arabic justification
stretches letters using Kashida strokes.

Unicode Kashida character: U+0640 (ARABIC TATWEEL)

**OT feature:** `calt` (contextual alternates) enables Kashida-aware justification.

**Developer note:** Never add `\u0640` characters yourself for justification —
let the font and Pango handle it via `calt`. Manual Kashida insertion breaks
copy-paste and search.

---

## Numerals in Arabic Text

Arabic-speaking countries use two numeral systems:

| System | Example | Unicode Block |
|--------|---------|---------------|
| Western Arabic (ASCII) | 0123456789 | U+0030–U+0039 |
| Eastern Arabic | ٠١٢٣٤٥٦٧٨٩ | U+0660–U+0669 |
| Persian digits | ۰۱۲۳۴۵۶۷۸۹ | U+06F0–U+06F9 |

**BiDi behavior:** Digits are "weak" characters in the Unicode BiDi algorithm.
Sequences of digits take the direction of the surrounding text. This means:
- In an RTL context: `رقم 123` — the "123" appears to the LEFT of "رقم"
- In an LTR context: `رقم 123` — the "123" appears to the RIGHT of "رقم"

This is correct behavior. Do NOT try to override it.

---

## The Unicode Bidirectional Algorithm (UAX #9)

The BiDi algorithm reorders text for visual display. Key concepts:

### Paragraph Base Direction

Each paragraph has a base direction (RTL or LTR). Auto-detection uses the
**first strong character** rule (paragraph direction = direction of first
Arabic/Hebrew or Latin letter).

**Developer tip:** For Arabic-primary UIs, always set base direction explicitly:
```c
// GTK/Pango
pango_layout_set_auto_dir(layout, TRUE);  // let Pango auto-detect
// or
pango_context_set_base_dir(ctx, PANGO_DIRECTION_RTL);  // explicit
```

```css
/* CSS */
[lang="ar"] { direction: rtl; unicode-bidi: embed; }
```

### Directional Embedding Characters

| Character | Name | Unicode | Effect |
|-----------|------|---------|--------|
| RLM | Right-to-Left Mark | U+200F | Forces RTL direction at insertion point |
| LRM | Left-to-Right Mark | U+200E | Forces LTR direction at insertion point |
| RLE | RTL Embedding | U+202B | Begin RTL embedding |
| LRE | LTR Embedding | U+202A | Begin LTR embedding |
| PDF | Pop Directional Format | U+202C | End embedding |
| RLI | RTL Isolate | U+2067 | Isolated RTL (preferred over RLE) |
| LRI | LTR Isolate | U+2066 | Isolated LTR (preferred over LRE) |
| PDI | Pop Directional Isolate | U+2069 | End isolate |

**Modern best practice:** Use isolate marks (RLI/LRI/PDI) instead of embedding
marks (RLE/LRE/PDF). Isolate marks prevent the surrounding text from being
affected by the directional change.

### Mirror Characters

In RTL text, paired characters are mirrored:
- `(` becomes `)` and vice versa
- `<` becomes `>`
- `[` becomes `]`

The FriBidi `fribidi_get_mirror_char()` function handles this.
Arabian Shield's `arabic_shield_apply_mirroring()` applies it correctly.

---

## Font Selection Best Practices

### Font Priority Recommendation

```
Primary (UI/body text):     Noto Sans Arabic
Primary (serif/body text):  Noto Naskh Arabic or Amiri
Fallback (any style):       Scheherazade New
UI at small sizes (<12px):  Cairo or Noto Sans Arabic
Persian/Farsi:              Vazirmatn → Noto Sans Arabic
Urdu:                       Noto Nastaliq Urdu → Noto Sans Arabic
```

### Never Use These for Arabic

- DejaVu Sans (partial/broken Arabic coverage)
- Liberation Sans/Serif (no Arabic coverage)
- Bitstream Vera (no Arabic coverage)
- Ubuntu font (limited Arabic coverage, inconsistent metrics)

### Monospace Arabic

True Arabic monospace fonts do not exist (Arabic is inherently variable-width).
For terminals displaying Arabic, use:
- Noto Sans Arabic (proportional, acceptable in terminals)
- A terminal that handles Arabic shaping (Kitty, Alacritty with ligature support)

---

## Common Bugs and Fixes

| Bug | Symptom | Root Cause | Fix |
|-----|---------|------------|-----|
| Isolated forms | Letters not connected | init/medi/fina OT features disabled | Enable via HarfBuzz feature list |
| Missing lam-alef | لا appears as two separate glyphs | rlig/liga disabled | Enable rlig feature |
| Floating diacritics | Tashkeel misaligned | mark/mkmk disabled | Enable mark features |
| Baseline mismatch | Arabic floats in mixed lines | Different font metrics | Use BaselineAdjuster |
| Backwards brackets | ) appears where ( should be | BiDi mirroring skipped | Apply fribidi_get_mirror_char |
| Wrong number position | Numerals on wrong side | BiDi applied per-char not per-paragraph | Apply BiDi per paragraph |
| Boxes (□□□) | Arabic shows as empty squares | No Arabic font installed/selected | Install Noto Sans Arabic |
| Broken copy-paste | Pasted text reorders incorrectly | Missing directional marks | Wrap with RLI...PDI on copy |

---

## Testing Checklist

When building an Arabic-capable application, test each of these cases:

- [ ] Basic Arabic text renders with connected letterforms
- [ ] lam-alef ligature appears as one glyph
- [ ] Tashkeel (diacritics) are positioned above/below their base letters
- [ ] Mixed Arabic/Latin text: Latin appears on the correct side
- [ ] Numbers in Arabic text appear in correct position
- [ ] Brackets/parentheses are mirrored correctly in RTL context
- [ ] Text entry field accepts Arabic input from right
- [ ] Cursor movement in Arabic text moves right-to-left visually
- [ ] Line breaks in Arabic text occur at word boundaries
- [ ] Scroll direction in RTL text view is natural (right-anchored)
- [ ] Tooltips and labels in RTL dialogs are right-aligned
- [ ] Copy-pasted Arabic text retains correct direction in other apps
