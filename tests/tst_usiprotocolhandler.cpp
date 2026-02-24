/// @file tst_usiprotocolhandler.cpp
/// @brief UsiProtocolHandler のユニットテスト
///
/// USIプロトコルの解析ロジック（onDataReceived 経由）を QSignalSpy で検証する。
/// エンジンプロセスの起動は不要。

#include <QtTest>
#include <QSignalSpy>
#include <optional>

#include "usiprotocolhandler.h"

class TestUsiProtocolHandler : public QObject
{
    Q_OBJECT

private slots:
    // ================================================================
    // 1. info 行の解析
    // ================================================================

    void infoLine_depthScoreCpPv()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral(
            "info depth 20 seldepth 30 score cp 150 nodes 1000000 nps 500000 pv 7g7f 3c3d 2g2f");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    void infoLine_scoreMate()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral(
            "info depth 15 score mate 5 pv 5e5d 4a3b 5d5c");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    void infoLine_multipv1()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral(
            "info depth 10 multipv 1 score cp 100 pv 7g7f 3c3d");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    void infoLine_multipv2()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral(
            "info depth 10 multipv 2 score cp -50 pv 2g2f 8c8d");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    void infoLine_string()
    {
        // info string は UsiProtocolHandler レベルでは通常の info 行と同じ扱い
        // （フィルタリングは ThinkingInfoPresenter 側で行われる）
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral("info string evaluation started");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    void infoLine_noScore()
    {
        // score がない info 行でもシグナルは発行される
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        const QString line = QStringLiteral("info depth 5 nodes 100");
        handler.onDataReceived(line);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), line);
    }

    // ================================================================
    // 2. bestmove 行の解析
    // ================================================================

    void bestmove_normal()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
        QVERIFY(!handler.isResignMove());
        QVERIFY(!handler.isWinMove());
        QVERIFY(handler.predictedMove().isEmpty());
    }

    void bestmove_withPonder()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f ponder 3c3d"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
        QCOMPARE(handler.predictedMove(), QStringLiteral("3c3d"));
    }

    void bestmove_resign()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyResign(&handler, &UsiProtocolHandler::bestMoveResignReceived);

        handler.onDataReceived(QStringLiteral("bestmove resign"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(spyResign.count(), 1);
        QVERIFY(handler.isResignMove());
        QCOMPARE(handler.bestMove(), QStringLiteral("resign"));
    }

    void bestmove_win()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyWin(&handler, &UsiProtocolHandler::bestMoveWinReceived);

        handler.onDataReceived(QStringLiteral("bestmove win"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(spyWin.count(), 1);
        QVERIFY(handler.isWinMove());
        QCOMPARE(handler.bestMove(), QStringLiteral("win"));
    }

    void bestmove_resign_duplicateSuppressed()
    {
        // 2回目の resign は重複防止でシグナル発行されない
        UsiProtocolHandler handler;
        QSignalSpy spyResign(&handler, &UsiProtocolHandler::bestMoveResignReceived);

        handler.onDataReceived(QStringLiteral("bestmove resign"));
        handler.onDataReceived(QStringLiteral("bestmove resign"));

        QCOMPARE(spyResign.count(), 1);
    }

    void bestmove_win_duplicateSuppressed()
    {
        // 2回目の win は重複防止でシグナル発行されない
        UsiProtocolHandler handler;
        QSignalSpy spyWin(&handler, &UsiProtocolHandler::bestMoveWinReceived);

        handler.onDataReceived(QStringLiteral("bestmove win"));
        handler.onDataReceived(QStringLiteral("bestmove win"));

        QCOMPARE(spyWin.count(), 1);
    }

    void bestmove_clearsPreviousPonder()
    {
        UsiProtocolHandler handler;

        // 最初の bestmove で ponder あり
        handler.onDataReceived(QStringLiteral("bestmove 7g7f ponder 3c3d"));
        QCOMPARE(handler.predictedMove(), QStringLiteral("3c3d"));

        // リセットして次の bestmove で ponder なし
        handler.resetResignNotified();
        handler.onDataReceived(QStringLiteral("bestmove 2g2f"));
        QVERIFY(handler.predictedMove().isEmpty());
    }

    // ================================================================
    // 3. checkmate 行の解析
    // ================================================================

    void checkmate_solved()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateSolved);

        handler.onDataReceived(QStringLiteral("checkmate 5e5d 4a3b 5d5c"));

        QCOMPARE(spy.count(), 1);
        const QStringList pv = spy.at(0).at(0).toStringList();
        QCOMPARE(pv.size(), 3);
        QCOMPARE(pv.at(0), QStringLiteral("5e5d"));
        QCOMPARE(pv.at(1), QStringLiteral("4a3b"));
        QCOMPARE(pv.at(2), QStringLiteral("5d5c"));
    }

    void checkmate_nomate()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateNoMate);

        handler.onDataReceived(QStringLiteral("checkmate nomate"));

        QCOMPARE(spy.count(), 1);
    }

    void checkmate_notimplemented()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateNotImplemented);

        handler.onDataReceived(QStringLiteral("checkmate notimplemented"));

        QCOMPARE(spy.count(), 1);
    }

    void checkmate_unknown()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateUnknown);

        handler.onDataReceived(QStringLiteral("checkmate unknown"));

        QCOMPARE(spy.count(), 1);
    }

    void checkmate_empty()
    {
        // "checkmate" のみ（手順なし）→ unknown
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateUnknown);

        handler.onDataReceived(QStringLiteral("checkmate"));

        QCOMPARE(spy.count(), 1);
    }

    // ================================================================
    // 4. readyok / usiok
    // ================================================================

    void readyok_received()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::readyOkReceived);

        handler.onDataReceived(QStringLiteral("readyok"));

        QCOMPARE(spy.count(), 1);
    }

    void usiok_received()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::usiOkReceived);

        handler.onDataReceived(QStringLiteral("usiok"));

        QCOMPARE(spy.count(), 1);
    }

    // ================================================================
    // 5. option name 行
    // ================================================================

    void optionName_parsed()
    {
        // option name 行はオプション名を記録するのみでシグナルは発行しない
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(
            QStringLiteral("option name USI_Ponder type check default false"));

        // info や bestmove のシグナルは発行されない
        QCOMPARE(spyInfo.count(), 0);
        QCOMPARE(spyBest.count(), 0);
    }

    // ================================================================
    // 6. 不正入力への耐性
    // ================================================================

    void invalidInput_emptyLine()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyError(&handler, &UsiProtocolHandler::errorOccurred);

        handler.onDataReceived(QString());

        // 空行では何のシグナルも発行されない
        QCOMPARE(spyInfo.count(), 0);
        QCOMPARE(spyBest.count(), 0);
        QCOMPARE(spyError.count(), 0);
    }

    void invalidInput_bestmoveNoToken()
    {
        // "bestmove" の後にトークンがない
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyError(&handler, &UsiProtocolHandler::errorOccurred);

        handler.onDataReceived(QStringLiteral("bestmove"));

        // bestMoveReceived は handleBestMoveLine の前に flag 設定されるので emit される
        QCOMPARE(spyBest.count(), 1);
        // handleBestMoveLine でエラーシグナルが発行される
        QCOMPARE(spyError.count(), 1);
    }

    void invalidInput_unknownCommand()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyReady(&handler, &UsiProtocolHandler::readyOkReceived);
        QSignalSpy spyUsi(&handler, &UsiProtocolHandler::usiOkReceived);

        handler.onDataReceived(QStringLiteral("unknowncommand some data"));

        // 未知のコマンドは全て無視される
        QCOMPARE(spyInfo.count(), 0);
        QCOMPARE(spyBest.count(), 0);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(spyUsi.count(), 0);
    }

    void invalidInput_bestmoveOnlyWhitespace()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyError(&handler, &UsiProtocolHandler::errorOccurred);

        handler.onDataReceived(QStringLiteral("bestmove   "));

        // トークン分割後 "bestmove" のみ → エラー
        QCOMPARE(spyError.count(), 1);
    }

    // ================================================================
    // 7. タイムアウト後のドロップ
    // ================================================================

    void timeoutDeclared_dropsAllLines()
    {
        UsiProtocolHandler handler;
        handler.markHardTimeout();

        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyReady(&handler, &UsiProtocolHandler::readyOkReceived);

        handler.onDataReceived(QStringLiteral("info depth 10 score cp 100"));
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        handler.onDataReceived(QStringLiteral("readyok"));

        QCOMPARE(spyInfo.count(), 0);
        QCOMPARE(spyBest.count(), 0);
        QCOMPARE(spyReady.count(), 0);
    }

    void timeoutCleared_resumesProcessing()
    {
        UsiProtocolHandler handler;
        handler.markHardTimeout();

        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        handler.onDataReceived(QStringLiteral("info depth 5"));
        QCOMPARE(spyInfo.count(), 0);

        handler.clearHardTimeout();
        handler.onDataReceived(QStringLiteral("info depth 10"));
        QCOMPARE(spyInfo.count(), 1);
    }

    // ================================================================
    // 8. 座標変換ユーティリティ
    // ================================================================

    void rankToAlphabet_basic()
    {
        QCOMPARE(UsiProtocolHandler::rankToAlphabet(1), QChar('a'));
        QCOMPARE(UsiProtocolHandler::rankToAlphabet(5), QChar('e'));
        QCOMPARE(UsiProtocolHandler::rankToAlphabet(9), QChar('i'));
    }

    void alphabetToRank_basic()
    {
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('a')), std::optional<int>(1));
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('e')), std::optional<int>(5));
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('i')), std::optional<int>(9));
    }

    void alphabetToRank_invalidInput()
    {
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('z')), std::nullopt);
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('A')), std::nullopt);
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('0')), std::nullopt);
        QCOMPARE(UsiProtocolHandler::alphabetToRank(QChar('j')), std::nullopt);
    }

    // ================================================================
    // 9. 初期状態
    // ================================================================

    void initialState()
    {
        UsiProtocolHandler handler;

        QVERIFY(handler.bestMove().isEmpty());
        QVERIFY(handler.predictedMove().isEmpty());
        QVERIFY(!handler.isResignMove());
        QVERIFY(!handler.isWinMove());
        QVERIFY(!handler.isPonderEnabled());
        QCOMPARE(handler.currentPhase(), UsiProtocolHandler::SearchPhase::Idle);
        QVERIFY(!handler.isTimeoutDeclared());
    }
};

QTEST_MAIN(TestUsiProtocolHandler)
#include "tst_usiprotocolhandler.moc"
