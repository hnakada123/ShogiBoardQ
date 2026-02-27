/// @file kifexporter.cpp
/// @brief KIF形式棋譜エクスポータクラスの実装

#include "kifexporter.h"
#include "kifubranchtree.h"
#include "logcategories.h"

#include <QRegularExpression>
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

// ヘルパ関数: KIF形式の時間文字列にフォーマット（括弧付き）
// 仕様: ( m:ss/HH:MM:SS) 形式
static QString formatKifTime(const QString& timeText)
{
    // 既に括弧付きならそのまま返す
    if (timeText.startsWith(QLatin1Char('('))) return timeText;
    // 空なら既定値
    if (timeText.isEmpty()) return KifFormat::kDefaultTime.toString();

    // mm:ss/HH:MM:SS 形式を解析して ( m:ss/HH:MM:SS) 形式に変換
    const QStringList parts = timeText.split(QLatin1Char('/'));
    if (parts.size() == 2) {
        QString moveTime = parts[0];  // mm:ss
        QString totalTime = parts[1]; // HH:MM:SS

        // 分の先頭ゼロを除去（例: "00:00" → " 0:00", "01:30" → " 1:30"）
        if (moveTime.length() >= 2 && moveTime.at(0) == QLatin1Char('0')) {
            moveTime = QStringLiteral(" %1").arg(moveTime.mid(1));
        }

        return QStringLiteral("(%1/%2)").arg(moveTime, totalTime);
    }

    // 解析できない場合はそのまま括弧で囲む
    return QStringLiteral("( %1)").arg(timeText);
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
// KIF形式仕様: 「まで○手で○手の勝ち」または「まで○手で○○」
static QString buildEndingLine(int lastActualMoveNo, const QString& terminalMove)
{
    const QString stripped = removeTurnMarker(terminalMove);

    // 勝者判定: lastActualMoveNo が奇数なら先手の手、偶数なら後手の手が最後
    const bool lastMoveBySente = (lastActualMoveNo % 2 != 0);
    const QString senteStr = QStringLiteral("先手");
    const QString goteStr = QStringLiteral("後手");

    // 投了: 直前の指し手側が勝ち
    if (stripped.contains(QStringLiteral("投了"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }

    // 詰み: 詰ませた側（直前の指し手側）が勝ち
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }

    // 切れ負け: 手番側（次に指す側）が負け = 直前の指し手側が勝ち
    if (stripped.contains(QStringLiteral("切れ負け"))) {
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }

    // 反則勝ち: 手番記号があればその側が勝ち、なければ直前の相手側が勝ち
    if (stripped.contains(QStringLiteral("反則勝ち"))) {
        // 手番記号で判定（▲反則勝ち = 先手の勝ち）
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        }
        // 手番記号なし: 直前の相手が反則 = 直前の指し手側の勝ち
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }

    // 反則負け: 手番記号があればその側が負け
    if (stripped.contains(QStringLiteral("反則負け"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        }
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? goteStr : senteStr);
    }

    // 入玉勝ち: 手番記号で判定
    if (stripped.contains(QStringLiteral("入玉勝ち"))) {
        if (terminalMove.startsWith(QStringLiteral("▲"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), senteStr);
        } else if (terminalMove.startsWith(QStringLiteral("△"))) {
            return QStringLiteral("まで%1手で%2の勝ち").arg(QString::number(lastActualMoveNo), goteStr);
        }
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), lastMoveBySente ? senteStr : goteStr);
    }

    // 不戦勝/不戦敗
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

    // 千日手: 引き分け
    if (stripped.contains(QStringLiteral("千日手"))) {
        return QStringLiteral("まで%1手で千日手").arg(QString::number(lastActualMoveNo));
    }

    // 持将棋: 引き分け
    if (stripped.contains(QStringLiteral("持将棋"))) {
        return QStringLiteral("まで%1手で持将棋").arg(QString::number(lastActualMoveNo));
    }

    // 中断
    if (stripped.contains(QStringLiteral("中断"))) {
        return QStringLiteral("まで%1手で中断").arg(QString::number(lastActualMoveNo));
    }

    // 不詰
    if (stripped.contains(QStringLiteral("不詰"))) {
        return QStringLiteral("まで%1手で不詰").arg(QString::number(lastActualMoveNo));
    }

    // その他: 手数のみ
    return QStringLiteral("まで%1手").arg(QString::number(lastActualMoveNo));
}

// ヘルパ関数: ノードが次の兄弟を持つか（Kifu for Windows準拠の '+' マーク判定）
// '+' は、同じ親の子リストで自分より後ろに兄弟がいる場合に付加する。
// つまり「後続の変化ブロックがまだ出力される」ことを示す。
static bool hasNextSibling(KifuBranchNode* node)
{
    if (node == nullptr || node->parent() == nullptr) return false;
    const QVector<KifuBranchNode*>& siblings = node->parent()->children();
    const auto myIndex = siblings.indexOf(node);
    return myIndex >= 0 && myIndex < siblings.size() - 1;
}

// ヘルパ関数: KIF形式の指し手行を生成
static QString formatKifMoveLine(int moveNo, const QString& kifMove, const QString& time, bool hasBranch)
{
    const QString moveNoStr = QStringLiteral("%1").arg(moveNo, KifFormat::kMoveNumberWidth);
    const QString branchMark = hasBranch ? QStringLiteral("+") : QString();
    return QStringLiteral("%1 %2   %3%4")
        .arg(moveNoStr, kifMove.leftJustified(KifFormat::kMoveTextWidth), time, branchMark);
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
        // 後方互換: コメント中の【しおり】マーカーを & 行として出力
        if (t.startsWith(QStringLiteral("【しおり】"))) {
            const QString name = t.mid(5).trimmed(); // "【しおり】" is 5 chars
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

static void outputVariationFromBranchLine(const BranchLine& line, QStringList& out)
{
    // 本譜（lineIndex == 0）の場合は何もしない
    if (line.lineIndex == 0) {
        return;
    }

    // 変化ヘッダを出力（空行 + 変化：N手）
    out << QString();
    out << QStringLiteral("変化：%1手").arg(line.branchPly);

    // 変化の指し手を出力（branchPly以降のノード）
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        // 分岐点より前のノードはスキップ
        if (node->ply() < line.branchPly) {
            continue;
        }

        const QString moveText = node->displayText().trimmed();

        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;

        // 手番記号を除去してKIF形式に変換
        const QString kifMove = removeTurnMarker(moveText);

        // 時間フォーマット（括弧付き）
        const QString time = formatKifTime(node->timeText());

        // 分岐マークの判定（このノードに次の兄弟変化があるか）
        // Kifu for Windows準拠: 親の子リストで自分より後ろに兄弟がいれば '+' を付加
        const bool hasBranch = hasNextSibling(node);

        // KIF形式で出力
        out << formatKifMoveLine(node->ply(), kifMove, time, hasBranch);

        // しおり・コメント出力
        appendKifBookmarks(node->bookmark(), out);
        appendKifComments(node->comment(), out);
    }
}

// ========================================
// エクスポート
// ========================================

QStringList KifExporter::exportLines(const GameRecordModel& model,
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
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
            if (it.key.trimmed() == QStringLiteral("手合割") && it.value.trimmed() == QStringLiteral("その他"))
                isNonStandard = true;
        }
    }

    // 1.5) 非標準局面の場合はBOD盤面を挿入
    if (isNonStandard) {
        const QStringList bodLines = sfenToBodLines(ctx.startSfen);
        if (!bodLines.isEmpty())
            out << bodLines;
    }

    // 2) セパレータ
    out << KifFormat::kSeparatorLine.toString();

    // 3) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();

    // 4) 本譜ノードを手数でマップ（'+' マーク判定用）
    KifuBranchTree* branchTree = model.branchTree();
    QHash<int, KifuBranchNode*> mainLineNodesByPly;
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QVector<BranchLine> lines = branchTree->allLines();
        if (!lines.isEmpty()) {
            for (KifuBranchNode* node : std::as_const(lines.at(0).nodes)) {
                mainLineNodesByPly.insert(node->ply(), node);
            }
        }
    }

    // 5) 開始局面の処理（prettyMoveが空 or "開始局面" テキストのエントリ）
    int startIdx = 0;
    if (!disp.isEmpty()
        && (disp[0].prettyMove.trimmed().isEmpty()
            || disp[0].prettyMove.contains(QStringLiteral("開始局面")))) {
        // 開始局面のしおり・コメントを先に出力
        appendKifBookmarks(disp[0].bookmark, out);
        appendKifComments(disp[0].comment, out);
        startIdx = 1; // 実際の指し手は次から
    }

    // 6) 各指し手を出力
    int moveNo = 1;
    int lastActualMoveNo = 0;
    QString terminalMove;

    for (qsizetype i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();

        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;

        // 終局語の判定
        const bool isTerminal = isTerminalMove(moveText);

        // 手番記号を除去してKIF形式に変換
        const QString kifMove = removeTurnMarker(moveText);

        // 時間フォーマット（括弧付き）
        const QString time = formatKifTime(it.timeText);

        // 分岐マークの判定（このノードに次の兄弟変化があるか）
        const bool hasBranch = mainLineNodesByPly.contains(moveNo)
                               && hasNextSibling(mainLineNodesByPly.value(moveNo));

        // KIF形式で出力
        out << formatKifMoveLine(moveNo, kifMove, time, hasBranch);

        // しおり・コメント出力（指し手の後に）
        appendKifBookmarks(it.bookmark, out);
        appendKifComments(it.comment, out);

        if (isTerminal) {
            terminalMove = moveText;
        } else {
            lastActualMoveNo = moveNo;
        }
        ++moveNo;
    }

    // 7) 終了行（本譜のみ）
    out << buildEndingLine(lastActualMoveNo, terminalMove);

    // 8) 変化（分岐）を出力（KifuBranchTree から）
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QVector<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            outputVariationFromBranchLine(line, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKifLines (WITH VARIATIONS): generated"
                              << out.size() << "lines,"
                              << (moveNo - 1) << "moves, lastActualMoveNo=" << lastActualMoveNo
                              << "mainLineNodes=" << mainLineNodesByPly.size();

    return out;
}

// ========================================
// BOD形式出力
// ========================================

QStringList KifExporter::sfenToBodLines(const QString& sfen)
{
    if (sfen.trimmed().isEmpty()) return {};

    const QStringList parts = sfen.trimmed().split(QLatin1Char(' '));
    if (parts.size() < 3) return {};

    const QString& boardSfen = parts[0];
    const QString& turnSfen = parts[1];
    const QString& handSfen = parts[2];

    // 平手かどうかチェック（平手ならBOD不要）
    static const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    if (boardSfen == initPP && handSfen == QStringLiteral("-") && turnSfen == QStringLiteral("b"))
        return {};

    // 持ち駒を解析
    QMap<QChar, int> senteHand, goteHand;
    if (handSfen != QStringLiteral("-")) {
        int count = 0;
        for (const QChar c : handSfen) {
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                if (c.isUpper()) {
                    senteHand[c] += count;
                } else {
                    goteHand[c.toUpper()] += count;
                }
                count = 0;
            }
        }
    }

    // 持ち駒を日本語文字列に変換
    auto handToString = [](const QMap<QChar, int>& hand) -> QString {
        if (hand.isEmpty()) return QStringLiteral("なし");
        const char order[] = {'R','B','G','S','N','L','P'};
        const QMap<QChar, QString> pieceNames = {
            {QLatin1Char('R'), QStringLiteral("飛")}, {QLatin1Char('B'), QStringLiteral("角")},
            {QLatin1Char('G'), QStringLiteral("金")}, {QLatin1Char('S'), QStringLiteral("銀")},
            {QLatin1Char('N'), QStringLiteral("桂")}, {QLatin1Char('L'), QStringLiteral("香")},
            {QLatin1Char('P'), QStringLiteral("歩")}
        };
        const QMap<int, QString> kanjiNum = {
            {2, QStringLiteral("二")}, {3, QStringLiteral("三")}, {4, QStringLiteral("四")},
            {5, QStringLiteral("五")}, {6, QStringLiteral("六")}, {7, QStringLiteral("七")},
            {8, QStringLiteral("八")}, {9, QStringLiteral("九")}, {10, QStringLiteral("十")},
            {11, QStringLiteral("十一")}, {12, QStringLiteral("十二")}, {13, QStringLiteral("十三")},
            {14, QStringLiteral("十四")}, {15, QStringLiteral("十五")}, {16, QStringLiteral("十六")},
            {17, QStringLiteral("十七")}, {18, QStringLiteral("十八")}
        };
        QString result;
        for (char c : order) {
            const QChar piece = QLatin1Char(c);
            if (hand.contains(piece) && hand[piece] > 0) {
                result += pieceNames[piece];
                if (hand[piece] > 1)
                    result += kanjiNum.value(hand[piece], QString::number(hand[piece]));
                result += QStringLiteral("　");
            }
        }
        return result.isEmpty() ? QStringLiteral("なし") : result.trimmed();
    };

    // 盤面を9x9配列に展開
    const QMap<QChar, QString> unpromoted = {
        {QLatin1Char('P'), QStringLiteral("歩")}, {QLatin1Char('L'), QStringLiteral("香")},
        {QLatin1Char('N'), QStringLiteral("桂")}, {QLatin1Char('S'), QStringLiteral("銀")},
        {QLatin1Char('G'), QStringLiteral("金")}, {QLatin1Char('B'), QStringLiteral("角")},
        {QLatin1Char('R'), QStringLiteral("飛")}, {QLatin1Char('K'), QStringLiteral("玉")}
    };
    const QMap<QChar, QString> promoted = {
        {QLatin1Char('P'), QStringLiteral("と")}, {QLatin1Char('L'), QStringLiteral("杏")},
        {QLatin1Char('N'), QStringLiteral("圭")}, {QLatin1Char('S'), QStringLiteral("全")},
        {QLatin1Char('G'), QStringLiteral("金")}, {QLatin1Char('B'), QStringLiteral("馬")},
        {QLatin1Char('R'), QStringLiteral("龍")}, {QLatin1Char('K'), QStringLiteral("玉")}
    };

    QVector<QVector<QString>> board(9, QVector<QString>(9, QStringLiteral(" ・")));
    const QStringList ranks = boardSfen.split(QLatin1Char('/'));
    for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
        const QString& rankStr = ranks[rank];
        int file = 0;
        bool isPromoted = false;
        for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
            const QChar c = rankStr.at(k);
            if (c == QLatin1Char('+')) {
                isPromoted = true;
            } else if (c.isDigit()) {
                file += c.toLatin1() - '0';
                isPromoted = false;
            } else {
                const QString prefix = c.isUpper() ? QStringLiteral(" ") : QStringLiteral("v");
                const QChar upper = c.toUpper();
                const auto& map = isPromoted ? promoted : unpromoted;
                board[rank][file] = prefix + map.value(upper, QStringLiteral("？"));
                ++file;
                isPromoted = false;
            }
        }
    }

    // BOD行を生成
    static const QStringList rankNames = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };

    QStringList bodLines;
    bodLines << QStringLiteral("後手の持駒：%1").arg(handToString(goteHand));
    bodLines << QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １");
    bodLines << QStringLiteral("+---------------------------+");
    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("|");
        for (int file = 0; file < 9; ++file)
            line += board[rank][file];
        line += QStringLiteral("|") + rankNames[rank];
        bodLines << line;
    }
    bodLines << QStringLiteral("+---------------------------+");
    bodLines << QStringLiteral("先手の持駒：%1").arg(handToString(senteHand));
    if (turnSfen == QStringLiteral("w"))
        bodLines << QStringLiteral("後手番");
    else
        bodLines << QStringLiteral("先手番");

    return bodLines;
}
