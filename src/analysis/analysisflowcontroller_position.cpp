/// @file analysisflowcontroller_position.cpp
/// @brief AnalysisFlowController の局面準備・盤面同期処理

#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "kifudisplay.h"
#include "kifurecordlistmodel.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "usi.h"

#include <limits>

#include "logcategories.h"

void AnalysisFlowController::onPositionPrepared(int ply, const QString& sfen)
{
    // 各局面の解析開始時のログ
    qCDebug(lcAnalysis).noquote() << "onPositionPrepared: ply=" << ply << "sfen=" << sfen.left(50);

    // Usiにも設定（ThinkingInfoPresenter経由での変換用）
    if (m_usi) {
        // SFENから盤面データを生成
        QString pureSfen = sfen;
        if (pureSfen.startsWith(QStringLiteral("position sfen "))) {
            pureSfen = pureSfen.mid(14);
        } else if (pureSfen.startsWith(QStringLiteral("position "))) {
            pureSfen = pureSfen.mid(9);
        }

        ShogiBoard tempBoard;
        tempBoard.setSfen(pureSfen);
        const QList<Piece> pieceBoardData = tempBoard.boardData();
        if (pieceBoardData.size() == 81) {
            // Usi::setClonedBoardData は QList<QChar> を受け取るため変換
            QList<QChar> charBoardData;
            charBoardData.reserve(81);
            for (const Piece p : pieceBoardData) {
                charBoardData.append(pieceToChar(p));
            }
            m_usi->setClonedBoardData(charBoardData);
        }

        // ThinkingInfoPresenterに基準SFENを設定（手番情報用）
        m_usi->setBaseSfen(pureSfen);
        qCDebug(lcAnalysis).noquote() << "setBaseSfen: pureSfen=" << pureSfen.left(50);

        // 開始局面に至った最後のUSI指し手を設定（読み筋表示ウィンドウのハイライト用）
        // ply=0は開始局面なので指し手なし、ply>=1はm_usiMoves[ply-1]が最後の指し手
        QString lastUsiMove;
        qCDebug(lcAnalysis).noquote() << "ply=" << ply
                                      << "m_usiMoves=" << m_usiMoves
                                      << "m_usiMoves->size()=" << (m_usiMoves ? m_usiMoves->size() : -1);
        if (m_usiMoves && ply > 0 && ply <= m_usiMoves->size()) {
            lastUsiMove = m_usiMoves->at(ply - 1);
            qCDebug(lcAnalysis).noquote() << "extracted lastUsiMove from m_usiMoves[" << (ply - 1) << "]=" << lastUsiMove;
        } else if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
            // フォールバック: 棋譜表記からUSI形式の指し手を抽出
            // 形式: 「▲７六歩(77)」または「△５五角打」など
            KifuDisplay* moveDisp = m_recordModel->item(ply);
            if (moveDisp) {
                QString moveLabel = moveDisp->currentMove();
                qCDebug(lcAnalysis).noquote() << "extracting USI move from kanji:" << moveLabel;
                lastUsiMove = AnalysisResultHandler::extractUsiMoveFromKanji(moveLabel);
                qCDebug(lcAnalysis).noquote() << "extracted lastUsiMove from kanji:" << lastUsiMove;
            }
        } else {
            qCDebug(lcAnalysis).noquote() << "no lastUsiMove: ply=" << ply << "is out of range or m_usiMoves/m_recordModel is null";
        }
        m_usi->setLastUsiMove(lastUsiMove);
        qCDebug(lcAnalysis).noquote() << "setLastUsiMove:" << lastUsiMove;

        // 直前の指し手の移動先を設定（読み筋の最初の指し手で「同」表記を正しく判定するため）
        // ply=0は開始局面なので直前の指し手なし
        // ply>=1の場合、その局面に至った指し手はrecordModel->item(ply)（ply番目の指し手）
        bool previousMoveSet = false;
        if (m_recordModel && ply > 0 && ply < m_recordModel->rowCount()) {
            KifuDisplay* prevDisp = m_recordModel->item(ply);  // plyの指し手（その局面に至った指し手）
            if (prevDisp) {
                QString prevMoveLabel = prevDisp->currentMove();
                qCDebug(lcAnalysis).noquote() << "prevMoveLabel from recordModel[" << ply << "]:" << prevMoveLabel;

                // 漢字の移動先を抽出して整数座標に変換
                // 形式: 「▲７六歩(77)」または「△同　銀(31)」
                static const QString senteMark = QStringLiteral("▲");
                static const QString goteMark = QStringLiteral("△");

                qsizetype markPos = prevMoveLabel.indexOf(senteMark);
                if (markPos < 0) {
                    markPos = prevMoveLabel.indexOf(goteMark);
                }

                if (markPos >= 0 && prevMoveLabel.length() > markPos + 2) {
                    QString afterMark = prevMoveLabel.mid(markPos + 1);

                    // 「同」の場合はスキップ（前回の移動先をそのまま使用）
                    if (!afterMark.startsWith(QStringLiteral("同"))) {
                        // 「７六」のような漢字座標を取得
                        QChar fileChar = afterMark.at(0);  // 全角数字 '１'〜'９'
                        QChar rankChar = afterMark.at(1);  // 漢数字 '一'〜'九'

                        // 全角数字を整数に変換（'１'=0xFF11 → 1）
                        int fileTo = 0;
                        if (fileChar >= QChar(0xFF11) && fileChar <= QChar(0xFF19)) {
                            fileTo = fileChar.unicode() - 0xFF11 + 1;
                        }

                        // 漢数字を整数に変換
                        int rankTo = 0;
                        static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
                        qsizetype rankIdxPos = kanjiRanks.indexOf(rankChar);
                        if (rankIdxPos >= 0) {
                            rankTo = static_cast<int>(rankIdxPos) + 1;
                        }

                        if (fileTo >= 1 && fileTo <= 9 && rankTo >= 1 && rankTo <= 9) {
                            m_usi->setPreviousFileTo(fileTo);
                            m_usi->setPreviousRankTo(rankTo);
                            previousMoveSet = true;
                            qCDebug(lcAnalysis).noquote() << "setPreviousMove from recordModel:"
                                                          << "fileTo=" << fileTo << "rankTo=" << rankTo;
                        }
                    } else {
                        // 「同」の場合は、前回設定した座標をそのまま維持
                        previousMoveSet = true;
                        qCDebug(lcAnalysis).noquote() << "previousMove kept (同 notation)";
                    }
                }
            }
        }

        if (!previousMoveSet) {
            // 開始局面（ply=0）または取得失敗の場合は移動先をリセット
            m_usi->setPreviousFileTo(0);
            m_usi->setPreviousRankTo(0);
            qCDebug(lcAnalysis).noquote() << "reset previousMove";
        }

        // SFENから手番を抽出してGameControllerに設定
        // 形式: "盤面 手番 駒台 手数" 例: "lnsgkgsnl/... b - 1"
        if (m_gameController) {
            // " b " または " w " を探す
            bool isPlayer1Turn = pureSfen.contains(QStringLiteral(" b "));
            ShogiGameController::Player player = isPlayer1Turn
                ? ShogiGameController::Player1
                : ShogiGameController::Player2;
            m_gameController->setCurrentPlayer(player);
            qCDebug(lcAnalysis).noquote() << "set player="
                                          << (isPlayer1Turn ? "P1(sente)" : "P2(gote)");
        } else {
            qCDebug(lcAnalysis).noquote() << "m_gameController is null!";
        }
    } else {
        qCDebug(lcAnalysis).noquote() << "m_usi is null!";
    }

    // 通常対局と同じ流れ：
    // 1. 局面と指し手を確定（上記で完了）
    // 2. GUI更新（棋譜欄ハイライト、将棋盤更新）
    // 3. エンジンにコマンド送信
    // 4. 思考タブ更新（info行受信時）

    // 前の手の評価値をGUIに反映（lastCommittedPlyが確定した手数）
    if (m_resultHandler->lastCommittedPly() >= 0) {
        qCDebug(lcAnalysis).noquote() << "emitting analysisProgressReported: ply="
                                      << m_resultHandler->lastCommittedPly() << "scoreCp=" << m_resultHandler->lastCommittedScoreCp();
        Q_EMIT analysisProgressReported(m_resultHandler->lastCommittedPly(), m_resultHandler->lastCommittedScoreCp());

        // リセット
        m_resultHandler->resetLastCommitted();
    }

    // INT_MINは評価値追加スキップのマーカー（盤面移動のみ実行）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    qCDebug(lcAnalysis).noquote() << "moving board to ply=" << ply;
    Q_EMIT analysisProgressReported(ply, POSITION_ONLY_MARKER);

    // 思考タブをクリアしてからgoコマンドを送信
    if (m_usi) {
        m_usi->requestClearThinkingInfo();
    }

    // GUI更新後にgoコマンドを送信
    if (m_coord) {
        qCDebug(lcAnalysis).noquote() << "calling sendGoCommand";
        m_coord->sendGoCommand();
    }
}
