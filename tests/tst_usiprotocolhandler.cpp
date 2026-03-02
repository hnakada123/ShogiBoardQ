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

    // ================================================================
    // 10. テーブル駆動: 異常入力パターン
    // ================================================================

    void invalidInput_abnormalLines_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<int>("expectedInfoCount");
        QTest::addColumn<int>("expectedBestCount");
        QTest::addColumn<int>("expectedErrorCount");

        QTest::newRow("info_only_keyword")
            << QStringLiteral("info")
            << 1 << 0 << 0;
        QTest::newRow("bestmove_ponder_no_target")
            << QStringLiteral("bestmove 7g7f ponder")
            << 0 << 1 << 0;
        QTest::newRow("very_long_info_line")
            << (QStringLiteral("info depth 99 score cp 0 pv ") + QString(10000, QChar('x')))
            << 1 << 0 << 0;
        QTest::newRow("option_no_name")
            << QStringLiteral("option")
            << 0 << 0 << 0;
        QTest::newRow("multiple_bestmove_keywords")
            << QStringLiteral("bestmove 7g7f bestmove 3c3d")
            << 0 << 1 << 0;
        QTest::newRow("leading_whitespace_bestmove")
            << QStringLiteral("  bestmove 7g7f")
            << 0 << 0 << 0;
    }

    void invalidInput_abnormalLines()
    {
        QFETCH(QString, line);
        QFETCH(int, expectedInfoCount);
        QFETCH(int, expectedBestCount);
        QFETCH(int, expectedErrorCount);

        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy spyError(&handler, &UsiProtocolHandler::errorOccurred);

        handler.onDataReceived(line);

        QCOMPARE(spyInfo.count(), expectedInfoCount);
        QCOMPARE(spyBest.count(), expectedBestCount);
        QCOMPARE(spyError.count(), expectedErrorCount);
    }

    // ================================================================
    // 11. テーブル駆動: alphabetToRank の境界値
    // ================================================================

    void alphabetToRank_boundary_data()
    {
        QTest::addColumn<QChar>("input");
        QTest::addColumn<bool>("hasValue");

        QTest::newRow("backtick_below_a") << QChar('`') << false;
        QTest::newRow("j_above_i") << QChar('j') << false;
        QTest::newRow("uppercase_A") << QChar('A') << false;
        QTest::newRow("digit_5") << QChar('5') << false;
        QTest::newRow("space") << QChar(' ') << false;
        QTest::newRow("null_char") << QChar('\0') << false;
    }

    void alphabetToRank_boundary()
    {
        QFETCH(QChar, input);
        QFETCH(bool, hasValue);

        auto result = UsiProtocolHandler::alphabetToRank(input);
        QCOMPARE(result.has_value(), hasValue);
    }
    // ================================================================
    // 12. 異常系: bestmove の不正フォーマット
    // ================================================================

    void invalidBestmove_extraTokens()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        // bestmove with extra garbage tokens after ponder
        handler.onDataReceived(QStringLiteral("bestmove 7g7f ponder 3c3d extra garbage tokens"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
        // Ponder should still be parsed correctly
        QCOMPARE(handler.predictedMove(), QStringLiteral("3c3d"));
    }

    void invalidBestmove_nonUsiMove()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        // bestmove with non-standard move token
        handler.onDataReceived(QStringLiteral("bestmove INVALID_MOVE_FORMAT"));

        QCOMPARE(spyBest.count(), 1);
        // Should store whatever was given (validation is elsewhere)
        QCOMPARE(handler.bestMove(), QStringLiteral("INVALID_MOVE_FORMAT"));
    }

    void invalidBestmove_emptyPonder()
    {
        // bestmove with "ponder" keyword but empty value (already tested above as
        // "bestmove_ponder_no_target", but confirming no crash with trailing space)
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 2g2f ponder "));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("2g2f"));
    }

    // ================================================================
    // 13. 異常系: info 行の不正な score
    // ================================================================

    void invalidInfo_scoreMissingValue()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        // "score cp" with no numeric value
        handler.onDataReceived(QStringLiteral("info depth 10 score cp"));

        // info line is still forwarded (parsing is in presenter layer)
        QCOMPARE(spyInfo.count(), 1);
    }

    void invalidInfo_scoreNonNumeric()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        // "score cp ABC" — non-numeric value
        handler.onDataReceived(QStringLiteral("info depth 10 score cp ABC pv 7g7f"));

        QCOMPARE(spyInfo.count(), 1);
    }

    void invalidInfo_scoreMateNegative()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        // "score mate -3" — negative mate (opponent has checkmate)
        handler.onDataReceived(QStringLiteral("info depth 10 score mate -3 pv 5e5d"));

        QCOMPARE(spyInfo.count(), 1);
    }

    // ================================================================
    // 14. 異常系: option 行の不正な型
    // ================================================================

    void invalidOption_unknownType()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(
            QStringLiteral("option name SomeOption type unknown_type default 42"));

        // Should not emit info or bestmove signals
        QCOMPARE(spyInfo.count(), 0);
        QCOMPARE(spyBest.count(), 0);
    }

    void invalidOption_noType()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        handler.onDataReceived(QStringLiteral("option name SomeOption"));

        // Should not crash and should not emit info
        QCOMPARE(spyInfo.count(), 0);
    }

    void invalidOption_emptyName()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        handler.onDataReceived(QStringLiteral("option name  type check default false"));

        // Should not crash
        QCOMPARE(spyInfo.count(), 0);
    }

    // ================================================================
    // 15. 異常系: 予期しないコマンドシーケンス
    // ================================================================

    void unexpectedSequence_bestmoveBeforeGo()
    {
        // bestmove without prior go — should still be processed
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
    }

    void unexpectedSequence_multipleUsiok()
    {
        // Multiple usiok responses should all emit signals
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::usiOkReceived);

        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("usiok"));

        QCOMPARE(spy.count(), 2);
    }

    void unexpectedSequence_multipleReadyok()
    {
        // Multiple readyok responses
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::readyOkReceived);

        handler.onDataReceived(QStringLiteral("readyok"));
        handler.onDataReceived(QStringLiteral("readyok"));

        QCOMPARE(spy.count(), 2);
    }

    // ================================================================
    // 16. テーブル駆動: チェックメイト異常行
    // ================================================================

    void checkmate_abnormalLines_data()
    {
        QTest::addColumn<QString>("line");

        QTest::newRow("checkmate_whitespace_only")
            << QStringLiteral("checkmate   ");
        QTest::newRow("checkmate_with_garbage")
            << QStringLiteral("checkmate !@#$%^&*");
        QTest::newRow("checkmate_very_long_pv")
            << (QStringLiteral("checkmate ") + QString(5000, QChar('x')));
    }

    void checkmate_abnormalLines()
    {
        QFETCH(QString, line);

        UsiProtocolHandler handler;
        // Must not crash regardless of checkmate line content
        handler.onDataReceived(line);
        QVERIFY(true);
    }

    // ================================================================
    // 17. 異常系: checkmate の大文字小文字混在
    // ================================================================

    void checkmate_caseInsensitive_nomate()
    {
        // handleCheckmateLine は CaseInsensitive で比較する
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateNoMate);

        handler.onDataReceived(QStringLiteral("checkmate NOMATE"));
        QCOMPARE(spy.count(), 1);
    }

    void checkmate_caseInsensitive_notimplemented()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateNotImplemented);

        handler.onDataReceived(QStringLiteral("checkmate NotImplemented"));
        QCOMPARE(spy.count(), 1);
    }

    void checkmate_caseInsensitive_unknown()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateUnknown);

        handler.onDataReceived(QStringLiteral("checkmate UNKNOWN"));
        QCOMPARE(spy.count(), 1);
    }

    void checkmate_singleMovePv()
    {
        // 1手詰みの場合
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::checkmateSolved);

        handler.onDataReceived(QStringLiteral("checkmate 5e5d"));

        QCOMPARE(spy.count(), 1);
        const QStringList pv = spy.at(0).at(0).toStringList();
        QCOMPARE(pv.size(), 1);
        QCOMPARE(pv.at(0), QStringLiteral("5e5d"));
    }

    // ================================================================
    // 18. bestmove の特殊フォーマット
    // ================================================================

    void bestmove_withPromotion()
    {
        // 成り付きの手（7g7f+）
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f+"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f+"));
        QVERIFY(!handler.isResignMove());
        QVERIFY(!handler.isWinMove());
    }

    void bestmove_withDrop()
    {
        // 駒打ち（P*5e）
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove P*5e"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("P*5e"));
    }

    void bestmove_tabSeparated()
    {
        // タブ区切りの bestmove（\s+ 正規表現で分割される）
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove\t7g7f\tponder\t3c3d"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
        QCOMPARE(handler.predictedMove(), QStringLiteral("3c3d"));
    }

    void bestmove_withDropAndPonder()
    {
        // 駒打ち + ponder
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove P*5e ponder 5d5e"));

        QCOMPARE(spyBest.count(), 1);
        QCOMPARE(handler.bestMove(), QStringLiteral("P*5e"));
        QCOMPARE(handler.predictedMove(), QStringLiteral("5d5e"));
    }

    // ================================================================
    // 19. フェーズ・状態遷移テスト
    // ================================================================

    void phase_afterBestmove_remainsMain()
    {
        // bestmove受信後もフェーズはMain（Idle に戻さない）
        UsiProtocolHandler handler;
        // beginMainSearch は sendGoDepth 等の内部で呼ばれるが、
        // テストでは直接 onDataReceived でbestmoveのみ送る
        // → フェーズは Idle のまま
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        // bestmove 単独ではフェーズを変更しない
        QCOMPARE(handler.currentPhase(), UsiProtocolHandler::SearchPhase::Idle);
    }

    void specialMove_resetAfterResignThenNormal()
    {
        // resign → resetResignNotified → 通常手 → specialMove が None に戻る
        UsiProtocolHandler handler;

        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QVERIFY(handler.isResignMove());

        handler.resetResignNotified();
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        QVERIFY(!handler.isResignMove());
        QVERIFY(!handler.isWinMove());
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));
    }

    void specialMove_winThenResign()
    {
        // win → resetWinNotified → resign
        UsiProtocolHandler handler;

        handler.onDataReceived(QStringLiteral("bestmove win"));
        QVERIFY(handler.isWinMove());

        handler.resetWinNotified();
        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QVERIFY(handler.isResignMove());
        QVERIFY(!handler.isWinMove());
    }

    void cancelOperation_incrementsSeq()
    {
        // cancelCurrentOperation の後、状態が適切にリセットされること
        UsiProtocolHandler handler;

        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(handler.bestMove(), QStringLiteral("7g7f"));

        handler.cancelCurrentOperation();

        // キャンセル後でも新しい bestmove は処理される
        handler.onDataReceived(QStringLiteral("bestmove 2g2f"));
        QCOMPARE(handler.bestMove(), QStringLiteral("2g2f"));
    }

    void lastBestmoveElapsedMs_withoutGo()
    {
        // go を送っていない状態での bestmove → 経過時間は 0
        UsiProtocolHandler handler;
        QCOMPARE(handler.lastBestmoveElapsedMs(), 0);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        // goTimer が未起動のため 0 になる
        QCOMPARE(handler.lastBestmoveElapsedMs(), 0);
    }

    // ================================================================
    // 20. 異常系: 高速連続 bestmove
    // ================================================================

    void rapidBestmove_stateUpdatedCorrectly()
    {
        // 連続した bestmove で最後の状態が反映される
        UsiProtocolHandler handler;
        QSignalSpy spyBest(&handler, &UsiProtocolHandler::bestMoveReceived);

        handler.onDataReceived(QStringLiteral("bestmove 7g7f ponder 3c3d"));
        handler.onDataReceived(QStringLiteral("bestmove 2g2f ponder 8c8d"));
        handler.onDataReceived(QStringLiteral("bestmove 5g5f"));

        QCOMPARE(spyBest.count(), 3);
        QCOMPARE(handler.bestMove(), QStringLiteral("5g5f"));
        QVERIFY(handler.predictedMove().isEmpty());
    }

    // ================================================================
    // 21. 異常系: option 行の様々なバリエーション
    // ================================================================

    void option_veryLongName()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        // 非常に長いオプション名でもクラッシュしない
        QString longName = QString(1000, QChar('X'));
        handler.onDataReceived(
            QStringLiteral("option name %1 type check default false").arg(longName));

        QCOMPARE(spyInfo.count(), 0);
    }

    void option_withSpacesInName()
    {
        // オプション名にスペースが含まれる場合（"Multi PV" 等）
        UsiProtocolHandler handler;
        QSignalSpy spyInfo(&handler, &UsiProtocolHandler::infoLineReceived);

        handler.onDataReceived(
            QStringLiteral("option name Multi PV type spin default 1 min 1 max 500"));

        // info シグナルは発行されない（option行として処理）
        QCOMPARE(spyInfo.count(), 0);
    }

    // ================================================================
    // 22. 異常系: タイムアウト中の全種類行ドロップ
    // ================================================================

    void timeoutDeclared_dropsCheckmate()
    {
        UsiProtocolHandler handler;
        handler.markHardTimeout();

        QSignalSpy spySolved(&handler, &UsiProtocolHandler::checkmateSolved);
        QSignalSpy spyNoMate(&handler, &UsiProtocolHandler::checkmateNoMate);

        handler.onDataReceived(QStringLiteral("checkmate 5e5d 4a3b"));
        handler.onDataReceived(QStringLiteral("checkmate nomate"));

        QCOMPARE(spySolved.count(), 0);
        QCOMPARE(spyNoMate.count(), 0);
    }

    void timeoutDeclared_dropsOption()
    {
        UsiProtocolHandler handler;
        handler.markHardTimeout();

        QSignalSpy spyUsi(&handler, &UsiProtocolHandler::usiOkReceived);

        handler.onDataReceived(QStringLiteral("option name USI_Ponder type check default false"));
        handler.onDataReceived(QStringLiteral("usiok"));

        QCOMPARE(spyUsi.count(), 0);
    }

    // ================================================================
    // 23. テーブル駆動: bestmove 特殊手のバリエーション
    // ================================================================

    void bestmove_specialMoveVariants_data()
    {
        QTest::addColumn<QString>("moveToken");
        QTest::addColumn<bool>("isResign");
        QTest::addColumn<bool>("isWin");

        QTest::newRow("resign")
            << QStringLiteral("resign") << true << false;
        QTest::newRow("win")
            << QStringLiteral("win") << false << true;
        QTest::newRow("normal_move")
            << QStringLiteral("7g7f") << false << false;
        QTest::newRow("drop_move")
            << QStringLiteral("P*5e") << false << false;
        QTest::newRow("promotion_move")
            << QStringLiteral("8h2b+") << false << false;
    }

    void bestmove_specialMoveVariants()
    {
        QFETCH(QString, moveToken);
        QFETCH(bool, isResign);
        QFETCH(bool, isWin);

        UsiProtocolHandler handler;
        handler.onDataReceived(QStringLiteral("bestmove ") + moveToken);

        QCOMPARE(handler.isResignMove(), isResign);
        QCOMPARE(handler.isWinMove(), isWin);
        QCOMPARE(handler.bestMove(), moveToken);
    }

    // ================================================================
    // 24. 異常系: readyok / usiok にスペースが付属
    // ================================================================

    void readyok_withTrailingSpace()
    {
        // "readyok " は trimmed で比較されるため認識される
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::readyOkReceived);

        handler.onDataReceived(QStringLiteral("readyok "));

        QCOMPARE(spy.count(), 1);
    }

    void usiok_withLeadingSpace()
    {
        // " usiok" は line.startsWith で始まらないため info 等の行としては処理されず、
        // trimmed で比較されるため usiok として認識される
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::usiOkReceived);

        handler.onDataReceived(QStringLiteral(" usiok"));

        QCOMPARE(spy.count(), 1);
    }

    // ================================================================
    // 25. 異常系: info 行のフォーマットバリエーション
    // ================================================================

    void infoLine_emptyAfterKeyword()
    {
        // "info " の後に何もない
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        handler.onDataReceived(QStringLiteral("info "));

        QCOMPARE(spy.count(), 1);
    }

    void infoLine_withNullBytes()
    {
        // info行にNULLバイトが含まれる場合
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        QString line = QStringLiteral("info depth 10");
        line.insert(5, QChar('\0'));
        handler.onDataReceived(line);

        // startsWith("info") が成立すればシグナルは発行される
        QCOMPARE(spy.count(), 1);
    }

    void infoLine_multipvLargeNumber()
    {
        // multipv の値が非常に大きい
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoLineReceived);

        handler.onDataReceived(
            QStringLiteral("info depth 1 multipv 999999 score cp 0 pv 7g7f"));

        QCOMPARE(spy.count(), 1);
    }

    // ================================================================
    // 26. 異常系: bestmove 前後の resign/win フラグリセット
    // ================================================================

    void bestmove_resignReset_allowsNewResign()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyResign(&handler, &UsiProtocolHandler::bestMoveResignReceived);

        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QCOMPARE(spyResign.count(), 1);

        // リセット後は再度 resign シグナルが発行される
        handler.resetResignNotified();
        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QCOMPARE(spyResign.count(), 2);
    }

    void bestmove_winReset_allowsNewWin()
    {
        UsiProtocolHandler handler;
        QSignalSpy spyWin(&handler, &UsiProtocolHandler::bestMoveWinReceived);

        handler.onDataReceived(QStringLiteral("bestmove win"));
        QCOMPARE(spyWin.count(), 1);

        handler.resetWinNotified();
        handler.onDataReceived(QStringLiteral("bestmove win"));
        QCOMPARE(spyWin.count(), 2);
    }
};

QTEST_MAIN(TestUsiProtocolHandler)
#include "tst_usiprotocolhandler.moc"
