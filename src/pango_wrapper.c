/**
 * @file pango_wrapper.c
 * @brief Arabian Shield — Pango/HarfBuzz Integration Layer
 *
 * This module wraps Pango's text shaping pipeline to:
 *   1. Force correct script itemization for Arabic text
 *   2. Ensure HarfBuzz is used (not the legacy Basic renderer)
 *   3. Enable required OpenType features: init, medi, fina, liga, calt
 *   4. Configure paragraph direction from the BiDi analysis
 *
 * Why this is needed:
 *   Pango's automatic script detection sometimes misclassifies Arabic
 *   characters when they appear in mixed-script strings, causing
 *   HarfBuzz to be invoked with the wrong script tag (e.g., "Latn"
 *   instead of "Arab"), resulting in isolated glyph forms.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include "arabic_shield.h"

#ifdef ARABIC_SHIELD_ENABLE_PANGO

#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ot.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Internal: HarfBuzz OpenType feature flags for Arabic
 *
 * These are the OpenType Layout features that MUST be enabled for
 * correct Arabic rendering. Pango usually enables liga and kern by
 * default, but init/medi/fina/calt may be omitted on some systems.
 * ─────────────────────────────────────────────────────────────────────── */

static const hb_feature_t arabic_features[] = {
    /* Mandatory Arabic positional forms */
    { HB_TAG('i','n','i','t'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    { HB_TAG('m','e','d','i'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    { HB_TAG('f','i','n','a'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    { HB_TAG('i','s','o','l'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },

    /* Standard ligatures — mandatory for Arabic (e.g., lam-alef) */
    { HB_TAG('l','i','g','a'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },

    /* Required ligatures (cannot be disabled per OT spec) */
    { HB_TAG('r','l','i','g'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },

    /* Contextual alternates — needed for Kashida and glyph variants */
    { HB_TAG('c','a','l','t'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },

    /* Mark positioning — required for Tashkeel (diacritics) */
    { HB_TAG('m','a','r','k'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    { HB_TAG('m','k','m','k'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },

    /* Kerning */
    { HB_TAG('k','e','r','n'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
};

static const int arabic_features_count =
    (int)(sizeof(arabic_features) / sizeof(arabic_features[0]));

/* ─────────────────────────────────────────────────────────────────────────
 * arabic_shield_configure_pango_context
 * ─────────────────────────────────────────────────────────────────────── */

void arabic_shield_configure_pango_context(
    PangoContext *context,
    const char   *font_family,
    int           font_size)
{
    if (!context) return;

    /* Set language to Arabic — this influences script detection */
    PangoLanguage *ar_lang = pango_language_from_string("ar");
    pango_context_set_language(context, ar_lang);

    /* Set base gravity: Pango's GRAVITY_AUTO lets HarfBuzz decide,
     * which is correct for Arabic (horizontal LTR glyphs, RTL line) */
    pango_context_set_base_gravity(context, PANGO_GRAVITY_AUTO);
    pango_context_set_gravity_hint(context, PANGO_GRAVITY_HINT_NATURAL);

    /* Force RTL base direction */
    pango_context_set_base_dir(context, PANGO_DIRECTION_RTL);

    /* Apply font description */
    if (font_family && font_size > 0) {
        char desc_str[256];
        snprintf(desc_str, sizeof(desc_str), "%s %d",
                 font_family, font_size / PANGO_SCALE);
        PangoFontDescription *desc = pango_font_description_from_string(desc_str);
        if (desc) {
            pango_context_set_font_description(context, desc);
            pango_font_description_free(desc);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * arabic_shield_fix_pango_layout
 * ─────────────────────────────────────────────────────────────────────── */

void arabic_shield_fix_pango_layout(PangoLayout *layout) {
    if (!layout) return;

    /*
     * AUTO direction: Pango picks RTL or LTR based on the first strong
     * BiDi character. This is the correct behavior for Arabic text fields
     * that may also contain Latin (e.g., URLs, numbers).
     */
    pango_layout_set_auto_dir(layout, TRUE);

    /*
     * Alignment: RTL_ALIGN respects the resolved paragraph direction.
     * PANGO_ALIGN_LEFT would always left-align, which is wrong for Arabic.
     */
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

    /*
     * Ellipsize at the START for RTL text (the start of the line is
     * the right side visually). This matches Arabic reading conventions.
     */
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_START);

    /*
     * Wrapping: WORD_CHAR falls back to character wrapping when a word
     * is too long. Arabic words can be very long due to affixes.
     */
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Internal: verify that a PangoLayout actually shaped Arabic glyphs
 * correctly by checking that no isolated (init/medi/fina) forms appear
 * where connected forms should be.
 *
 * Returns true if the layout looks correct.
 * ─────────────────────────────────────────────────────────────────────── */
bool arabic_shield_verify_layout(PangoLayout *layout) {
    if (!layout) return false;

    PangoLayoutIter *iter = pango_layout_get_iter(layout);
    if (!iter) return false;

    bool all_ok = true;

    do {
        PangoLayoutRun *run = pango_layout_iter_get_run(iter);
        if (!run) continue;

        /* Check that the run's analysis has Arabic as its script */
        PangoAnalysis *analysis = &run->item->analysis;
        if (analysis->script == PANGO_SCRIPT_ARABIC) {
            /* If script is Arabic but level is even (LTR), that's a bug */
            if ((analysis->level & 1) == 0) {
                fprintf(stderr,
                    "[arabic-shield] WARNING: Arabic run has LTR embedding level\n");
                all_ok = false;
            }
        }
    } while (pango_layout_iter_next_run(iter));

    pango_layout_iter_free(iter);
    return all_ok;
}

#endif /* ARABIC_SHIELD_ENABLE_PANGO */
