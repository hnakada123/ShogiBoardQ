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

    QFont originalFont;

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

    void verifyGlyphs(const QFont& font, const QString& family,
                      const QString& text = QStringLiteral("判定時間骨直令あいうアイウ"))
    {
        // 指定名だけでなく、漢字・仮名を描画する実フォントを検査する。
        QTextLayout layout(text, font);
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
        originalFont = original;
        QApplication::setFont(original);
        ApplicationFonts::initialize();
        QCOMPARE(QApplication::font().pointSize(), 13);
        QCOMPARE(QApplication::font().weight(), QFont::DemiBold);
    }

    void init() { ApplicationFonts::initialize({}, QStringLiteral("ja_JP")); }

    void languageDefaults_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("family");
        QTest::newRow("english") << QStringLiteral("en") << QStringLiteral("Noto Sans");
        QTest::newRow("japanese") << QStringLiteral("ja_JP") << QStringLiteral("Noto Sans CJK JP");
        QTest::newRow("simplified") << QStringLiteral("zh_CN") << QStringLiteral("Noto Sans CJK SC");
        QTest::newRow("traditional") << QStringLiteral("zh_TW") << QStringLiteral("Noto Sans CJK TC");
    }

    void languageDefaults()
    {
        QFETCH(QString, language);
        QFETCH(QString, family);
#ifndef Q_OS_LINUX
        if (language != QLatin1String("en")) QSKIP("This glyph check uses Linux Noto CJK fonts.");
#endif
        if (!QFontDatabase::families().contains(family)) QSKIP("Required test font is not installed.");
        ApplicationFonts::initialize({}, language);
        QCOMPARE(ApplicationFonts::defaultFamily(), family);
        const QFont font = QApplication::font();
        QCOMPARE(font.pointSize(), originalFont.pointSize());
        QCOMPARE(font.weight(), originalFont.weight());
        verifyGlyphs(font, family, language == QLatin1String("en") ? QStringLiteral("ABC 123")
                                                                                : QStringLiteral("骨直令"));
        if (language == QLatin1String("en")) {
            QCOMPARE(ApplicationFonts::monospaceFont().family(),
                     QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
        } else {
            const QString mono = QString(family).replace(QStringLiteral("Noto Sans CJK"), QStringLiteral("Noto Sans Mono CJK"));
            if (QFontDatabase::families().contains(mono))
                verifyGlyphs(ApplicationFonts::monospaceFont(), mono, QStringLiteral("骨直令"));
        }
        if (hasJapaneseFonts())
            verifyGlyphs(ApplicationFonts::japaneseFont(font), QStringLiteral("Noto Sans CJK JP"));

        // 言語を往復しても、以前の標準書体がOS標準として残らない。
        ApplicationFonts::initialize({}, QStringLiteral("ja_JP"));
        ApplicationFonts::initialize(QStringLiteral("ShogiBoardQ nonexistent font 12345"), language);
        QCOMPARE(QApplication::font().family(), family);
        ApplicationFonts::applyFamily(QStringLiteral("Noto Sans"));
        QCOMPARE(QApplication::font().family(), QStringLiteral("Noto Sans"));
        QCOMPARE(ApplicationFonts::japaneseFont(font).family(), QStringLiteral("Noto Sans"));
        ApplicationFonts::applyFamily({});
        QCOMPARE(QApplication::font().family(), family);
    }

    void noRegionalFontsUsesSystemFont()
    {
        if (!QFontDatabase::families(QFontDatabase::Japanese).isEmpty()
            || !QFontDatabase::families(QFontDatabase::SimplifiedChinese).isEmpty()
            || !QFontDatabase::families(QFontDatabase::TraditionalChinese).isEmpty())
            QSKIP("Run with a Latin-only fontconfig configuration to test missing regional fonts.");
        for (const auto* language : {"ja_JP", "en", "zh_CN", "zh_TW"}) {
            ApplicationFonts::initialize({}, QString::fromLatin1(language));
            QCOMPARE(QApplication::font().family(), originalFont.family());
            QCOMPARE(ApplicationFonts::japaneseFont(QApplication::font()).family(), originalFont.family());
            ApplicationFonts::initialize(QStringLiteral("ShogiBoardQ nonexistent font 12345"), QString::fromLatin1(language));
            QCOMPARE(QApplication::font().family(), originalFont.family());
        }
    }

    void chineseFallbackAndJapaneseNotation()
    {
        if (!hasJapaneseFonts() || !QFontDatabase::families().contains(QStringLiteral("Noto Sans CJK SC")))
            QSKIP("Noto CJK JP/SC fonts are not installed.");
        ApplicationFonts::initialize({}, QStringLiteral("zh_CN"));
        QLabel label(QStringLiteral("▲７六歩"));
        ApplicationFonts::useJapaneseFont(&label);
        verifyGlyphs(label.font(), QStringLiteral("Noto Sans CJK JP"));
        QFont resized = QApplication::font();
        resized.setPointSize(23);
        resized.setItalic(true);
        label.setFont(resized);
        QCOMPARE(label.font().pointSize(), 23);
        QVERIFY(label.font().italic());
        QCOMPARE(label.font().family(), QStringLiteral("Noto Sans CJK JP"));
        ApplicationFonts::applyFamily(QStringLiteral("Noto Sans CJK SC"));
        QCOMPARE(label.font().family(), QStringLiteral("Noto Sans CJK SC"));
        ApplicationFonts::applyFamily({});
        QCOMPARE(label.font().family(), QStringLiteral("Noto Sans CJK JP"));
        QCOMPARE(label.font().pointSize(), 23);
        ApplicationFonts::useJapaneseFont(&label, false);
        QCOMPARE(label.font().family(), QStringLiteral("Noto Sans CJK SC"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        verifyGlyphs(QFont(QStringLiteral("Noto Sans")), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("骨直令"));
        verifyGlyphs(QFont(QStringLiteral("Noto Sans")), QStringLiteral("Noto Sans CJK JP"), QStringLiteral("あいうアイウ"));
        ApplicationFonts::initialize({}, QStringLiteral("en"));
        verifyGlyphs(QFont(QStringLiteral("Noto Sans")), QStringLiteral("Noto Sans CJK JP"));
#endif
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
