/// @file kifexporter.cpp
/// @brief KIF形式棋譜エクスポータクラスの実装

#include "kifexporter.h"
#include "gameinfo.h"
#include "gameinfokeys.h"
#include "handicaps.h"
#include "kifubranchtree.h"
#include "bodtextgenerator.h"
#include "logcategories.h"
#include "kifuexportcommon.h"
#include "kifubranchnode.h"
#include "kifmovetext.h"
#include "shogiposition.h"

#include <QStringView>

// ========================================
// KIF形式の定数
// ========================================
namespace KifFormat {
    // 指し手フォーマットの幅設定
    constexpr int kMoveNumberWidth = 4;    // 手数の幅（右寄せ）
    constexpr int kMoveTextWidth = 12;     // 指し手テキストの幅（左寄せ）

    // セパレータ行
    constexpr QStringView kSeparatorLine = u"手数----指手---------消費時間--";

    // 既定の消費時間
    constexpr QStringView kDefaultTime = u"( 0:00/00:00:00)";
}

// ========================================
// ヘルパ関数
// ========================================

using KifuExportCommon::appendKifBookmarks;
using KifuExportCommon::appendKifComments;
using KifuExportCommon::buildEndingLine;
using KifuExportCommon::fwColonLine;
using KifuExportCommon::removeTurnMarker;

// ヘルパ関数: 一手の消費時間を KIF 形式（( m:ss/HH:MM:SS)）にする
static QString formatKifTime(const RecordTime& time)
{
    if (!time.valid) {
        // 読めなかった表記は、そのまま括弧で囲んで残す。時間が無ければ既定値
        const QString text = time.importedText.trimmed();
        if (text.isEmpty()) return KifFormat::kDefaultTime.toString();
        return text.startsWith(QLatin1Char('(')) ? text : QStringLiteral("( %1)").arg(text);
    }
    // KIF の時間は整数秒（ミリ秒は切り捨てる）。分は2桁の幅で右寄せ
    const qint64 elapsed = time.elapsedMs / 1000;
    const qint64 total = time.totalMs / 1000;
    return QStringLiteral("(%1:%2/%3:%4:%5)")
        .arg(elapsed / 60, 2, 10, QLatin1Char(' '))
        .arg(elapsed % 60, 2, 10, QLatin1Char('0'))
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

// ヘルパ関数: ノードが次の兄弟を持つか（Kifu for Windows準拠の '+' マーク判定）
// '+' は、同じ親の子リストで自分より後ろに兄弟がいる場合に付加する。
// つまり「後続の変化ブロックがまだ出力される」ことを示す。
static bool hasNextSibling(KifuBranchNode* node)
{
    if (node == nullptr || node->parent() == nullptr) return false;
    const int myIndex = node->parent()->indexOfChild(node);
    return myIndex >= 0 && myIndex < node->parent()->childCount() - 1;
}

// ヘルパ関数: KIF形式の指し手行を生成
static QString formatKifMoveLine(int moveNo, const QString& kifMove, const QString& time, bool hasBranch)
{
    const QString moveNoStr = QStringLiteral("%1").arg(moveNo, KifFormat::kMoveNumberWidth);
    const QString branchMark = hasBranch ? QStringLiteral("+") : QString();
    return QStringLiteral("%1 %2   %3%4")
        .arg(moveNoStr, kifMove.leftJustified(KifFormat::kMoveTextWidth), time, branchMark);
}

// ヘルパ関数: 指し手から KIF の表記を作る（「同　銀(31)」「２二角(88)」）。
// 読み込んだ原文（displayText）は形式ごとに書き方が違うので使わない。指し手が無いノードだけ原文に頼る
static QString kifMoveText(const KifuBranchNode* node)
{
    const ShogiMove move = node->move();
    if (!move.isValid()) return node->displayText().trimmed();
    BoardSquare previousTo;
    if (const KifuBranchNode* parent = node->parent(); parent && parent->isActualMove())
        previousTo = parent->move().move.to;
    // KIF 仕様では不成の手に「不成」を付けない（KI2 や画面上の表記とは異なる）。
    // https://kakinoki.o.oo7.jp/kif_format.html
    return KifMoveText::format(move.mover(), move.movingPiece, move.move, previousTo);
}

// ========================================
// 分岐出力用ヘルパ
// ========================================

// 手順のノードのうち fromPly 手目以降を KIF の指し手行として出力する。
// 終局語を除いた最後の指し手の手数と終局語を返す（本譜の終局行に使う）。
static void outputMoveNodes(const QList<KifuBranchNode*>& nodes, int fromPly, QStringList& out,
                            int* lastActualMoveNo = nullptr, const KifuBranchNode** terminalMove = nullptr)
{
    for (KifuBranchNode* node : nodes) {
        if (node->ply() < fromPly) continue;

        QString moveText = node->isTerminal()
            ? KifuExportCommon::singleLine(node->record().terminalLabel())
            : kifMoveText(node);
        if (node->terminalType() == TerminalType::Timeout) moveText = QStringLiteral("切れ負け");
        if (moveText.isEmpty()) continue;

        // 分岐マーク: Kifu for Windows 準拠で、親の子リストで自分より後ろに兄弟がいれば '+'
        out << formatKifMoveLine(node->ply(), removeTurnMarker(moveText), formatKifTime(node->time()),
                                 hasNextSibling(node));

        // しおり・コメント出力（指し手の後に）
        appendKifBookmarks(node->bookmark(), out);
        appendKifComments(node->comment(), out);

        if (node->isTerminal()) {
            if (terminalMove) *terminalMove = node;
        } else if (lastActualMoveNo) {
            *lastActualMoveNo = node->ply();
        }
    }
}

// ========================================
// エクスポート
// ========================================

QStringList KifExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    // 0〜1) 文字コード宣言・対局情報・平手でない開始局面の盤面図
    const bool handicapNames = appendHeaderLines(ctx, out, HeaderOptions{});

    // 2) セパレータ
    out << KifFormat::kSeparatorLine.toString();

    // 3) 本譜の手順（分岐ツリーの各ノードの最初の子をたどる）
    const KifuBranchTree* branchTree = &recordTree;
    const QList<KifuBranchNode*> mainNodes =
        (branchTree != nullptr) ? branchTree->mainLine() : QList<KifuBranchNode*>();

    // 4) 開始局面のしおり・コメントを先に出力
    if (!mainNodes.isEmpty() && mainNodes.first()->ply() == 0) {
        appendKifBookmarks(mainNodes.first()->bookmark(), out);
        appendKifComments(mainNodes.first()->comment(), out);
    }

    // 5) 各指し手を出力（変化と同じ書き方）
    int lastActualMoveNo = 0;
    const KifuBranchNode* terminalMove = nullptr;
    outputMoveNodes(mainNodes, 1, out, &lastActualMoveNo, &terminalMove);

    // 6) 終了行（本譜のみ）
    out << buildEndingLine(lastActualMoveNo, terminalMove ? terminalMove->terminalType() : TerminalType::None,
                           terminalMove ? terminalMove->terminalSide() : Turn::Black,
                           handicapNames, terminalMove ? terminalMove->record().terminalText : QString());

    // 8) 変化（分岐）を出力（KifuBranchTree から）
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            // 変化ヘッダ（空行 + 変化：N手）と、分岐点以降の指し手
            out << QString();
            out << QStringLiteral("変化：%1手").arg(line.branchPly);
            outputMoveNodes(line.nodesFrom(line.branchPly), line.branchPly, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKifLines (WITH VARIATIONS): generated"
                              << out.size() << "lines,"
                              << "lastActualMoveNo=" << lastActualMoveNo;

    return out;
}

// ========================================
// 先頭の行（KIF・KI2 で共通）
// ========================================

bool KifExporter::appendHeaderLines(const KifuExportMetadata& ctx, QStringList& out, HeaderOptions options)
{
    out << QStringLiteral("#KIF version=2.0 encoding=UTF-8");
    out << QStringLiteral("# ---- Generated by ShogiBoardQ ----");

    const QList<KifGameInfoItem>& header = ctx.gameInfoItems;
    const bool handicapNames = GameInfo::usesHandicapNames(header, ctx.startSfen);
    // 手合割の行が削除されていても、開始局面は盤面図で保持する。
    bool isNonStandard = !Handicaps::isHirate(ctx.startSfen);
    for (const auto& it : std::as_const(header)) {
        const QString key = it.key.trimmed();
        const QString value = it.value.trimmed();
        if (key.isEmpty()) continue;
        if (!options.consumedTime && key == GameInfoKeys::kConsumedTime) continue;
        out << fwColonLine(key, value);
        if (key == GameInfoKeys::kHandicap && value == GameInfoKeys::kHandicapOther) isNonStandard = true;
    }

    if (isNonStandard) out << sfenToBodLines(ctx.startSfen, handicapNames);
    return handicapNames;
}

// ========================================
// BOD形式出力
// ========================================

QStringList KifExporter::sfenToBodLines(const QString& sfen, bool handicapNames)
{
    const std::optional<Position> position = Position::fromSfenLike(sfen);
    // 平手ならBODは不要
    if (!position || Handicaps::isHirate(*position)) return {};
    return BodTextGenerator::generate(*position, 0, QString(), handicapNames).split(QLatin1Char('\n'));
}
