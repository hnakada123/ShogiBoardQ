#include <QtTest>
#include <QApplication>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTextLayout>
#include <QPlainTextEdit>
#include "applicationfonts.h"

class TestApplicationFonts : public QObject
{
    Q_OBJECT

    bool hasJapaneseFonts() const
    {
#ifdef Q_OS_LINUX
        const auto families = QFontDatabase::families();
        return families.contains(QStringLiteral("Noto Sans CJK JP")) &&
               families.contains(QStringLiteral("Noto Sans Mono CJK JP"));
#else
        return false;
#endif
    }

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
        QFont original(QStringLiteral("Noto Sans"), 13, QFont::DemiBold);
        QApplication::setFont(original);
        ApplicationFonts::initialize();
        QCOMPARE(QApplication::font().pointSize(), 13);
        QCOMPARE(QApplication::font().weight(), QFont::DemiBold);
    }

    void widgetsUseJapaneseGlyphs()
    {
        if (!hasJapaneseFonts()) QSKIP("Linux Noto CJK JP fonts are not installed.");
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
        if (!hasJapaneseFonts()) QSKIP("Linux Noto CJK JP fonts are not installed.");
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        verifyGlyphs(QFont(QStringLiteral("Noto Sans")), QStringLiteral("Noto Sans CJK JP"));
#else
        QSKIP("スクリプト別フォールバックは Qt 6.8 以降で利用します。");
#endif
    }

    void monospaceKeepsJapaneseGlyphsAndFixedWidth()
    {
        if (!hasJapaneseFonts()) QSKIP("Linux Noto CJK JP fonts are not installed.");
        const QFont font = ApplicationFonts::monospaceFont();
        verifyGlyphs(font, QStringLiteral("Noto Sans Mono CJK JP"));
        const QFontMetricsF metrics(font);
        QCOMPARE(metrics.horizontalAdvance(QStringLiteral("iiii")),
                 metrics.horizontalAdvance(QStringLiteral("WWWW")));
        QCOMPARE(metrics.horizontalAdvance(QStringLiteral("判")),
                 metrics.horizontalAdvance(QStringLiteral("00")));
    }

    void changeExistingAndFutureWidgets()
    {
        QStringList families = QFontDatabase::families();
        families.removeAll(ApplicationFonts::defaultFamily());
        if (families.isEmpty()) QSKIP("An alternative font is required.");
        const QString selected = families.first();

        const QString originalStyleSheet = qApp->styleSheet();
        qApp->setStyleSheet(QStringLiteral("QPushButton { padding: 4px; }"));

        QWidget panel;
        panel.setStyleSheet(QStringLiteral("QLabel { color: #123456; }"));
        QLabel label(QStringLiteral("棋譜"), &panel);
        QFont local = label.font();
        local.setPointSize(19);
        local.setBold(true);
        label.setFont(local);
        QMenu menu;
        QPlainTextEdit log;
        QFont mono = ApplicationFonts::monospaceFont();
        mono.setPointSize(11);
        log.setFont(mono);
        panel.show();

        ApplicationFonts::applyFamily(selected);
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::font().family(), selected);
        QCOMPARE(panel.font().family(), selected);
        QCOMPARE(label.font().family(), selected);
        QCOMPARE(label.font().pointSize(), 19);
        QVERIFY(label.font().bold());
        QCOMPARE(menu.font().family(), selected);
        QCOMPARE(log.font().family(), selected);
        QCOMPARE(log.font().pointSize(), 11);
        QLabel future;
        QCOMPARE(future.font().family(), selected);
        QCOMPARE(ApplicationFonts::monospaceFont().family(), selected);

        ApplicationFonts::applyFamily({});
        QCoreApplication::processEvents();
        QCOMPARE(label.font().family(), ApplicationFonts::defaultFamily());
        QCOMPARE(label.font().pointSize(), 19);
        QVERIFY(label.font().bold());
        QCOMPARE(log.font().family(), mono.family());
        QCOMPARE(log.font().pointSize(), 11);
        qApp->setStyleSheet(originalStyleSheet);
    }

    void unavailableFontFallsBackToDefault()
    {
        ApplicationFonts::initialize(QStringLiteral("ShogiBoardQ nonexistent font 12345"));
        QCOMPARE(QApplication::font().family(), ApplicationFonts::defaultFamily());
    }
};

QTEST_MAIN(TestApplicationFonts)
#include "tst_applicationfonts.moc"
