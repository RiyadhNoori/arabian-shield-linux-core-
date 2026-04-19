/**
 * @file arabic_shield.c
 * @brief Arabian Shield Linux Core — Core Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include "arabic_shield.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <fribidi.h>
#include <fontconfig/fontconfig.h>
#include <harfbuzz/hb.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Internal state
 * ─────────────────────────────────────────────────────────────────────── */

static struct {
    bool     initialized;
    uint32_t flags;
    FcConfig *fc_config;
} g_shield = {
    .initialized = false,
    .flags       = ARABIC_SHIELD_FLAG_NONE,
    .fc_config   = NULL,
};

/* ─────────────────────────────────────────────────────────────────────────
 * Debug logging (internal)
 * ─────────────────────────────────────────────────────────────────────── */

static void shield_log(const char *fmt, ...) {
    if (!(g_shield.flags & ARABIC_SHIELD_FLAG_DEBUG_LOG)) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[arabic-shield] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ─────────────────────────────────────────────────────────────────────── */

const char *arabic_shield_version(void) {
    return ARABIC_SHIELD_VERSION;
}

int arabic_shield_version_check(int major, int minor, int patch) {
    int cur = ARABIC_SHIELD_VERSION_MAJOR * 10000
            + ARABIC_SHIELD_VERSION_MINOR * 100
            + ARABIC_SHIELD_VERSION_PATCH;
    int req = major * 10000 + minor * 100 + patch;
    return cur >= req;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Status strings
 * ─────────────────────────────────────────────────────────────────────── */

const char *arabic_shield_status_string(ArabicShieldStatus status) {
    switch (status) {
        case ARABIC_SHIELD_OK:                   return "Success";
        case ARABIC_SHIELD_ERR_ALREADY_INIT:     return "Already initialized";
        case ARABIC_SHIELD_ERR_PANGO_MISSING:    return "Pango library not found";
        case ARABIC_SHIELD_ERR_FRIBIDI_MISSING:  return "FriBidi library not found";
        case ARABIC_SHIELD_ERR_HARFBUZZ_MISSING: return "HarfBuzz library not found";
        case ARABIC_SHIELD_ERR_FONTCONFIG_FAIL:  return "Fontconfig initialization failed";
        case ARABIC_SHIELD_ERR_NO_ARABIC_FONT:   return "No Arabic-capable font found";
        case ARABIC_SHIELD_ERR_OUT_OF_MEMORY:    return "Out of memory";
        case ARABIC_SHIELD_ERR_INVALID_UTF8:     return "Invalid UTF-8 input";
        case ARABIC_SHIELD_ERR_NULL_ARG:         return "NULL argument";
        default:                                  return "Unknown error";
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * Fontconfig initialization
 * ─────────────────────────────────────────────────────────────────────── */

static ArabicShieldStatus init_fontconfig(void) {
    if (!FcInit()) {
        fprintf(stderr, "[arabic-shield] FATAL: FcInit() failed\n");
        return ARABIC_SHIELD_ERR_FONTCONFIG_FAIL;
    }

    g_shield.fc_config = FcConfigGetCurrent();
    if (!g_shield.fc_config) {
        fprintf(stderr, "[arabic-shield] FATAL: FcConfigGetCurrent() returned NULL\n");
        return ARABIC_SHIELD_ERR_FONTCONFIG_FAIL;
    }

    if (g_shield.flags & ARABIC_SHIELD_FLAG_APPLY_FONTCONFIG) {
        /* Attempt to load our override config file */
        const char *cfg_path = "/etc/fonts/conf.d/99-arabic-shield.conf";
        if (FcConfigParseAndLoad(g_shield.fc_config,
                                 (const FcChar8 *)cfg_path, FcFalse)) {
            shield_log("Loaded Fontconfig override: %s", cfg_path);
        } else {
            shield_log("WARNING: Could not load %s (not installed?)", cfg_path);
            /* Non-fatal: continue without our overrides */
        }
    }

    return ARABIC_SHIELD_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Init / Cleanup
 * ─────────────────────────────────────────────────────────────────────── */

ArabicShieldStatus arabic_shield_init(uint32_t flags) {
    if (g_shield.initialized) {
        return ARABIC_SHIELD_ERR_ALREADY_INIT;
    }

    g_shield.flags = flags;

    shield_log("Initializing Arabian Shield v%s", arabic_shield_version());
    shield_log("Flags: 0x%04X", flags);

    /* Verify HarfBuzz is available */
    {
        unsigned int hb_major, hb_minor, hb_micro;
        hb_version(&hb_major, &hb_minor, &hb_micro);
        shield_log("HarfBuzz version: %u.%u.%u", hb_major, hb_minor, hb_micro);
    }

    /* Verify FriBidi is available */
    {
        int fb_major = fribidi_version_info ?
                       FRIBIDI_MAJOR_VERSION : -1;
        (void)fb_major;
        shield_log("FriBidi version: %d.%d.%d",
                   FRIBIDI_MAJOR_VERSION,
                   FRIBIDI_MINOR_VERSION,
                   FRIBIDI_MICRO_VERSION);
    }

    /* Initialize Fontconfig */
    ArabicShieldStatus fc_status = init_fontconfig();
    if (fc_status != ARABIC_SHIELD_OK) {
        return fc_status;
    }

    g_shield.initialized = true;
    shield_log("Initialization complete");
    return ARABIC_SHIELD_OK;
}

void arabic_shield_cleanup(void) {
    if (!g_shield.initialized) return;
    shield_log("Cleanup");
    FcFini();
    g_shield.initialized = false;
    g_shield.fc_config   = NULL;
    g_shield.flags       = ARABIC_SHIELD_FLAG_NONE;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Memory
 * ─────────────────────────────────────────────────────────────────────── */

void arabic_shield_free(void *ptr) {
    free(ptr);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Direction detection
 * ─────────────────────────────────────────────────────────────────────── */

ArabicShieldDirection arabic_shield_detect_direction(
    const char *utf8_input,
    ssize_t     input_len)
{
    if (!utf8_input) return ARABIC_SHIELD_DIR_LTR;

    size_t len = (input_len < 0) ? strlen(utf8_input) : (size_t)input_len;

    /*
     * Decode UTF-8 to UCS-4 and ask FriBidi for the base paragraph direction.
     * FriBidi's fribidi_get_par_direction() implements UAX #9 P2/P3.
     */
    size_t ucs4_len = len + 1;
    FriBidiChar *ucs4 = calloc(ucs4_len, sizeof(FriBidiChar));
    if (!ucs4) return ARABIC_SHIELD_DIR_LTR;

    FriBidiStrIndex nchars = fribidi_charset_to_unicode(
        FRIBIDI_CHAR_SET_UTF8,
        utf8_input,
        (FriBidiStrIndex)len,
        ucs4
    );

    FriBidiParType par_type = FRIBIDI_PAR_ON; /* ON = auto */
    (void)fribidi_get_par_embedding_levels(ucs4, nchars, &par_type, NULL);

    free(ucs4);

    return FRIBIDI_IS_RTL(par_type)
           ? ARABIC_SHIELD_DIR_RTL
           : ARABIC_SHIELD_DIR_LTR;
}

/* ─────────────────────────────────────────────────────────────────────────
 * BiDi reordering
 * ─────────────────────────────────────────────────────────────────────── */

char *arabic_shield_bidi_reorder(
    const char            *utf8_input,
    ssize_t                input_len,
    ArabicShieldDirection  base_direction,
    size_t                *output_len)
{
    if (!utf8_input) return NULL;

    size_t byte_len = (input_len < 0) ? strlen(utf8_input) : (size_t)input_len;
    if (byte_len == 0) {
        if (output_len) *output_len = 0;
        char *empty = calloc(1, 1);
        return empty;
    }

    /* Step 1: UTF-8 → UCS-4 */
    FriBidiChar *logical = calloc(byte_len + 1, sizeof(FriBidiChar));
    if (!logical) return NULL;

    FriBidiStrIndex nchars = fribidi_charset_to_unicode(
        FRIBIDI_CHAR_SET_UTF8,
        utf8_input,
        (FriBidiStrIndex)byte_len,
        logical
    );

    /* Step 2: Allocate visual buffer */
    FriBidiChar *visual = calloc((size_t)nchars + 1, sizeof(FriBidiChar));
    if (!visual) { free(logical); return NULL; }

    /* Step 3: Compute embedding levels */
    FriBidiLevel *levels = calloc((size_t)nchars, sizeof(FriBidiLevel));
    if (!levels) { free(logical); free(visual); return NULL; }

    FriBidiParType par_type;
    switch (base_direction) {
        case ARABIC_SHIELD_DIR_RTL: par_type = FRIBIDI_PAR_RTL; break;
        case ARABIC_SHIELD_DIR_LTR: par_type = FRIBIDI_PAR_LTR; break;
        default:                    par_type = FRIBIDI_PAR_ON;  break;
    }

    fribidi_get_par_embedding_levels(logical, nchars, &par_type, levels);

    /* Step 4: Reorder to visual order */
    memcpy(visual, logical, (size_t)nchars * sizeof(FriBidiChar));
    FriBidiStrIndex *map = calloc((size_t)nchars, sizeof(FriBidiStrIndex));
    if (!map) { free(logical); free(visual); free(levels); return NULL; }

    fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAG_REORDER_NSM,
                         logical, nchars, 0, par_type, levels, visual, map);

    free(map);
    free(levels);
    free(logical);

    /* Step 5: UCS-4 → UTF-8 */
    /* Maximum UTF-8 bytes per UCS-4 character is 4 */
    size_t out_bytes_max = (size_t)nchars * 4 + 1;
    char *output = calloc(out_bytes_max, 1);
    if (!output) { free(visual); return NULL; }

    FriBidiStrIndex out_len = fribidi_unicode_to_charset(
        FRIBIDI_CHAR_SET_UTF8,
        visual,
        nchars,
        output
    );

    free(visual);

    if (output_len) *output_len = (size_t)out_len;
    return output;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Font probing via Fontconfig
 * ─────────────────────────────────────────────────────────────────────── */

bool arabic_shield_font_has_arabic(const char *family_name) {
    if (!family_name || !g_shield.fc_config) return false;

    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)family_name);
    FcPatternAddString(pat, FC_LANG,   (const FcChar8 *)"ar");

    FcObjectSet *os = FcObjectSetBuild(FC_CHARSET, FC_FAMILY, NULL);
    FcFontSet *fs = FcFontList(g_shield.fc_config, pat, os);

    FcPatternDestroy(pat);
    FcObjectSetDestroy(os);

    if (!fs || fs->nfont == 0) {
        if (fs) FcFontSetDestroy(fs);
        return false;
    }

    /*
     * Check that the font's charset covers a meaningful portion of
     * the Arabic Unicode block (U+0600–U+06FF, 256 codepoints).
     * Require at least 200 covered codepoints (~78%) as a threshold.
     */
    bool has_arabic = false;
    for (int i = 0; i < fs->nfont && !has_arabic; i++) {
        FcCharSet *cs = NULL;
        if (FcPatternGetCharSet(fs->fonts[i], FC_CHARSET, 0, &cs) == FcResultMatch) {
            int count = 0;
            for (FcChar32 cp = 0x0600; cp <= 0x06FF; cp++) {
                if (FcCharSetHasChar(cs, cp)) count++;
            }
            shield_log("Font '%s' Arabic coverage: %d/256 codepoints", family_name, count);
            has_arabic = (count >= 200);
        }
    }

    FcFontSetDestroy(fs);
    return has_arabic;
}

char *arabic_shield_best_arabic_font(int style) {
    if (!g_shield.fc_config) return NULL;

    const char *style_name;
    switch (style) {
        case 1:  style_name = "serif";     break;
        case 2:  style_name = "monospace"; break;
        default: style_name = "sans-serif"; break;
    }

    FcPattern *req = FcPatternCreate();
    FcPatternAddString(req, FC_FAMILY, (const FcChar8 *)style_name);
    FcPatternAddString(req, FC_LANG,   (const FcChar8 *)"ar");

    FcConfigSubstitute(g_shield.fc_config, req, FcMatchPattern);
    FcDefaultSubstitute(req);

    FcResult result;
    FcPattern *match = FcFontMatch(g_shield.fc_config, req, &result);
    FcPatternDestroy(req);

    if (!match || result != FcResultMatch) {
        if (match) FcPatternDestroy(match);
        return NULL;
    }

    FcChar8 *family = NULL;
    if (FcPatternGetString(match, FC_FAMILY, 0, &family) != FcResultMatch) {
        FcPatternDestroy(match);
        return NULL;
    }

    char *ret = strdup((const char *)family);
    FcPatternDestroy(match);
    shield_log("Best Arabic font (style=%s): %s", style_name, ret);
    return ret;
}
