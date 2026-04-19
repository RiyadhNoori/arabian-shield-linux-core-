/**
 * @file simple_qt_app.cpp
 * @brief Arabian Shield — Simple Qt5/Qt6 Arabic Text Demo
 *
 * Demonstrates correct Arabic text rendering in a Qt application
 * using the Arabian Shield library for initialization and BiDi support.
 *
 * Build (Qt5):
 *   g++ simple_qt_app.cpp \
 *       $(pkg-config --cflags --libs arabic-shield) \
 *       $(pkg-config --cflags --libs Qt5Widgets Qt5Gui) \
 *       -fPIC -o simple_qt_app
 *
 * Build (Qt6):
 *   g++ simple_qt_app.cpp \
 *       $(pkg-config --cflags --libs arabic-shield) \
 *       $(pkg-config --cflags --libs Qt6Widgets Qt6Gui) \
 *       -fPIC -o simple_qt_app
 *
 * Run:
 *   ./simple_qt_app
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2024 Arabian Shield Contributors
 */

#include <arabic_shield.h>

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QTextOption>
#include <QLocale>
#include <QDir>
#include <QString>
#include <QStringList>
#include <cstdio>

/* ─────────────────────────────────────────────────────────────────────────
 * ArabicTextWidget: Custom widget demonstrating correct Arabic rendering
 * ─────────────────────────────────────────────────────────────────────── */

class ArabicTextWidget : public QWidget {
    Q_OBJECT
public:
    explicit ArabicTextWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(500);
        setStyleSheet("background-color: #1a1a2e;");
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        /* Background */
        painter.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));

        struct Sample {
            const char *label;
            const char *text;
            Qt::LayoutDirection direction;
        };

        static const Sample samples[] = {
            { "Basic Arabic",
              "مرحبا بالعالم — Hello World",
              Qt::RightToLeft },
            { "Required lam-alef ligature",
              "لا إله إلا الله",
              Qt::RightToLeft },
            { "Mixed BiDi",
              "النص العربي مع English في المنتصف والرقم 123",
              Qt::RightToLeft },
            { "With Tashkeel (diacritics)",
              "الْعَرَبِيَّةُ لُغَةٌ جَمِيلَةٌ",
              Qt::RightToLeft },
            { "Persian (Farsi)",
              "زبان فارسی — Persian Language",
              Qt::RightToLeft },
            { "LTR with embedded Arabic",
              "English text with مرحبا embedded",
              Qt::LeftToRight },
        };
        static const int n_samples = static_cast<int>(
            sizeof(samples) / sizeof(samples[0]));

        /* Choose the best available Arabic font */
        QStringList arabic_families = {
            "Noto Sans Arabic", "Noto Naskh Arabic", "Amiri",
            "Cairo", "Scheherazade New", "DejaVu Sans"
        };
        QFont arabic_font;
        for (const auto &fam : arabic_families) {
            if (QFontDatabase::families().contains(fam)) {
                arabic_font = QFont(fam, 14);
                break;
            }
        }
        arabic_font.setHintingPreference(QFont::PreferLightHinting);

        QFont label_font("DejaVu Sans", 9);
        QColor label_color(0x7a, 0xb8, 0xe8);
        QColor text_color(0xf0, 0xec, 0xe0);
        QColor sep_color(255, 255, 255, 20);

        double y = 20.0;
        const double x_margin = 20.0;
        const double content_width = width() - 2 * x_margin;

        for (int i = 0; i < n_samples; i++) {
            /* Draw label */
            painter.setFont(label_font);
            painter.setPen(label_color);
            painter.drawText(QRectF(x_margin, y, content_width, 20),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString(samples[i].label));
            y += 22;

            /* Apply BiDi reordering via Arabian Shield */
            size_t out_len = 0;
            char *visual = arabic_shield_bidi_reorder(
                samples[i].text, -1,
                (samples[i].direction == Qt::RightToLeft)
                    ? ARABIC_SHIELD_DIR_RTL
                    : ARABIC_SHIELD_DIR_LTR,
                &out_len
            );

            QString display_text = visual
                ? QString::fromUtf8(visual)
                : QString::fromUtf8(samples[i].text);
            arabic_shield_free(visual);

            /* Render Arabic text */
            painter.setFont(arabic_font);
            painter.setPen(text_color);

            QTextOption text_option;
            text_option.setTextDirection(samples[i].direction);
            text_option.setAlignment(
                samples[i].direction == Qt::RightToLeft
                    ? Qt::AlignRight
                    : Qt::AlignLeft
            );
            text_option.setWrapMode(QTextOption::WordWrap);

            QRectF text_rect(x_margin, y, content_width, 60);
            painter.drawText(text_rect, display_text, text_option);

            QFontMetrics fm(arabic_font);
            int text_height = fm.height();
            y += text_height + 20;

            /* Separator */
            if (i < n_samples - 1) {
                painter.setPen(QPen(sep_color, 1));
                painter.drawLine(QPointF(x_margin, y - 10),
                                 QPointF(width() - x_margin, y - 10));
            }
        }
    }
};

/* ─────────────────────────────────────────────────────────────────────────
 * MainWindow
 * ─────────────────────────────────────────────────────────────────────── */

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("Arabian Shield — Qt Arabic Demo / عرض توضيحي");
        setMinimumSize(700, 600);

        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *root = new QVBoxLayout(central);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        /* Header */
        QLabel *header = new QLabel(
            "Arabian Shield Linux Core — Qt Arabic Rendering Demo");
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet(
            "font-size: 14px; font-weight: bold; padding: 12px; "
            "background: #16213e; color: #a8d8ea;");
        root->addWidget(header);

        /* Scroll area with custom canvas */
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("border: none;");
        ArabicTextWidget *canvas = new ArabicTextWidget;
        scroll->setWidget(canvas);
        root->addWidget(scroll, 1);

        /* Input area */
        root->addWidget(buildInputArea());

        /* Status bar */
        char *best_font = arabic_shield_best_arabic_font(0);
        QString status = QString("Arabian Shield v%1  |  Best Arabic font: %2")
            .arg(arabic_shield_version())
            .arg(best_font ? best_font : "(none — run fonts/download_fonts.sh)");
        arabic_shield_free(best_font);
        statusBar()->showMessage(status);
        statusBar()->setStyleSheet("color: #666; font-size: 10px;");
    }

private:
    QWidget *buildInputArea() {
        QGroupBox *group = new QGroupBox("Live Arabic Input Test / اختبار الإدخال");
        group->setStyleSheet(
            "QGroupBox { background: #16213e; color: #a8d8ea; "
            "border: 1px solid #2a2a4a; margin: 4px; padding: 8px; }");

        QVBoxLayout *vl = new QVBoxLayout(group);

        /* Arabic input field */
        QHBoxLayout *hl1 = new QHBoxLayout;
        QLabel *lbl1 = new QLabel("Arabic:");
        lbl1->setStyleSheet("color: #888;");
        QLineEdit *ar_edit = new QLineEdit;
        ar_edit->setLayoutDirection(Qt::RightToLeft);
        ar_edit->setPlaceholderText("أدخل نصاً عربياً هنا...");
        ar_edit->setText("مرحبا بالعالم");
        ar_edit->setStyleSheet(
            "background: #0f0f1a; color: #f0ece0; border: 1px solid #333; "
            "padding: 4px; font-size: 14px;");
        hl1->addWidget(lbl1);
        hl1->addWidget(ar_edit);
        vl->addLayout(hl1);

        /* Direction indicator */
        QLabel *dir_label = new QLabel;
        dir_label->setStyleSheet("color: #666; font-size: 10px;");
        vl->addWidget(dir_label);

        /* Update direction indicator on text change */
        connect(ar_edit, &QLineEdit::textChanged, [dir_label](const QString &text) {
            QByteArray utf8 = text.toUtf8();
            ArabicShieldDirection dir = arabic_shield_detect_direction(
                utf8.constData(), utf8.size()
            );
            dir_label->setText(
                QString("Detected direction: %1")
                .arg(dir == ARABIC_SHIELD_DIR_RTL ? "RTL ← العربية" : "LTR → Latin")
            );
        });
        emit ar_edit->textChanged(ar_edit->text()); // trigger initial update

        return group;
    }
};

/* ─────────────────────────────────────────────────────────────────────────
 * main()
 * ─────────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Initialize Arabian Shield BEFORE QApplication */
    ArabicShieldStatus st = arabic_shield_init(ARABIC_SHIELD_FLAG_RECOMMENDED);
    if (st < 0) {
        fprintf(stderr, "Arabian Shield warning: %s\n",
                arabic_shield_status_string(st));
    }

    QApplication app(argc, argv);

    /* Configure Qt for Arabic */
    app.setLayoutDirection(Qt::LeftToRight); // Let individual widgets set RTL

    /* Set default Arabic-capable font */
    QStringList arabic_fonts = {
        "Noto Sans Arabic", "Noto Naskh Arabic", "Amiri", "Cairo"
    };
    for (const auto &fam : arabic_fonts) {
        if (QFontDatabase::families().contains(fam)) {
            // Don't override app default, just make it available
            break;
        }
    }

    MainWindow window;
    window.show();

    int ret = app.exec();
    arabic_shield_cleanup();
    return ret;
}

#include "simple_qt_app.moc"
