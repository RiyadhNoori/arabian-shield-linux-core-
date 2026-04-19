/**
 * @file fribidi_wrapper.c
 * @brief Arabian Shield — FriBidi Bidirectional Algorithm Wrapper
 *
 * Provides higher-level BiDi operations built on FriBidi:
 *   - Multi-paragraph BiDi reordering
 *   - Paragraph splitting with correct direction per paragraph
 *   - Mirror character substitution (e.g., "(" ↔ ")" in RTL context)
 *   - Explicit directional mark insertion for clipboard compatibility
 *
 * Background on the problem:
 *   Linux applications often apply BiDi reordering at the wrong level
 *   (e.g., per-widget instead of per-paragraph), or they skip the
 *   mirroring step, causing bracket characters to render backwards.
 *   This wrapper applies UAX #9 correctly at paragraph granularity.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include "arabic_shield.h"

#include <fribidi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Arabic Unicode ranges for quick script detection
 * ─────────────────────────────────────────────────────────────────────── */

/** Returns true if the UCS-4 codepoint is in an Arabic Unicode block */
static inline bool is_arabic_codepoint(FriBidiChar cp) {
    return (cp >= 0x0600 && cp <= 0x06FF)   /* Arabic */
        || (cp >= 0x0750 && cp <= 0x077F)   /* Arabic Supplement */
        || (cp >= 0x08A0 && cp <= 0x08FF)   /* Arabic Extended-A */
        || (cp >= 0xFB50 && cp <= 0xFDFF)   /* Arabic Presentation Forms-A */
        || (cp >= 0xFE70 && cp <= 0xFEFF)   /* Arabic Presentation Forms-B */
        || (cp >= 0x10E60 && cp <= 0x10E7F) /* Rumi Numeral Symbols */
        || (cp >= 0x1EE00 && cp <= 0x1EEFF);/* Arabic Mathematical Alphabetic Symbols */
}

/* ─────────────────────────────────────────────────────────────────────────
 * Internal: UTF-8 to FriBidiChar array
 * ─────────────────────────────────────────────────────────────────────── */

static FriBidiChar *utf8_to_ucs4(const char *utf8, size_t byte_len,
                                  FriBidiStrIndex *out_nchars) {
    FriBidiChar *buf = calloc(byte_len + 1, sizeof(FriBidiChar));
    if (!buf) return NULL;
    *out_nchars = fribidi_charset_to_unicode(
        FRIBIDI_CHAR_SET_UTF8,
        utf8,
        (FriBidiStrIndex)byte_len,
        buf
    );
    return buf;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Arabic paragraph count detection
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * Count how many paragraphs in the text contain Arabic content.
 * Paragraphs are separated by U+000A (LF) or U+000D U+000A (CRLF).
 */
int arabic_shield_count_arabic_paragraphs(const char *utf8_input, ssize_t len) {
    if (!utf8_input) return 0;
    size_t byte_len = (len < 0) ? strlen(utf8_input) : (size_t)len;

    FriBidiStrIndex nchars;
    FriBidiChar *ucs4 = utf8_to_ucs4(utf8_input, byte_len, &nchars);
    if (!ucs4) return 0;

    int arabic_paragraphs = 0;
    bool in_arabic_para = false;
    bool para_has_arabic = false;

    for (FriBidiStrIndex i = 0; i < nchars; i++) {
        FriBidiChar cp = ucs4[i];
        if (cp == 0x000A || cp == 0x000D) {
            /* End of paragraph */
            if (para_has_arabic) arabic_paragraphs++;
            para_has_arabic = false;
            in_arabic_para = false;
            /* Skip CR+LF pair */
            if (cp == 0x000D && i + 1 < nchars && ucs4[i+1] == 0x000A) i++;
        } else {
            if (is_arabic_codepoint(cp)) {
                para_has_arabic = true;
                in_arabic_para = true;
            }
            (void)in_arabic_para;
        }
    }
    /* Handle last paragraph (no trailing newline) */
    if (para_has_arabic) arabic_paragraphs++;

    free(ucs4);
    return arabic_paragraphs;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Apply mirroring (UAX #9 step L4)
 *
 * In RTL runs, paired characters like parentheses and brackets must
 * be mirrored: "(" becomes ")" and vice versa. FriBidi handles this
 * but applications sometimes skip this step.
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * Apply Unicode mirroring to a UCS-4 string given its embedding levels.
 * Modifies visual_str in-place.
 */
void arabic_shield_apply_mirroring(FriBidiChar        *visual_str,
                                    FriBidiStrIndex    nchars,
                                    const FriBidiLevel *levels) {
    for (FriBidiStrIndex i = 0; i < nchars; i++) {
        if (levels && FRIBIDI_LEVEL_IS_RTL(levels[i])) {
            FriBidiChar mirrored = fribidi_get_mirror_char(visual_str[i],
                                                           &visual_str[i]);
            (void)mirrored;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * Add explicit directional marks for clipboard / interoperability
 *
 * When Arabic text is copied to the clipboard or passed to applications
 * that don't perform their own BiDi analysis, the text may render
 * incorrectly. Prepending a RLM (U+200F) and appending a PDF (U+202C)
 * wraps the text in explicit directionality hints.
 * ─────────────────────────────────────────────────────────────────────── */

#define FRIBIDI_CHAR_RLM  0x200F  /* RIGHT-TO-LEFT MARK */
#define FRIBIDI_CHAR_LRM  0x200E  /* LEFT-TO-RIGHT MARK */
#define FRIBIDI_CHAR_RLE  0x202B  /* RIGHT-TO-LEFT EMBEDDING */
#define FRIBIDI_CHAR_PDF  0x202C  /* POP DIRECTIONAL FORMATTING */

/**
 * Wrap a UTF-8 Arabic string with RLE...PDF directional marks.
 * Returns a newly allocated string that the caller must free with
 * arabic_shield_free().
 */
char *arabic_shield_wrap_rtl_marks(const char *utf8_input, ssize_t len) {
    if (!utf8_input) return NULL;
    size_t byte_len = (len < 0) ? strlen(utf8_input) : (size_t)len;

    /*
     * RLE = 3 bytes in UTF-8 (E2 80 AB)
     * PDF = 3 bytes in UTF-8 (E2 80 AC)
     */
    size_t out_len = 3 + byte_len + 3 + 1;
    char *out = malloc(out_len);
    if (!out) return NULL;

    /* Write RLE (U+202B): 0xE2 0x80 0xAB */
    out[0] = (char)0xE2; out[1] = (char)0x80; out[2] = (char)0xAB;
    memcpy(out + 3, utf8_input, byte_len);
    /* Write PDF (U+202C): 0xE2 0x80 0xAC */
    out[3 + byte_len + 0] = (char)0xE2;
    out[3 + byte_len + 1] = (char)0x80;
    out[3 + byte_len + 2] = (char)0xAC;
    out[3 + byte_len + 3] = '\0';

    return out;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Diagnostic: dump BiDi analysis of a string to stderr
 * Useful for debugging incorrect rendering.
 * ─────────────────────────────────────────────────────────────────────── */

void arabic_shield_dump_bidi_analysis(const char *utf8_input, ssize_t len) {
    if (!utf8_input) return;
    size_t byte_len = (len < 0) ? strlen(utf8_input) : (size_t)len;

    FriBidiStrIndex nchars;
    FriBidiChar *logical = utf8_to_ucs4(utf8_input, byte_len, &nchars);
    if (!logical) return;

    FriBidiLevel   *levels   = calloc((size_t)nchars, sizeof(FriBidiLevel));
    FriBidiCharType *types   = calloc((size_t)nchars, sizeof(FriBidiCharType));
    if (!levels || !types) { free(logical); free(levels); free(types); return; }

    FriBidiParType par_type = FRIBIDI_PAR_ON;
    fribidi_get_bidi_types(logical, nchars, types);
    fribidi_get_par_embedding_levels(logical, nchars, &par_type, levels);

    fprintf(stderr, "[arabic-shield] BiDi analysis (paragraph type: %s):\n",
            FRIBIDI_IS_RTL(par_type) ? "RTL" : "LTR");

    for (FriBidiStrIndex i = 0; i < nchars; i++) {
        fprintf(stderr, "  [%3d] U+%04X level=%d type=%s\n",
                (int)i,
                (unsigned)logical[i],
                (int)levels[i],
                fribidi_get_bidi_type_name(types[i]));
    }

    free(logical);
    free(levels);
    free(types);
}
