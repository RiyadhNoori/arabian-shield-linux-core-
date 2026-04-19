/**
 * @file simple_gtk_app.c
 * @brief Arabian Shield — Simple GTK3 Arabic Text Demo
 *
 * Demonstrates correct Arabic text rendering in a GTK3 application
 * using the Arabian Shield library.
 *
 * Build:
 *   gcc simple_gtk_app.c \
 *       -DARABIC_SHIELD_ENABLE_PANGO \
 *       $(pkg-config --cflags --libs arabic-shield gtk+-3.0 pangocairo) \
 *       -o simple_gtk_app
 *
 * Run:
 *   ./simple_gtk_app
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include <arabic_shield.h>

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Sample Arabic texts to display
 * ─────────────────────────────────────────────────────────────────────── */

static const struct {
    const char *label;
    const char *text;
} SAMPLES[] = {
    { "Basic Arabic",          "مرحبا بالعالم — Hello World" },
    { "Required ligature",     "لا إله إلا الله" },
    { "Mixed RTL/LTR",        "النص العربي مع English في المنتصف" },
    { "Numbers (BiDi)",        "الرقم ١٢٣ والرقم 456 معاً" },
    { "With Tashkeel",         "الْعَرَبِيَّةُ لُغَةٌ جَمِيلَةٌ" },
    { "Long paragraph",
      "العربية هي أكثر اللغات السامية تحدثاً، وإحدى أكثر لغات العالم "
      "انتشاراً، يتحدثها أكثر من 422 مليون نسمة." },
    { "URL in Arabic",         "زوروا موقعنا على https://example.com للمزيد" },
};

static const int NUM_SAMPLES = (int)(sizeof(SAMPLES) / sizeof(SAMPLES[0]));

/* ─────────────────────────────────────────────────────────────────────────
 * Custom drawing area: renders Arabic text via Cairo + Pango
 * ─────────────────────────────────────────────────────────────────────── */

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void)user_data;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    /* Background */
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.18);
    cairo_paint(cr);

    /* Create Pango layout */
    PangoLayout *layout = pango_cairo_create_layout(cr);

    /* Apply Arabian Shield Pango configuration */
    PangoContext *ctx = pango_layout_get_context(layout);
    arabic_shield_configure_pango_context(ctx, "Noto Sans Arabic", 16 * PANGO_SCALE);

    /* Apply layout fixes */
    arabic_shield_fix_pango_layout(layout);

    /* Set layout width for wrapping */
    pango_layout_set_width(layout, (alloc.width - 40) * PANGO_SCALE);

    double y = 20.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* Draw label */
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.9);
        PangoFontDescription *label_fd =
            pango_font_description_from_string("DejaVu Sans 9");
        pango_layout_set_font_description(layout, label_fd);
        pango_font_description_free(label_fd);
        pango_layout_set_text(layout, SAMPLES[i].label, -1);
        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
        cairo_move_to(cr, 20.0, y);
        pango_cairo_show_layout(cr, layout);

        /* Advance y */
        int lw, lh;
        pango_layout_get_pixel_size(layout, &lw, &lh);
        y += lh + 4;

        /* Draw Arabic text */
        cairo_set_source_rgb(cr, 0.95, 0.93, 0.88);
        PangoFontDescription *ar_fd =
            pango_font_description_from_string("Noto Sans Arabic 14");
        pango_layout_set_font_description(layout, ar_fd);
        pango_font_description_free(ar_fd);

        /* Apply BiDi reordering via Arabian Shield */
        size_t out_len;
        char *visual = arabic_shield_bidi_reorder(
            SAMPLES[i].text, -1,
            ARABIC_SHIELD_DIR_AUTO,
            &out_len
        );

        pango_layout_set_text(layout, visual ? visual : SAMPLES[i].text, -1);
        arabic_shield_free(visual);

        /* Right-align for RTL text */
        ArabicShieldDirection dir =
            arabic_shield_detect_direction(SAMPLES[i].text, -1);
        pango_layout_set_alignment(layout,
            dir == ARABIC_SHIELD_DIR_RTL ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);

        cairo_move_to(cr, 20.0, y);
        pango_cairo_show_layout(cr, layout);

        pango_layout_get_pixel_size(layout, &lw, &lh);
        y += lh + 16;

        /* Separator line */
        if (i < NUM_SAMPLES - 1) {
            cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, 20.0, y - 8);
            cairo_line_to(cr, alloc.width - 20.0, y - 8);
            cairo_stroke(cr);
        }
    }

    g_object_unref(layout);
    return FALSE;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Font info panel
 * ─────────────────────────────────────────────────────────────────────── */

static GtkWidget *build_info_panel(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);

    char *best_font = arabic_shield_best_arabic_font(0);
    char info[256];
    snprintf(info, sizeof(info),
             "Arabian Shield v%s  |  Best Arabic font: %s",
             arabic_shield_version(),
             best_font ? best_font : "(none found — run fonts/download_fonts.sh)");
    arabic_shield_free(best_font);

    GtkWidget *label = gtk_label_new(info);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_widget_set_name(label, "info-label");

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "#info-label { font-size: 10px; color: #888; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(label),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    return box;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Main
 * ─────────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Initialize Arabian Shield BEFORE GTK */
    ArabicShieldStatus st = arabic_shield_init(
        ARABIC_SHIELD_FLAG_RECOMMENDED
    );
    if (st < 0) {
        fprintf(stderr, "Warning: Arabian Shield: %s\n",
                arabic_shield_status_string(st));
    }

    gtk_init(&argc, &argv);

    /* Main window */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window),
                         "Arabian Shield — Arabic GTK3 Demo / عرض توضيحي");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Root box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Header */
    GtkWidget *header = gtk_label_new(
        "Arabian Shield Linux Core — Arabic Rendering Demo"
    );
    gtk_widget_set_name(header, "header");
    GtkCssProvider *hcss = gtk_css_provider_new();
    gtk_css_provider_load_from_data(hcss,
        "#header { font-size: 14px; font-weight: bold; "
        "padding: 12px; background: #1a1a2e; color: #a8d8ea; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(header),
        GTK_STYLE_PROVIDER(hcss), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(hcss);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    /* Scrollable drawing area */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(canvas, -1, NUM_SAMPLES * 80);
    g_signal_connect(canvas, "draw", G_CALLBACK(on_draw), NULL);
    gtk_container_add(GTK_CONTAINER(scroll), canvas);

    /* Info panel at bottom */
    gtk_box_pack_end(GTK_BOX(vbox), build_info_panel(), FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    arabic_shield_cleanup();
    return 0;
}
