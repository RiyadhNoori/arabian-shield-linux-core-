/**
 * @file baseline_adjuster.c
 * @brief Arabian Shield — Cross-Script Baseline Normalization
 *
 * The Problem:
 *   Arabic fonts and Latin fonts are designed around different baseline
 *   geometry conventions:
 *
 *   Latin:  ascender─┐
 *                     │  cap-height
 *           baseline──┤──────────────────── (reference line)
 *                     │  descender depth
 *           descender─┘
 *
 *   Arabic: Latin baseline often does NOT align with the Arabic baseline
 *           (the bottom of most Arabic letters). Arabic has no ascenders
 *           in the Latin sense, but has tall letters (alef, lam) and
 *           deep descenders (like ي ya).
 *
 *   When Pango renders a mixed Arabic/Latin run, if the baselines are
 *   not normalized, Arabic glyphs will float above or sink below Latin
 *   text, making the line look broken and unreadable.
 *
 * The Fix:
 *   Query FreeType metrics for both fonts at the same point size.
 *   Compute the delta between their descender depths (which are more
 *   stable than ascenders across Arabic fonts). Apply the delta as a
 *   vertical offset to the Arabic PangoAttrRise attribute.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include "arabic_shield.h"
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_TABLES_H

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─────────────────────────────────────────────────────────────────────────
 * FreeType library instance (initialized lazily)
 * ─────────────────────────────────────────────────────────────────────── */

static FT_Library g_ft_lib = NULL;

static FT_Error ensure_freetype(void) {
    if (g_ft_lib) return 0;
    return FT_Init_FreeType(&g_ft_lib);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Font metrics structure
 * ─────────────────────────────────────────────────────────────────────── */

typedef struct {
    double ascender;        /* pixels above baseline */
    double descender;       /* pixels below baseline (positive = below) */
    double line_gap;
    double x_height;        /* height of lowercase 'x' */
    double cap_height;      /* height of uppercase 'H' */
    bool   valid;
} FontMetrics;

/* ─────────────────────────────────────────────────────────────────────────
 * Fontconfig: resolve family name → file path
 * ─────────────────────────────────────────────────────────────────────── */

static char *resolve_font_file(const char *family, const char *lang) {
    FcPattern *pat = FcPatternCreate();
    if (!pat) return NULL;

    FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)family);
    if (lang) {
        FcPatternAddString(pat, FC_LANG, (const FcChar8 *)lang);
    }

    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    FcPattern *match = FcFontMatch(NULL, pat, &result);
    FcPatternDestroy(pat);

    if (!match || result != FcResultMatch) {
        if (match) FcPatternDestroy(match);
        return NULL;
    }

    FcChar8 *file_path = NULL;
    if (FcPatternGetString(match, FC_FILE, 0, &file_path) != FcResultMatch) {
        FcPatternDestroy(match);
        return NULL;
    }

    char *ret = strdup((const char *)file_path);
    FcPatternDestroy(match);
    return ret;
}

/* ─────────────────────────────────────────────────────────────────────────
 * FreeType: load font and extract metrics at given pixel size
 * ─────────────────────────────────────────────────────────────────────── */

static FontMetrics get_font_metrics(const char *family,
                                    const char *lang,
                                    double pixel_size) {
    FontMetrics m = {0};

    if (ensure_freetype() != 0) {
        fprintf(stderr, "[arabic-shield] FT_Init_FreeType failed\n");
        return m;
    }

    char *font_file = resolve_font_file(family, lang);
    if (!font_file) {
        fprintf(stderr, "[arabic-shield] Could not resolve font: %s\n", family);
        return m;
    }

    FT_Face face;
    FT_Error err = FT_New_Face(g_ft_lib, font_file, 0, &face);
    free(font_file);

    if (err != 0) {
        fprintf(stderr, "[arabic-shield] FT_New_Face failed: %d\n", err);
        return m;
    }

    /* Set pixel size — use 72dpi for clean pixel arithmetic */
    err = FT_Set_Char_Size(face,
                           0,                          /* width = same as height */
                           (FT_F26Dot6)(pixel_size * 64.0), /* height in 1/64 pt */
                           72, 72);                    /* DPI */
    if (err != 0) {
        FT_Done_Face(face);
        return m;
    }

    /*
     * FreeType metrics are in 26.6 fixed-point, scaled to the requested size.
     * face->size->metrics gives us the scaled values directly.
     */
    double scale = pixel_size / (double)face->units_per_EM;

    m.ascender  =  (double)face->size->metrics.ascender  / 64.0;
    m.descender = -(double)face->size->metrics.descender / 64.0; /* make positive */
    m.line_gap  =  ((double)face->size->metrics.height / 64.0)
                   - m.ascender - m.descender;

    /* OS/2 table has more reliable x-height and cap-height */
    TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
    if (os2) {
        if (os2->sxHeight > 0)
            m.x_height   = (double)os2->sxHeight   * scale;
        if (os2->sCapHeight > 0)
            m.cap_height = (double)os2->sCapHeight * scale;
    }

    m.valid = true;
    FT_Done_Face(face);
    return m;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Public: arabic_shield_baseline_offset
 * ─────────────────────────────────────────────────────────────────────── */

double arabic_shield_baseline_offset(
    const char *arabic_family,
    const char *latin_family,
    double      pixel_size)
{
    if (!arabic_family || !latin_family || pixel_size <= 0.0) return 0.0;

    FontMetrics arabic_m = get_font_metrics(arabic_family, "ar", pixel_size);
    FontMetrics latin_m  = get_font_metrics(latin_family,  "en", pixel_size);

    if (!arabic_m.valid || !latin_m.valid) {
        fprintf(stderr,
            "[arabic-shield] baseline_offset: could not load metrics for "
            "'%s' or '%s'\n", arabic_family, latin_family);
        return 0.0;
    }

    /*
     * Strategy: align the descender depths.
     *
     * Both scripts should have their deepest descender at the same
     * distance below the baseline. If Arabic's descender is deeper,
     * we shift Arabic up (positive offset = up).
     *
     * offset > 0: Arabic should be shifted UP
     * offset < 0: Arabic should be shifted DOWN
     */
    double offset = arabic_m.descender - latin_m.descender;

    fprintf(stderr,
        "[arabic-shield] baseline_offset('%s' vs '%s' at %.1fpx): "
        "arabic_desc=%.2f latin_desc=%.2f offset=%.2f\n",
        arabic_family, latin_family, pixel_size,
        arabic_m.descender, latin_m.descender, offset);

    return offset;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Pango integration: apply baseline offset as PangoAttribute
 * ─────────────────────────────────────────────────────────────────────── */

#ifdef ARABIC_SHIELD_ENABLE_PANGO
#include <pango/pango.h>

/**
 * Apply a baseline offset to a Pango attribute list for a given
 * character range. The offset is in Pango units (PANGO_SCALE * pixels).
 *
 * Use this to fix Arabic runs in mixed Arabic/Latin layouts:
 *
 *   double offset_px = arabic_shield_baseline_offset("Noto Sans Arabic",
 *                                                    "DejaVu Sans", 14.0);
 *   arabic_shield_apply_baseline_attr(attrs, offset_px, start, end);
 *   pango_layout_set_attributes(layout, attrs);
 */
void arabic_shield_apply_baseline_attr(PangoAttrList *attrs,
                                        double         offset_px,
                                        guint          start_index,
                                        guint          end_index) {
    if (!attrs || fabs(offset_px) < 0.1) return;

    /*
     * PangoAttrRise is in Pango units (1/PANGO_SCALE points at 96dpi).
     * Positive rise = move UP; negative = move DOWN.
     * offset_px > 0 means Arabic is too low, so we rise it.
     */
    int rise_pango_units = (int)(offset_px * PANGO_SCALE);

    PangoAttribute *attr = pango_attr_rise_new(rise_pango_units);
    attr->start_index = start_index;
    attr->end_index   = end_index;
    pango_attr_list_insert(attrs, attr);
}

#endif /* ARABIC_SHIELD_ENABLE_PANGO */

/* ─────────────────────────────────────────────────────────────────────────
 * Cleanup FreeType (call from arabic_shield_cleanup)
 * ─────────────────────────────────────────────────────────────────────── */

void baseline_adjuster_cleanup(void) {
    if (g_ft_lib) {
        FT_Done_FreeType(g_ft_lib);
        g_ft_lib = NULL;
    }
}
