/// @file tst_nyugyoku_judgement.cpp
/// @brief 入玉宣言の判定（人間の宣言とエンジンの bestmove win で共通）のテスト

#include <QtTest>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "nyugyokujudgement.h"
#include "settingscommon.h"
#include "shogiboard.h"

namespace {
// 先手玉２二・後手玉５八。先手は敵陣に15枚（23点）と持ち駒（桂・香・歩3）で28点
const QString kWin = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b NL3P3lp 1");
// 持ち駒を後手に渡して23点
const QString kShort = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b n4l4p 1");
// 後手の持ち駒も先手に渡して32点
const QString kMany = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b N4L4P 1");
// ２三の後手の金が先手玉に王手
const QString kInCheck = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+Pg+P/9/p1p1p1p1p/1s1g3s1/2n3n1n/1r2k4/2b6 b NL4P3lp 1");
}

class TestNyugyokuJudgement : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;

    static void setRule(int rule)
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("GameSettings/jishogiRule"), rule);
        settings.sync();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        QStandardPaths::setTestModeEnabled(true);
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }

    void judge_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<bool>("sente");
        QTest::addColumn<int>("rule");
        QTest::addColumn<bool>("success");
        QTest::addColumn<bool>("isDraw");
        QTest::addColumn<QString>("verdict");
        QTest::newRow("27-win") << kWin << true << int(NyugyokuJudgement::Rule27) << true << false << "条件達成: 勝ち";
        QTest::newRow("27-short") << kShort << true << int(NyugyokuJudgement::Rule27) << false << false << "点数不足: 宣言失敗";
        QTest::newRow("24-draw") << kWin << true << int(NyugyokuJudgement::Rule24) << true << true << "24〜30点: 引き分け";
        QTest::newRow("24-win") << kMany << true << int(NyugyokuJudgement::Rule24) << true << false << "31点以上: 勝ち";
        QTest::newRow("24-short") << kShort << true << int(NyugyokuJudgement::Rule24) << false << false << "24点未満: 宣言失敗";
        QTest::newRow("in-check") << kInCheck << true << int(NyugyokuJudgement::Rule27) << false << false << "条件未達: 宣言失敗";
        // 後手は敵陣の駒が5枚しかない
        QTest::newRow("white-few-pieces") << kWin << false << int(NyugyokuJudgement::Rule27) << false << false << "条件未達: 宣言失敗";
    }

    void judge()
    {
        QFETCH(QString, sfen);
        QFETCH(bool, sente);
        QFETCH(int, rule);
        QFETCH(bool, success);
        QFETCH(bool, isDraw);
        QFETCH(QString, verdict);
        ShogiBoard board;
        board.setSfen(sfen);
        const auto result = NyugyokuJudgement::judge(board, sente, rule);
        QCOMPARE(result.success, success);
        QCOMPARE(result.isDraw, isDraw);
        QVERIFY2(result.message.contains(verdict), qPrintable(result.message));
        QVERIFY(result.message.startsWith(sente ? QStringLiteral("先手の入玉宣言") : QStringLiteral("後手の入玉宣言")));
    }

    void engineDeclarationUsesConfiguredRule()
    {
        ShogiBoard board;
        board.setSfen(kWin);
        setRule(NyugyokuJudgement::Rule24);
        auto result = NyugyokuJudgement::judgeEngineDeclaration(board, true);
        QVERIFY(result.success && result.isDraw);

        // 「なし」でもエンジンの宣言は取り消せないため、27点法で判定して説明する
        setRule(NyugyokuJudgement::RuleNone);
        result = NyugyokuJudgement::judgeEngineDeclaration(board, true);
        QVERIFY(result.success && !result.isDraw);
        QVERIFY(result.message.startsWith(QStringLiteral("持将棋ルールが「なし」のため、27点法で判定しました。")));

        board.setSfen(kShort);
        setRule(NyugyokuJudgement::Rule27);
        result = NyugyokuJudgement::judgeEngineDeclaration(board, true);
        QVERIFY(!result.success);
        QCOMPARE(result.resultText, QStringLiteral("宣言失敗（負け）"));
    }
};

QTEST_GUILESS_MAIN(TestNyugyokuJudgement)
#include "tst_nyugyoku_judgement.moc"
