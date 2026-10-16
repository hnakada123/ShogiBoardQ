/// @file usenexporter.cpp
/// @brief USEN形式棋譜エクスポータクラスの実装

#include "usenexporter.h"
#include "terminalcodes.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "logcategories.h"
#include "kifuexportcommon.h"
#include "sfenutils.h"

#include <QStringView>

// ========================================
// USEN形式エンコーディング
// ========================================

// USEN形式の座標系:
// - マス番号: sq = (rank - 1) * 9 + (file - 1)
// - 例: 1一 = 0, 9九 = 80
// - rank: 段（一=1, 二=2, ..., 九=9）
// - file: 筋（1=1, 2=2, ..., 9=9）

// 駒打ち時の駒コード (from_sq = 81 + piece_code):
// 歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
static int usiPieceToUsenDropCode(QChar usiPiece)
{
    switch (usiPiece.toUpper().toLatin1()) {
    case 'P': return 10;  // 歩
    case 'L': return 11;  // 香
    case 'N': return 12;  // 桂
    case 'S': return 13;  // 銀
    case 'G': return 9;   // 金
    case 'B': return 14;  // 角
    case 'R': return 15;  // 飛
    default:  return -1;  // 無効
    }
}

// base36文字セット
static constexpr QStringView kUsenBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";

static QString intToBase36(int moveCode)
{
    // 3文字のbase36に変換（範囲: 0-46655）
    if (moveCode < 0 || moveCode > 46655) {
        qCWarning(lcKifu) << "moveCode out of range:" << moveCode;
        return QStringLiteral("000");
    }

    QString result;
    result.reserve(3);

    int v2 = moveCode % 36;
    moveCode /= 36;
    int v1 = moveCode % 36;
    moveCode /= 36;
    int v0 = moveCode;

    result.append(kUsenBase36Chars.at(v0));
    result.append(kUsenBase36Chars.at(v1));
    result.append(kUsenBase36Chars.at(v2));

    return result;
}

static QString encodeUsiMoveToUsen(const QString& usiMove)
{
    if (usiMove.isEmpty() || usiMove.size() < 4) {
        return QString();
    }

    // 成りフラグを確認
    bool isPromotion = usiMove.endsWith(QLatin1Char('+'));

    int from_sq, to_sq;

    // 駒打ちの判定 (P*5e 形式)
    if (usiMove.at(1) == QLatin1Char('*')) {
        // 駒打ち: from_sq = 81 + piece_code
        QChar pieceChar = usiMove.at(0);
        int pieceCode = usiPieceToUsenDropCode(pieceChar);
        if (pieceCode < 0) {
            qCWarning(lcKifu) << "Unknown drop piece:" << pieceChar;
            return QString();
        }
        from_sq = 81 + pieceCode;

        // 移動先を解析
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    } else {
        // 盤上移動: 7g7f 形式
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        from_sq = (fromRank - 1) * 9 + (fromFile - 1);
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    }

    // エンコード: code = (from_sq * 81 + to_sq) * 2 + (成る場合は1)
    int moveCode = (from_sq * 81 + to_sq) * 2 + (isPromotion ? 1 : 0);

    return intToBase36(moveCode);
}

static QString sfenToUsenPosition(const QString& sfen)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QString();  // 平手: 空文字列（~の前に何も置かない）
    }

    // 平手初期局面の場合は空文字列を返す
    if (SfenUtils::isHirateStart(trimmed)) {
        return QString();
    }

    // カスタム局面: SFEN -> USEN変換
    // 手数（4番目のフィールド）を除去してからエンコード
    // SFEN形式: "board turn hands [movecount]"
    const QStringList parts = trimmed.split(QLatin1Char(' '));
    QString sfenWithoutMoveCount;
    if (parts.size() >= 3) {
        sfenWithoutMoveCount = parts[0] + QLatin1Char(' ') + parts[1] + QLatin1Char(' ') + parts[2];
    } else {
        sfenWithoutMoveCount = trimmed;
    }

    // '/' -> '_', ' ' -> '.', '+' -> 'z'
    QString usen = sfenWithoutMoveCount;
    usen.replace(QLatin1Char('/'), QLatin1Char('_'));
    usen.replace(QLatin1Char(' '), QLatin1Char('.'));
    usen.replace(QLatin1Char('+'), QLatin1Char('z'));

    return usen;
}

// ========================================
// エクスポート
// ========================================

QStringList UsenExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    // 1) 初期局面をUSEN形式に変換
    QString position = sfenToUsenPosition(ctx.startSfen);

    // 2) 本譜の指し手をエンコード
    QString mainMoves;
    QString terminalCode;

    // 本譜の各手（分岐ツリーのノード）をUSEN形式にエンコード
    const KifuBranchTree* branchTree = &recordTree;
    const QList<KifuBranchNode*> mainLine =
        (branchTree && !branchTree->isEmpty()) ? branchTree->mainLine() : QList<KifuBranchNode*>();
    for (const KifuBranchNode* node : mainLine) {
        if (node->isTerminal()) {
            terminalCode = TerminalCodes::usen(node->terminalType());
            break;
        }
        if (node->isActualMove()) mainMoves += encodeUsiMoveToUsen(node->move().move.toString());
    }

    // 本譜のUSEN文字列を構築
    // USEN構造: [初期局面]~[オフセット].[指し手][.終局コード]
    // 初期局面は~の前に置く（平手の場合は空）、本譜のオフセットは0
    QString usen = position + QStringLiteral("~0.%1").arg(mainMoves);

    // 未終局は空の終局フィールドで表す。
    usen += QStringLiteral(".") + terminalCode;

    // 3) 分岐を追加
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);

            // オフセット（分岐開始位置 - 1）
            int offset = line.branchPly - 1;
            if (offset < 0) offset = 0;

            QString branchMoves;
            QString branchTerminal;

            // 分岐点より後の各手（分岐ツリーのノード）
            for (qsizetype i = line.branchPly; i < line.nodes.size(); ++i) {
                const KifuBranchNode* node = line.nodes.at(i);
                if (node->isTerminal()) {
                    branchTerminal = TerminalCodes::usen(node->terminalType());
                    break;
                }
                if (node->isActualMove()) branchMoves += encodeUsiMoveToUsen(node->move().move.toString());
            }

            // 分岐のUSEN文字列を構築
            QString branchUsen = QStringLiteral("~%1.%2.%3").arg(offset).arg(branchMoves, branchTerminal);

            usen += branchUsen;
        }
    }

    out << usen;

    const int branchCount = (branchTree != nullptr) ? (branchTree->lineCount() - 1) : 0;
    qCDebug(lcKifu).noquote() << "toUsenLines: generated USEN with"
                              << mainMoves.size() / 3 << "mainline moves,"
                              << branchCount << "variations";

    return out;
}
