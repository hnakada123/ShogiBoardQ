#include <QtTest>
#include <QApplication>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTextLayout>
#include "applicationfonts.h"

class TestApplicationFonts : public QObject
{
    Q_OBJECT

    void verifyGlyphs(const QFont& font, const QString& family)
    {
        // 指定名だけでなく、漢字・仮名を描画する実フォントを検査する。
        QTextLayout layout(QStringLiteral("判定時間骨直令あいうアイウ"), font);
        layout.beginLayout();
        layout.createLine().setLineWidth(1000);
        layout.endLayout();
        const auto runs = layout.glyphRuns();
        QVERIFY(!runs.isEmpty());
        for (const auto& run : runs) {
            QCOMPARE(run.rawFont().familyName(), family);
            for (const auto glyph : run.glyphIndexes()) QVERIFY(glyph != 0);
        }
    }

private slots:
    void initTestCase()
    {
        // 地域別 CJK フォントが共存する環境での回帰テスト。
#ifndef Q_OS_LINUX
        QSKIP("Linux の CJK フォント選択を検証するテストです。");
#endif
        if (!QFontDatabase::families().contains(QStringLiteral("Noto Sans CJK JP")) ||
            !QFontDatabase::families().contains(QStringLiteral("Noto Sans Mono CJK JP")))
            QSKIP("Noto CJK JP fonts are not installed.");
        QFont original(QStringLiteral("Noto Sans"), 13, QFont::DemiBold);
        QApplication::setFont(original);
        ApplicationFonts::initialize();
        QCOMPARE(QApplication::font().pointSize(), 13);
        QCOMPARE(QApplication::font().weight(), QFont::DemiBold);
    }

    void widgetsUseJapaneseGlyphs()
    {
        QLabel label(QStringLiteral("判定時間:"));
        QPushButton button(QStringLiteral("再判定"));
        QMenu menu(QStringLiteral("設定"));
        const QList<QWidget*> widgets{&label, &button, &menu};
        for (QWidget* widget : widgets) {
            verifyGlyphs(widget->font(), QStringLiteral("Noto Sans CJK JP"));
            QFont resized = widget->font();
            resized.setPointSize(18);
            verifyGlyphs(resized, QStringLiteral("Noto Sans CJK JP"));
        }
    }

    void latinFontFallsBackToJapanese()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        verifyGlyphs(QFont(QStringLiteral("Noto Sans")), QStringLiteral("Noto Sans CJK JP"));
#else
        QSKIP("スクリプト別フォールバックは Qt 6.8 以降で利用します。");
#endif
    }

    void monospaceKeepsJapaneseGlyphsAndFixedWidth()
    {
        const QFont font = ApplicationFonts::monospaceFont();
        verifyGlyphs(font, QStringLiteral("Noto Sans Mono CJK JP"));
        const QFontMetricsF metrics(font);
        QCOMPARE(metrics.horizontalAdvance(QStringLiteral("iiii")),
                 metrics.horizontalAdvance(QStringLiteral("WWWW")));
        QCOMPARE(metrics.horizontalAdvance(QStringLiteral("判")),
                 metrics.horizontalAdvance(QStringLiteral("00")));
    }
};

QTEST_MAIN(TestApplicationFonts)
#include "tst_applicationfonts.moc"
