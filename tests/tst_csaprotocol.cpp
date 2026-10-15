/// @file tst_csaprotocol.cpp
/// @brief CSAプロトコル関連のユニットテスト
///
/// テスト対象:
/// - CsaClient::GameSummary（時間単位変換、初期化、手番判定）
/// - CsaMoveConverter（CSA↔USI変換、駒変換、結果文字列変換）
/// - SfenCsaPositionConverter（CSA↔SFEN局面変換）

#include <QtTest>
#include <QCoreApplication>
#include <QTcpServer>
#include <QSslServer>
#include <QSslConfiguration>
#include <QSslKey>
#include <QFile>
#include <QTcpSocket>
#include <algorithm>

// private メンバへは CsaClient の friend 宣言（SHOGIBOARDQ_TESTING 時のみ有効）経由でアクセスする
#include "csaclient.h"
#include "csamoveconverter.h"
#include "sfencsapositionconverter.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"

class Tst_CsaProtocol : public QObject
{
    Q_OBJECT

private slots:
    void csaClient_extensionRepliesDoNotEndGame()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::InGame;
        client.m_isMyTurn = true;
        QSignalSpy ended(&client, &CsaClient::gameEnded);
        client.processLine(QStringLiteral("##[WHO] BoardQ CSA game test-300-5 +"));
        client.processLine(QStringLiteral("##[WHO] Peer CSA game test-300-5 -"));
        client.processLine(QStringLiteral("##[WHO] +OK"));
        QCOMPARE(ended.count(), 0);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::InGame);
        QVERIFY(client.isMyTurn());

        // 結果の2行の間に拡張応答が挟まっても、元の終局理由と勝敗を保持する。
        client.processLine(QStringLiteral("#RESIGN"));
        client.processLine(QStringLiteral("##[LIST] +OK"));
        QCOMPARE(ended.count(), 0);
        client.processLine(QStringLiteral("#WIN"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(qvariant_cast<CsaClient::GameResult>(ended.at(0).at(0)), CsaClient::GameResult::Win);
        QCOMPARE(qvariant_cast<CsaClient::GameEndCause>(ended.at(0).at(1)), CsaClient::GameEndCause::Resign);
    }

    void csaClient_singleChudanEndsGame()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::InGame;
        client.m_isMyTurn = true;
        QSignalSpy ended(&client, &CsaClient::gameEnded);
        client.processLine(QStringLiteral("#CHUDAN"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(qvariant_cast<CsaClient::GameResult>(ended.at(0).at(0)), CsaClient::GameResult::Chudan);
        QCOMPARE(qvariant_cast<CsaClient::GameEndCause>(ended.at(0).at(1)), CsaClient::GameEndCause::Chudan);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::GameOver);
        QVERIFY(!client.isMyTurn());
        client.processLine(QStringLiteral("#CHUDAN"));
        QCOMPARE(ended.count(), 1);
    }

    void csaMoveConverter_promotedPieceMoveIsNotPromotion()
    {
        ShogiGameController gc;
        QString start = QStringLiteral("lnsgkg1nl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL b B 5");
        gc.newGame(start);
        const std::optional<RecordEntry> played = CsaMoveConverter::applyMoveToBoard(QStringLiteral("+2231UM"), &gc);
        QVERIFY(played);
        QCOMPARE(played->move.move.toString(), QStringLiteral("2b3a"));
        QVERIFY(played->sfen.endsWith(QStringLiteral(" 6")));
    }

    // ========================================
    // GameSummary: 時間単位変換
    // ========================================

    void gameSummary_timeUnitMs_data()
    {
        QTest::addColumn<QString>("timeUnit");
        QTest::addColumn<int>("expectedMs");

        QTest::newRow("1sec")  << QStringLiteral("1sec")  << 1000;
        QTest::newRow("sec")   << QStringLiteral("sec")   << 1000;
        QTest::newRow("1min")  << QStringLiteral("1min")  << 60000;
        QTest::newRow("min")   << QStringLiteral("min")   << 60000;
        QTest::newRow("1msec") << QStringLiteral("1msec") << 1;
        QTest::newRow("msec")  << QStringLiteral("msec")  << 1;
        QTest::newRow("empty") << QString()               << 1000;  // デフォルトは秒
    }

    void gameSummary_timeUnitMs()
    {
        QFETCH(QString, timeUnit);
        QFETCH(int, expectedMs);

        CsaClient::GameSummary gs;
        gs.timeBlack.timeUnit = timeUnit;
        QCOMPARE(gs.timeUnitMs(true), expectedMs);
    }

    /// 持ち時間は Time_Unit で換算し、Time+/Time- があれば手番ごとの値を使う
    void gameSummary_timeControlMs()
    {
        CsaClient::GameSummary gs;
        for (CsaClient::TimeControl* t : {&gs.timeBlack, &gs.timeWhite}) {
            t->timeUnit = QStringLiteral("1min");
            t->totalTime = 10;
            t->byoyomi = 1;
            t->increment = 0;
        }
        QCOMPARE(gs.totalTimeMs(true), 600000);
        QCOMPARE(gs.totalTimeMs(false), 600000);
        QCOMPARE(gs.byoyomiMs(false), 60000);
        QCOMPARE(gs.incrementMs(true), 0);

        // Time+/Time- は先後で別の値を持てる（加算・遅延も含む）
        gs.hasIndividualTime = true;
        gs.timeBlack.timeUnit = QStringLiteral("1sec");
        gs.timeWhite.timeUnit = QStringLiteral("1sec");
        gs.timeBlack.totalTime = 300;
        gs.timeWhite.totalTime = 600;
        gs.timeBlack.byoyomi = 0;
        gs.timeWhite.byoyomi = 30;
        gs.timeBlack.increment = 10;
        gs.timeWhite.increment = 20;
        gs.timeBlack.delay = 3;
        QCOMPARE(gs.totalTimeMs(true), 300000);
        QCOMPARE(gs.totalTimeMs(false), 600000);
        QCOMPARE(gs.byoyomiMs(true), 0);
        QCOMPARE(gs.byoyomiMs(false), 30000);
        QCOMPARE(gs.incrementMs(true), 10000);
        QCOMPARE(gs.incrementMs(false), 20000);
        QCOMPARE(gs.delayMs(true), 3000);
        QCOMPARE(gs.delayMs(false), 0);
    }

    // ========================================
    // GameSummary: 初期化とクリア
    // ========================================

    void gameSummary_clear()
    {
        CsaClient::GameSummary gs;
        gs.gameId = QStringLiteral("test-game-123");
        gs.blackName = QStringLiteral("Sente");
        gs.whiteName = QStringLiteral("Gote");
        gs.myTurn = Turn::Black;
        gs.timeBlack.totalTime = 600;
        gs.timeBlack.byoyomi = 30;
        gs.timeBlack.specified = true;
        gs.moveConsumedTimes.append(3);

        gs.clear();

        QVERIFY(gs.gameId.isEmpty());
        QVERIFY(gs.blackName.isEmpty());
        QVERIFY(gs.whiteName.isEmpty());
        QVERIFY(!gs.myTurn.has_value());
        QCOMPARE(gs.timeBlack.totalTime, 0);
        QCOMPARE(gs.timeBlack.byoyomi, 0);
        QVERIFY(!gs.timeBlack.specified);
        QVERIFY(gs.isTimeUnlimited());
        QVERIFY(gs.moveConsumedTimes.isEmpty());
        QCOMPARE(gs.toMove, Turn::Black);
        QCOMPARE(gs.protocolMode, QStringLiteral("Server"));
        QCOMPARE(gs.timeBlack.timeUnit, QStringLiteral("1sec"));
        QCOMPARE(gs.maxMoves, 0);
        QVERIFY(!gs.rematchOnDraw);
        QVERIFY(!gs.hasIndividualTime);
    }

    // ========================================
    // GameSummary: 手番判定
    // ========================================

    void gameSummary_isBlackSide()
    {
        CsaClient::GameSummary gs;

        gs.myTurn = Turn::Black;
        QVERIFY(gs.isBlackSide());
        QVERIFY(gs.movesFirst());

        gs.myTurn = Turn::White;
        QVERIFY(!gs.isBlackSide());
        QVERIFY(!gs.movesFirst());

        gs.myTurn.reset();
        QVERIFY(!gs.isBlackSide());
        QVERIFY(!gs.movesFirst());
    }

    // ========================================
    // CsaMoveConverter: CSA -> USI 変換
    // ========================================

    void csaToUsi_data()
    {
        QTest::addColumn<QString>("csaMove");
        QTest::addColumn<QString>("expectedUsi");

        // 通常の移動
        QTest::newRow("7776FU") << QStringLiteral("+7776FU") << QStringLiteral("7g7f");
        QTest::newRow("3334FU") << QStringLiteral("-3334FU") << QStringLiteral("3c3d");
        QTest::newRow("2628HI") << QStringLiteral("+2628HI") << QStringLiteral("2f2h");
        QTest::newRow("8822UM") << QStringLiteral("+8822UM") << QStringLiteral("8h2b+");

        // 駒打ち
        QTest::newRow("drop_FU") << QStringLiteral("+0055FU") << QStringLiteral("P*5e");
        QTest::newRow("drop_KA") << QStringLiteral("-0033KA") << QStringLiteral("B*3c");
        QTest::newRow("drop_KI") << QStringLiteral("+0041KI") << QStringLiteral("G*4a");

        // 成り駒（成った結果の表記 → USIでは +）
        QTest::newRow("promote_TO") << QStringLiteral("+2324TO") << QStringLiteral("2c2d+");
        QTest::newRow("promote_RY") << QStringLiteral("-8228RY") << QStringLiteral("8b2h+");
        QTest::newRow("promote_NY") << QStringLiteral("+1112NY") << QStringLiteral("1a1b+");
        QTest::newRow("promote_NK") << QStringLiteral("+2133NK") << QStringLiteral("2a3c+");
        QTest::newRow("promote_NG") << QStringLiteral("+3122NG") << QStringLiteral("3a2b+");

        // 短すぎる入力
        QTest::newRow("too_short") << QStringLiteral("+77") << QString();
        QTest::newRow("empty")     << QString()             << QString();
    }

    void csaToUsi()
    {
        QFETCH(QString, csaMove);
        QFETCH(QString, expectedUsi);

        QCOMPARE(CsaMoveConverter::csaToUsi(csaMove), expectedUsi);
    }

    // ========================================
    // CsaMoveConverter: CSA駒記号 -> USI駒記号
    // ========================================

    void csaPieceToUsi_data()
    {
        QTest::addColumn<QString>("csaPiece");
        QTest::addColumn<QString>("expectedUsi");

        QTest::newRow("FU") << QStringLiteral("FU") << QStringLiteral("P");
        QTest::newRow("KY") << QStringLiteral("KY") << QStringLiteral("L");
        QTest::newRow("KE") << QStringLiteral("KE") << QStringLiteral("N");
        QTest::newRow("GI") << QStringLiteral("GI") << QStringLiteral("S");
        QTest::newRow("KI") << QStringLiteral("KI") << QStringLiteral("G");
        QTest::newRow("KA") << QStringLiteral("KA") << QStringLiteral("B");
        QTest::newRow("HI") << QStringLiteral("HI") << QStringLiteral("R");
        QTest::newRow("OU") << QStringLiteral("OU") << QStringLiteral("K");
        QTest::newRow("TO") << QStringLiteral("TO") << QStringLiteral("+P");
        QTest::newRow("NY") << QStringLiteral("NY") << QStringLiteral("+L");
        QTest::newRow("NK") << QStringLiteral("NK") << QStringLiteral("+N");
        QTest::newRow("NG") << QStringLiteral("NG") << QStringLiteral("+S");
        QTest::newRow("UM") << QStringLiteral("UM") << QStringLiteral("+B");
        QTest::newRow("RY") << QStringLiteral("RY") << QStringLiteral("+R");
    }

    void csaPieceToUsi()
    {
        QFETCH(QString, csaPiece);
        QFETCH(QString, expectedUsi);

        QCOMPARE(CsaMoveConverter::csaPieceToUsi(csaPiece), expectedUsi);
    }

    // ========================================
    // CsaMoveConverter: USI駒記号 -> CSA駒記号
    // ========================================

    void usiPieceToCsa_data()
    {
        QTest::addColumn<QString>("usiPiece");
        QTest::addColumn<bool>("promoted");
        QTest::addColumn<QString>("expectedCsa");

        QTest::newRow("P")        << QStringLiteral("P") << false << QStringLiteral("FU");
        QTest::newRow("L")        << QStringLiteral("L") << false << QStringLiteral("KY");
        QTest::newRow("N")        << QStringLiteral("N") << false << QStringLiteral("KE");
        QTest::newRow("S")        << QStringLiteral("S") << false << QStringLiteral("GI");
        QTest::newRow("G")        << QStringLiteral("G") << false << QStringLiteral("KI");
        QTest::newRow("B")        << QStringLiteral("B") << false << QStringLiteral("KA");
        QTest::newRow("R")        << QStringLiteral("R") << false << QStringLiteral("HI");
        QTest::newRow("K")        << QStringLiteral("K") << false << QStringLiteral("OU");
        QTest::newRow("P_prom")   << QStringLiteral("P") << true  << QStringLiteral("TO");
        QTest::newRow("L_prom")   << QStringLiteral("L") << true  << QStringLiteral("NY");
        QTest::newRow("N_prom")   << QStringLiteral("N") << true  << QStringLiteral("NK");
        QTest::newRow("S_prom")   << QStringLiteral("S") << true  << QStringLiteral("NG");
        QTest::newRow("B_prom")   << QStringLiteral("B") << true  << QStringLiteral("UM");
        QTest::newRow("R_prom")   << QStringLiteral("R") << true  << QStringLiteral("RY");
    }

    void usiPieceToCsa()
    {
        QFETCH(QString, usiPiece);
        QFETCH(bool, promoted);
        QFETCH(QString, expectedCsa);

        QCOMPARE(CsaMoveConverter::usiPieceToCsa(usiPiece, promoted), expectedCsa);
    }

    // ========================================
    // CsaMoveConverter: CSA駒記号 -> 漢字
    // ========================================

    void csaPieceToKanji_data()
    {
        QTest::addColumn<QString>("csaPiece");
        QTest::addColumn<QString>("expectedKanji");

        QTest::newRow("FU") << QStringLiteral("FU") << QStringLiteral("歩");
        QTest::newRow("KY") << QStringLiteral("KY") << QStringLiteral("香");
        QTest::newRow("KE") << QStringLiteral("KE") << QStringLiteral("桂");
        QTest::newRow("GI") << QStringLiteral("GI") << QStringLiteral("銀");
        QTest::newRow("KI") << QStringLiteral("KI") << QStringLiteral("金");
        QTest::newRow("KA") << QStringLiteral("KA") << QStringLiteral("角");
        QTest::newRow("HI") << QStringLiteral("HI") << QStringLiteral("飛");
        QTest::newRow("OU") << QStringLiteral("OU") << QStringLiteral("玉");
        QTest::newRow("TO") << QStringLiteral("TO") << QStringLiteral("と");
        QTest::newRow("NY") << QStringLiteral("NY") << QStringLiteral("成香");
        QTest::newRow("NK") << QStringLiteral("NK") << QStringLiteral("成桂");
        QTest::newRow("NG") << QStringLiteral("NG") << QStringLiteral("成銀");
        QTest::newRow("UM") << QStringLiteral("UM") << QStringLiteral("馬");
        QTest::newRow("RY") << QStringLiteral("RY") << QStringLiteral("龍");
    }

    void csaPieceToKanji()
    {
        QFETCH(QString, csaPiece);
        QFETCH(QString, expectedKanji);

        QCOMPARE(CsaMoveConverter::csaPieceToKanji(csaPiece), expectedKanji);
    }

    // ========================================
    // CsaMoveConverter: 駒タイプインデックス
    // ========================================

    void pieceTypeFromCsa_data()
    {
        QTest::addColumn<QString>("csaPiece");
        QTest::addColumn<int>("expectedType");

        QTest::newRow("FU") << QStringLiteral("FU") << 1;
        QTest::newRow("TO") << QStringLiteral("TO") << 1;
        QTest::newRow("KY") << QStringLiteral("KY") << 2;
        QTest::newRow("NY") << QStringLiteral("NY") << 2;
        QTest::newRow("KE") << QStringLiteral("KE") << 3;
        QTest::newRow("NK") << QStringLiteral("NK") << 3;
        QTest::newRow("GI") << QStringLiteral("GI") << 4;
        QTest::newRow("NG") << QStringLiteral("NG") << 4;
        QTest::newRow("KI") << QStringLiteral("KI") << 5;
        QTest::newRow("KA") << QStringLiteral("KA") << 6;
        QTest::newRow("UM") << QStringLiteral("UM") << 6;
        QTest::newRow("HI") << QStringLiteral("HI") << 7;
        QTest::newRow("RY") << QStringLiteral("RY") << 7;
        QTest::newRow("OU") << QStringLiteral("OU") << 8;
        QTest::newRow("XX") << QStringLiteral("XX") << 0;
    }

    void pieceTypeFromCsa()
    {
        QFETCH(QString, csaPiece);
        QFETCH(int, expectedType);

        QCOMPARE(CsaMoveConverter::pieceTypeFromCsa(csaPiece), expectedType);
    }

    void csaPieceToSfenPiece_unknownReturnsNone()
    {
        QCOMPARE(CsaMoveConverter::csaPieceToSfenPiece(QStringLiteral("XX"), true), Piece::None);
        QCOMPARE(CsaMoveConverter::csaPieceToSfenPiece(QString(), false), Piece::None);
    }

    // ========================================
    // CsaMoveConverter: 対局結果文字列
    // ========================================

    void gameResultToString()
    {
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Win).isEmpty());
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Lose).isEmpty());
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Draw).isEmpty());
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Censored).isEmpty());
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Chudan).isEmpty());
        QVERIFY(!CsaMoveConverter::gameResultToString(CsaClient::GameResult::Unknown).isEmpty());
    }

    // ========================================
    // CsaMoveConverter: 終局原因文字列
    // ========================================

    void gameEndCauseToString()
    {
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Resign).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::TimeUp).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::IllegalMove).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Sennichite).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::OuteSennichite).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Jishogi).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::MaxMoves).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Chudan).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::IllegalAction).isEmpty());
        QVERIFY(!CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Unknown).isEmpty());

        // 各結果が異なる文字列であることを確認
        QVERIFY(CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Resign)
                != CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::TimeUp));
    }

    // ========================================
    // CsaMoveConverter: 表示用変換（csaToPretty）
    // ========================================

    void csaToPretty_normalMove()
    {
        // ▲７六歩(77) — 通常の移動
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+7776FU"), false, BoardSquare{});
        QVERIFY(result.contains(QStringLiteral("▲")));
        QVERIFY(result.contains(QStringLiteral("歩")));
        QVERIFY(result.contains(QStringLiteral("(77)")));
    }

    void csaToPretty_goteMove()
    {
        // △３四歩(33)
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("-3334FU"), false, BoardSquare{});
        QVERIFY(result.contains(QStringLiteral("△")));
        QVERIFY(result.contains(QStringLiteral("歩")));
    }

    void csaToPretty_drop()
    {
        // ▲５五歩打
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+0055FU"), false, BoardSquare{});
        QVERIFY(result.contains(QStringLiteral("打")));
        QVERIFY(result.contains(QStringLiteral("歩")));
    }

    void csaToPretty_sameSquare()
    {
        // 「同」表記 — 前の手の移動先と同じ座標への移動
        // -3334GI: from(3,3) to(3,4)、前の手が(3,4)への移動 → 同銀
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("-3334GI"), false, BoardSquare{3, 4});
        QVERIFY(result.contains(QStringLiteral("同")));
        QVERIFY(result.contains(QStringLiteral("銀")));
        QVERIFY(result.contains(QStringLiteral("(33)")));  // 移動元
    }

    void csaToPretty_promotion()
    {
        // 成り: isPromotion=true の場合、成る前の駒名+「成」
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+8822UM"), true, BoardSquare{});
        QVERIFY(result.contains(QStringLiteral("角")));  // 成る前の駒名
        QVERIFY(result.contains(QStringLiteral("成")));
    }

    void csaToPretty_shortInput()
    {
        // 短すぎる入力はそのまま返す
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+77"), false, BoardSquare{});
        QCOMPARE(result, QStringLiteral("+77"));
    }

    // ========================================
    // SfenCsaPositionConverter: CSA -> SFEN 平手局面
    // ========================================

    void fromCsaPositionLines_hirate_pi()
    {
        // PI形式（平手初期局面指定）
        QStringList csaLines = {
            QStringLiteral("PI"),
            QStringLiteral("+"),
        };

        QString error;
        auto sfen = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &error);
        QVERIFY2(sfen.has_value(), qPrintable(error));
        QVERIFY(!sfen->isEmpty());
        QVERIFY(sfen->startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL")));
    }

    void fromCsaPositionLines_pn_fullPieces()
    {
        // P1-P9形式: 全マスが駒で埋まる行は trimming の影響を受けない
        QStringList csaLines = {
            QStringLiteral("P1-KY-KE-GI-KI-OU-KI-GI-KE-KY"),
            QStringLiteral("P3-FU-FU-FU-FU-FU-FU-FU-FU-FU"),
            QStringLiteral("P7+FU+FU+FU+FU+FU+FU+FU+FU+FU"),
            QStringLiteral("P9+KY+KE+GI+KI+OU+KI+GI+KE+KY"),
            QStringLiteral("+"),
        };

        QString error;
        auto sfen = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &error);
        QVERIFY2(sfen.has_value(), qPrintable(error));
        QVERIFY(sfen->contains(QStringLiteral("lnsgkgsnl")));
        QVERIFY(sfen->contains(QStringLiteral("ppppppppp")));
        QVERIFY(sfen->contains(QStringLiteral("PPPPPPPPP")));
        QVERIFY(sfen->contains(QStringLiteral("LNSGKGSNL")));
    }

    // ========================================
    // SfenCsaPositionConverter: SFEN -> CSA 変換
    // ========================================

    void sfenToCsa_hirate()
    {
        const QString sfen =
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

        QString error;
        auto csaLines = SfenCsaPositionConverter::toCsaPositionLines(sfen, &error);
        QVERIFY2(csaLines.has_value(), qPrintable(error));
        QVERIFY(!csaLines->isEmpty());
        // P1 行が含まれること
        bool hasP1 = false;
        for (const QString& line : std::as_const(*csaLines)) {
            if (line.startsWith(QStringLiteral("P1"))) {
                hasP1 = true;
                QVERIFY(line.contains(QStringLiteral("-KY")));
                QVERIFY(line.contains(QStringLiteral("-OU")));
            }
        }
        QVERIFY(hasP1);
    }

    // ========================================
    // SfenCsaPositionConverter: 空の入力
    // ========================================

    void fromCsaPositionLines_empty()
    {
        QStringList csaLines;
        QString error;
        auto sfen = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &error);
        // 空の入力は失敗するか空のSFENを返す
        if (sfen) {
            // 成功する場合もある（空局面として処理）
            QVERIFY(true);
        } else {
            QVERIFY(!error.isEmpty());
        }
    }

    // ========================================
    // CsaClient: 初期状態
    // ========================================

    void csaClient_initialState()
    {
        CsaClient client;
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
        QVERIFY(!client.isConnected());
        QVERIFY(!client.isMyTurn());
    }

    // ========================================
    // CsaClient: 接続状態の列挙値
    // ========================================

    void csaClient_connectionStateEnum()
    {
        // 全ての接続状態が定義されていることを確認
        QVERIFY(static_cast<int>(CsaClient::ConnectionState::Disconnected) == 0);
        QVERIFY(static_cast<int>(CsaClient::ConnectionState::GameOver) == 7);
    }

    // ========================================
    // CsaClient: 対局結果の列挙値
    // ========================================

    void csaClient_gameResultEnum()
    {
        // 全ての結果が定義されていることを確認
        QVERIFY(static_cast<int>(CsaClient::GameResult::Win) == 0);
        QVERIFY(static_cast<int>(CsaClient::GameResult::Unknown) == 5);
    }

    // ========================================
    // CsaClient: 終局原因の列挙値
    // ========================================

    void csaClient_gameEndCauseEnum()
    {
        // 全ての原因が定義されていることを確認
        QVERIFY(static_cast<int>(CsaClient::GameEndCause::Resign) == 0);
        QVERIFY(static_cast<int>(CsaClient::GameEndCause::Unknown) == 9);
    }

    // ========================================
    // Table-driven: csaToUsi 異常入力の堅牢性
    // ========================================

    void csaToUsi_robustness_data()
    {
        QTest::addColumn<QString>("csaMove");
        QTest::addColumn<bool>("expectEmpty");

        QTest::newRow("single_char")
            << QStringLiteral("X") << true;
        QTest::newRow("prefix_only")
            << QStringLiteral("+") << true;
        QTest::newRow("three_chars")
            << QStringLiteral("+77") << true;
        QTest::newRow("six_chars_one_short")
            << QStringLiteral("+7776F") << true;
        QTest::newRow("non_digit_coordinates")
            << QStringLiteral("+ABCDFU") << true;
        QTest::newRow("whitespace_7chars")
            << QStringLiteral("       ") << true;
    }

    void csaToUsi_robustness()
    {
        QFETCH(QString, csaMove);
        QFETCH(bool, expectEmpty);

        QString result = CsaMoveConverter::csaToUsi(csaMove);
        if (expectEmpty) {
            QVERIFY(result.isEmpty());
        }
        // All cases: must not crash
    }

    // ========================================
    // Table-driven: SfenCsaPositionConverter 異常入力
    // ========================================

    void fromCsaPositionLines_abnormal_data()
    {
        QTest::addColumn<QStringList>("csaLines");

        QTest::newRow("invalid_rank_line")
            << QStringList{QStringLiteral("PX-KY-KE-GI-KI-OU-KI-GI-KE-KY"), QStringLiteral("+")};
        QTest::newRow("turn_indicator_only")
            << QStringList{QStringLiteral("+")};
        QTest::newRow("garbage_lines")
            << QStringList{QStringLiteral("foo"), QStringLiteral("bar"), QStringLiteral("baz")};
    }

    void fromCsaPositionLines_abnormal()
    {
        QFETCH(QStringList, csaLines);

        QString error;
        const auto sfen = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &error);
        QVERIFY(!sfen.has_value());
        QVERIFY(!error.isEmpty());
    }

    // ========================================
    // Table-driven: csaPieceToUsi 異常入力
    // ========================================

    void csaPieceToUsi_abnormal_data()
    {
        QTest::addColumn<QString>("csaPiece");
        QTest::addColumn<QString>("expectedUsi");

        // 未知駒種は空文字列を返して異常を呼び出し側に通知する
        QTest::newRow("empty_string")
            << QString() << QString();
        QTest::newRow("single_char")
            << QStringLiteral("F") << QString();
        QTest::newRow("unknown_two_chars")
            << QStringLiteral("XX") << QString();
        QTest::newRow("lowercase")
            << QStringLiteral("fu") << QString();
        QTest::newRow("three_chars")
            << QStringLiteral("FUX") << QString();
    }

    void csaPieceToUsi_abnormal()
    {
        QFETCH(QString, csaPiece);
        QFETCH(QString, expectedUsi);

        QString result = CsaMoveConverter::csaPieceToUsi(csaPiece);
        QCOMPARE(result, expectedUsi);
    }

    // ========================================
    // Table-driven: GameSummary 異常な時間単位
    // ========================================

    void gameSummary_timeUnitMs_abnormal_data()
    {
        QTest::addColumn<QString>("timeUnit");
        QTest::addColumn<int>("expectedMs");

        QTest::newRow("unknown_unit")
            << QStringLiteral("1hour") << 1000;
        QTest::newRow("numeric_only")
            << QStringLiteral("1000") << 1000;
        QTest::newRow("garbage")
            << QStringLiteral("xyz") << 1000;
        QTest::newRow("whitespace")
            << QStringLiteral("  ") << 1000;
    }

    void gameSummary_timeUnitMs_abnormal()
    {
        QFETCH(QString, timeUnit);
        QFETCH(int, expectedMs);

        CsaClient::GameSummary gs;
        gs.timeWhite.timeUnit = timeUnit;
        QCOMPARE(gs.timeUnitMs(false), expectedMs);
    }
    // ========================================
    // 異常系: CsaClient 不正なログインレスポンス
    // ========================================

    void csaClient_invalidLoginResponse()
    {
        // CsaClient should handle unexpected server responses gracefully
        CsaClient client;
        // Verify initial state is safe (actual server interaction requires TCP)
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
        QVERIFY(!client.isConnected());
    }

    // ========================================
    // 異常系: GameSummary 不完全なデータ
    // ========================================

    void gameSummary_incompleteData()
    {
        // A GameSummary with missing fields should still be usable
        CsaClient::GameSummary gs;
        // Defaults should be safe
        QCOMPARE(gs.timeBlack.totalTime, 0);
        QCOMPARE(gs.timeWhite.byoyomi, 0);
        QCOMPARE(gs.maxMoves, 0);
        QVERIFY(gs.blackName.isEmpty());
        QVERIFY(gs.whiteName.isEmpty());
        // timeUnitMs should return default even with empty fields
        QCOMPARE(gs.timeUnitMs(true), 1000);
        QCOMPARE(gs.timeUnitMs(false), 1000);
    }

    // ========================================
    // 異常系: csaToPretty ロバストネス
    // ========================================

    void csaToPretty_robustness_data()
    {
        QTest::addColumn<QString>("csaMove");

        QTest::newRow("empty_string")
            << QString();
        QTest::newRow("single_plus")
            << QStringLiteral("+");
        QTest::newRow("only_coordinates")
            << QStringLiteral("+7776");
        QTest::newRow("digit_string")
            << QStringLiteral("1234567");
    }

    void csaToPretty_robustness()
    {
        QFETCH(QString, csaMove);

        // Must not crash regardless of input
        (void)CsaMoveConverter::csaToPretty(csaMove, false, BoardSquare{});
        QVERIFY(true);
    }

    // ========================================
    // 異常系: CsaMoveConverter usiPieceToCsa 不正入力
    // ========================================

    void usiPieceToCsa_abnormal_data()
    {
        QTest::addColumn<QString>("usiPiece");
        QTest::addColumn<bool>("promoted");
        QTest::addColumn<bool>("expectedEmpty");

        QTest::newRow("empty_not_promoted")  << QString()             << false << true;
        QTest::newRow("empty_promoted")      << QString()             << true  << true;
        QTest::newRow("unknown_piece")       << QStringLiteral("X")   << false << true;
        QTest::newRow("lowercase_p")         << QStringLiteral("p")   << false << true;
        QTest::newRow("multi_char")          << QStringLiteral("PK")  << false << true;
    }

    void usiPieceToCsa_abnormal()
    {
        QFETCH(QString, usiPiece);
        QFETCH(bool, promoted);
        QFETCH(bool, expectedEmpty);

        const QString result = CsaMoveConverter::usiPieceToCsa(usiPiece, promoted);
        QCOMPARE(result.isEmpty(), expectedEmpty);
    }

    // ========================================
    // 異常系: SfenCsaPositionConverter SFEN 不正入力
    // ========================================

    void toCsaPositionLines_abnormal_data()
    {
        QTest::addColumn<QString>("sfen");

        QTest::newRow("empty_string")
            << QString();
        QTest::newRow("garbage_string")
            << QStringLiteral("not a valid sfen at all");
        QTest::newRow("incomplete_sfen")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp");
        QTest::newRow("invalid_chars")
            << QStringLiteral("XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/9/9/9/9/9/9 b - 1");
    }

    void toCsaPositionLines_abnormal()
    {
        QFETCH(QString, sfen);

        QString error;
        // Must not crash regardless of input
        (void)SfenCsaPositionConverter::toCsaPositionLines(sfen, &error);
        QVERIFY(true);
    }

    // ========================================
    // 異常系: CsaClient 状態ガード（不正状態でのメソッド呼び出し）
    // ========================================

    void csaClient_connectToServer_invalidPortNegative()
    {
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);
        QSignalSpy spyState(&client, &CsaClient::connectionStateChanged);

        client.connectToServer(QStringLiteral("127.0.0.1"), -1);

        QCOMPARE(spyError.count(), 1);
        QCOMPARE(spyState.count(), 0);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
    }

    void csaClient_connectToServer_invalidPortTooLarge()
    {
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);
        QSignalSpy spyState(&client, &CsaClient::connectionStateChanged);

        client.connectToServer(QStringLiteral("127.0.0.1"), 70000);

        QCOMPARE(spyError.count(), 1);
        QCOMPARE(spyState.count(), 0);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
    }

    void csaClient_loginWhenDisconnected()
    {
        // Disconnected 状態で login → errorOccurred が発行される
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.login(QStringLiteral("user"), QStringLiteral("pass"));

        QCOMPARE(spyError.count(), 1);
    }

    void csaClient_agreeWhenDisconnected()
    {
        // Disconnected 状態で agree → errorOccurred が発行される
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.agree();

        QCOMPARE(spyError.count(), 1);
    }

    void csaClient_sendMoveWhenDisconnected()
    {
        // Disconnected 状態で sendMove → errorOccurred が発行される
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.sendMove(QStringLiteral("+7776FU"));

        QCOMPARE(spyError.count(), 1);
    }

    void csaClient_resignWhenDisconnected()
    {
        // Disconnected 状態で resign → 何も起きない（静かに無視）
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.resign();

        QCOMPARE(spyError.count(), 0);
    }

    void csaClient_declareWinWhenDisconnected()
    {
        // Disconnected 状態で declareWin → 何も起きない
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.declareWin();

        QCOMPARE(spyError.count(), 0);
    }

    void csaClient_requestChudanWhenDisconnected()
    {
        // Disconnected 状態で requestChudan → 何も起きない
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.requestChudan();

        QCOMPARE(spyError.count(), 0);
    }

    void csaClient_rejectWhenDisconnected()
    {
        // Disconnected 状態で reject → 何も起きない（静かに無視）
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.reject();

        QCOMPARE(spyError.count(), 0);
    }

    void csaClient_logoutWhenDisconnected()
    {
        // Disconnected 状態で logout → 何も起きない（静かに無視）
        CsaClient client;
        QSignalSpy spyError(&client, &CsaClient::errorOccurred);

        client.logout();

        QCOMPARE(spyError.count(), 0);
    }

    void csaClient_disconnectFromServerWhenAlreadyDisconnected()
    {
        // Disconnected 状態で disconnectFromServer → 安全に無視される
        CsaClient client;
        QSignalSpy spyState(&client, &CsaClient::connectionStateChanged);

        client.disconnectFromServer();

        // Disconnected → Disconnected は状態変化なし
        QCOMPARE(spyState.count(), 0);
    }

    void csaClient_resetSessionState_clearsParserState()
    {
        CsaClient client;

        client.m_receiveBuffer = QByteArrayLiteral("partial");
        client.m_gameSummary.gameId = QStringLiteral("game-id");
        client.m_isMyTurn = true;
        client.m_summarySection = CsaClient::SummarySection::TimeBlack;
        client.m_pendingFirstResultLine = QStringLiteral("#RESIGN");
        client.m_endMoveConsumedTimeMs = 1234;
        client.m_connectionState = CsaClient::ConnectionState::InGame;

        client.resetSessionState();

        QVERIFY(client.m_receiveBuffer.isEmpty());
        QVERIFY(client.m_gameSummary.gameId.isEmpty());
        QVERIFY(!client.m_isMyTurn);
        QCOMPARE(client.m_summarySection, CsaClient::SummarySection::Outside);
        QVERIFY(client.m_pendingFirstResultLine.isEmpty());
        QCOMPARE(client.m_endMoveConsumedTimeMs, 0);
        // 接続状態は resetSessionState() では変更しない
        QCOMPARE(client.m_connectionState, CsaClient::ConnectionState::InGame);
    }

    void csaClient_processGameSummary_invalidNumeric_keepsCurrentValues()
    {
        CsaClient client;
        client.m_summarySection = CsaClient::SummarySection::Time;
        for (CsaClient::TimeControl* t : {&client.m_gameSummary.timeBlack, &client.m_gameSummary.timeWhite}) {
            t->totalTime = 600;
            t->byoyomi = 30;
            t->delay = 5;
        }

        client.processGameSummary(QStringLiteral("Total_Time:abc"));
        client.processGameSummary(QStringLiteral("Byoyomi:-1"));
        client.processGameSummary(QStringLiteral("Delay:xyz"));

        for (const CsaClient::TimeControl* t : {&client.m_gameSummary.timeBlack, &client.m_gameSummary.timeWhite}) {
            QCOMPARE(t->totalTime, 600);
            QCOMPARE(t->byoyomi, 30);
            QCOMPARE(t->delay, 5);
        }

        client.m_summarySection = CsaClient::SummarySection::TopLevel;
        client.m_gameSummary.maxMoves = 256;
        client.processGameSummary(QStringLiteral("Max_Moves:not-a-number"));
        QCOMPARE(client.m_gameSummary.maxMoves, 256);
    }

    // ========================================
    // 異常系: GameSummary 個別時間設定
    // ========================================

    void gameSummary_individualTime()
    {
        CsaClient::GameSummary gs;

        // 個別時間が設定された場合
        gs.hasIndividualTime = true;
        gs.timeBlack.totalTime = 600;
        gs.timeWhite.totalTime = 300;
        gs.timeBlack.byoyomi = 30;
        gs.timeWhite.byoyomi = 60;

        QCOMPARE(gs.timeFor(true).totalTime, 600);
        QCOMPARE(gs.timeFor(false).totalTime, 300);
        QCOMPARE(gs.timeFor(true).byoyomi, 30);
        QCOMPARE(gs.timeFor(false).byoyomi, 60);

        // clear で全てリセットされる
        gs.clear();
        QVERIFY(!gs.hasIndividualTime);
        QCOMPARE(gs.timeBlack.totalTime, 0);
        QCOMPARE(gs.timeWhite.totalTime, 0);
        QCOMPARE(gs.timeBlack.byoyomi, 0);
        QCOMPARE(gs.timeWhite.byoyomi, 0);
    }

    void gameSummary_positionAndMovesClear()
    {
        CsaClient::GameSummary gs;
        gs.positionLines.append(QStringLiteral("PI"));
        gs.moves.append(QStringLiteral("+7776FU"));
        gs.moveConsumedTimes.append(12);

        QCOMPARE(gs.positionLines.size(), 1);
        QCOMPARE(gs.moves.size(), 1);
        QCOMPARE(gs.positionMoveConsumedMs(0), 12000);
        QCOMPARE(gs.positionMoveConsumedMs(1), 0);

        gs.clear();
        QVERIFY(gs.positionLines.isEmpty());
        QVERIFY(gs.moves.isEmpty());
        QVERIFY(gs.moveConsumedTimes.isEmpty());
    }

    void gameSummary_maxMovesNonNumeric()
    {
        // maxMoves は toInt() で解析される - 0 が返る
        CsaClient::GameSummary gs;
        gs.maxMoves = QStringLiteral("ABC").toInt();  // 0
        QCOMPARE(gs.maxMoves, 0);
    }

    // ========================================
    // テーブル駆動: CsaMoveConverter csaToUsi 成駒バリエーション
    // ========================================

    void csaToUsi_promotedPieces_data()
    {
        QTest::addColumn<QString>("csaMove");
        QTest::addColumn<QString>("expectedUsi");

        // 成駒の移動（成った駒が移動するケース）
        QTest::newRow("TO_move") << QStringLiteral("+2324TO") << QStringLiteral("2c2d+");
        QTest::newRow("RY_move") << QStringLiteral("-8228RY") << QStringLiteral("8b2h+");
        QTest::newRow("UM_move") << QStringLiteral("+8822UM") << QStringLiteral("8h2b+");
        QTest::newRow("NY_move") << QStringLiteral("+1112NY") << QStringLiteral("1a1b+");
        QTest::newRow("NK_move") << QStringLiteral("+2133NK") << QStringLiteral("2a3c+");
        QTest::newRow("NG_move") << QStringLiteral("+3122NG") << QStringLiteral("3a2b+");
    }

    void csaToUsi_promotedPieces()
    {
        QFETCH(QString, csaMove);
        QFETCH(QString, expectedUsi);

        QCOMPARE(CsaMoveConverter::csaToUsi(csaMove), expectedUsi);
    }

    // ========================================
    // CsaMoveConverter: csaPieceToKanji 異常入力
    // ========================================

    void csaPieceToKanji_abnormal_data()
    {
        QTest::addColumn<QString>("csaPiece");

        QTest::newRow("empty_string") << QString();
        QTest::newRow("single_char") << QStringLiteral("F");
        QTest::newRow("unknown_piece") << QStringLiteral("XX");
        QTest::newRow("lowercase") << QStringLiteral("fu");
        QTest::newRow("three_chars") << QStringLiteral("FUX");
    }

    void csaPieceToKanji_abnormal()
    {
        QFETCH(QString, csaPiece);

        // Must not crash; returns some default value
        (void)CsaMoveConverter::csaPieceToKanji(csaPiece);
        QVERIFY(true);
    }

    // ========================================
    // CsaMoveConverter: 全結果値が一意であること
    // ========================================

    void gameResultToString_allDistinct()
    {
        QStringList results;
        results << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Win)
                << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Lose)
                << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Draw)
                << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Censored)
                << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Chudan)
                << CsaMoveConverter::gameResultToString(CsaClient::GameResult::Unknown);

        // 全て非空
        for (const QString& r : std::as_const(results)) {
            QVERIFY(!r.isEmpty());
        }

        // 全て異なる
        QSet<QString> unique(results.begin(), results.end());
        QCOMPARE(unique.size(), results.size());
    }

    void gameEndCauseToString_allDistinct()
    {
        QStringList causes;
        causes << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Resign)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::TimeUp)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::IllegalMove)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Sennichite)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::OuteSennichite)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Jishogi)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::MaxMoves)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Chudan)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::IllegalAction)
               << CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause::Unknown);

        for (const QString& c : std::as_const(causes)) {
            QVERIFY(!c.isEmpty());
        }

        QSet<QString> unique(causes.begin(), causes.end());
        QCOMPARE(unique.size(), causes.size());
    }

    // ========================================
    // 異常系: SfenCsaPositionConverter 双方向変換の整合性
    // ========================================

    void sfenCsaRoundTrip_handicap_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("right-edge-empty") << QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("four-piece") << QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("sparse-with-hands") << QStringLiteral("4k4/9/9/9/4+P4/9/9/9/4K4 b 2R2b 1");
    }

    void sfenCsaRoundTrip_handicap()
    {
        QFETCH(QString, sfen);
        QString error;
        const auto csa = SfenCsaPositionConverter::toCsaPositionLines(sfen, &error);
        QVERIFY2(csa.has_value(), qPrintable(error));
        const auto decoded = SfenCsaPositionConverter::fromCsaPositionLines(*csa, &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        QCOMPARE(*decoded, sfen);
        QStringList crlf;
        for (const auto& line : *csa) crlf.append(line + QStringLiteral("\r\n"));
        QCOMPARE(SfenCsaPositionConverter::fromCsaPositionLines(crlf), decoded);
    }

    void sfenCsaRoundTrip_hirate()
    {
        const QString originalSfen =
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

        QString error;

        // SFEN → CSA
        auto csaLines = SfenCsaPositionConverter::toCsaPositionLines(originalSfen, &error);
        QVERIFY2(csaLines.has_value(), qPrintable(error));
        QVERIFY(!csaLines->isEmpty());

        // CSA → SFEN（ラウンドトリップ）
        auto roundTripSfen = SfenCsaPositionConverter::fromCsaPositionLines(*csaLines, &error);
        QVERIFY2(roundTripSfen.has_value(), qPrintable(error));

        QCOMPARE(*roundTripSfen, originalSfen);
    }

    // ========================================
    // 異常系: CsaMoveConverter csaToUsi 座標境界値
    // ========================================

    void csaToUsi_coordinateBoundary_data()
    {
        QTest::addColumn<QString>("csaMove");
        QTest::addColumn<bool>("expectNonEmpty");

        // 有効な座標範囲 (1-9)
        QTest::newRow("corner_11") << QStringLiteral("+1112FU") << true;
        QTest::newRow("corner_99") << QStringLiteral("-9998FU") << true;
        QTest::newRow("corner_19") << QStringLiteral("+1918FU") << true;
        QTest::newRow("corner_91") << QStringLiteral("-9192FU") << true;

        // 座標0を含む（駒打ちと区別）
        QTest::newRow("zero_from_drop") << QStringLiteral("+0055FU") << true;

        // 非数字座標は不正扱い（空文字列）
        QTest::newRow("alpha_coordinates") << QStringLiteral("+ABCDFU") << false;
    }

    void csaToUsi_coordinateBoundary()
    {
        QFETCH(QString, csaMove);
        QFETCH(bool, expectNonEmpty);

        QString result = CsaMoveConverter::csaToUsi(csaMove);
        QCOMPARE(!result.isEmpty(), expectNonEmpty);
    }

    // ========================================
    // 異常系: CsaMoveConverter csaToPretty 大きな moveCount
    // ========================================

    void csaToPretty_invalidPreviousSquare()
    {
        // 盤外の直前の升でもクラッシュせず、「同」にしない
        const QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+7776FU"), false, BoardSquare{0, 10});
        QVERIFY(!result.isEmpty());
        QVERIFY(!result.contains(QStringLiteral("同")));
    }

    void csaMoveConverter_applyMoveToBoard_dropWithoutStandPieceFails()
    {
        ShogiGameController gc;
        QString startSfen = QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        gc.newGame(startSfen);

        QVERIFY(gc.board() != nullptr);

        const Piece before = gc.board()->pieceCharacter(5, 5);
        QVERIFY(before == Piece::None);

        QVERIFY(!CsaMoveConverter::applyMoveToBoard(QStringLiteral("+0055FU"), &gc));
        QCOMPARE(gc.board()->pieceCharacter(5, 5), Piece::None);
        QCOMPARE(gc.board()->toSfen(), startSfen);
    }

    // ========================================
    // 異常系: CsaClient isConnected / isMyTurn の安全性
    // ========================================

    void csaClient_isConnectedWhenFreshlyCreated()
    {
        CsaClient client;
        QVERIFY(!client.isConnected());
    }

    void csaClient_gameSummaryDefault()
    {
        CsaClient client;
        const CsaClient::GameSummary& gs = client.gameSummary();

        // デフォルト値が適切であること
        QVERIFY(gs.gameId.isEmpty());
        QVERIFY(gs.blackName.isEmpty());
        QVERIFY(gs.whiteName.isEmpty());
        QVERIFY(!gs.myTurn.has_value());
        QCOMPARE(gs.toMove, Turn::Black);
        QCOMPARE(gs.protocolMode, QStringLiteral("Server"));
        QCOMPARE(gs.timeBlack.timeUnit, QStringLiteral("1sec"));
        QCOMPARE(gs.timeBlack.totalTime, 0);
        QCOMPARE(gs.timeWhite.byoyomi, 0);
    }

    // ========================================
    // テーブル駆動: CsaMoveConverter pieceTypeFromCsa 境界値
    // ========================================

    void pieceTypeFromCsa_boundary_data()
    {
        QTest::addColumn<QString>("csaPiece");
        QTest::addColumn<int>("expectedType");

        QTest::newRow("empty")       << QString()              << 0;
        QTest::newRow("single_char") << QStringLiteral("F")    << 0;
        QTest::newRow("lowercase")   << QStringLiteral("fu")   << 0;
        QTest::newRow("null_chars")  << QString(2, QChar('\0')) << 0;
    }

    void pieceTypeFromCsa_boundary()
    {
        QFETCH(QString, csaPiece);
        QFETCH(int, expectedType);

        QCOMPARE(CsaMoveConverter::pieceTypeFromCsa(csaPiece), expectedType);
    }

    // ========================================
    // 仕様 3.2.2: Time / Time+ / Time- の先後別フィールド
    // ========================================

    void csaClient_timeSections_keepPerSideFields()
    {
        CsaClient client;
        client.m_summarySection = CsaClient::SummarySection::TopLevel;
        for (const char* line : {"BEGIN Time", "Time_Unit:1sec", "Total_Time:300", "Byoyomi:10",
                                 "Increment:5", "Delay:3", "Least_Time_Per_Move:1", "Time_Roundup:YES", "END Time",
                                 "BEGIN Time+", "Increment:7", "END Time+",
                                 "BEGIN Time-", "Time_Unit:1msec", "Total_Time:600000", "Delay:0", "END Time-"}) {
            client.processGameSummary(QString::fromLatin1(line));
        }
        const CsaClient::GameSummary& gs = client.m_gameSummary;
        QVERIFY(gs.hasIndividualTime);
        QVERIFY(gs.timeBlack.specified);
        QVERIFY(gs.timeWhite.specified);
        QVERIFY(!gs.isTimeUnlimited());
        // 共通 Time の値を引き継ぎ、Time+/Time- で書かれた項目だけ上書きされる
        QCOMPARE(gs.incrementMs(true), 7000);
        QCOMPARE(gs.timeWhite.increment, 5);
        QCOMPARE(gs.incrementMs(false), 5);  // 後手の単位は msec なので 5ms
        QCOMPARE(gs.delayMs(true), 3000);
        QCOMPARE(gs.delayMs(false), 0);
        QCOMPARE(gs.totalTimeMs(true), 300000);
        QCOMPARE(gs.totalTimeMs(false), 600000);
        QCOMPARE(gs.byoyomiMs(false), 10);  // 後手の単位は msec
        QCOMPARE(gs.timeBlack.leastTimePerMove, 1);
        QVERIFY(gs.timeBlack.timeRoundup);
        QVERIFY(gs.timeWhite.timeRoundup);
    }

    void csaClient_gameSummaryWithoutTime_isUnlimited()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::LoggedIn;
        QSignalSpy summary(&client, &CsaClient::gameSummaryReceived);
        for (const char* line : {"BEGIN Game_Summary", "Protocol_Version:1.2", "Format:Shogi 1.0",
                                 "Name+:A", "Name-:B", "Your_Turn:+", "To_Move:+",
                                 "BEGIN Position", "PI", "+", "END Position", "END Game_Summary"}) {
            client.processLine(QString::fromLatin1(line));
        }
        QCOMPARE(summary.count(), 1);
        QVERIFY(client.gameSummary().isTimeUnlimited());
        QVERIFY(!client.gameSummary().allowsJishogiDeclaration());

        // 全時間要素が省略された Time 階層も無制限
        CsaClient::GameSummary gs;
        gs.timeBlack.specified = gs.timeWhite.specified = true;
        QVERIFY(gs.isTimeUnlimited());
        gs.timeWhite.delay = 3;
        QVERIFY(!gs.isTimeUnlimited());
    }

    // ========================================
    // 仕様 3.2.3: 再開局面の指し手は消費時間を保持する
    // ========================================

    void csaClient_positionMoves_keepConsumedTimes()
    {
        CsaClient client;
        client.m_summarySection = CsaClient::SummarySection::TopLevel;
        for (const char* line : {"BEGIN Time+", "Time_Unit:1msec", "END Time+",
                                 "BEGIN Position", "PI", "+", "+7776FU,T1500", "-3334FU,T6", "+2726FU",
                                 "END Position"}) {
            client.processGameSummary(QString::fromLatin1(line));
        }
        const CsaClient::GameSummary& gs = client.m_gameSummary;
        QCOMPARE(gs.moves, (QStringList{QStringLiteral("+7776FU"), QStringLiteral("-3334FU"), QStringLiteral("+2726FU")}));
        QCOMPARE(gs.moveConsumedTimes, (QList<int>{1500, 6, 0}));
        // 指した側の時間単位で換算する（先手は msec、後手は sec）
        QCOMPARE(gs.positionMoveConsumedMs(0), 1500);
        QCOMPARE(gs.positionMoveConsumedMs(1), 6000);
        QCOMPARE(gs.positionMoveConsumedMs(2), 0);
        QCOMPARE(gs.positionLines, (QStringList{QStringLiteral("PI"), QStringLiteral("+")}));
    }

    void csaClient_moveLine_usesMoverTimeUnit()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::InGame;
        client.m_gameSummary.myTurn = Turn::Black;
        client.m_gameSummary.timeBlack.timeUnit = QStringLiteral("1msec");
        client.m_gameSummary.timeWhite.timeUnit = QStringLiteral("1sec");
        client.m_isMyTurn = true;
        QSignalSpy confirmed(&client, &CsaClient::moveConfirmed);
        QSignalSpy received(&client, &CsaClient::moveReceived);

        client.processLine(QStringLiteral("+7776FU,T1500"));
        QCOMPARE(confirmed.count(), 1);
        QCOMPARE(confirmed.at(0).at(1).toInt(), 1500);
        QVERIFY(!client.isMyTurn());

        client.processLine(QStringLiteral("-3334FU,T2"));
        QCOMPARE(received.count(), 1);
        QCOMPARE(received.at(0).at(1).toInt(), 2000);
        QVERIFY(client.isMyTurn());
    }

    // ========================================
    // 仕様 3.3: Game_ID 省略時の REJECT 通知
    // ========================================

    void csaClient_rejectWithEmptyGameId_isAccepted()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::GameReady;
        QSignalSpy rejected(&client, &CsaClient::gameRejected);
        QSignalSpy error(&client, &CsaClient::errorOccurred);

        client.processLine(QStringLiteral("REJECT: by Peer"));
        QCOMPARE(error.count(), 0);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(rejected.at(0).at(0).toString().isEmpty());
        QCOMPARE(rejected.at(0).at(1).toString(), QStringLiteral("Peer"));
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::WaitingForGame);

        client.m_connectionState = CsaClient::ConnectionState::GameReady;
        client.processLine(QStringLiteral("REJECT:game-1 by Other"));
        QCOMPARE(rejected.count(), 2);
        QCOMPARE(rejected.at(1).at(0).toString(), QStringLiteral("game-1"));

        client.m_connectionState = CsaClient::ConnectionState::GameReady;
        client.processLine(QStringLiteral("REJECT:broken"));
        QCOMPARE(error.count(), 1);
        QCOMPARE(rejected.count(), 2);
    }

    // ========================================
    // 仕様 3.1/3.4: 終局後も対局待ち状態として次の対局条件と LOGOUT 応答を受ける
    // ========================================

    void csaClient_gameOver_acceptsNextGameSummaryAndLogout()
    {
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::InGame;
        client.m_gameSummary.myTurn = Turn::Black;
        client.m_gameSummary.toMove = Turn::Black;
        client.m_isMyTurn = true;
        client.m_awaitingMoveConfirmation = true;
        QSignalSpy ended(&client, &CsaClient::gameEnded);
        QSignalSpy summary(&client, &CsaClient::gameSummaryReceived);
        QSignalSpy started(&client, &CsaClient::gameStarted);
        QSignalSpy logout(&client, &CsaClient::logoutCompleted);

        client.processLine(QStringLiteral("%TORYO,T4"));
        client.processLine(QStringLiteral("#RESIGN"));
        client.processLine(QStringLiteral("#LOSE"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(2).toInt(), 4000);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::GameOver);
        QVERIFY(!client.isAwaitingMoveConfirmation());
        QVERIFY(CsaClient::isWaitingState(client.connectionState()));

        for (const char* line : {"BEGIN Game_Summary", "Game_ID:second", "Name+:A", "Name-:B",
                                 "Your_Turn:-", "To_Move:+", "END Game_Summary"}) {
            client.processLine(QString::fromLatin1(line));
        }
        QCOMPARE(summary.count(), 1);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::GameReady);
        QCOMPARE(client.gameSummary().gameId, QStringLiteral("second"));

        client.processLine(QStringLiteral("START:second"));
        QCOMPARE(started.count(), 1);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::InGame);
        QVERIFY(!client.isMyTurn());

        client.processLine(QStringLiteral("#TIME_UP"));
        client.processLine(QStringLiteral("#WIN"));
        QCOMPARE(ended.count(), 2);
        client.processLine(QStringLiteral("LOGOUT:completed"));
        QCOMPARE(logout.count(), 1);
    }

    // ========================================
    // 実ソケット: ログアウト応答待ち・確認待ちガード・キープアライブ
    // ========================================

private:
    // MOC は slots セクション内の構造体定義を受け付けないので、ヘルパーは private に置く
    struct LocalServer {
        QTcpServer server;
        QTcpSocket* peer = nullptr;
        QStringList lines;
        QByteArray buffer;

        bool listen() { return server.listen(QHostAddress::LocalHost); }
        int port() const { return server.serverPort(); }
        bool accept()
        {
            if (!QTest::qWaitFor([this] { return server.hasPendingConnections(); }, 3000)) return false;
            peer = server.nextPendingConnection();
            return peer != nullptr;
        }
        void pump()
        {
            if (!peer) return;
            buffer += peer->readAll();
            int idx;
            while ((idx = static_cast<int>(buffer.indexOf('\n'))) != -1) {
                lines.append(QString::fromUtf8(buffer.left(idx)));
                buffer = buffer.mid(idx + 1);
            }
        }
        bool waitForLine(const QString& prefix, int timeoutMs = 3000)
        {
            return QTest::qWaitFor([this, &prefix] {
                pump();
                return std::any_of(lines.cbegin(), lines.cend(),
                                   [&prefix](const QString& l) { return l.startsWith(prefix); });
            }, timeoutMs);
        }
        int countLines(const QString& exact)
        {
            pump();
            return static_cast<int>(std::count(lines.cbegin(), lines.cend(), exact));
        }
        void send(const char* text)
        {
            peer->write(text);
            peer->write("\n");
            peer->flush();
        }
    };

    static bool bringToWaiting(CsaClient& client, LocalServer& server)
    {
        client.connectToServer(QStringLiteral("127.0.0.1"), server.port(), CsaClient::Transport::Plain);
        if (!server.accept()) return false;
        if (!QTest::qWaitFor([&client] { return client.connectionState() == CsaClient::ConnectionState::Connected; }, 3000)) return false;
        client.login(QStringLiteral("user"), QStringLiteral("pass"));
        if (!server.waitForLine(QStringLiteral("LOGIN user pass"))) return false;
        server.send("LOGIN:user OK");
        return QTest::qWaitFor([&client] { return client.connectionState() == CsaClient::ConnectionState::LoggedIn; }, 3000);
    }

    static bool bringInGame(CsaClient& client, LocalServer& server, const char* yourTurn)
    {
        if (!bringToWaiting(client, server)) return false;
        server.send("BEGIN Game_Summary");
        server.send("Game_ID:g1");
        server.send("Name+:A");
        server.send("Name-:B");
        server.send((QByteArrayLiteral("Your_Turn:") + yourTurn).constData());
        server.send("To_Move:+");
        server.send("BEGIN Time");
        server.send("Total_Time:300");
        server.send("END Time");
        server.send("END Game_Summary");
        if (!QTest::qWaitFor([&client] { return client.connectionState() == CsaClient::ConnectionState::GameReady; }, 3000)) return false;
        client.agree(QStringLiteral("g1"));
        if (!server.waitForLine(QStringLiteral("AGREE g1"))) return false;
        server.send("START:g1");
        return QTest::qWaitFor([&client] { return client.connectionState() == CsaClient::ConnectionState::InGame; }, 3000);
    }

private slots:
    void csaClient_tlsCertificateVerification_data()
    {
        QTest::addColumn<bool>("trustCertificate");
        QTest::addColumn<bool>("wrongHostname");
        QTest::newRow("trusted") << true << false;
        QTest::newRow("untrusted") << false << false;
        QTest::newRow("wrong-hostname") << true << true;
    }

    void csaClient_tlsCertificateVerification()
    {
        if (!QSslSocket::supportsSsl()) QSKIP("TLS backend is not installed");
        QFETCH(bool, trustCertificate);
        QFETCH(bool, wrongHostname);
        QFile certFile(QStringLiteral(SOURCE_DIR "/tests/fixtures/tls/server-cert.pem"));
        QFile keyFile(QStringLiteral(SOURCE_DIR "/tests/fixtures/tls/server-key.pem"));
        QVERIFY(certFile.open(QIODevice::ReadOnly));
        QVERIFY(keyFile.open(QIODevice::ReadOnly));
        const QSslCertificate cert(&certFile, QSsl::Pem);
        const QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
        QVERIFY(!cert.isNull());
        QVERIFY(!key.isNull());
        QSslConfiguration serverConfig = QSslConfiguration::defaultConfiguration();
        serverConfig.setLocalCertificate(cert);
        serverConfig.setPrivateKey(key);
        serverConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // No client certificate required.
        QSslServer server;
        server.setSslConfiguration(serverConfig);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CsaClient client;
        QSignalSpy errors(&client, &CsaClient::errorOccurred);
        QSignalSpy states(&client, &CsaClient::connectionStateChanged);
        if (trustCertificate) {
            QSslConfiguration clientConfig = client.m_socket->sslConfiguration();
            clientConfig.addCaCertificate(cert); // Trust only in this test client, never globally.
            client.m_socket->setSslConfiguration(clientConfig);
        }
        if (wrongHostname) client.m_socket->setPeerVerifyName(QStringLiteral("wrong.invalid"));
        client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort(), CsaClient::Transport::Tls);
        if (!trustCertificate || wrongHostname) {
            QTRY_VERIFY(!errors.isEmpty());
            QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
            for (const auto& state : states)
                QVERIFY(qvariant_cast<CsaClient::ConnectionState>(state.at(0)) != CsaClient::ConnectionState::Connected);
            return;
        }
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Connected);
        QVERIFY(client.m_socket->isEncrypted());
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        client.login(QStringLiteral("audit-user"), QStringLiteral("audit-secret"));
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(peer->readLine(), QByteArrayLiteral("LOGIN audit-user audit-secret\n"));
        QVERIFY(errors.isEmpty());
    }

    void csaClient_defaultPlaintextSupportsLocalhost()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        client.connectToServer(QStringLiteral("localhost"), server.port());
        QVERIFY(server.accept());
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Connected);
        QVERIFY(!client.m_socket->isEncrypted());
        client.login(QStringLiteral("audit-user"), QStringLiteral("audit-secret"));
        QTRY_VERIFY(server.peer->canReadLine());
        QCOMPARE(server.peer->readLine(), QByteArrayLiteral("LOGIN audit-user audit-secret\n"));
    }

    void csaClient_tlsDoesNotLoginBeforeHandshake()
    {
        if (!QSslSocket::supportsSsl()) QSKIP("TLS backend is not installed");
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        client.connectToServer(QStringLiteral("127.0.0.1"), server.port(), CsaClient::Transport::Tls);
        QVERIFY(server.accept());
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Connecting);
        client.login(QStringLiteral("audit-user"), QStringLiteral("audit-secret"));
        client.sendRawCommand(QStringLiteral("LOGIN audit-user audit-secret"));
        QTest::qWait(50);
        const QByteArray received = server.peer->readAll();
        QVERIFY(!received.contains("LOGIN"));
        QVERIFY(!received.contains("audit-secret"));
        client.onConnectionTimeout();
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
    }

    void csaClient_oversizedLineDisconnects_data()
    {
        QTest::addColumn<bool>("newline");
        QTest::newRow("unterminated") << false;
        QTest::newRow("terminated") << true;
    }

    void csaClient_oversizedLineDisconnects()
    {
        QFETCH(bool, newline);
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QVERIFY(bringToWaiting(client, server));
        QSignalSpy errors(&client, &CsaClient::errorOccurred);
        QByteArray input(CsaClient::kMaxLineBytes + 1, 'x');
        if (newline) input += '\n';
        server.peer->write(input);
        server.peer->flush();
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
        QCOMPARE(errors.count(), 1);
        QVERIFY(client.m_receiveBuffer.isEmpty());
    }

    void csaClient_summaryLimits_data()
    {
        QTest::addColumn<bool>("byteLimit");
        QTest::newRow("lines") << false;
        QTest::newRow("bytes") << true;
    }

    void csaClient_summaryLimits()
    {
        QFETCH(bool, byteLimit);
        CsaClient client;
        client.m_connectionState = CsaClient::ConnectionState::LoggedIn;
        QSignalSpy errors(&client, &CsaClient::errorOccurred);
        client.processLine(QStringLiteral("BEGIN Game_Summary"));
        client.processLine(QStringLiteral("BEGIN Position"));
        const QString line = byteLimit ? QString(CsaClient::kMaxLineBytes, QLatin1Char('P')) : QStringLiteral("P+");
        const int count = byteLimit ? 33 : CsaClient::kMaxSummaryLines;
        for (int i = 0; i < count && client.m_summarySection != CsaClient::SummarySection::Outside; ++i) client.processGameSummary(line);
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
        QCOMPARE(errors.count(), 1);
        QVERIFY(client.gameSummary().positionLines.isEmpty());
    }

    void csaClient_fragmentedUtf8AndMultipleLines()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QVERIFY(bringToWaiting(client, server));
        QSignalSpy received(&client, &CsaClient::rawMessageReceived);
        const QByteArray bytes = QStringLiteral("監査").toUtf8();
        server.peer->write(bytes.left(1));
        server.peer->flush();
        QTRY_COMPARE(client.m_receiveBuffer.size(), 1);
        server.peer->write(bytes.mid(1) + "\r\nOK\n");
        server.peer->flush();
        QTRY_COMPARE(received.count(), 2);
        QCOMPARE(received.at(0).at(0).toString(), QStringLiteral("監査"));
        QCOMPARE(received.at(1).at(0).toString(), QStringLiteral("OK"));
    }

    void csaClient_socket_logoutWaitsForCompleted()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QSignalSpy logout(&client, &CsaClient::logoutCompleted);
        QSignalSpy error(&client, &CsaClient::errorOccurred);
        QVERIFY(bringToWaiting(client, server));

        client.disconnectFromServer();
        QVERIFY(server.waitForLine(QStringLiteral("LOGOUT")));
        // 応答が来るまでは切断しない
        QTest::qWait(100);
        QVERIFY(client.isConnected());
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::LoggedIn);

        server.send("LOGOUT:completed");
        QTRY_COMPARE(logout.count(), 1);
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
        QCOMPARE(error.count(), 0);
    }

    void csaClient_socket_logoutTimeoutCloses()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QSignalSpy error(&client, &CsaClient::errorOccurred);
        QVERIFY(bringToWaiting(client, server));

        client.disconnectFromServer();
        QVERIFY(server.waitForLine(QStringLiteral("LOGOUT")));
        QTRY_COMPARE_WITH_TIMEOUT(client.connectionState(), CsaClient::ConnectionState::Disconnected,
                                  CsaClient::kLogoutTimeoutMs + 2000);
        QCOMPARE(error.count(), 0);
    }

    void csaClient_socket_gameReadyDisconnectRejectsThenLogsOut()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QVERIFY(bringToWaiting(client, server));
        server.send("BEGIN Game_Summary");
        server.send("Game_ID:g1");
        server.send("END Game_Summary");
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::GameReady);

        client.disconnectFromServer();
        QVERIFY(server.waitForLine(QStringLiteral("REJECT g1")));
        QVERIFY(server.waitForLine(QStringLiteral("LOGOUT")));
        QVERIFY(server.lines.indexOf(QStringLiteral("REJECT g1")) < server.lines.indexOf(QStringLiteral("LOGOUT")));
        server.send("LOGOUT:completed");
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Disconnected);
    }

    void csaClient_socket_reconnectWhileLogoutPending()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QSignalSpy error(&client, &CsaClient::errorOccurred);
        QSignalSpy states(&client, &CsaClient::connectionStateChanged);
        QVERIFY(bringToWaiting(client, server));

        client.disconnectFromServer();
        QVERIFY(server.waitForLine(QStringLiteral("LOGOUT")));
        states.clear();

        // 応答待ちのまま再接続すると、Disconnected を通知せずに Connecting へ進む
        client.connectToServer(QStringLiteral("127.0.0.1"), server.port(), CsaClient::Transport::Plain);
        QCOMPARE(qvariant_cast<CsaClient::ConnectionState>(states.at(0).at(0)), CsaClient::ConnectionState::Connecting);
        QVERIFY(server.accept());
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::Connected);
        QCOMPARE(error.count(), 0);
    }

    void csaClient_socket_sendMoveWaitsForConfirmation()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QSignalSpy rejected(&client, &CsaClient::commandRejected);
        QSignalSpy error(&client, &CsaClient::errorOccurred);
        QSignalSpy confirmed(&client, &CsaClient::moveConfirmed);
        QSignalSpy received(&client, &CsaClient::moveReceived);
        QVERIFY(bringInGame(client, server, "+"));
        QVERIFY(client.isMyTurn());

        client.sendMove(QStringLiteral("+7776FU"));
        QVERIFY(server.waitForLine(QStringLiteral("+7776FU")));
        QVERIFY(client.isAwaitingMoveConfirmation());

        // 確認が届くまで次の手も意思表示も送らない（手番外送信は #ILLEGAL_ACTION）
        client.sendMove(QStringLiteral("+2726FU"));
        client.resign();
        QCOMPARE(rejected.count(), 2);
        QTest::qWait(50);
        QCOMPARE(server.countLines(QStringLiteral("+2726FU")), 0);
        QCOMPARE(server.countLines(QStringLiteral("%TORYO")), 0);

        server.send("+7776FU,T1");
        QTRY_COMPARE(confirmed.count(), 1);
        QVERIFY(!client.isAwaitingMoveConfirmation());
        QVERIFY(!client.isMyTurn());

        client.sendMove(QStringLiteral("+2726FU"));
        QCOMPARE(rejected.count(), 3);

        server.send("-3334FU,T2");
        QTRY_COMPARE(received.count(), 1);
        QVERIFY(client.isMyTurn());
        client.sendMove(QStringLiteral("+2726FU"));
        QVERIFY(server.waitForLine(QStringLiteral("+2726FU")));

        // 生コマンドでも指し手・意思表示は同じガードを通り、%% の拡張コマンドは通る
        client.sendRawCommand(QStringLiteral("%TORYO"));
        QCOMPARE(rejected.count(), 4);
        client.sendRawCommand(QStringLiteral("%%WHO"));
        QVERIFY(server.waitForLine(QStringLiteral("%%WHO")));
        QCOMPARE(error.count(), 0);
    }

    void csaClient_keepAliveSettings()
    {
        CsaClient client;
        QVERIFY(client.isKeepAliveEnabled());
        QVERIFY(client.isTcpKeepAliveEnabled());
        QCOMPARE(client.keepAliveIntervalMs(), CsaClient::kKeepAliveIntervalMs);

        // 仕様 3.4 の下限（30秒）より短くはならない
        client.setKeepAliveIntervalMs(5000);
        QCOMPARE(client.keepAliveIntervalMs(), CsaClient::kKeepAliveMinIntervalMs);
        client.setKeepAliveIntervalMs(45000);
        QCOMPARE(client.keepAliveIntervalMs(), 45000);

        // 無効にすると対局中でもタイマーは動かない
        client.setKeepAliveEnabled(false);
        client.m_connectionState = CsaClient::ConnectionState::GameReady;
        client.processLine(QStringLiteral("START:g1"));
        QCOMPARE(client.connectionState(), CsaClient::ConnectionState::InGame);
        QVERIFY(!client.m_keepAliveTimer->isActive());
        client.setKeepAliveEnabled(true);
        QVERIFY(client.m_keepAliveTimer->isActive());
        client.setTcpKeepAliveEnabled(false);
        QVERIFY(!client.isTcpKeepAliveEnabled());
    }

    void csaClient_socket_keepAliveDuringGameOnly()
    {
        LocalServer server;
        QVERIFY(server.listen());
        CsaClient client;
        QVERIFY(bringToWaiting(client, server));
        QVERIFY(!client.m_keepAliveTimer->isActive());
        QVERIFY(CsaClient::kKeepAliveIntervalMs >= CsaClient::kKeepAliveMinIntervalMs);

        server.send("BEGIN Game_Summary");
        server.send("Game_ID:g1");
        server.send("Your_Turn:-");
        server.send("To_Move:+");
        server.send("END Game_Summary");
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::GameReady);
        client.agree();
        QVERIFY(server.waitForLine(QStringLiteral("AGREE")));
        server.send("START:g1");
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::InGame);
        QVERIFY(client.m_keepAliveTimer->isActive());
        QCOMPARE(client.m_keepAliveTimer->interval(), CsaClient::kKeepAliveIntervalMs);

        // テストでは間隔を縮めて、相手の手番中に LF だけの行が届くことを確かめる
        client.m_keepAliveTimer->setInterval(100);
        QVERIFY(QTest::qWaitFor([&server] {
            server.pump();
            return server.lines.contains(QString());
        }, 3000));

        // サーバーの LF 応答は無視され、相手の指し手で手番が戻る
        server.send("");
        server.send("+7776FU,T1");
        QTRY_VERIFY(client.isMyTurn());

        // 手動のキープアライブは直前の送信から30秒未満では送らない
        QSignalSpy rejected(&client, &CsaClient::commandRejected);
        client.sendMove(QStringLiteral("-3334FU"));
        client.sendRawCommand(QString());
        QCOMPARE(rejected.count(), 1);

        server.send("-3334FU,T1");
        server.send("#TIME_UP");
        server.send("#WIN");
        QTRY_COMPARE(client.connectionState(), CsaClient::ConnectionState::GameOver);
        QVERIFY(!client.m_keepAliveTimer->isActive());
    }
};

QTEST_MAIN(Tst_CsaProtocol)
#include "tst_csaprotocol.moc"
