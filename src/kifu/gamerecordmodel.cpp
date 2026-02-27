/// @file gamerecordmodel.cpp
/// @brief 棋譜データ中央管理クラスの実装

#include "gamerecordmodel.h"
#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2exporter.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifexporter.h"
#include "usenexporter.h"
#include "usiexporter.h"

#include <QTableWidget>
#include <QDateTime>
#include "logcategories.h"

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
// KIF形式出力（KifExporter へ委譲）
// ========================================

QStringList GameRecordModel::toKifLines(const ExportContext& ctx) const
{
    return KifExporter::exportLines(*this, ctx);
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
// KI2形式出力（Ki2Exporter へ委譲）
// ========================================

QStringList GameRecordModel::toKi2Lines(const ExportContext& ctx) const
{
    return Ki2Exporter::exportLines(*this, ctx);
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
// USEN形式出力（UsenExporter へ委譲）
// ========================================

QStringList GameRecordModel::toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return UsenExporter::exportLines(*this, ctx, usiMoves);
}

// ========================================
// USI形式出力（UsiExporter へ委譲）
// ========================================

QStringList GameRecordModel::toUsiLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return UsiExporter::exportLines(*this, ctx, usiMoves);
}
