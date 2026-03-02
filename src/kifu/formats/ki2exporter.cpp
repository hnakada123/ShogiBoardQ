/// @file ki2exporter.cpp
/// @brief KI2形式棋譜エクスポータクラスの実装

#include "ki2exporter.h"
#include "ki2tosfenconverter.h"
#include "kifexporter.h"
#include "kifubranchtree.h"
#include "logcategories.h"

#include <QRegularExpression>

// ========================================
// ヘルパ関数
// ========================================

static inline QString fwColonLine(const QString& key, const QString& val)
{
    return QStringLiteral("%1：%2").arg(key, val);
}

// ヘルパ関数: 指し手から手番記号（▲△）を除去
static QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

// ヘルパ関数: 終局語を判定
static bool isTerminalMove(const QString& move)
{
    static const QStringList terminals = {
        QStringLiteral("投了"),
        QStringLiteral("中断"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("不詰")
    };
    const QString stripped = removeTurnMarker(move);
    for (const QString& t : terminals) {
        if (stripped.contains(t)) return true;
    }
    return false;
}

// ヘルパ関数: 終局結果文字列を生成
static QString buildEndingLine(int lastActualMoveNo, const QString& terminalMove)
{
    const QString stripped = removeTurnMarker(terminalMove);

    const bool lastMoveBySente = (lastActualMoveNo % 2 != 0);
    const QString senteStr = QStringLiteral("先手");
    const QString goteStr = QStringLiteral("後手");

    if (stripped.contains(QStringLiteral("投了"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("切れ負け"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("反則勝ち"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        }
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("反則負け"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        }
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? goteStr : senteStr);
    }
    if (stripped.contains(QStringLiteral("入玉勝ち"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        }
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }
    if (stripped.contains(QStringLiteral("不戦勝"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        }
    }
    if (stripped.contains(QStringLiteral("不戦敗"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        }
    }
    if (stripped.contains(QStringLiteral("千日手"))) {
        return QStringLiteral("まで%1手で千日手").arg(QString::number(lastActualMoveNo));
    }
    if (stripped.contains(QStringLiteral("持将棋"))) {
        return QStringLiteral("まで%1手で持将棋").arg(QString::number(lastActualMoveNo));
    }
    if (stripped.contains(QStringLiteral("中断"))) {
        return QStringLiteral("まで%1手で中断").arg(QString::number(lastActualMoveNo));
    }
    if (stripped.contains(QStringLiteral("不詰"))) {
        return QStringLiteral("まで%1手で不詰").arg(QString::number(lastActualMoveNo));
    }

    return QStringLiteral("まで%1手").arg(QString::number(lastActualMoveNo));
}

// ヘルパ関数: しおり行を出力リストに追加
static void appendKifBookmarks(const QString& bookmark, QStringList& out)
{
    if (bookmark.trimmed().isEmpty()) return;

    const QStringList lines = bookmark.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& name : lines) {
        const QString t = name.trimmed();
        if (!t.isEmpty()) {
            out << (QStringLiteral("&") + t);
        }
    }
}

// ヘルパ関数: コメント行を出力リストに追加
static void appendKifComments(const QString& comment, QStringList& out)
{
    if (comment.trimmed().isEmpty()) return;

    static const QRegularExpression newlineRe(QStringLiteral("\r?\n"));
    const QStringList lines = comment.split(newlineRe, Qt::KeepEmptyParts);
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith(QStringLiteral("【しおり】"))) {
            const QString name = t.mid(5).trimmed();
            if (!name.isEmpty()) {
                out << (QStringLiteral("&") + name);
            }
            continue;
        }
        if (t.startsWith(QLatin1Char('*'))) {
            out << t;
        } else {
            out << (QStringLiteral("*") + t);
        }
    }
}

// ========================================
// 分岐出力用ヘルパ
// ========================================

static void outputKi2VariationFromBranchLine(const BranchLine& line, const QString& startSfen, QStringList& out)
{
    // 本譜（lineIndex == 0）の場合は何もしない
    if (line.lineIndex == 0) {
        return;
    }

    // 変化ヘッダを出力（空行 + 変化：N手）
    out << QString();
    out << QStringLiteral("変化：%1手").arg(line.branchPly);

    // 盤面状態を初期化し、分岐点まで進める
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    Ki2ToSfenConverter::initBoardFromSfen(startSfen, boardState, blackHands, whiteHands);
    bool blackToMove = !startSfen.contains(QStringLiteral(" w "));
    int prevToFile = 0, prevToRank = 0;

    // 分岐点より前のノードで盤面を進める（出力はしない）
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() >= line.branchPly) break;
        const QString moveText = node->displayText().trimmed();
        if (moveText.isEmpty() || isTerminalMove(moveText)) continue;

        Ki2ToSfenConverter::convertPrettyMoveToKi2(
            moveText, boardState, blackHands, whiteHands,
            blackToMove, prevToFile, prevToRank);
        blackToMove = !blackToMove;
    }

    // 変化の指し手を出力（branchPly以降のノード）
    QStringList movesOnLine;
    bool lastMoveHadComment = false;

    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        // 分岐点より前のノードはスキップ
        if (node->ply() < line.branchPly) {
            continue;
        }

        const QString moveText = node->displayText().trimmed();

        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;

        const bool isTerminal = isTerminalMove(moveText);

        // KI2形式: 修飾子付き変換 + 盤面更新
        QString ki2Move;
        if (!isTerminal) {
            ki2Move = Ki2ToSfenConverter::convertPrettyMoveToKi2(
                moveText, boardState, blackHands, whiteHands,
                blackToMove, prevToFile, prevToRank);
            blackToMove = !blackToMove;
        } else {
            ki2Move = moveText;
            static const QRegularExpression fromPosPattern(QStringLiteral("\\([0-9０-９]+[0-9０-９]+\\)$"));
            ki2Move.remove(fromPosPattern);
        }

        const QString cmt = node->comment().trimmed();
        const QString bm = node->bookmark().trimmed();
        const bool hasComment = !cmt.isEmpty();
        const bool hasBookmark = !bm.isEmpty();

        if (hasComment || hasBookmark || lastMoveHadComment || isTerminal) {
            if (!movesOnLine.isEmpty()) {
                out << movesOnLine.join(QStringLiteral("    "));
                movesOnLine.clear();
            }
            out << ki2Move;

            appendKifBookmarks(bm, out);
            if (hasComment) {
                appendKifComments(cmt, out);
            }
            lastMoveHadComment = hasComment || hasBookmark;
        } else {
            movesOnLine.append(ki2Move);
            lastMoveHadComment = false;
        }
    }

    // 残りの指し手を出力
    if (!movesOnLine.isEmpty()) {
        out << movesOnLine.join(QStringLiteral("    "));
    }
}

// ========================================
// エクスポート
// ========================================

QStringList Ki2Exporter::exportLines(const GameRecordModel& model,
                                     const GameRecordModel::ExportContext& ctx)
{
    QStringList out;

    // 0) プログラムコメント・文字コード宣言
    out << QStringLiteral("#KIF version=2.0 encoding=UTF-8");
    out << QStringLiteral("# ---- Generated by ShogiBoardQ ----");

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = GameRecordModel::collectGameInfo(ctx);
    bool isNonStandard = false;
    for (const auto& it : header) {
        if (!it.key.trimmed().isEmpty()) {
            // 消費時間は KI2 では省略
            if (it.key.trimmed() == QStringLiteral("消費時間")) continue;
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
            if (it.key.trimmed() == QStringLiteral("手合割") && it.value.trimmed() == QStringLiteral("その他"))
                isNonStandard = true;
        }
    }

    // 1.5) 非標準局面の場合はBOD盤面を挿入
    if (isNonStandard) {
        const QStringList bodLines = KifExporter::sfenToBodLines(ctx.startSfen);
        if (!bodLines.isEmpty())
            out << bodLines;
    }

    // 2) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();

    // 3) 開始局面のしおり・コメントを先に出力
    int startIdx = 0;
    if (!disp.isEmpty()
        && (disp[0].prettyMove.trimmed().isEmpty()
            || disp[0].prettyMove.contains(QStringLiteral("開始局面")))) {
        appendKifBookmarks(disp[0].bookmark, out);
        appendKifComments(disp[0].comment, out);
        startIdx = 1; // 実際の指し手は次から
    }

    // 4) 盤面状態を初期化（修飾子生成用）
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    Ki2ToSfenConverter::initBoardFromSfen(ctx.startSfen, boardState, blackHands, whiteHands);
    bool blackToMove = !ctx.startSfen.contains(QStringLiteral(" w "));
    int prevToFile = 0, prevToRank = 0;

    // 5) 各指し手を出力（KI2形式）
    int moveNo = 1;
    int lastActualMoveNo = 0;
    QString terminalMove;
    QStringList movesOnLine;  // 現在の行に蓄積する指し手
    bool lastMoveHadComment = false;

    for (qsizetype i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();

        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;

        // 終局語の判定
        const bool isTerminal = isTerminalMove(moveText);

        // KI2形式: 修飾子付き変換 + 盤面更新
        QString ki2Move;
        if (!isTerminal) {
            ki2Move = Ki2ToSfenConverter::convertPrettyMoveToKi2(
                moveText, boardState, blackHands, whiteHands,
                blackToMove, prevToFile, prevToRank);
            blackToMove = !blackToMove;
        } else {
            ki2Move = moveText;
            // 移動元情報 (xx) を削除（終局語の場合は通常不要だが念のため）
            static const QRegularExpression fromPosPattern(QStringLiteral("\\([0-9０-９]+[0-9０-９]+\\)$"));
            ki2Move.remove(fromPosPattern);
        }

        const QString cmt = it.comment.trimmed();
        const QString bm = it.bookmark.trimmed();
        const bool hasComment = !cmt.isEmpty();
        const bool hasBookmark = !bm.isEmpty();

        if (isTerminal) {
            // 終局手（投了など）は結果行で表すため、指し手としては出力しない
            if (!movesOnLine.isEmpty()) {
                out << movesOnLine.join(QStringLiteral("    "));
                movesOnLine.clear();
            }
        } else if (hasComment || hasBookmark || lastMoveHadComment) {
            // コメント/しおりがある場合は指し手を吐き出してから単独行で出力
            if (!movesOnLine.isEmpty()) {
                out << movesOnLine.join(QStringLiteral("    "));
                movesOnLine.clear();
            }
            out << ki2Move;

            // しおり・コメント出力
            appendKifBookmarks(bm, out);
            if (hasComment) {
                appendKifComments(cmt, out);
            }
            lastMoveHadComment = hasComment || hasBookmark;
        } else {
            // コメント・しおりがない場合は指し手を蓄積
            movesOnLine.append(ki2Move);
            lastMoveHadComment = false;
        }

        if (isTerminal) {
            terminalMove = moveText;
        } else {
            lastActualMoveNo = moveNo;
        }
        ++moveNo;
    }

    // 残りの指し手を出力
    if (!movesOnLine.isEmpty()) {
        out << movesOnLine.join(QStringLiteral("    "));
    }

    // 6) 終了行
    out << buildEndingLine(lastActualMoveNo, terminalMove);

    // 7) 変化（分岐）を出力（KifuBranchTree から）
    KifuBranchTree* branchTree = model.branchTree();
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            outputKi2VariationFromBranchLine(line, ctx.startSfen, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKi2Lines: generated"
                              << out.size() << "lines,"
                              << lastActualMoveNo << "moves";

    return out;
}
