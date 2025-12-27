#include "kifucontentbuilder.h"
#include "kiftosfenconverter.h" // KifGameInfoItem
#include "kifurecordlistmodel.h"
#include <QTableWidget>
#include <QDateTime>
#include <QRegularExpression>

static inline QString fwColonLine(const QString& key, const QString& val)
{
    return QStringLiteral("%1：%2").arg(key, val);
}

QStringList KifuContentBuilder::buildKifuDataList(const KifuExportContext& ctx)
{
    QStringList out;

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = collectGameInfo(ctx);
    for (const auto& it : header) {
        if (!it.key.trimmed().isEmpty())
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
    }
    if (!out.isEmpty()) out << QString();

    // 2) セパレータ
    out << QStringLiteral("手数----指手---------消費時間--");

    // 3) 本譜
    const QList<KifDisplayItem> disp = collectMainline(ctx);

    int moveNo = 1;
    for (const auto& it : disp) {
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) out << t;
                else                                out << (QStringLiteral("*") + t);
            }
        }
        const QString time = it.timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : it.timeText;
        out << QStringLiteral("%1 %2 %3").arg(QString::number(moveNo), it.prettyMove, time);
        ++moveNo;
    }

    // 4) 終了
    out << QString();
    out << QStringLiteral("まで%1手").arg(QString::number(qMax(0, disp.size())));

    return out;
}

QList<KifGameInfoItem> KifuContentBuilder::collectGameInfo(const KifuExportContext& ctx)
{
    QList<KifGameInfoItem> items;

    // a) 既存の「対局情報」テーブルがあれば採用
    if (ctx.gameInfoTable && ctx.gameInfoTable->rowCount() > 0) {
        const int rows = ctx.gameInfoTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            const QTableWidgetItem* keyItem   = ctx.gameInfoTable->item(r, 0);
            const QTableWidgetItem* valueItem = ctx.gameInfoTable->item(r, 1);
            if (!keyItem)   continue;
            const QString key = keyItem->text().trimmed();
            const QString val = valueItem ? valueItem->text().trimmed() : QString();
            if (!key.isEmpty()) items.push_back({key, val});
        }
        return items;
    }

    // b) 自動生成
    QString black, white;
    resolvePlayerNames(ctx, black, white);

    items.push_back({ QStringLiteral("開始日時"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm")) });
    items.push_back({ QStringLiteral("先手"), black });
    items.push_back({ QStringLiteral("後手"), white });

    const QString sfen = ctx.startSfen.trimmed();
    const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    QString teai = QStringLiteral("平手");
    if (!sfen.isEmpty()) {
        const QString pp = sfen.section(QLatin1Char(' '), 0, 0);
        if (!pp.isEmpty() && pp != initPP) teai = QStringLiteral("その他");
    }
    items.push_back({ QStringLiteral("手合割"), teai });

    return items;
}

QList<KifDisplayItem> KifuContentBuilder::collectMainline(const KifuExportContext& ctx)
{
    QList<KifDisplayItem> result;

    // 優先: 解析済みデータ（ResolvedRows）
    if (ctx.resolvedRows && !ctx.resolvedRows->isEmpty()) {
        // アクティブ行を優先、なければparent<0の本譜行を探す
        int targetRow = ctx.activeResolvedRow;
        if (targetRow < 0 || targetRow >= ctx.resolvedRows->size()) {
            targetRow = -1;
            for (qsizetype i = 0; i < ctx.resolvedRows->size(); ++i) {
                if (ctx.resolvedRows->at(i).parent < 0) {
                    targetRow = static_cast<int>(i);
                    break;
                }
            }
            if (targetRow < 0) targetRow = 0;
        }

        const ResolvedRow& rr = ctx.resolvedRows->at(targetRow);
        result = rr.disp;

        // ★ ResolvedRow::comments から編集済みコメントをマージ
        for (qsizetype i = 0; i < result.size() && i < rr.comments.size(); ++i) {
            if (!rr.comments[i].isEmpty()) {
                result[i].comment = rr.comments[i];
            }
        }

        // ★ さらに ctx.commentsByRow から最新のコメントをマージ（優先度最高）
        if (ctx.commentsByRow) {
            for (qsizetype i = 0; i < result.size() && i < ctx.commentsByRow->size(); ++i) {
                if (!ctx.commentsByRow->at(i).isEmpty()) {
                    result[i].comment = ctx.commentsByRow->at(i);
                }
            }
        }

        return result;
    }

    // 次点: ライブ記録
    if (ctx.liveDisp && !ctx.liveDisp->isEmpty()) {
        result = *ctx.liveDisp;

        // ★ ctx.commentsByRow からコメントをマージ
        if (ctx.commentsByRow) {
            for (qsizetype i = 0; i < result.size() && i < ctx.commentsByRow->size(); ++i) {
                if (!ctx.commentsByRow->at(i).isEmpty()) {
                    result[i].comment = ctx.commentsByRow->at(i);
                }
            }
        }

        return result;
    }

    // 最終手段: モデルから抽出
    if (ctx.recordModel) {
        const int rows = ctx.recordModel->rowCount();
        for (int r = 0; r < rows; ++r) {
            const QString move = ctx.recordModel->data(ctx.recordModel->index(r, 0), Qt::DisplayRole).toString();
            const QString time = ctx.recordModel->data(ctx.recordModel->index(r, 1), Qt::DisplayRole).toString();
            const QString t = move.trimmed();
            if (t.isEmpty()) continue;
            if (t.startsWith(QLatin1Char('='))) continue; // ヘッダ除外
            KifDisplayItem it;
            it.prettyMove = t;
            it.timeText   = time.isEmpty() ? QStringLiteral("00:00/00:00:00") : time;

            // ★ ctx.commentsByRow からコメントを取得
            const qsizetype idx = result.size();
            if (ctx.commentsByRow && idx < ctx.commentsByRow->size()) {
                it.comment = ctx.commentsByRow->at(idx);
            }

            result.push_back(it);
        }
    }

    return result;
}

void KifuContentBuilder::resolvePlayerNames(const KifuExportContext& ctx, QString& outBlack, QString& outWhite)
{
    // MainWindow::resolvePlayerNamesForHeader_ のロジックを移植
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
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine1"): ctx.engine1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine2"): ctx.engine2;
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
