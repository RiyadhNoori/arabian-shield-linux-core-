# API Reference — Arabian Shield Linux Core

Complete reference for the `arabic_shield.h` C API.

---

## Initialization

### `arabic_shield_init()`

```c
ArabicShieldStatus arabic_shield_init(uint32_t flags);
```

Initialize the Arabian Shield library. Must be called once before any other
function. Safe (no-op) to call multiple times.

**Not thread-safe.** Call from the main thread before spawning workers.

**Parameters:**

| Flag | Value | Effect |
|------|-------|--------|
| `ARABIC_SHIELD_FLAG_NONE` | `0x0000` | Minimal safe initialization |
| `ARABIC_SHIELD_FLAG_AUTO_BIDI` | `0x0001` | Automatic BiDi paragraph detection |
| `ARABIC_SHIELD_FLAG_FIX_BASELINE` | `0x0002` | Enable baseline normalization |
| `ARABIC_SHIELD_FLAG_APPLY_FONTCONFIG` | `0x0004` | Load Arabian Shield Fontconfig rules |
| `ARABIC_SHIELD_FLAG_DEBUG_LOG` | `0x0008` | Verbose stderr logging |
| `ARABIC_SHIELD_FLAG_RECOMMENDED` | `0x0007` | All production flags combined |

**Returns:** `ARABIC_SHIELD_OK` on success. See [Error Codes](#error-codes).

**Example:**
```c
ArabicShieldStatus st = arabic_shield_init(ARABIC_SHIELD_FLAG_RECOMMENDED);
if (st != ARABIC_SHIELD_OK) {
    fprintf(stderr, "Arabian Shield init failed: %s\n",
            arabic_shield_status_string(st));
    // Non-fatal: app can continue without Arabian Shield
}
```

---

### `arabic_shield_cleanup()`

```c
void arabic_shield_cleanup(void);
```

Release all resources. Call on application exit.

---

## Version

### `arabic_shield_version()`

```c
const char *arabic_shield_version(void);
```

Returns a static string like `"0.1.0"`. Thread-safe. Never free the return value.

### `arabic_shield_version_check()`

```c
int arabic_shield_version_check(int major, int minor, int patch);
```

Returns non-zero if the runtime version is >= (major, minor, patch).

```c
if (!arabic_shield_version_check(0, 1, 0)) {
    fprintf(stderr, "Arabian Shield >= 0.1.0 required\n");
}
```

---

## Bidirectional Text

### `arabic_shield_bidi_reorder()`

```c
char *arabic_shield_bidi_reorder(
    const char            *utf8_input,
    ssize_t                input_len,
    ArabicShieldDirection  base_direction,
    size_t                *output_len
);
```

Apply the Unicode Bidirectional Algorithm (UAX #9) to a UTF-8 string,
returning the text reordered for visual display.

**Parameters:**

- `utf8_input` — Input string in logical (storage) order, UTF-8 encoded.
- `input_len` — Byte length of input. Pass `-1` to use `strlen()`.
- `base_direction` — `ARABIC_SHIELD_DIR_AUTO`, `_LTR`, or `_RTL`.
  Use `AUTO` for normal text; use explicit direction when you know the paragraph
  direction from context (e.g., a UI element tagged with `lang="ar"`).
- `output_len` — If non-NULL, set to the byte length of the returned string.

**Returns:** Newly allocated UTF-8 string in visual order.
**Must be freed** with `arabic_shield_free()`.
Returns `NULL` on error.

**Example:**
```c
size_t out_len;
char *visual = arabic_shield_bidi_reorder(
    "Hello مرحبا World",
    -1,
    ARABIC_SHIELD_DIR_AUTO,
    &out_len
);
if (visual) {
    render_text(visual);
    arabic_shield_free(visual);
}
```

---

### `arabic_shield_detect_direction()`

```c
ArabicShieldDirection arabic_shield_detect_direction(
    const char *utf8_input,
    ssize_t     input_len
);
```

Detect the dominant script direction of a UTF-8 string using the first-strong-
character rule (UAX #9 P2/P3).

**Returns:**
- `ARABIC_SHIELD_DIR_RTL` if Arabic, Hebrew, or other RTL characters dominate.
- `ARABIC_SHIELD_DIR_LTR` otherwise (including for empty strings).

**Example — set widget alignment based on content:**
```c
ArabicShieldDirection dir = arabic_shield_detect_direction(text, -1);
if (dir == ARABIC_SHIELD_DIR_RTL) {
    gtk_widget_set_direction(widget, GTK_TEXT_DIR_RTL);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);  // right-align
}
```

---

## Font Utilities

### `arabic_shield_font_has_arabic()`

```c
bool arabic_shield_font_has_arabic(const char *family_name);
```

Probe whether a font family has adequate Arabic coverage (≥90% of U+0600–U+06FF
with required OpenType features).

Requires `arabic_shield_init()` to have been called.

**Example:**
```c
if (!arabic_shield_font_has_arabic("DejaVu Sans")) {
    fprintf(stderr, "DejaVu Sans lacks Arabic — using fallback\n");
}
```

---

### `arabic_shield_best_arabic_font()`

```c
char *arabic_shield_best_arabic_font(int style);
```

Query Fontconfig (with Arabian Shield rules) for the best available Arabic font.

**Parameters:**
- `style`: `0` = sans-serif, `1` = serif, `2` = monospace

**Returns:** Heap-allocated font family name string.
**Must be freed** with `arabic_shield_free()`.
Returns `NULL` if no Arabic font is available.

**Example:**
```c
char *font = arabic_shield_best_arabic_font(0);  // sans-serif
if (font) {
    printf("Best Arabic font: %s\n", font);  // e.g., "Noto Sans Arabic"
    arabic_shield_free(font);
}
```

---

## Baseline Adjustment

### `arabic_shield_baseline_offset()`

```c
double arabic_shield_baseline_offset(
    const char *arabic_family,
    const char *latin_family,
    double      pixel_size
);
```

Compute the pixel offset to align an Arabic font's baseline with a Latin font's
baseline at a given size. Uses FreeType OS/2 table metrics.

**Returns:** Vertical offset in pixels.
- Positive = Arabic should be shifted **up**
- Negative = Arabic should be shifted **down**
- `0.0` if fonts cannot be loaded

**Example — apply to a Pango layout:**
```c
#ifdef ARABIC_SHIELD_ENABLE_PANGO
double offset = arabic_shield_baseline_offset(
    "Noto Sans Arabic", "DejaVu Sans", 14.0
);
PangoAttrList *attrs = pango_attr_list_new();
// Apply offset to Arabic run (bytes 6–15 in this example)
arabic_shield_apply_baseline_attr(attrs, offset, 6, 15);
pango_layout_set_attributes(layout, attrs);
pango_attr_list_unref(attrs);
#endif
```

---

## Pango Integration (optional)

These functions are available when compiled with `-DARABIC_SHIELD_ENABLE_PANGO`
and linked against Pango.

### `arabic_shield_configure_pango_context()`

```c
void arabic_shield_configure_pango_context(
    PangoContext *context,
    const char   *font_family,
    int           font_size
);
```

Configure an existing `PangoContext` for correct Arabic rendering:
- Sets language to Arabic
- Sets base direction to RTL
- Sets gravity to AUTO (HarfBuzz decides glyph orientation)
- Applies the specified font description

**Parameters:**
- `font_family` — Font family name, or `NULL` to keep existing font.
- `font_size` — Size in Pango units (`PANGO_SCALE * pt`), or `0` to keep existing.

---

### `arabic_shield_fix_pango_layout()`

```c
void arabic_shield_fix_pango_layout(PangoLayout *layout);
```

Apply Arabian Shield corrections to an existing `PangoLayout`:
- Enables auto-direction detection
- Sets `PANGO_ALIGN_RIGHT` for RTL content
- Sets `PANGO_ELLIPSIZE_START` (correct for RTL)
- Sets `PANGO_WRAP_WORD_CHAR`

Call this after creating the layout and before the first `pango_cairo_show_layout()`.

---

## Memory Management

### `arabic_shield_free()`

```c
void arabic_shield_free(void *ptr);
```

Free a pointer returned by any `arabic_shield_*` function.
Always use this instead of `free()` directly.

---

## Error Codes

```c
typedef enum ArabicShieldStatus {
    ARABIC_SHIELD_OK                  =  0,
    ARABIC_SHIELD_ERR_ALREADY_INIT    =  1,  // Safe to ignore
    ARABIC_SHIELD_ERR_PANGO_MISSING   = -1,
    ARABIC_SHIELD_ERR_FRIBIDI_MISSING = -2,
    ARABIC_SHIELD_ERR_HARFBUZZ_MISSING= -3,
    ARABIC_SHIELD_ERR_FONTCONFIG_FAIL = -4,
    ARABIC_SHIELD_ERR_NO_ARABIC_FONT  = -5,
    ARABIC_SHIELD_ERR_OUT_OF_MEMORY   = -6,
    ARABIC_SHIELD_ERR_INVALID_UTF8    = -7,
    ARABIC_SHIELD_ERR_NULL_ARG        = -8,
} ArabicShieldStatus;
```

### `arabic_shield_status_string()`

```c
const char *arabic_shield_status_string(ArabicShieldStatus status);
```

Returns a human-readable string. Thread-safe. Never free the return value.

---

## Complete Initialization Example

```c
#include <arabic_shield.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Initialize — call before any rendering
    ArabicShieldStatus st = arabic_shield_init(
        ARABIC_SHIELD_FLAG_RECOMMENDED |
        ARABIC_SHIELD_FLAG_DEBUG_LOG    // remove in production
    );

    if (st < 0) {
        fprintf(stderr, "Warning: Arabian Shield failed: %s\n",
                arabic_shield_status_string(st));
        // Non-fatal: continue without Arabic fixes
    }

    // Detect direction of user input
    const char *text = "مرحبا World";
    ArabicShieldDirection dir = arabic_shield_detect_direction(text, -1);
    printf("Direction: %s\n", dir == ARABIC_SHIELD_DIR_RTL ? "RTL" : "LTR");

    // Reorder for visual display
    size_t out_len;
    char *visual = arabic_shield_bidi_reorder(text, -1,
                                              ARABIC_SHIELD_DIR_AUTO,
                                              &out_len);
    if (visual) {
        printf("Visual order: %s (%zu bytes)\n", visual, out_len);
        arabic_shield_free(visual);
    }

    // Get best Arabic font
    char *font = arabic_shield_best_arabic_font(0);
    if (font) {
        printf("Best Arabic font: %s\n", font);
        arabic_shield_free(font);
    }

    arabic_shield_cleanup();
    return 0;
}
```

Compile:
```bash
gcc example.c $(pkg-config --cflags --libs arabic-shield) -o example
```
