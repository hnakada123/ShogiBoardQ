/// @file gamerecordmodel.cpp
/// @brief 棋譜データ中央管理クラスの実装

#include "gamerecordmodel.h"
#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2tosfenconverter.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"

#include <QTableWidget>
#include <QDateTime>
#include <QRegularExpression>
#include "kifulogging.h"
#include <QPair>
#include <algorithm>

// ========================================
// KIF形式の定数
// ========================================
namespace KifFormat {
    // 指し手フォーマットの幅設定
    constexpr int kMoveNumberWidth = 4;    // 手数の幅（右寄せ）
    constexpr int kMoveTextWidth = 12;     // 指し手テキストの幅（左寄せ）

    // セパレータ行
    const QString kSeparatorLine = QStringLiteral("手数----指手---------消費時間--");

    // 既定の消費時間
    const QString kDefaultTime = QStringLiteral("( 0:00/00:00:00)");
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
    if (timeText.isEmpty()) return KifFormat::kDefaultTime;

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

// ヘルパ関数: KIF形式の指し手行を生成
static QString formatKifMoveLine(int moveNo, const QString& kifMove, const QString& time, bool hasBranch)
{
    const QString moveNoStr = QStringLiteral("%1").arg(moveNo, KifFormat::kMoveNumberWidth);
    const QString branchMark = hasBranch ? QStringLiteral("+") : QString();
    return QStringLiteral("%1 %2   %3%4")
        .arg(moveNoStr, kifMove.leftJustified(KifFormat::kMoveTextWidth), time, branchMark);
}

// ヘルパ関数: コメント行を出力リストに追加
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

static void appendKifComments(const QString& comment, QStringList& out)
{
    if (comment.trimmed().isEmpty()) return;

    const QStringList lines = comment.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
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
// コンストラクタ・デストラクタ
// ========================================

GameRecordModel::GameRecordModel(QObject* parent)
    : QObject(parent)
{
}

// ========================================
// 初期化・バインド
// ========================================

void GameRecordModel::bind(QList<KifDisplayItem>* liveDisp)
{
    m_liveDisp = liveDisp;

    qCDebug(lcKifu).noquote() << "bind:"
                              << " liveDisp=" << (liveDisp ? "valid" : "null");
}

void GameRecordModel::setBranchTree(KifuBranchTree* tree)
{
    m_branchTree = tree;
    qCDebug(lcKifu).noquote() << "setBranchTree:" << (tree ? "valid" : "null");
}

void GameRecordModel::setNavigationState(KifuNavigationState* state)
{
    m_navState = state;
    qCDebug(lcKifu).noquote() << "setNavigationState:" << (state ? "valid" : "null");
}

void GameRecordModel::initializeFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount)
{
    m_comments.clear();
    m_comments.resize(qMax(0, rowCount));
    m_bookmarks.clear();
    m_bookmarks.resize(qMax(0, rowCount));

    // disp からコメント・しおりを抽出
    for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
        m_comments[i] = disp[i].comment;
        m_bookmarks[i] = disp[i].bookmark;
    }

    m_isDirty = false;

    qCDebug(lcKifu).noquote() << "initializeFromDisplayItems:"
                              << " disp.size=" << disp.size()
                              << " rowCount=" << rowCount
                              << " m_comments.size=" << m_comments.size();
}

void GameRecordModel::clear()
{
    m_comments.clear();
    m_bookmarks.clear();
    m_isDirty = false;

    qCDebug(lcKifu).noquote() << "clear";
}

// ========================================
// コメント操作
// ========================================

void GameRecordModel::setComment(int ply, const QString& comment)
{
    if (ply < 0) {
        qCWarning(lcKifu).noquote() << "setComment: invalid ply=" << ply;
        return;
    }

    // 1) 内部配列を拡張・更新
    ensureCommentCapacity(ply);
    const QString oldComment = m_comments[ply];
    m_comments[ply] = comment;

    // 2) 外部データストアへ同期
    syncToExternalStores(ply, comment);

    // 3) 変更フラグを設定
    if (oldComment != comment) {
        m_isDirty = true;
        emit commentChanged(ply, comment);
        emit dataChanged();

        // 4) コールバックを呼び出し（RecordPresenterへの通知など）
        if (m_commentUpdateCallback) {
            m_commentUpdateCallback(ply, comment);
        }
    }

    qCDebug(lcKifu).noquote() << "setComment:"
                              << " ply=" << ply
                              << " comment.len=" << comment.size()
                              << " isDirty=" << m_isDirty;
}

void GameRecordModel::setCommentUpdateCallback(const CommentUpdateCallback& callback)
{
    m_commentUpdateCallback = callback;
}

QString GameRecordModel::comment(int ply) const
{
    if (ply >= 0 && ply < m_comments.size()) {
        return m_comments[ply];
    }
    return QString();
}

void GameRecordModel::ensureCommentCapacity(int ply)
{
    while (m_comments.size() <= ply) {
        m_comments.append(QString());
    }
}

// ========================================
// しおり操作
// ========================================

void GameRecordModel::setBookmark(int ply, const QString& bookmark)
{
    if (ply < 0) {
        qCWarning(lcKifu).noquote() << "setBookmark: invalid ply=" << ply;
        return;
    }

    ensureBookmarkCapacity(ply);
    const QString oldBookmark = m_bookmarks[ply];
    m_bookmarks[ply] = bookmark;

    // 外部データストアへ同期
    if (m_liveDisp && ply >= 0 && ply < m_liveDisp->size()) {
        (*m_liveDisp)[ply].bookmark = bookmark;
    }

    // KifuBranchTree のノードにも同期
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        int lineIndex = 0;
        if (m_navState != nullptr) {
            lineIndex = m_navState->currentLineIndex();
        }
        QVector<BranchLine> lines = m_branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            if (ply >= 0 && ply < line.nodes.size()) {
                line.nodes[ply]->setBookmark(bookmark);
            }
        }
    }

    if (oldBookmark != bookmark) {
        m_isDirty = true;
        emit dataChanged();

        if (m_bookmarkUpdateCallback) {
            m_bookmarkUpdateCallback(ply, bookmark);
        }
    }
}

QString GameRecordModel::bookmark(int ply) const
{
    if (ply >= 0 && ply < m_bookmarks.size()) {
        return m_bookmarks[ply];
    }
    return QString();
}

void GameRecordModel::setBookmarkUpdateCallback(const BookmarkUpdateCallback& callback)
{
    m_bookmarkUpdateCallback = callback;
}

void GameRecordModel::ensureBookmarkCapacity(int ply)
{
    while (m_bookmarks.size() <= ply) {
        m_bookmarks.append(QString());
    }
}

int GameRecordModel::activeRow() const
{
    // 新システム: KifuNavigationState から現在のラインインデックスを取得
    if (m_navState != nullptr) {
        return m_navState->currentLineIndex();
    }
    return 0;
}

// ========================================
// 内部ヘルパ：外部データストアへの同期
// ========================================

void GameRecordModel::syncToExternalStores(int ply, const QString& comment)
{
    // liveDisp への同期
    if (m_liveDisp) {
        if (ply >= 0 && ply < m_liveDisp->size()) {
            (*m_liveDisp)[ply].comment = comment;
            qCDebug(lcKifu).noquote() << "syncToExternalStores:"
                                      << " updated liveDisp[" << ply << "]";
        }
    }

    // KifuBranchTree のノードにも同期
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        int lineIndex = 0;
        if (m_navState != nullptr) {
            lineIndex = m_navState->currentLineIndex();
        }
        QVector<BranchLine> lines = m_branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            if (ply >= 0 && ply < line.nodes.size()) {
                line.nodes[ply]->setComment(comment);
            }
        }
    }
}

// ========================================
// 棋譜出力
// ========================================

QStringList GameRecordModel::toKifLines(const ExportContext& ctx) const
{
    QStringList out;

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = collectGameInfo(ctx);
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
    out << KifFormat::kSeparatorLine;

    // 3) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport();

    // 4) 分岐ポイントを収集（KifuBranchTree から）
    QSet<int> branchPoints;
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            branchPoints.insert(line.branchPly);
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

        // 分岐マークの判定（この手に分岐があるか）
        const bool hasBranch = branchPoints.contains(moveNo);

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
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            outputVariationFromBranchLine(line, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKifLines (WITH VARIATIONS): generated"
                              << out.size() << "lines,"
                              << (moveNo - 1) << "moves, lastActualMoveNo=" << lastActualMoveNo
                              << "branchPoints=" << branchPoints.size();

    return out;
}

QList<KifDisplayItem> GameRecordModel::collectMainlineForExport() const
{
    QList<KifDisplayItem> result;

    // 優先0: KifuBranchTree から取得（新システム）
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        // 本譜は常にlineIndex=0（ナビゲーション状態に依存しない）
        constexpr int lineIndex = 0;

        result = m_branchTree->getDisplayItemsForLine(lineIndex);
        qCDebug(lcKifu).noquote() << "collectMainlineForExport: from BranchTree"
                                  << "lineIndex=" << lineIndex << "items=" << result.size();

        // m_comments / m_bookmarks をマージ
        // getDisplayItemsForLine() がツリーのコメント・しおりを設定済みなので、
        // ここでは m_comments / m_bookmarks による上書き（Single Source of Truth）のみ行う
        for (qsizetype i = 0; i < result.size(); ++i) {
            if (i < m_comments.size() && !m_comments[i].isEmpty()) {
                result[i].comment = m_comments[i];
            }
            if (i < m_bookmarks.size() && !m_bookmarks[i].isEmpty()) {
                result[i].bookmark = m_bookmarks[i];
            }
        }

        // 空でない結果が得られた場合は返す
        if (!result.isEmpty()) {
            return result;
        }
    }

    // フォールバック: liveDisp から取得
    if (m_liveDisp && !m_liveDisp->isEmpty()) {
        result = *m_liveDisp;
    }

    // 最終: 内部配列から最新データをマージ（Single Source of Truth）
    for (qsizetype i = 0; i < result.size(); ++i) {
        if (i < m_comments.size() && !m_comments[i].isEmpty()) {
            result[i].comment = m_comments[i];
        }
        if (i < m_bookmarks.size() && !m_bookmarks[i].isEmpty()) {
            result[i].bookmark = m_bookmarks[i];
        }
    }

    return result;
}

QList<KifGameInfoItem> GameRecordModel::collectGameInfo(const ExportContext& ctx)
{
    QList<KifGameInfoItem> items;

    // a) 既存の「対局情報」テーブルがあれば採用
    if (ctx.gameInfoTable && ctx.gameInfoTable->rowCount() > 0) {
        const int rows = ctx.gameInfoTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            const QTableWidgetItem* keyItem   = ctx.gameInfoTable->item(r, 0);
            const QTableWidgetItem* valueItem = ctx.gameInfoTable->item(r, 1);
            if (!keyItem) continue;
            const QString key = keyItem->text().trimmed();
            const QString val = valueItem ? valueItem->text().trimmed() : QString();
            if (!key.isEmpty()) {
                items.push_back({key, val});
            }
        }
        return items;
    }

    // b) 自動生成
    QString black, white;
    resolvePlayerNames(ctx, black, white);

    // 開始日時の決定（ctx.gameStartDateTimeが有効ならそれを使用、なければ現在時刻）
    const QDateTime startDateTime = ctx.gameStartDateTime.isValid()
        ? ctx.gameStartDateTime
        : QDateTime::currentDateTime();

    // 対局日
    items.push_back({
        QStringLiteral("対局日"),
        startDateTime.toString(QStringLiteral("yyyy/MM/dd"))
    });

    // 開始日時（秒まで表示）
    items.push_back({
        QStringLiteral("開始日時"),
        startDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))
    });

    items.push_back({ QStringLiteral("先手"), black });
    items.push_back({ QStringLiteral("後手"), white });

    const QString sfen = ctx.startSfen.trimmed();
    const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    QString teai = QStringLiteral("平手");
    if (!sfen.isEmpty()) {
        const QString pp = sfen.section(QLatin1Char(' '), 0, 0);
        if (!pp.isEmpty() && pp != initPP) {
            teai = QStringLiteral("その他");
        }
    }
    items.push_back({ QStringLiteral("手合割"), teai });

    // 持ち時間（時間制御が有効な場合のみ）
    if (ctx.hasTimeControl) {
        // mm:ss+ss 形式（初期持ち時間:秒読み+加算秒）
        const int baseMin = ctx.initialTimeMs / 60000;
        const int baseSec = (ctx.initialTimeMs % 60000) / 1000;
        const int byoyomiSec = ctx.byoyomiMs / 1000;
        const int incrementSec = ctx.fischerIncrementMs / 1000;

        QString timeStr;
        if (baseMin > 0 || baseSec > 0) {
            timeStr = QStringLiteral("%1:%2")
                .arg(baseMin, 2, 10, QLatin1Char('0'))
                .arg(baseSec, 2, 10, QLatin1Char('0'));
        } else {
            timeStr = QStringLiteral("00:00");
        }
        if (byoyomiSec > 0) {
            timeStr += QStringLiteral("+%1").arg(byoyomiSec);
        } else if (incrementSec > 0) {
            timeStr += QStringLiteral("+%1").arg(incrementSec);
        }
        items.push_back({ QStringLiteral("持ち時間"), timeStr });
    }

    // 終了日時（ctx.gameEndDateTimeが有効な場合のみ）
    if (ctx.gameEndDateTime.isValid()) {
        items.push_back({
            QStringLiteral("終了日時"),
            ctx.gameEndDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))
        });
    }

    return items;
}

void GameRecordModel::resolvePlayerNames(const ExportContext& ctx, QString& outBlack, QString& outWhite)
{
    switch (ctx.playMode) {
    case PlayMode::HumanVsHuman:
        outBlack = ctx.human1.isEmpty() ? QObject::tr("先手") : ctx.human1;
        outWhite = ctx.human2.isEmpty() ? QObject::tr("後手") : ctx.human2;
        break;
    case PlayMode::EvenHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    case PlayMode::EvenEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case PlayMode::EvenEngineVsEngine:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine1") : ctx.engine1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine2") : ctx.engine2;
        break;
    case PlayMode::HandicapEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case PlayMode::HandicapHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    default:
        outBlack = QObject::tr("先手");
        outWhite = QObject::tr("後手");
        break;
    }
}

// ========================================
// 分岐出力用ヘルパ
// ========================================

void GameRecordModel::outputVariationFromBranchLine(const BranchLine& line, QStringList& out) const
{
    // 本譜（lineIndex == 0）の場合は何もしない
    if (line.lineIndex == 0) {
        return;
    }

    // 変化ヘッダを出力
    out << QStringLiteral("変化：%1手").arg(line.branchPly);

    // この変化上の分岐点を収集（子分岐があるノードの手数）
    QSet<int> childBranchPlys;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->hasBranch()) {
            childBranchPlys.insert(node->ply() + 1);
        }
    }

    // 同じ分岐点で、この変化より後に出力される兄弟変化があるかチェック
    bool hasSiblingAfterThis = false;
    if (line.branchPoint != nullptr) {
        const QVector<KifuBranchNode*>& siblings = line.branchPoint->children();
        // 自分の位置を見つける
        int myIndex = -1;
        for (int i = 0; i < siblings.size(); ++i) {
            if (!line.nodes.isEmpty() && siblings.at(i) == line.nodes.at(line.branchPly)) {
                myIndex = i;
                break;
            }
        }
        // 自分より後ろに兄弟がいるか
        if (myIndex >= 0 && myIndex < siblings.size() - 1) {
            hasSiblingAfterThis = true;
        }
    }

    // 変化の指し手を出力（branchPly以降のノード）
    bool isFirstMove = true;
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

        // 分岐マークの判定
        bool hasBranch = childBranchPlys.contains(node->ply());
        if (isFirstMove && hasSiblingAfterThis) {
            hasBranch = true;
        }

        // KIF形式で出力
        out << formatKifMoveLine(node->ply(), kifMove, time, hasBranch);

        // しおり・コメント出力
        appendKifBookmarks(node->bookmark(), out);
        appendKifComments(node->comment(), out);

        isFirstMove = false;
    }
}

// ========================================
// KI2形式出力
// ========================================

QStringList GameRecordModel::toKi2Lines(const ExportContext& ctx) const
{
    QStringList out;

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = collectGameInfo(ctx);
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
        const QStringList bodLines = sfenToBodLines(ctx.startSfen);
        if (!bodLines.isEmpty())
            out << bodLines;
    }

    // 2) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport();

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
    QMap<QChar, int> blackHands, whiteHands;
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
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
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

void GameRecordModel::outputKi2VariationFromBranchLine(const BranchLine& line, const QString& startSfen, QStringList& out) const
{
    // 本譜（lineIndex == 0）の場合は何もしない
    if (line.lineIndex == 0) {
        return;
    }

    // 変化ヘッダを出力
    out << QStringLiteral("変化：%1手").arg(line.branchPly);

    // 盤面状態を初期化し、分岐点まで進める
    QString boardState[9][9];
    QMap<QChar, int> blackHands, whiteHands;
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
// CSA形式出力（CsaExporter へ委譲）
// ========================================

QStringList GameRecordModel::toCsaLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return CsaExporter::exportLines(*this, ctx, usiMoves);
}

// ========================================
// JKF形式出力（JkfExporter へ委譲）
// ========================================

QStringList GameRecordModel::toJkfLines(const ExportContext& ctx) const
{
    return JkfExporter::exportLines(*this, ctx);
}


// ========================================
// USEN形式出力
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
static const QString kUsenBase36Chars = QStringLiteral("0123456789abcdefghijklmnopqrstuvwxyz");

QString GameRecordModel::intToBase36(int moveCode)
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

QString GameRecordModel::encodeUsiMoveToUsen(const QString& usiMove)
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

QString GameRecordModel::sfenToUsenPosition(const QString& sfen)
{
    // 平手初期局面のチェック
    static const QString kHirateSfenPrefix = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QString();  // 平手: 空文字列（~の前に何も置かない）
    }

    // 平手初期局面の場合は空文字列を返す
    if (trimmed.startsWith(kHirateSfenPrefix)) {
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

QString GameRecordModel::getUsenTerminalCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);
    
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("i");
    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("r");
    if (move.contains(QStringLiteral("時間切れ")) || move.contains(QStringLiteral("切れ負け"))) 
        return QStringLiteral("t");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("p");
    if (move.contains(QStringLiteral("持将棋")) || move.contains(QStringLiteral("千日手"))) 
        return QStringLiteral("j");
    
    return QString();
}

QString GameRecordModel::inferUsiFromSfenDiff(const QString& sfenBefore, const QString& sfenAfter, bool isSente)
{
    // 2つのSFEN間の差分からUSI形式の指し手を推測
    // SFEN形式: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    // 盤面部分を比較して、移動した駒を特定する
    
    if (sfenBefore.isEmpty() || sfenAfter.isEmpty()) {
        return QString();
    }
    
    // SFEN盤面部分を抽出
    QString boardBefore = sfenBefore.section(QLatin1Char(' '), 0, 0);
    QString boardAfter = sfenAfter.section(QLatin1Char(' '), 0, 0);
    
    // 盤面を9x9配列に展開 (promoted pieces use 0x100 flag)
    // board[rank][file] の順序で格納 (rank: 0-8 = 一段〜九段, file: 0-8 = 1筋〜9筋)
    auto expandBoard = [](const QString& board) -> QVector<int> {
        QVector<int> result(81, 0);  // 0 = empty
        int sfenIdx = 0;  // SFEN上のインデックス（左上から右へ、上から下へ）
        bool promoted = false;
        
        for (QChar c : board) {
            if (c == QLatin1Char('/')) continue;
            if (c == QLatin1Char('+')) {
                promoted = true;
                continue;
            }
            if (c.isDigit()) {
                int n = c.toLatin1() - '0';
                sfenIdx += n;
            } else {
                if (sfenIdx < 81) {
                    int rank = sfenIdx / 9;       // 0-8 (一段目〜九段目)
                    int sfenFile = sfenIdx % 9;   // 0-8 (9筋〜1筋の順)
                    int file = 8 - sfenFile;      // 0-8 (1筋〜9筋の順に変換)
                    int boardIdx = rank * 9 + file;
                    
                    int pieceCode = c.toLatin1();
                    if (promoted) pieceCode |= 0x100;  // 成駒フラグ
                    result[boardIdx] = pieceCode;
                }
                sfenIdx++;
                promoted = false;
            }
        }
        return result;
    };
    
    QVector<int> before = expandBoard(boardBefore);
    QVector<int> after = expandBoard(boardAfter);
    
    // 差分を探す
    int fromRank = -1, fromFile = -1;
    int toRank = -1, toFile = -1;
    int movedPiece = 0;
    bool isPromotion = false;
    
    for (int rank = 0; rank < 9; ++rank) {
        for (int file = 0; file < 9; ++file) {
            int idx = rank * 9 + file;
            int beforePiece = before[idx];
            int afterPiece = after[idx];
            
            if (beforePiece != afterPiece) {
                // beforeにあってafterにない → 移動元
                if (beforePiece != 0 && afterPiece == 0) {
                    fromRank = rank;
                    fromFile = file;
                    movedPiece = beforePiece;
                }
                // beforeになくてafterにある → 移動先
                else if (beforePiece == 0 && afterPiece != 0) {
                    toRank = rank;
                    toFile = file;
                    // 成り判定: 移動元が非成りで移動先が成り
                    if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                        isPromotion = true;
                    }
                }
                // 両方に駒がある → 駒取り
                else if (beforePiece != 0 && afterPiece != 0) {
                    // 先手の駒は大文字、後手の駒は小文字
                    char beforeChar = static_cast<char>(beforePiece & 0xFF);
                    char afterChar = static_cast<char>(afterPiece & 0xFF);
                    bool beforeIsSente = (beforeChar >= 'A' && beforeChar <= 'Z');
                    bool afterIsSente = (afterChar >= 'A' && afterChar <= 'Z');
                    
                    if (beforeIsSente != afterIsSente) {
                        if ((isSente && afterIsSente) || (!isSente && !afterIsSente)) {
                            toRank = rank;
                            toFile = file;
                            // 成り判定
                            if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                                isPromotion = true;
                            }
                        } else {
                            fromRank = rank;
                            fromFile = file;
                            movedPiece = beforePiece;
                        }
                    }
                }
            }
        }
    }
    
    // 駒打ちの検出（fromRank == -1 で toRank != -1）
    if (fromRank < 0 && toRank >= 0) {
        // 持ち駒から打った
        int piece = after[toRank * 9 + toFile];
        char pieceChar = static_cast<char>(piece & 0xFF);
        QChar usiPiece = QChar(pieceChar).toUpper();
        
        // USI座標: ファイルは1-9、ランクはa-i
        int usiFile = toFile + 1;  // 1-9
        char usiRank = static_cast<char>('a' + toRank);  // a-i
        
        return QStringLiteral("%1*%2%3")
            .arg(usiPiece)
            .arg(usiFile)
            .arg(QChar(usiRank));
    }
    
    // 盤上移動
    if (fromRank >= 0 && toRank >= 0) {
        // USI座標: ファイルは1-9、ランクはa-i
        int fromUsiFile = fromFile + 1;
        char fromUsiRank = static_cast<char>('a' + fromRank);
        int toUsiFile = toFile + 1;
        char toUsiRank = static_cast<char>('a' + toRank);
        
        QString usi = QStringLiteral("%1%2%3%4")
            .arg(fromUsiFile)
            .arg(QChar(fromUsiRank))
            .arg(toUsiFile)
            .arg(QChar(toUsiRank));
        
        if (isPromotion) {
            usi += QLatin1Char('+');
        }
        
        return usi;
    }
    
    return QString();
}

QStringList GameRecordModel::toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    QStringList out;
    
    // 1) 初期局面をUSEN形式に変換
    QString position = sfenToUsenPosition(ctx.startSfen);
    
    // 2) 本譜の指し手をエンコード
    QString mainMoves;
    QString terminalCode;
    
    // 本譜の指し手リストを取得
    const QList<KifDisplayItem> disp = collectMainlineForExport();
    
    // USI moves が渡された場合はそれを使用
    const QStringList& mainlineUsi = usiMoves;

    // 各USI指し手をUSEN形式にエンコード
    for (const QString& usi : std::as_const(mainlineUsi)) {
        QString encoded = encodeUsiMoveToUsen(usi);
        if (!encoded.isEmpty()) {
            mainMoves += encoded;
        }
    }
    
    // 終局語のチェック
    if (!disp.isEmpty()) {
        const QString& lastMove = disp.last().prettyMove;
        if (isTerminalMove(lastMove)) {
            terminalCode = getUsenTerminalCode(lastMove);
        }
    }
    
    // 本譜のUSEN文字列を構築
    // USEN構造: [初期局面]~[オフセット].[指し手][.終局コード]
    // 初期局面は~の前に置く（平手の場合は空）、本譜のオフセットは0
    QString usen = position + QStringLiteral("~0.%1").arg(mainMoves);
    
    // 終局コードを追加
    if (!terminalCode.isEmpty()) {
        usen += QStringLiteral(".") + terminalCode;
    }
    
    // 3) 分岐を追加
    // 新システム: KifuBranchTree から出力
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);

            // オフセット（分岐開始位置 - 1）
            int offset = line.branchPly - 1;
            if (offset < 0) offset = 0;

            QString branchMoves;
            QString branchTerminal;

            // 分岐のSFENリストとディスプレイアイテムを取得
            QStringList sfenList = m_branchTree->getSfenListForLine(lineIdx);
            QList<KifDisplayItem> branchDisp = m_branchTree->getDisplayItemsForLine(lineIdx);

            // SFENの差分からUSI指し手を推測
            for (qsizetype i = line.branchPly; i < branchDisp.size(); ++i) {
                const KifDisplayItem& item = branchDisp[i];
                if (item.prettyMove.isEmpty()) continue;

                // 終局語チェック
                if (isTerminalMove(item.prettyMove)) {
                    branchTerminal = getUsenTerminalCode(item.prettyMove);
                    break;
                }

                // SFENリストを使ってUSI指し手を推測
                // i-1番目のSFENからi番目のSFENへの差分を計算
                if (i < sfenList.size() && (i - 1) >= 0 && (i - 1) < sfenList.size()) {
                    QString usi = inferUsiFromSfenDiff(sfenList[i - 1], sfenList[i], (i % 2 != 0));
                    if (!usi.isEmpty()) {
                        QString encoded = encodeUsiMoveToUsen(usi);
                        if (!encoded.isEmpty()) {
                            branchMoves += encoded;
                        }
                    }
                }
            }

            // 分岐のUSEN文字列を構築
            QString branchUsen = QStringLiteral("~%1.%2").arg(offset).arg(branchMoves);
            if (!branchTerminal.isEmpty()) {
                branchUsen += QStringLiteral(".") + branchTerminal;
            }

            usen += branchUsen;
        }
    }

    out << usen;

    const int branchCount = (m_branchTree != nullptr) ? (m_branchTree->lineCount() - 1) : 0;
    qCDebug(lcKifu).noquote() << "toUsenLines: generated USEN with"
                              << mainMoves.size() / 3 << "mainline moves,"
                              << branchCount << "variations";
    
    return out;
}

// ========================================
// USI形式（position コマンド）出力
// ========================================

QString GameRecordModel::getUsiTerminalCode(const QString& terminalMove)
{
    const QString stripped = removeTurnMarker(terminalMove);
    
    // 投了 → resign
    if (stripped.contains(QStringLiteral("投了"))) {
        return QStringLiteral("resign");
    }
    // 中断 → break (将棋所の仕様)
    if (stripped.contains(QStringLiteral("中断"))) {
        return QStringLiteral("break");
    }
    // 千日手 → rep_draw
    if (stripped.contains(QStringLiteral("千日手"))) {
        return QStringLiteral("rep_draw");
    }
    // 持将棋・引き分け → draw
    if (stripped.contains(QStringLiteral("持将棋")) || stripped.contains(QStringLiteral("引き分け"))) {
        return QStringLiteral("draw");
    }
    // 切れ負け → timeout
    if (stripped.contains(QStringLiteral("切れ負け"))) {
        return QStringLiteral("timeout");
    }
    // 入玉勝ち → win
    if (stripped.contains(QStringLiteral("入玉勝ち"))) {
        return QStringLiteral("win");
    }
    // 詰み → mate (カスタム)
    if (stripped.contains(QStringLiteral("詰み"))) {
        return QStringLiteral("mate");
    }
    // 反則勝ち/反則負け → illegal (カスタム)
    if (stripped.contains(QStringLiteral("反則"))) {
        return QStringLiteral("illegal");
    }
    
    return QString();
}

QStringList GameRecordModel::sfenToBodLines(const QString& sfen)
{
    if (sfen.trimmed().isEmpty()) return {};

    const QStringList parts = sfen.trimmed().split(QLatin1Char(' '));
    if (parts.size() < 3) return {};

    const QString boardSfen = parts[0];
    const QString turnSfen = parts[1];
    const QString handSfen = parts[2];

    // 平手かどうかチェック（平手ならBOD不要）
    static const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    if (boardSfen == initPP && handSfen == QStringLiteral("-") && turnSfen == QStringLiteral("b"))
        return {};

    // 持ち駒を解析
    QMap<QChar, int> senteHand, goteHand;
    if (handSfen != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handSfen.size(); ++i) {
            const QChar c = handSfen.at(i);
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

QStringList GameRecordModel::toUsiLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    QStringList out;
    
    // 1) 初期局面を判定
    // 平手初期局面のSFEN（手数部分は省略して比較）
    static const QString HIRATE_SFEN = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    
    QString positionStr;
    
    // 初期SFENが空または平手初期局面の場合は "startpos" を使用
    if (ctx.startSfen.isEmpty()) {
        positionStr = QStringLiteral("position startpos");
    } else {
        // SFENの手数部分（最後のスペースと数字）を除いて比較
        QString sfenWithoutMoveNum = ctx.startSfen;
        const qsizetype lastSpace = ctx.startSfen.lastIndexOf(QLatin1Char(' '));
        if (lastSpace > 0) {
            const QString lastPart = ctx.startSfen.mid(lastSpace + 1);
            bool isNumber = false;
            lastPart.toInt(&isNumber);
            if (isNumber) {
                sfenWithoutMoveNum = ctx.startSfen.left(lastSpace);
            }
        }
        
        if (sfenWithoutMoveNum == HIRATE_SFEN) {
            positionStr = QStringLiteral("position startpos");
        } else {
            // 駒落ち等の場合は sfen を使用
            positionStr = QStringLiteral("position sfen %1").arg(ctx.startSfen);
        }
    }
    
    // 2) 指し手部分を構築
    QString movesStr;
    
    // USI moves が渡された場合はそれを使用
    QStringList mainlineUsi = usiMoves;
    
    // 3) 終局コードを取得
    QString terminalCode;
    const QList<KifDisplayItem> disp = collectMainlineForExport();
    if (!disp.isEmpty()) {
        const QString& lastMove = disp.last().prettyMove;
        if (isTerminalMove(lastMove)) {
            terminalCode = getUsiTerminalCode(lastMove);
        }
    }
    
    // 4) position コマンド文字列を構築
    QString usiLine = positionStr;
    
    if (!mainlineUsi.isEmpty()) {
        usiLine += QStringLiteral(" moves");
        for (const QString& move : std::as_const(mainlineUsi)) {
            usiLine += QStringLiteral(" ") + move;
        }
    }
    
    // 5) 終局コードを追加（指し手の後にスペース区切りで）
    if (!terminalCode.isEmpty()) {
        usiLine += QStringLiteral(" ") + terminalCode;
    }
    
    out << usiLine;
    
    qCDebug(lcKifu).noquote() << "toUsiLines: generated USI position command with"
                              << mainlineUsi.size() << "moves,"
                              << "terminal:" << terminalCode;
    
    return out;
}
