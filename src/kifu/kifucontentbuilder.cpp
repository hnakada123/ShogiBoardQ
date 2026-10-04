/// @file kifucontentbuilder.cpp
/// @brief 棋譜コンテンツビルダークラスの実装

#include "kifucontentbuilder.h"
#include "kifurecordlistmodel.h"
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
    for (const auto& it : std::as_const(header)) {
        if (!it.key.trimmed().isEmpty())
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
    }
    if (!out.isEmpty()) out << QString();

    // 2) セパレータ
    out << QStringLiteral("手数----指手---------消費時間--");

    // 3) 本譜
    const QList<KifDisplayItem> disp = collectMainline(ctx);

    int moveNo = 1;
    for (const auto& it : std::as_const(disp)) {
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            static const QRegularExpression newlineRe(QStringLiteral("\r?\n"));
            const QStringList lines = cmt.split(newlineRe, Qt::KeepEmptyParts);
            for (const QString& raw : std::as_const(lines)) {
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
    return KifuExportMetadataBuilder::collect(ctx, KifuExportMetadataBuilder::HeaderStyle::Compact);
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

        // ResolvedRow::comments から編集済みコメントをマージ
        for (qsizetype i = 0; i < result.size() && i < rr.comments.size(); ++i) {
            if (!rr.comments[i].isEmpty()) {
                result[i].comment = rr.comments[i];
            }
        }

        return result;
    }

    // 次点: ライブ記録
    if (ctx.liveDisp && !ctx.liveDisp->isEmpty()) {
        result = *ctx.liveDisp;

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

            if (const auto* item = ctx.recordModel->item(r)) {
                it.comment = item->comment();
                it.bookmark = item->bookmark();
            }

            result.push_back(it);
        }
    }

    return result;
}

QString KifuContentBuilder::toRichHtmlWithStarBreaksAndLinks(const QString& raw)
{
    // 改行正規化
    QString s = raw;
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QChar('\r'), QChar('\n'));

    // '*' が行頭でない場合、その直前に改行を入れる（直前の余分な空白は削る）
    QString withBreaks;
    withBreaks.reserve(s.size() + 16);
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('*') && i > 0 && s.at(i - 1) != QLatin1Char('\n')) {
            while (!withBreaks.isEmpty()) {
                const QChar tail = withBreaks.at(withBreaks.size() - 1);
                if (tail == QLatin1Char('\n')) break;
                if (!tail.isSpace()) break;
                withBreaks.chop(1);
            }
            withBreaks.append(QLatin1Char('\n'));
        }
        withBreaks.append(ch);
    }

    // URL をリンク化（非URL部分は都度エスケープ）
    static const QRegularExpression urlRe(
        QStringLiteral(R"((https?://[^\s<>"']+))"),
        QRegularExpression::CaseInsensitiveOption);

    QString html;
    html.reserve(withBreaks.size() + 64);

    int last = 0;
    QRegularExpressionMatchIterator it = urlRe.globalMatch(withBreaks);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const qsizetype start = m.capturedStart();
        const qsizetype end   = m.capturedEnd();

        // 非URL部分をエスケープして追加
        html += withBreaks.mid(last, start - last).toHtmlEscaped();

        // URL部分を <a href="...">...</a>
        const QString url   = m.captured(0);
        const QString href  = url.toHtmlEscaped();   // 属性用の最低限エスケープ
        const QString label = url.toHtmlEscaped();   // 表示用
        html += QStringLiteral("<a href=\"%1\">%2</a>").arg(href, label);

        last = static_cast<int>(end);
    }
    // 末尾の非URL部分
    html += withBreaks.mid(last).toHtmlEscaped();

    // 改行 → <br/>
    html.replace(QChar('\n'), QStringLiteral("<br/>"));
    return html;
}
