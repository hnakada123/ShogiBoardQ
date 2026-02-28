/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"

#include "analysiscoordinator.h"
#include "analysisresultspresenter.h"
#include "kifuanalysislistmodel.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "kifuanalysisresultsdisplay.h"
#include "pvboarddialog.h"

#include <limits>
#include <QString>
#include <QStringList>

#include "logcategories.h"

void AnalysisResultHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void AnalysisResultHandler::reset()
{
    m_prevEvalCp = 0;
    m_pendingPly = -1;
    m_pendingScoreCp = 0;
    m_pendingMate = 0;
    m_pendingPv.clear();
    m_pendingPvKanji.clear();
    m_lastCommittedPly = -1;
    m_lastCommittedScoreCp = 0;
}

void AnalysisResultHandler::updatePending(int ply, int scoreCp, int mate, const QString& pv)
{
    // 結果を一時保存（bestmove時に確定）
    // 最新のinfo行で更新し続ける
    // 注意: m_pendingPvKanjiはupdatePendingPvKanjiで設定されるため、ここではクリアしない
    //       plyが変わった場合でも、ThinkingInfoUpdatedが先に呼ばれて設定済み
    m_pendingPly = ply;
    m_pendingScoreCp = scoreCp;
    m_pendingMate = mate;
    m_pendingPv = pv;

    qCDebug(lcAnalysis).noquote() << "updatePending: ply=" << ply << "pv=" << pv.left(30) << "pvKanji=" << m_pendingPvKanji.left(30);
}

void AnalysisResultHandler::updatePendingPvKanji(const QString& pvKanjiStr)
{
    // ThinkingInfoPresenterからの漢字PVを保存（思考タブと同じ内容を棋譜解析結果に使用）
    m_pendingPvKanji = pvKanjiStr;
    qCDebug(lcAnalysis).noquote() << "saved pvKanjiStr=" << pvKanjiStr.left(50);
}

void AnalysisResultHandler::commitPendingResult()
{
    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult: analysisModel is null";
        return;
    }

    // m_pendingPlyが-1の場合は定跡（infoなしでbestmoveが来た）
    // m_coordから現在のplyを取得
    int ply = m_pendingPly;
    bool isBook = false;
    if (ply < 0 && m_refs.coord) {
        ply = m_refs.coord->currentPly();
        isBook = true;
        qCDebug(lcAnalysis).noquote() << "book move detected, ply=" << ply;
    }

    if (ply < 0) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult skipped: ply=" << ply;
        return;
    }

    const int scoreCp = m_pendingScoreCp;
    const int mate = m_pendingMate;
    // 漢字PVがあればそれを使用、なければUSI形式PV、定跡なら「定跡」
    QString pv;
    if (isBook) {
        pv = tr("（定跡）");
    } else if (!m_pendingPvKanji.isEmpty()) {
        pv = m_pendingPvKanji;
    } else {
        pv = m_pendingPv;
    }

    // 一時保存をリセット
    m_pendingPly = -1;
    m_pendingPvKanji.clear();

    // 指し手ラベル（m_recordModelから取得、なければフォールバック）
    QString moveLabel;
    if (m_refs.recordModel && ply >= 0 && m_refs.recordModel->rowCount() > ply) {
        KifuDisplay* disp = m_refs.recordModel->item(ply);
        if (disp) {
            moveLabel = disp->currentMove();
        }
    }
    // 最終フォールバック
    if (moveLabel.isEmpty()) {
        moveLabel = QStringLiteral("ply %1").arg(ply);
    }

    // 評価値 / 差分（定跡の場合は0）
    // エンジンの評価値は「手番側から見た評価値」なので、
    // 先手視点で統一するために後手番（奇数ply）の場合は符号を反転
    bool isGoteTurn = (ply % 2 == 1);  // 奇数plyは後手番

    QString evalStr;
    int curVal = 0;
    if (isBook) {
        evalStr = QStringLiteral("-");
        curVal = m_prevEvalCp;  // 差分0
    } else if (mate != 0) {
        // 詰み表示も後手番なら反転
        int adjustedMate = isGoteTurn ? -mate : mate;
        evalStr = QStringLiteral("mate %1").arg(adjustedMate);
        curVal  = m_prevEvalCp; // 詰みは差分0扱い（前回値維持）
    } else if (scoreCp != std::numeric_limits<int>::min()) {
        // 後手番なら評価値を反転
        int adjustedScore = isGoteTurn ? -scoreCp : scoreCp;
        evalStr = QString::number(adjustedScore);
        curVal  = adjustedScore;
    } else {
        evalStr = QStringLiteral("0");
        curVal  = 0;
    }
    const QString diff = isBook ? QStringLiteral("-") : QString::number(curVal - m_prevEvalCp);
    m_prevEvalCp = curVal;

    qCDebug(lcAnalysis).noquote() << "commitPendingResult: ply=" << ply << "moveLabel=" << moveLabel << "evalStr=" << evalStr << "pv=" << pv.left(30);

    // KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    KifuAnalysisResultsDisplay* resultItem = new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        );

    // USI形式PVを設定（括弧で囲まれた確率情報を除去）
    QString usiPv = m_pendingPv;
    // 末尾の括弧部分（例: "(100.00%)"）を除去
    qsizetype parenPos = usiPv.indexOf('(');
    if (parenPos > 0) {
        usiPv = usiPv.left(parenPos).trimmed();
    }
    resultItem->setUsiPv(usiPv);

    // 局面SFENを設定
    if (m_refs.sfenHistory && ply >= 0 && ply < m_refs.sfenHistory->size()) {
        QString sfen = m_refs.sfenHistory->at(ply);
        // "position sfen ..."形式の場合は除去
        if (sfen.startsWith(QStringLiteral("position sfen "))) {
            sfen = sfen.mid(14);
        } else if (sfen.startsWith(QStringLiteral("position "))) {
            sfen = sfen.mid(9);
        }
        resultItem->setSfen(sfen);
    }

    // 最後の指し手（USI形式）を設定（読み筋表示ウィンドウのハイライト用）
    // ply=0 は開始局面なので指し手なし、ply>=1 は usiMoves[ply-1] が最後の指し手
    QString lastMove;
    if (m_refs.usiMoves && ply > 0 && ply <= m_refs.usiMoves->size()) {
        lastMove = m_refs.usiMoves->at(ply - 1);
        qCDebug(lcAnalysis).noquote() << "lastMove from usiMoves[" << (ply - 1) << "]=" << lastMove;
    } else if (m_refs.recordModel && ply > 0 && ply < m_refs.recordModel->rowCount()) {
        // フォールバック: 棋譜表記からUSI形式の指し手を抽出
        KifuDisplay* moveDisp = m_refs.recordModel->item(ply);
        if (moveDisp) {
            QString kanjiMoveStr = moveDisp->currentMove();
            lastMove = extractUsiMoveFromKanji(kanjiMoveStr);
            qCDebug(lcAnalysis).noquote() << "lastMove from kanji:" << kanjiMoveStr << "->" << lastMove;
        }
    } else {
        qCDebug(lcAnalysis).noquote() << "no lastMove: usiMoves=" << m_refs.usiMoves
                                      << "ply=" << ply
                                      << "usiMoves.size=" << (m_refs.usiMoves ? m_refs.usiMoves->size() : -1);
    }
    if (!lastMove.isEmpty()) {
        resultItem->setLastUsiMove(lastMove);
        qCDebug(lcAnalysis).noquote() << "setLastUsiMove: ply=" << ply << "lastMove=" << lastMove;
    }

    // 候補手を設定（前の行の読み筋の最初の指し手）
    int prevRow = m_refs.analysisModel->rowCount() - 1;  // 今追加しようとしている行の1つ前
    if (prevRow >= 0) {
        KifuAnalysisResultsDisplay* prevItem = m_refs.analysisModel->item(prevRow);
        if (prevItem) {
            QString prevPv = prevItem->principalVariation();
            if (!prevPv.isEmpty() && prevPv != tr("（定跡）")) {
                // 最初の指し手を取得（スペース区切りまたは末尾まで）
                // 読み筋は "▲７六歩(77)△８四歩(83)..." のような形式
                QString candidateMove;

                // 先手/後手を示す記号
                static const QString senteMark = QStringLiteral("▲");
                static const QString goteMark = QStringLiteral("△");

                // 漢字表記の場合: 先頭から次の△/▲の前までを取得
                qsizetype nextMark = -1;
                if (prevPv.startsWith(senteMark) || prevPv.startsWith(goteMark)) {
                    // 2文字目以降で次の△/▲を探す
                    qsizetype sentePos = prevPv.indexOf(senteMark, 1);
                    qsizetype gotePos = prevPv.indexOf(goteMark, 1);

                    if (sentePos > 0 && gotePos > 0) {
                        nextMark = qMin(sentePos, gotePos);
                    } else if (sentePos > 0) {
                        nextMark = sentePos;
                    } else if (gotePos > 0) {
                        nextMark = gotePos;
                    }
                }

                if (nextMark > 0) {
                    candidateMove = prevPv.left(nextMark);
                } else {
                    // △/▲が見つからなければ全体（1手のみの読み筋）
                    candidateMove = prevPv;
                }

                // 前の行の指し手の移動先と候補手の移動先が同じ場合は「同」表記に変換
                // 例: 前の指し手「△８八角成(22)」、候補手「▲８八銀(79)」→「▲同　銀(79)」
                // 注: prevMoveLabelは「   3 ▲２二角成(88)」のように行番号が先頭についている場合がある
                QString prevMoveLabel = prevItem->currentMove();
                qCDebug(lcAnalysis).noquote() << "prevMoveLabel=" << prevMoveLabel << "candidateMove=" << candidateMove;

                if (candidateMove.length() >= 3) {
                    // 前の指し手から移動先を抽出
                    // 行番号を除去して▲/△の位置を見つける
                    QString prevDestination;
                    qsizetype prevMarkPos = prevMoveLabel.indexOf(senteMark);
                    if (prevMarkPos < 0) {
                        prevMarkPos = prevMoveLabel.indexOf(goteMark);
                    }

                    if (prevMarkPos >= 0) {
                        // ▲/△の後の文字が「同」でなければ移動先を抽出
                        QString afterMark = prevMoveLabel.mid(prevMarkPos + 1);
                        if (!afterMark.startsWith(QStringLiteral("同"))) {
                            // 「２二」のような2文字の移動先を抽出
                            if (afterMark.length() >= 2) {
                                prevDestination = afterMark.left(2);
                            }
                        }
                    }

                    // 候補手から移動先を抽出
                    QString candDestination;
                    qsizetype candMarkPos = candidateMove.indexOf(senteMark);
                    if (candMarkPos < 0) {
                        candMarkPos = candidateMove.indexOf(goteMark);
                    }

                    if (candMarkPos >= 0) {
                        QString afterMark = candidateMove.mid(candMarkPos + 1);
                        if (!afterMark.startsWith(QStringLiteral("同"))) {
                            if (afterMark.length() >= 2) {
                                candDestination = afterMark.left(2);
                            }
                        }
                    }

                    qCDebug(lcAnalysis).noquote() << "prevDestination=" << prevDestination << "candDestination=" << candDestination;

                    // 移動先が同じなら「同」表記に変換
                    if (!prevDestination.isEmpty() && !candDestination.isEmpty() &&
                        prevDestination == candDestination) {
                        // 「▲８八銀(79)」→「▲同　銀(79)」
                        // candMarkPosの位置から変換
                        QString prefix = candidateMove.left(candMarkPos + 1);  // "▲" or "△"まで
                        QString suffix = candidateMove.mid(candMarkPos + 3);    // 駒種以降（銀(79)など）
                        candidateMove = prefix + QStringLiteral("同　") + suffix;
                        qCDebug(lcAnalysis).noquote() << "converted to 同 notation:" << candidateMove;
                    }
                }

                // 候補手に「同」が含まれている場合、「同　」（同＋全角空白）に統一
                // 「△同銀(31)」→「△同　銀(31)」
                if (candidateMove.contains(QStringLiteral("同"))) {
                    // 「同」の後に全角空白がない場合は追加
                    // ただし「同　」は既にあるのでスキップ
                    if (!candidateMove.contains(QStringLiteral("同　"))) {
                        candidateMove.replace(QStringLiteral("同"), QStringLiteral("同　"));
                        qCDebug(lcAnalysis).noquote() << "added space after 同:" << candidateMove;
                    }
                }

                resultItem->setCandidateMove(candidateMove);
                qCDebug(lcAnalysis).noquote() << "setCandidateMove:" << candidateMove;
            }
        }
    }

    m_refs.analysisModel->appendItem(resultItem);

    // GUI更新用に結果を保存（次のonPositionPreparedでシグナルを発行）
    m_lastCommittedPly = ply;
    m_lastCommittedScoreCp = curVal;
}

void AnalysisResultHandler::showPvBoardDialog(int row)
{
    qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row=" << row;

    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: analysisModel is null";
        return;
    }

    if (row < 0 || row >= m_refs.analysisModel->rowCount()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row out of range";
        return;
    }

    // KifuAnalysisResultsDisplayから読み筋を取得
    KifuAnalysisResultsDisplay* item = m_refs.analysisModel->item(row);
    if (!item) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: item is null";
        return;
    }

    QString kanjiPv = item->principalVariation();
    qCDebug(lcAnalysis).noquote() << "kanjiPv=" << kanjiPv.left(50);

    if (kanjiPv.isEmpty()) {
        qCDebug(lcAnalysis).noquote() << "kanjiPv is empty";
        return;
    }

    // 局面SFENを取得（itemから）
    QString baseSfen = item->sfen();
    if (baseSfen.isEmpty()) {
        // フォールバック: m_sfenHistoryから取得
        if (m_refs.sfenHistory && row < m_refs.sfenHistory->size()) {
            baseSfen = m_refs.sfenHistory->at(row);
            // "position sfen ..."形式の場合は除去
            if (baseSfen.startsWith(QStringLiteral("position sfen "))) {
                baseSfen = baseSfen.mid(14);
            } else if (baseSfen.startsWith(QStringLiteral("position "))) {
                baseSfen = baseSfen.mid(9);
            }
        }
    }
    if (baseSfen.isEmpty()) {
        baseSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    qCDebug(lcAnalysis).noquote() << "baseSfen=" << baseSfen.left(50);

    // USI形式の読み筋を取得（itemから）
    QString usiPvStr = item->usiPv();
    QStringList usiMoves;
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(' ', Qt::SkipEmptyParts);
        qCDebug(lcAnalysis).noquote() << "usiMoves from item:" << usiMoves;
    }

    // PvBoardDialogを表示
    QWidget* parentWidget = nullptr;
    if (m_refs.presenter && m_refs.presenter->container()) {
        parentWidget = m_refs.presenter->container();
    }

    PvBoardDialog* dlg = new PvBoardDialog(baseSfen, usiMoves, parentWidget);
    dlg->setKanjiPv(kanjiPv);

    // 対局者名を設定（Refsから取得した名前を使用）
    QString blackName = m_refs.blackPlayerName.isEmpty() ? tr("先手") : m_refs.blackPlayerName;
    QString whiteName = m_refs.whitePlayerName.isEmpty() ? tr("後手") : m_refs.whitePlayerName;
    dlg->setPlayerNames(blackName, whiteName);

    // GUI本体の盤面反転状態を反映
    dlg->setFlipMode(m_refs.boardFlipped);

    // 最後の指し手を設定（初期局面のハイライト用）
    QString lastMove = item->lastUsiMove();
    if (lastMove.isEmpty()) {
        // フォールバック: 棋譜表記からUSI形式の指し手を抽出
        // rowはitem->moveNum()と同じはずだが、念のためrowを使用
        int ply = row;  // 解析結果のrow番号は手数(ply)と一致
        if (m_refs.recordModel && ply > 0 && ply < m_refs.recordModel->rowCount()) {
            KifuDisplay* moveDisp = m_refs.recordModel->item(ply);
            if (moveDisp) {
                QString moveLabel = moveDisp->currentMove();
                lastMove = extractUsiMoveFromKanji(moveLabel);
                qCDebug(lcAnalysis).noquote() << "lastMove from kanji:" << moveLabel << "->" << lastMove;
            }
        }
    }
    if (!lastMove.isEmpty()) {
        qCDebug(lcAnalysis).noquote() << "setting lastMove:" << lastMove;
        dlg->setLastMove(lastMove);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

QString AnalysisResultHandler::extractUsiMoveFromKanji(const QString& kanjiMove)
{
    // 漢字表記からUSI形式の指し手を抽出する
    // 形式例:
    //   「▲７六歩(77)」 → "7g7f" （通常移動）
    //   「△同　銀(31)」 → 同表記は前回の移動先が必要なのでスキップ
    //   「▲５五角打」   → "B*5e" （駒打ち）
    //   「▲３三歩成(34)」 → "3d3c+" （成り）

    if (kanjiMove.isEmpty()) {
        return QString();
    }

    // 先手・後手マークを探す
    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = kanjiMove.indexOf(senteMark);
    if (markPos < 0) {
        markPos = kanjiMove.indexOf(goteMark);
    }
    if (markPos < 0 || kanjiMove.length() <= markPos + 2) {
        return QString();
    }

    QString afterMark = kanjiMove.mid(markPos + 1);

    // 「同」表記の場合は前回の移動先が必要なのでスキップ
    if (afterMark.startsWith(QStringLiteral("同"))) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: 同 notation, cannot extract USI move";
        return QString();
    }

    // 駒打ち判定（「打」を含む場合）
    bool isDrop = afterMark.contains(QStringLiteral("打"));

    // 成り判定（「成」を含む場合）
    bool isPromotion = afterMark.contains(QStringLiteral("成")) && !afterMark.contains(QStringLiteral("不成"));

    // 移動先座標を取得（全角数字 + 漢数字）
    QChar fileChar = afterMark.at(0);  // 全角数字 '１'〜'９'
    QChar rankChar = afterMark.at(1);  // 漢数字 '一'〜'九'

    // 全角数字を整数に変換
    int fileTo = 0;
    if (fileChar >= QChar(0xFF11) && fileChar <= QChar(0xFF19)) {
        fileTo = fileChar.unicode() - 0xFF11 + 1;
    }

    // 漢数字を整数に変換
    int rankTo = 0;
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    qsizetype rankIdx = kanjiRanks.indexOf(rankChar);
    if (rankIdx >= 0) {
        rankTo = static_cast<int>(rankIdx) + 1;
    }

    if (fileTo < 1 || fileTo > 9 || rankTo < 1 || rankTo > 9) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid destination:" << fileTo << rankTo;
        return QString();
    }

    QChar toRankAlpha = QChar('a' + rankTo - 1);

    if (isDrop) {
        // 駒打ちの場合: "P*5e" 形式
        // 駒の種類を抽出（歩/香/桂/銀/金/角/飛）
        static const QString pieceChars = QStringLiteral("歩香桂銀金角飛");
        static const QString usiPieces = QStringLiteral("PLNSGBR");

        QChar pieceUsi;
        for (qsizetype i = 0; i < pieceChars.size(); ++i) {
            if (afterMark.contains(pieceChars.at(i))) {
                pieceUsi = usiPieces.at(i);
                break;
            }
        }

        if (pieceUsi.isNull()) {
            qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: could not identify piece for drop";
            return QString();
        }

        return QString("%1*%2%3").arg(pieceUsi).arg(fileTo).arg(toRankAlpha);
    } else {
        // 通常移動の場合: 括弧内の元位置を取得 "(77)" → file=7, rank=7
        qsizetype parenStart = afterMark.indexOf('(');
        qsizetype parenEnd = afterMark.indexOf(')');

        if (parenStart < 0 || parenEnd < 0 || parenEnd <= parenStart + 1) {
            qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: could not find source position in parentheses";
            return QString();
        }

        QString srcStr = afterMark.mid(parenStart + 1, parenEnd - parenStart - 1);
        if (srcStr.length() != 2) {
            qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid source string:" << srcStr;
            return QString();
        }

        int fileFrom = srcStr.at(0).digitValue();
        int rankFrom = srcStr.at(1).digitValue();

        if (fileFrom < 1 || fileFrom > 9 || rankFrom < 1 || rankFrom > 9) {
            qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid source coordinates:" << fileFrom << rankFrom;
            return QString();
        }

        QChar fromRankAlpha = QChar('a' + rankFrom - 1);

        QString usiMove = QString("%1%2%3%4").arg(fileFrom).arg(fromRankAlpha).arg(fileTo).arg(toRankAlpha);
        if (isPromotion) {
            usiMove += '+';
        }

        return usiMove;
    }
}
