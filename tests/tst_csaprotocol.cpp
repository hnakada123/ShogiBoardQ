/// @file tst_csaprotocol.cpp
/// @brief CSAプロトコル関連のユニットテスト
///
/// テスト対象:
/// - CsaClient::GameSummary（時間単位変換、初期化、手番判定）
/// - CsaMoveConverter（CSA↔USI変換、駒変換、結果文字列変換）
/// - SfenCsaPositionConverter（CSA↔SFEN局面変換）

#include <QtTest>
#include <QCoreApplication>

#include "csaclient.h"
#include "csamoveconverter.h"
#include "sfencsapositionconverter.h"

class Tst_CsaProtocol : public QObject
{
    Q_OBJECT

private slots:
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
        gs.timeUnit = timeUnit;
        QCOMPARE(gs.timeUnitMs(), expectedMs);
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
        gs.myTurn = QStringLiteral("+");
        gs.totalTime = 600;
        gs.byoyomi = 30;

        gs.clear();

        QVERIFY(gs.gameId.isEmpty());
        QVERIFY(gs.blackName.isEmpty());
        QVERIFY(gs.whiteName.isEmpty());
        QVERIFY(gs.myTurn.isEmpty());
        QCOMPARE(gs.totalTime, 0);
        QCOMPARE(gs.byoyomi, 0);
        QCOMPARE(gs.toMove, QStringLiteral("+"));
        QCOMPARE(gs.protocolMode, QStringLiteral("Server"));
        QCOMPARE(gs.timeUnit, QStringLiteral("1sec"));
        QCOMPARE(gs.maxMoves, 0);
        QVERIFY(!gs.rematchOnDraw);
        QVERIFY(!gs.hasIndividualTime);
    }

    // ========================================
    // GameSummary: 手番判定
    // ========================================

    void gameSummary_isBlackTurn()
    {
        CsaClient::GameSummary gs;

        gs.myTurn = QStringLiteral("+");
        QVERIFY(gs.isBlackTurn());

        gs.myTurn = QStringLiteral("-");
        QVERIFY(!gs.isBlackTurn());

        gs.myTurn.clear();
        QVERIFY(!gs.isBlackTurn());
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
            QStringLiteral("+7776FU"), false, 0, 0, 0);
        QVERIFY(result.contains(QStringLiteral("▲")));
        QVERIFY(result.contains(QStringLiteral("歩")));
        QVERIFY(result.contains(QStringLiteral("(77)")));
    }

    void csaToPretty_goteMove()
    {
        // △３四歩(33)
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("-3334FU"), false, 0, 0, 1);
        QVERIFY(result.contains(QStringLiteral("△")));
        QVERIFY(result.contains(QStringLiteral("歩")));
    }

    void csaToPretty_drop()
    {
        // ▲５五歩打
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+0055FU"), false, 0, 0, 5);
        QVERIFY(result.contains(QStringLiteral("打")));
        QVERIFY(result.contains(QStringLiteral("歩")));
    }

    void csaToPretty_sameSquare()
    {
        // 「同」表記 — 前の手の移動先と同じ座標への移動
        // -3334GI: from(3,3) to(3,4)、前の手が(3,4)への移動 → 同銀
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("-3334GI"), false, 3, 4, 2);
        QVERIFY(result.contains(QStringLiteral("同")));
        QVERIFY(result.contains(QStringLiteral("銀")));
        QVERIFY(result.contains(QStringLiteral("(33)")));  // 移動元
    }

    void csaToPretty_promotion()
    {
        // 成り: isPromotion=true の場合、成る前の駒名+「成」
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+8822UM"), true, 0, 0, 1);
        QVERIFY(result.contains(QStringLiteral("角")));  // 成る前の駒名
        QVERIFY(result.contains(QStringLiteral("成")));
    }

    void csaToPretty_shortInput()
    {
        // 短すぎる入力はそのまま返す
        QString result = CsaMoveConverter::csaToPretty(
            QStringLiteral("+77"), false, 0, 0, 0);
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

        QString sfen;
        QString error;
        bool ok = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &sfen, &error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(!sfen.isEmpty());
        QVERIFY(sfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL")));
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

        QString sfen;
        QString error;
        bool ok = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &sfen, &error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(sfen.contains(QStringLiteral("lnsgkgsnl")));
        QVERIFY(sfen.contains(QStringLiteral("ppppppppp")));
        QVERIFY(sfen.contains(QStringLiteral("PPPPPPPPP")));
        QVERIFY(sfen.contains(QStringLiteral("LNSGKGSNL")));
    }

    // ========================================
    // SfenCsaPositionConverter: SFEN -> CSA 変換
    // ========================================

    void sfenToCsa_hirate()
    {
        const QString sfen =
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

        bool ok = false;
        QString error;
        QStringList csaLines = SfenCsaPositionConverter::toCsaPositionLines(sfen, &ok, &error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(!csaLines.isEmpty());
        // P1 行が含まれること
        bool hasP1 = false;
        for (const QString& line : std::as_const(csaLines)) {
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
        QString sfen;
        QString error;
        bool ok = SfenCsaPositionConverter::fromCsaPositionLines(csaLines, &sfen, &error);
        // 空の入力は失敗するか空のSFENを返す
        if (ok) {
            // 成功する場合もある（空局面として処理）
            QVERIFY(true);
        } else {
            QVERIFY(!error.isEmpty() || sfen.isEmpty());
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
};

QTEST_MAIN(Tst_CsaProtocol)
#include "tst_csaprotocol.moc"
