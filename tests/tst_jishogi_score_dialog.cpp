#include <QtTest>
#include <QClipboard>
#include <QDir>
#include <QStandardPaths>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>

#include "applicationfonts.h"
#include "gamesettings.h"
#include "jishogiscoredialog.h"
#include "settingscommon.h"

class TestJishogiScoreDialog : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;
    const JishogiCalculator::JishogiResult m_initial{{27, 0, 0, false}, {27, 0, 0, false}};

    static QString text(const JishogiScoreDialog& dialog, const char* name)
    {
        const auto* label = dialog.findChild<QLabel*>(QLatin1String(name));
        return label ? label->text() : QString();
    }

    static void capture(QWidget& widget, const QString& name)
    {
        const QString directory = qEnvironmentVariable("JISHOGI_SCREENSHOT_DIR");
        if (directory.isEmpty()) return;
        QDir().mkpath(directory);
        QVERIFY(widget.grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        QStandardPaths::setTestModeEnabled(true);
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("JishogiScoreDialogTest"));
        ApplicationFonts::initialize();
    }

    void init() { SettingsCommon::openSettings().clear(); }

    void initialPositionAndCopy()
    {
        JishogiScoreDialog dialog(m_initial, false, false);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(text(dialog, "sente_declarationPoints"), QStringLiteral("0点"));
        QCOMPARE(text(dialog, "sente_totalPoints"), QStringLiteral("27点"));
        QCOMPARE(text(dialog, "gote_totalPoints"), QStringLiteral("27点"));
        QCOMPARE(text(dialog, "sente_kingInCamp"), QStringLiteral("× 未達"));
        QCOMPARE(text(dialog, "sente_notInCheck"), QStringLiteral("○ 達成"));
        QCOMPARE(text(dialog, "sente_rule27"), QStringLiteral("条件未達\nあと28点"));
        QCOMPARE(text(dialog, "gote_rule27"), QStringLiteral("条件未達\nあと27点"));
        auto* copy = dialog.findChild<QPushButton*>(QStringLiteral("copyReport"));
        QVERIFY(copy);
        QTest::mouseClick(copy, Qt::LeftButton);
        const QString report = QApplication::clipboard()->text();
        QVERIFY(report.contains(QStringLiteral("先手\t後手")));
        QVERIFY(report.contains(QStringLiteral("27点法\t条件未達 あと28点\t条件未達 あと27点")));
        QVERIFY(report.contains(QStringLiteral("盤上の全駒")));
        QVERIFY(!report.contains(QStringLiteral("負け")));
        QVERIFY(dialog.isVisible());
        capture(dialog, QStringLiteral("jishogi-score"));
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!dialog.isVisible());
    }

    void ruleBoundaries_data()
    {
        QTest::addColumn<int>("points");
        QTest::addColumn<bool>("inCheck");
        QTest::addColumn<QString>("rule24");
        QTest::addColumn<QString>("sente27");
        QTest::addColumn<QString>("gote27");
        QTest::newRow("below24") << 23 << false << QStringLiteral("点数不足\nあと1点") << QStringLiteral("点数不足\nあと5点") << QStringLiteral("点数不足\nあと4点");
        QTest::newRow("draw24") << 24 << false << QStringLiteral("宣言時：引き分け") << QStringLiteral("点数不足\nあと4点") << QStringLiteral("点数不足\nあと3点");
        QTest::newRow("gote27") << 27 << false << QStringLiteral("宣言時：引き分け") << QStringLiteral("点数不足\nあと1点") << QStringLiteral("宣言時：勝ち");
        QTest::newRow("sente28") << 28 << false << QStringLiteral("宣言時：引き分け") << QStringLiteral("宣言時：勝ち") << QStringLiteral("宣言時：勝ち");
        QTest::newRow("draw30") << 30 << false << QStringLiteral("宣言時：引き分け") << QStringLiteral("宣言時：勝ち") << QStringLiteral("宣言時：勝ち");
        QTest::newRow("win31") << 31 << false << QStringLiteral("宣言時：勝ち") << QStringLiteral("宣言時：勝ち") << QStringLiteral("宣言時：勝ち");
        QTest::newRow("inCheck") << 31 << true << QStringLiteral("条件未達") << QStringLiteral("条件未達") << QStringLiteral("条件未達");
    }

    void ruleBoundaries()
    {
        QFETCH(int, points);
        QFETCH(bool, inCheck);
        QFETCH(QString, rule24);
        QFETCH(QString, sente27);
        QFETCH(QString, gote27);
        const JishogiCalculator::PlayerScore score{points, points, 10, true};
        JishogiScoreDialog dialog({score, score}, inCheck, inCheck);
        QCOMPARE(text(dialog, "sente_rule24"), rule24);
        QCOMPARE(text(dialog, "gote_rule24"), rule24);
        QCOMPARE(text(dialog, "sente_rule27"), sente27);
        QCOMPARE(text(dialog, "gote_rule27"), gote27);
    }

    void fontSizeScrollingAndPersistence()
    {
        GameSettings::setJishogiScoreFontSize(200);
        GameSettings::setJishogiScoreDialogSize(QSize(250, 280));
        {
            JishogiScoreDialog dialog(m_initial, false, false);
            QCOMPARE(dialog.size(), QSize(660, 640));
            dialog.resize(480, 360);
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            auto* increase = dialog.findChild<QPushButton*>(QStringLiteral("fontIncrease"));
            auto* decrease = dialog.findChild<QPushButton*>(QStringLiteral("fontDecrease"));
            auto* scroll = dialog.findChild<QScrollArea*>();
            auto* close = dialog.findChild<QPushButton*>(QStringLiteral("closeButton"));
            QVERIFY(increase && decrease && scroll && close);
            QVERIFY(!increase->isEnabled());
            QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
            QTRY_VERIFY(scroll->horizontalScrollBar()->maximum() > 0);
            QVERIFY(dialog.rect().contains(close->geometry()));
            scroll->ensureWidgetVisible(dialog.findChild<QLabel*>(QStringLiteral("gote_rule27")));
            capture(dialog, QStringLiteral("jishogi-score-large"));
            for (int i = 0; i < 30; ++i) decrease->click();
            QVERIFY(!decrease->isEnabled());
            QCOMPARE(GameSettings::jishogiScoreFontSize(), 8);
            increase->click();
            increase->click();
            QCOMPARE(GameSettings::jishogiScoreFontSize(), 10);
            dialog.resize(700, 680);
            close->click();
            QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
        }
        QCOMPARE(GameSettings::jishogiScoreDialogSize(), QSize(700, 680));
        JishogiScoreDialog reopened(m_initial, false, false);
        QCOMPARE(reopened.size(), QSize(700, 680));
        QCOMPARE(reopened.findChild<QLabel*>(QStringLiteral("sente_totalPoints"))->font().pointSize(), 10);
    }

    void englishPresentation()
    {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATION_DIR "/ShogiBoardQ_en.qm")));
        QVERIFY(qApp->installTranslator(&translator));
        JishogiScoreDialog dialog(m_initial, false, false);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Jishogi Points"));
        QCOMPARE(text(dialog, "sente_rule27"), QStringLiteral("Conditions unmet\n28 more points needed"));
        capture(dialog, QStringLiteral("jishogi-score-en"));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(TestJishogiScoreDialog)
#include "tst_jishogi_score_dialog.moc"
