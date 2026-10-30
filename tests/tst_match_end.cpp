/// @file tst_match_end.cpp
/// @brief 対局の司令塔の終局処理のテスト
///
/// 投了・時間切れ・中断・千日手・連続王手の千日手・持将棋・入玉宣言が、
/// 正しい終局情報・終局の行（印と消費時間）・知らせの順序になることを検証する。

#include <QTest>
#include <QSettings>
#include <QSignalSpy>

#include "match_test_helper.h"
#include "nyugyokujudgement.h"
#include "settingscommon.h"
#include "terminaltype.h"
#include "usi.h"

namespace {
// 先手玉は敵陣にあり、敵陣15枚と持ち駒で28点。
const QString blackWin = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b NL3P3lp 1");

// 対局ダイアログの持将棋ルールの設定値（GameSettings::StartGame::jishogiRule）
static constexpr int kSetting24 = 1;
static constexpr int kSetting27 = 2;

void setRule(int rule)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("GameSettings/jishogiRule"), rule);
    settings.sync();
}

// 終局行の消費時間がどちらの側のものかを見分けるため、先手・後手で違う考慮時間を時計に入れる
const QString kBlackElapsed = QStringLiteral("01:01/00:01:01");  // 先手の考慮61秒

void setDistinctConsideration(ShogiClock& clock)
{
    clock.setConsiderationTimeMs(Turn::Black, 61000);
    clock.setConsiderationTimeMs(Turn::White, 122000);
}

/// 棋譜の行の追記を、終局の知らせと同じ記録に残す受け手
class EventSink final : public MatchRecordSink {
public:
    explicit EventSink(QStringList& events) : m_events(events) {}
    void beginRecord(const MatchRecordStart&) override {}
    bool canAppendRecord() const override { return true; }
    void appendRecord(const RecordEntry& entry) override
    {
        m_events.append(QStringLiteral("append"));
        records.append(entry);
    }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}
    void finishRecord(bool) override { m_events.append(QStringLiteral("finish")); }
    QList<RecordEntry> records;

private:
    QStringList& m_events;
};

MatchCoordinator::StartOptions humanVsHuman(const QString& sfen = SfenUtils::hirateSfen())
{
    MatchCoordinator::StartOptions opt;
    opt.recordLine.startSfen = sfen;
    return opt;
}

/// 先手をエンジン（テスト用の USI エンジン）が指す対局
MatchCoordinator::StartOptions blackEngine(const QString& sfen)
{
    MatchCoordinator::StartOptions opt = humanVsHuman(sfen);
    opt.players.seat(Turn::Black).engine = true;
    opt.players.seat(Turn::Black).engineName = QStringLiteral("DeclarationTest");
    opt.engine(Turn::Black).path = MatchHarness::mockEnginePath();
    return opt;
}

/// 指し手を順に指す（どちらの手番もエンジンの手として確定する）。最後の手で終局したら false
bool playMoves(MatchHarness& h, const QStringList& moves)
{
    bool continuing = true;
    for (const QString& usi : moves) {
        continuing = h.mc->strategyHost().applyEngineMove(engineMove(usi, h.gc.board()->currentPlayer()));
    }
    return continuing;
}

} // namespace

class Tst_MatchEnd : public QObject
{
    Q_OBJECT

    QStringList m_events;

public:
    void recordEnded() { m_events.append(QStringLiteral("ended")); }
    void recordMessage() { m_events.append(QStringLiteral("dialog")); }
    void recordProcessed() { m_events.append(QStringLiteral("processed")); }

private slots:
    void init() { m_events.clear(); }

    // --- 時間切れ ---
    void timeout_keepsCauseAndLoser_data()
    {
        QTest::addColumn<bool>("blackLoses");
        QTest::newRow("black") << true;
        QTest::newRow("white") << false;
    }

    void timeout_keepsCauseAndLoser()
    {
        QFETCH(bool, blackLoses);
        const Turn side = blackLoses ? Turn::Black : Turn::White;
        MatchHarness h;
        h.start(humanVsHuman());
        QSignalSpy ended(h.mc.get(), &MatchCoordinator::gameEnded);
        QSignalSpy processed(h.mc.get(), &MatchCoordinator::gameEndProcessed);
        h.mc->handlePlayerTimeOut(side);
        h.mc->handlePlayerTimeOut(side);
        h.mc->handleResign();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(processed.count(), 1);
        QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Timeout);
        QCOMPARE(h.mc->gameEnd()->loser, side);
        QVERIFY(h.lastMessageText().contains(QStringLiteral("時間切れ")));
        QCOMPARE(h.sink.records.last().displayText, blackLoses ? QStringLiteral("▲時間切れ") : QStringLiteral("△時間切れ"));
    }

    /// 最大手数: 一手 → 終局の行 → 棋譜の確定（自動保存） → 終局 → 知らせ → 後処理の完了、の順に1回ずつ
    void maxMoves_notifiesAfterAppendingAndBeforeCompletion()
    {
        MatchHarness h;
        EventSink sink(m_events);
        h.mc->setRecordSink(&sink);
        connect(h.mc.get(), &MatchCoordinator::gameEnded, this, &Tst_MatchEnd::recordEnded);
        connect(h.mc.get(), &MatchCoordinator::gameOverMessage, this, &Tst_MatchEnd::recordMessage);
        connect(h.mc.get(), &MatchCoordinator::gameEndProcessed, this, &Tst_MatchEnd::recordProcessed);
        MatchCoordinator::StartOptions opt = humanVsHuman();
        opt.maxMoves = 1;
        h.start(opt);
        QVERIFY(!playMoves(h, {QStringLiteral("7g7f")}));
        QCOMPARE(m_events, QStringList({QStringLiteral("append"), QStringLiteral("append"), QStringLiteral("finish"),
                                         QStringLiteral("ended"), QStringLiteral("dialog"), QStringLiteral("processed")}));
        QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Jishogi);
        // 持将棋の行は終局した局面の手番（後手）の行
        QCOMPARE(sink.records.last().displayText, QStringLiteral("△持将棋"));
        QCOMPARE(sink.records.last().mover, Turn::White);
        QVERIFY(h.lastMessageText().contains(QStringLiteral("最大手数")));
    }

    // --- 投了 ---
    void resign_sideToMoveLoses()
    {
        MatchHarness h;
        h.start(humanVsHuman());
        QVERIFY(h.playHuman(QStringLiteral("7g7f")));
        h.mc->handleResign();
        QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Resignation);
        QCOMPARE(h.mc->gameEnd()->loser, Turn::White);
        QCOMPARE(h.sink.records.last().displayText, QStringLiteral("△投了"));
        QVERIFY(h.lastMessageText().contains(QStringLiteral("後手の投了。先手の勝ちです。")));
    }

    void breakOff_recordsInterruptBySideToMove()
    {
        MatchHarness h;
        h.start(humanVsHuman());
        QSignalSpy ended(h.mc.get(), &MatchCoordinator::gameEnded);
        h.mc->handleBreakOff();
        QCOMPARE(ended.count(), 1);
        const auto info = ended.first().first().value<MatchCoordinator::GameEndInfo>();
        QCOMPARE(info.cause, MatchCoordinator::Cause::BreakOff);
        QCOMPARE(info.loser, Turn::Black);
        QCOMPARE(h.sink.records.last().displayText, QStringLiteral("▲中断"));
        h.mc->handleResign();
        QCOMPARE(ended.count(), 1);
    }

    // --- 入玉宣言 ---
    void nyugyokuWin_usesDeclarerTime()
    {
        // 入玉勝ちの行は宣言した勝者の手番の行なので、勝者の消費時間を記録する
        MatchHarness h;
        h.mc->start(humanVsHuman());
        setDistinctConsideration(h.clock);
        h.mc->handleNyugyokuDeclaration(Turn::Black, true, false);
        QCOMPARE(h.sink.records.last().displayText, QStringLiteral("▲入玉勝ち"));
        QCOMPARE(h.sink.records.last().time.toText(), kBlackElapsed);
    }

    void engineWin_judgesDeclaration_data()
    {
        QTest::addColumn<bool>("success");
        QTest::addColumn<bool>("isDraw");
        QTest::addColumn<int>("cause");
        QTest::addColumn<int>("loser");
        // 先手のエンジンが宣言。失敗なら先手の反則負け、24点法の24〜30点は持将棋
        QTest::newRow("win") << true << false << int(MatchCoordinator::Cause::NyugyokuWin) << int(Turn::White);
        QTest::newRow("draw") << true << true << int(MatchCoordinator::Cause::Jishogi) << int(Turn::Black);
        QTest::newRow("fail") << false << false << int(MatchCoordinator::Cause::IllegalMove) << int(Turn::Black);
    }

    /// エンジンの入玉宣言（bestmove win）は盤面から判定し、判定結果を知らせる
    void engineWin_judgesDeclaration()
    {
        QFETCH(bool, success);
        QFETCH(bool, isDraw);
        QFETCH(int, cause);
        QFETCH(int, loser);
        MatchHarness h;
        h.mc->start(blackEngine(success ? blackWin : SfenUtils::hirateSfen()));
        setRule(isDraw ? kSetting24 : kSetting27);
        emit h.engines.engineInSlot(1)->declaredWin(Turn::Black);

        QVERIFY(h.mc->isGameOver());
        QCOMPARE(int(h.mc->gameEnd()->cause), cause);
        QCOMPARE(int(h.mc->gameEnd()->loser), loser);
        QCOMPARE(h.messageCount(), 1);
        QCOMPARE(h.lastMessageTitle(), NyugyokuJudgement::resultTitle());
        QVERIFY(h.lastMessageText().contains(QStringLiteral("先手の入玉宣言")));
        setRule(kSetting27);
    }

    void engineWin_usesHandicapNames()
    {
        // 駒落ちでは入玉宣言結果の説明も宣言側を下手・上手と呼ぶ
        MatchHarness h;
        h.mc->start(blackEngine(blackWin));
        h.useHandicapNames(true);
        setRule(kSetting27);
        emit h.engines.engineInSlot(1)->declaredWin(Turn::Black);
        QVERIFY(h.mc->isGameOver());
        QVERIFY(h.lastMessageText().contains(QStringLiteral("下手の入玉宣言")));
    }

    // --- 千日手 ---
    void sennichite_lineUsesSideToMove_data()
    {
        QTest::addColumn<QString>("start");
        QTest::addColumn<QStringList>("cycle");
        QTest::addColumn<QString>("expectedLine");
        const QString hirate = SfenUtils::hirateSfen();
        QString whiteStart = hirate;
        whiteStart.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        QTest::newRow("black-to-move") << hirate << QStringList{"2h3h", "8b7b", "3h2h", "7b8b"}
                                       << QStringLiteral("▲千日手");
        QTest::newRow("white-to-move") << whiteStart << QStringList{"8b7b", "2h3h", "7b8b", "3h2h"}
                                       << QStringLiteral("△千日手");
    }

    /// 千日手・持将棋の終局行は、終局した局面の手番側の印で記録する（棋譜ファイルを読み込んだときと同じ）
    void sennichite_lineUsesSideToMove()
    {
        QFETCH(QString, start);
        QFETCH(QStringList, cycle);
        QFETCH(QString, expectedLine);
        MatchHarness h;
        h.mc->start(humanVsHuman(start));
        QStringList moves;
        for (int i = 0; i < 12; ++i) moves.append(cycle.at(i % 4));
        QVERIFY(!playMoves(h, moves));
        QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Sennichite);
        QCOMPARE(h.sink.records.last().displayText, expectedLine);
        QVERIFY(h.lastMessageText().contains(QStringLiteral("千日手が成立しました")));
    }

    void outeSennichite_lineRecordsFoulBySideToMove_data()
    {
        QTest::addColumn<QString>("start");
        QTest::addColumn<QStringList>("cycle");
        QTest::addColumn<QString>("expectedLine");
        // 先手の飛車が１筋・２筋で王手を続け、後手玉が１一・２一を行き来する
        // 王手をかけた手で千日手になると、手番は勝った側（反則勝ち）
        QTest::newRow("check-completes-white-to-move")
            << QStringLiteral("8k/9/9/9/9/9/9/9/K7R w - 1") << QStringList{"1a2a", "1i2i", "2a1a", "2i1i"}
            << QStringLiteral("△反則勝ち");
        // 王手を逃げた手で千日手になると、手番は王手を続けた側（反則負け）
        QTest::newRow("escape-completes-black-to-move")
            << QStringLiteral("8k/9/9/9/9/9/9/9/K6R1 b - 1") << QStringList{"2i1i", "1a2a", "1i2i", "2a1a"}
            << QStringLiteral("▲反則負け");
    }

    /// 連続王手の千日手は棋譜形式に終局語が無いので、手番側の「反則勝ち／反則負け」で記録し、
    /// 保存しても王手を続けた側（先手）の負けが残るようにする
    void outeSennichite_lineRecordsFoulBySideToMove()
    {
        QFETCH(QString, start);
        QFETCH(QStringList, cycle);
        QFETCH(QString, expectedLine);
        MatchHarness h;
        h.mc->start(humanVsHuman(start));
        QStringList moves;
        for (int i = 0; i < 12; ++i) moves.append(cycle.at(i % 4));
        QVERIFY(!playMoves(h, moves));
        QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::OuteSennichite);
        QCOMPARE(h.mc->gameEnd()->loser, Turn::Black);
        QCOMPARE(h.sink.records.last().displayText, expectedLine);
        QVERIFY(h.lastMessageText().contains(QStringLiteral("先手の連続王手の千日手。後手の勝ちです。")));
    }

    // --- 結果表示の呼び名 ---
    void resultMessage_followsHandicapNames_data()
    {
        QTest::addColumn<bool>("handicapNames");
        QTest::addColumn<int>("cause");
        QTest::addColumn<QString>("message");
        // 駒落ちで対局情報の見出しが下手・上手なら、対局終了のダイアログも下手（先手側）・上手（後手側）と呼ぶ
        QTest::newRow("timeout") << false << int(MatchCoordinator::Cause::Timeout)
                                 << QStringLiteral("後手の時間切れ。先手の勝ちです。");
        QTest::newRow("timeout-handicap") << true << int(MatchCoordinator::Cause::Timeout)
                                          << QStringLiteral("上手の時間切れ。下手の勝ちです。");
        QTest::newRow("resign-handicap") << true << int(MatchCoordinator::Cause::Resignation)
                                         << QStringLiteral("上手の投了。下手の勝ちです。");
    }

    void resultMessage_followsHandicapNames()
    {
        QFETCH(bool, handicapNames);
        QFETCH(int, cause);
        QFETCH(QString, message);
        MatchHarness h;
        h.start(humanVsHuman());
        h.useHandicapNames(handicapNames);
        if (cause == int(MatchCoordinator::Cause::Timeout)) {
            h.mc->handlePlayerTimeOut(Turn::White);
        } else {
            QVERIFY(h.playHuman(QStringLiteral("7g7f")));
            h.mc->handleResign();
        }
        QCOMPARE(h.lastMessageTitle(), QStringLiteral("対局終了"));
        QCOMPARE(h.lastMessageText(), message);
    }
};

QTEST_MAIN(Tst_MatchEnd)
#include "tst_match_end.moc"
