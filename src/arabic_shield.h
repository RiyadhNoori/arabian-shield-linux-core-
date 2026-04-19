/**
 * @file arabic_shield.h
 * @brief Arabian Shield Linux Core — Public C API
 *
 * Arabian Shield provides OS-level middleware for correct Arabic font
 * rendering and RTL text shaping on Linux. Include this header and
 * call arabic_shield_init() before any text rendering in your application.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#ifndef ARABIC_SHIELD_H
#define ARABIC_SHIELD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ─────────────────────────────────────────────────────────────────────── */

#define ARABIC_SHIELD_VERSION_MAJOR 0
#define ARABIC_SHIELD_VERSION_MINOR 1
#define ARABIC_SHIELD_VERSION_PATCH 0

/** Runtime version string, e.g. "0.1.0" */
const char *arabic_shield_version(void);

/** Returns non-zero if runtime version >= (major, minor, patch) */
int arabic_shield_version_check(int major, int minor, int patch);


/* ─────────────────────────────────────────────────────────────────────────
 * Initialization Flags
 * ─────────────────────────────────────────────────────────────────────── */

/** No special flags — minimal safe initialization */
#define ARABIC_SHIELD_FLAG_NONE             0x0000U

/**
 * Automatically detect bidirectional paragraphs and apply Unicode
 * BiDi algorithm (UAX #9) via FriBidi. Affects all text shaping calls
 * that go through the Arabian Shield wrapper.
 */
#define ARABIC_SHIELD_FLAG_AUTO_BIDI        0x0001U

/**
 * Normalize baselines for mixed Arabic/Latin runs. Without this flag,
 * Arabic glyphs may float above the Latin baseline in mixed text.
 */
#define ARABIC_SHIELD_FLAG_FIX_BASELINE     0x0002U

/**
 * Apply Fontconfig overrides from the Arabian Shield configuration file
 * (/etc/fonts/conf.d/99-arabic-shield.conf). Requires the config file
 * to be installed. Fails gracefully if not present.
 */
#define ARABIC_SHIELD_FLAG_APPLY_FONTCONFIG 0x0004U

/**
 * Enable verbose debug logging to stderr. Useful during development.
 * Should NOT be set in production builds.
 */
#define ARABIC_SHIELD_FLAG_DEBUG_LOG        0x0008U

/**
 * Convenience: enable all recommended flags for production use.
 */
#define ARABIC_SHIELD_FLAG_RECOMMENDED \
    (ARABIC_SHIELD_FLAG_AUTO_BIDI | \
     ARABIC_SHIELD_FLAG_FIX_BASELINE | \
     ARABIC_SHIELD_FLAG_APPLY_FONTCONFIG)


/* ─────────────────────────────────────────────────────────────────────────
 * Error Codes
 * ─────────────────────────────────────────────────────────────────────── */

typedef enum ArabicShieldStatus {
    ARABIC_SHIELD_OK                  =  0,  /**< Success */
    ARABIC_SHIELD_ERR_ALREADY_INIT    =  1,  /**< init() called twice; safe to ignore */
    ARABIC_SHIELD_ERR_PANGO_MISSING   = -1,  /**< Pango library not found */
    ARABIC_SHIELD_ERR_FRIBIDI_MISSING = -2,  /**< FriBidi library not found */
    ARABIC_SHIELD_ERR_HARFBUZZ_MISSING= -3,  /**< HarfBuzz library not found */
    ARABIC_SHIELD_ERR_FONTCONFIG_FAIL = -4,  /**< Fontconfig initialization failed */
    ARABIC_SHIELD_ERR_NO_ARABIC_FONT  = -5,  /**< No Arabic-capable font found */
    ARABIC_SHIELD_ERR_OUT_OF_MEMORY   = -6,  /**< Memory allocation failure */
    ARABIC_SHIELD_ERR_INVALID_UTF8    = -7,  /**< Input string is not valid UTF-8 */
    ARABIC_SHIELD_ERR_NULL_ARG        = -8,  /**< Required argument was NULL */
} ArabicShieldStatus;

/** Returns a human-readable string for a status code. Thread-safe. */
const char *arabic_shield_status_string(ArabicShieldStatus status);


/* ─────────────────────────────────────────────────────────────────────────
 * Core Lifecycle
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize the Arabian Shield library.
 *
 * Must be called once before any other arabic_shield_* function.
 * It is safe (and a no-op) to call this multiple times; the second
 * call returns ARABIC_SHIELD_ERR_ALREADY_INIT.
 *
 * This function is NOT thread-safe. Call it from the main thread
 * before spawning worker threads that use the library.
 *
 * @param flags  Bitwise OR of ARABIC_SHIELD_FLAG_* constants.
 * @return       ARABIC_SHIELD_OK on success, negative on failure.
 */
ArabicShieldStatus arabic_shield_init(uint32_t flags);

/**
 * @brief Release all resources held by Arabian Shield.
 *
 * Call this when your application exits. After this call, no other
 * arabic_shield_* function may be called (except arabic_shield_init).
 */
void arabic_shield_cleanup(void);


/* ─────────────────────────────────────────────────────────────────────────
 * BiDi / RTL Text Processing
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Base direction hint for arabic_shield_bidi_reorder.
 */
typedef enum ArabicShieldDirection {
    ARABIC_SHIELD_DIR_AUTO = 0,  /**< Auto-detect from first strong character */
    ARABIC_SHIELD_DIR_LTR  = 1,  /**< Force left-to-right base direction */
    ARABIC_SHIELD_DIR_RTL  = 2,  /**< Force right-to-left base direction */
} ArabicShieldDirection;

/**
 * @brief Apply the Unicode Bidirectional Algorithm to a UTF-8 string.
 *
 * Reorders the logical string for visual rendering. The caller is
 * responsible for freeing the returned string with arabic_shield_free().
 *
 * @param utf8_input     Input string in logical (storage) order, UTF-8.
 * @param input_len      Byte length of input, or -1 to use strlen().
 * @param base_direction Paragraph base direction.
 * @param[out] output_len  Set to the byte length of the returned string.
 * @return  Newly allocated UTF-8 string in visual order, or NULL on error.
 *          The caller must free this with arabic_shield_free().
 */
char *arabic_shield_bidi_reorder(
    const char            *utf8_input,
    ssize_t                input_len,
    ArabicShieldDirection  base_direction,
    size_t                *output_len
);

/**
 * @brief Detect the dominant script direction in a UTF-8 string.
 *
 * Scans the string for strong BiDi characters and returns the
 * dominant direction. Useful for setting paragraph alignment.
 *
 * @param utf8_input  UTF-8 encoded string.
 * @param input_len   Byte length, or -1 for strlen().
 * @return ARABIC_SHIELD_DIR_RTL if Arabic/Hebrew characters dominate,
 *         ARABIC_SHIELD_DIR_LTR otherwise.
 */
ArabicShieldDirection arabic_shield_detect_direction(
    const char *utf8_input,
    ssize_t     input_len
);


/* ─────────────────────────────────────────────────────────────────────────
 * Font & Glyph Utilities
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Probe whether a given font family has adequate Arabic coverage.
 *
 * "Adequate" means the font contains glyphs for at least 90% of the
 * Arabic Unicode block (U+0600–U+06FF) and supports required OpenType
 * features: init, medi, fina, liga, calt.
 *
 * @param family_name  Font family name (e.g., "Noto Sans Arabic").
 * @return  true if the font is adequate for Arabic text rendering.
 */
bool arabic_shield_font_has_arabic(const char *family_name);

/**
 * @brief Return the best available Arabic font on this system.
 *
 * Queries Fontconfig with Arabian Shield's priority rules. The returned
 * string is owned by the caller and must be freed with arabic_shield_free().
 *
 * @param style  0 = sans-serif, 1 = serif, 2 = monospace
 * @return  Font family name, or NULL if no Arabic font is found.
 */
char *arabic_shield_best_arabic_font(int style);


/* ─────────────────────────────────────────────────────────────────────────
 * Baseline Adjustment
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Compute the baseline offset for an Arabic font relative to a
 *        reference Latin font at the given pixel size.
 *
 * Returns the number of pixels the Arabic run should be shifted vertically
 * to align its baseline with the Latin run. Positive = shift down.
 *
 * @param arabic_family   Arabic font family name.
 * @param latin_family    Latin font family name (the reference baseline).
 * @param pixel_size      Font size in pixels.
 * @return Vertical offset in pixels (may be fractional via subpixel).
 */
double arabic_shield_baseline_offset(
    const char *arabic_family,
    const char *latin_family,
    double      pixel_size
);


/* ─────────────────────────────────────────────────────────────────────────
 * Memory Management
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Free a pointer allocated by any arabic_shield_* function.
 *
 * Always use this instead of free() for strings returned by this library,
 * in case the library uses a custom allocator internally.
 */
void arabic_shield_free(void *ptr);


/* ─────────────────────────────────────────────────────────────────────────
 * Pango Integration (optional — requires Pango at link time)
 * ─────────────────────────────────────────────────────────────────────── */

#ifdef ARABIC_SHIELD_ENABLE_PANGO
#include <pango/pango.h>

/**
 * @brief Configure a PangoContext for correct Arabic rendering.
 *
 * Sets the language, font description, and base gravity on the context
 * to produce correct Arabic glyph selection and layout. Call this after
 * creating the context and before any layout operations.
 *
 * @param context     An existing PangoContext to configure.
 * @param font_family Arabic font family, or NULL to use system default.
 * @param font_size   Font size in Pango units (PANGO_SCALE * points).
 */
void arabic_shield_configure_pango_context(
    PangoContext *context,
    const char   *font_family,
    int           font_size
);

/**
 * @brief Fix a PangoLayout for RTL text.
 *
 * Applies correct base direction, auto-alignment, and single-paragraph
 * mode settings to a PangoLayout containing Arabic text.
 *
 * @param layout  An existing PangoLayout to fix.
 */
void arabic_shield_fix_pango_layout(PangoLayout *layout);

#endif /* ARABIC_SHIELD_ENABLE_PANGO */


#ifdef __cplusplus
}
#endif

#endif /* ARABIC_SHIELD_H */
