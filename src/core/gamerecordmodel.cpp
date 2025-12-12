#include "gamerecordmodel.h"
#include "kiftosfenconverter.h"  // KifGameInfoItem
#include "kifurecordlistmodel.h"

#include <QTableWidget>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>

// ========================================
// ヘルパ関数
// ========================================

static inline QString fwColonLine(const QString& key, const QString& val)
{
    return QStringLiteral("%1：%2").arg(key, val);
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

void GameRecordModel::bind(QVector<ResolvedRow>* resolvedRows,
                           int* activeResolvedRow,
                           QList<KifDisplayItem>* liveDisp)
{
    m_resolvedRows = resolvedRows;
    m_activeResolvedRow = activeResolvedRow;
    m_liveDisp = liveDisp;

    qDebug().noquote() << "[GameRecordModel] bind:"
                       << " resolvedRows=" << (resolvedRows ? "valid" : "null")
                       << " activeResolvedRow=" << (activeResolvedRow ? *activeResolvedRow : -1)
                       << " liveDisp=" << (liveDisp ? "valid" : "null");
}

void GameRecordModel::initializeFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount)
{
    m_comments.clear();
    m_comments.resize(qMax(0, rowCount));

    // disp からコメントを抽出
    for (int i = 0; i < disp.size() && i < rowCount; ++i) {
        m_comments[i] = disp[i].comment;
    }

    m_isDirty = false;

    qDebug().noquote() << "[GameRecordModel] initializeFromDisplayItems:"
                       << " disp.size=" << disp.size()
                       << " rowCount=" << rowCount
                       << " m_comments.size=" << m_comments.size();
}

void GameRecordModel::clear()
{
    m_comments.clear();
    m_isDirty = false;

    qDebug().noquote() << "[GameRecordModel] clear";
}

// ========================================
// コメント操作
// ========================================

void GameRecordModel::setComment(int ply, const QString& comment)
{
    if (ply < 0) {
        qWarning().noquote() << "[GameRecordModel] setComment: invalid ply=" << ply;
        return;
    }

    // 1) 内部配列を拡張・更新
    ensureCommentCapacity(ply);
    const QString oldComment = m_comments[ply];
    m_comments[ply] = comment;

    // 2) 外部データストアへ同期
    syncToExternalStores_(ply, comment);

    // 3) 変更フラグを設定
    if (oldComment != comment) {
        m_isDirty = true;
        emit commentChanged(ply, comment);
        emit dataChanged();
    }

    qDebug().noquote() << "[GameRecordModel] setComment:"
                       << " ply=" << ply
                       << " comment.len=" << comment.size()
                       << " isDirty=" << m_isDirty;
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

int GameRecordModel::activeRow() const
{
    return m_activeResolvedRow ? *m_activeResolvedRow : 0;
}

// ========================================
// 内部ヘルパ：外部データストアへの同期
// ========================================

void GameRecordModel::syncToExternalStores_(int ply, const QString& comment)
{
    // 1) ResolvedRow への同期
    if (m_resolvedRows && m_activeResolvedRow) {
        const int row = *m_activeResolvedRow;
        if (row >= 0 && row < m_resolvedRows->size()) {
            ResolvedRow& rr = (*m_resolvedRows)[row];

            // ResolvedRow::comments を拡張・更新
            while (rr.comments.size() <= ply) {
                rr.comments.append(QString());
            }
            rr.comments[ply] = comment;

            // ResolvedRow::disp[ply].comment も更新
            if (ply >= 0 && ply < rr.disp.size()) {
                rr.disp[ply].comment = comment;
            }

            qDebug().noquote() << "[GameRecordModel] syncToExternalStores_:"
                               << " updated ResolvedRow[" << row << "]"
                               << " comments[" << ply << "]";
        }
    }

    // 2) liveDisp への同期
    if (m_liveDisp) {
        if (ply >= 0 && ply < m_liveDisp->size()) {
            (*m_liveDisp)[ply].comment = comment;
            qDebug().noquote() << "[GameRecordModel] syncToExternalStores_:"
                               << " updated liveDisp[" << ply << "]";
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
    const QList<KifGameInfoItem> header = collectGameInfo_(ctx);
    for (const auto& it : header) {
        if (!it.key.trimmed().isEmpty()) {
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
        }
    }
    if (!out.isEmpty()) {
        out << QString();
    }

    // 2) セパレータ
    out << QStringLiteral("手数----指手---------消費時間--");

    // 3) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport_();

    // 4) 各指し手を出力
    int moveNo = 1;
    for (const auto& it : disp) {
        // コメント行（*で始まる）
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) {
                    out << t;
                } else {
                    out << (QStringLiteral("*") + t);
                }
            }
        }

        // 指し手行
        const QString time = it.timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : it.timeText;
        out << QStringLiteral("%1 %2 %3").arg(QString::number(moveNo), it.prettyMove, time);
        ++moveNo;
    }

    // 5) 終了行
    out << QString();
    out << QStringLiteral("まで%1手").arg(QString::number(qMax(0, disp.size())));

    qDebug().noquote() << "[GameRecordModel] toKifLines: generated"
                       << out.size() << "lines,"
                       << disp.size() << "moves";

    return out;
}

QList<KifDisplayItem> GameRecordModel::collectMainlineForExport_() const
{
    QList<KifDisplayItem> result;

    // 優先1: ResolvedRows からアクティブ行を取得
    if (m_resolvedRows && !m_resolvedRows->isEmpty() && m_activeResolvedRow) {
        int targetRow = *m_activeResolvedRow;

        // 有効範囲外なら本譜行（parent < 0）を探す
        if (targetRow < 0 || targetRow >= m_resolvedRows->size()) {
            targetRow = -1;
            for (int i = 0; i < m_resolvedRows->size(); ++i) {
                if (m_resolvedRows->at(i).parent < 0) {
                    targetRow = i;
                    break;
                }
            }
            if (targetRow < 0) targetRow = 0;
        }

        const ResolvedRow& rr = m_resolvedRows->at(targetRow);
        result = rr.disp;

        // ResolvedRow::comments からコメントをマージ
        for (int i = 0; i < result.size() && i < rr.comments.size(); ++i) {
            if (!rr.comments[i].isEmpty()) {
                result[i].comment = rr.comments[i];
            }
        }
    }
    // 優先2: liveDisp から取得
    else if (m_liveDisp && !m_liveDisp->isEmpty()) {
        result = *m_liveDisp;
    }

    // 最終: 内部コメント配列 (m_comments) から最新コメントをマージ
    // （これが Single Source of Truth）
    for (int i = 0; i < result.size() && i < m_comments.size(); ++i) {
        if (!m_comments[i].isEmpty()) {
            result[i].comment = m_comments[i];
        }
    }

    return result;
}

QList<KifGameInfoItem> GameRecordModel::collectGameInfo_(const ExportContext& ctx)
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
    resolvePlayerNames_(ctx, black, white);

    items.push_back({
        QStringLiteral("開始日時"),
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm"))
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

    return items;
}

void GameRecordModel::resolvePlayerNames_(const ExportContext& ctx, QString& outBlack, QString& outWhite)
{
    switch (ctx.playMode) {
    case HumanVsHuman:
        outBlack = ctx.human1.isEmpty() ? QObject::tr("先手") : ctx.human1;
        outWhite = ctx.human2.isEmpty() ? QObject::tr("後手") : ctx.human2;
        break;
    case EvenHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    case EvenEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case EvenEngineVsEngine:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine1") : ctx.engine1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine2") : ctx.engine2;
        break;
    case HandicapEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case HandicapHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    default:
        outBlack = QObject::tr("先手");
        outWhite = QObject::tr("後手");
        break;
    }
}
