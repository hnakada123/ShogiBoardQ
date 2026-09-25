/// @file tst_tsumeshogi_export_header.cpp
/// @brief 詰将棋局面生成のファイル保存コメントヘッダ (TsumeshogiExportHeaderBuilder) テスト

#include <QtTest>

#include "tsumeshogiexportheaderbuilder.h"

class TestTsumeshogiExportHeader : public QObject
{
    Q_OBJECT

private:
    static TsumeshogiGenerator::Settings sampleSettings()
    {
        TsumeshogiGenerator::Settings s;
        s.engineName = QStringLiteral("KomoringHeights 1.1.0 64ZEN2");
        s.targetMoves = 5;
        s.timeoutMs = 7000;
        s.maxPositionsToFind = 20;
        s.allowFinalMoveAlternatives = true;
        s.posGenSettings.maxAttackPieces = 6;
        s.posGenSettings.maxDefendPieces = 2;
        s.posGenSettings.attackRange = 4;
        return s;
    }

    static QDateTime sampleDateTime()
    {
        return QDateTime(QDate(2026, 9, 25), QTime(8, 21, 54));
    }

private slots:
    void build_allLinesAreSingleLineComments()
    {
        const QStringList lines = TsumeshogiExportHeaderBuilder::build(
            QStringLiteral("2026.09.25"), sampleDateTime(), sampleSettings());
        QCOMPARE(lines.size(), 10);
        for (const QString& line : lines) {
            QVERIFY2(line.startsWith(QStringLiteral("# ")), qPrintable(line));
            QVERIFY2(!line.contains(QLatin1Char('\n')), qPrintable(line));
        }
    }

    void build_recordsVersionDateAndSettings()
    {
        const QStringList lines = TsumeshogiExportHeaderBuilder::build(
            QStringLiteral("2026.09.25"), sampleDateTime(), sampleSettings());
        QCOMPARE(lines.at(0), QStringLiteral("# ShogiBoardQ 2026.09.25 詰将棋局面生成"));
        QCOMPARE(lines.at(1), QStringLiteral("# 生成日時: 2026/09/25 08:21:54"));
        QCOMPARE(lines.at(2), QStringLiteral("# エンジン: KomoringHeights 1.1.0 64ZEN2"));
        QCOMPARE(lines.at(3), QStringLiteral("# 目標手数: 5 手詰"));
        QCOMPARE(lines.at(4), QStringLiteral("# 攻め駒上限: 6 枚"));
        QCOMPARE(lines.at(5), QStringLiteral("# 守り駒上限: 2 枚"));
        QCOMPARE(lines.at(6), QStringLiteral("# 配置範囲: 4 マス（玉中心）"));
        QCOMPARE(lines.at(7), QStringLiteral("# 探索時間/局面: 7 秒"));
        QCOMPARE(lines.at(8), QStringLiteral("# 生成上限: 20"));
        QCOMPARE(lines.at(9), QStringLiteral("# 余詰検査: 最終手の複数解を許容する"));
    }

    void build_unlimitedPositionsAndStrictFinalMove()
    {
        TsumeshogiGenerator::Settings s = sampleSettings();
        s.maxPositionsToFind = 0;
        s.allowFinalMoveAlternatives = false;
        s.timeoutMs = 2500;
        const QStringList lines = TsumeshogiExportHeaderBuilder::build(
            QStringLiteral("2026.09.25"), sampleDateTime(), s);
        QCOMPARE(lines.at(7), QStringLiteral("# 探索時間/局面: 2.5 秒"));
        QCOMPARE(lines.at(8), QStringLiteral("# 生成上限: 無制限"));
        QCOMPARE(lines.at(9), QStringLiteral("# 余詰検査: 最終手の複数解を許容しない"));
    }
};

QTEST_MAIN(TestTsumeshogiExportHeader)
#include "tst_tsumeshogi_export_header.moc"
