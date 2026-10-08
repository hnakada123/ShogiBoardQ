/// @file kifuexportmetadata.cpp
/// @brief 棋譜形式間で共有する対局情報と対局者名の生成
#include "kifuexportmetadata.h"
#include "notationutils.h"
#include <QObject>

QList<KifGameInfoItem> KifuExportMetadataBuilder::collect(const KifuExportMetadata& ctx, HeaderStyle style)
{
    QList<KifGameInfoItem> items;

    // 0) 呼び出し側が対局情報を直接渡した場合（CLI などテーブルを持たない環境）
    if (ctx.gameInfoProvided || !ctx.gameInfoItems.isEmpty()) {
        return ctx.gameInfoItems;
    }

    // b) 自動生成
    QString black, white;
    resolvePlayerNames(ctx, black, white);

    // 開始日時の決定（ctx.gameStartDateTimeが有効ならそれを使用、なければ現在時刻）
    const QDateTime startDateTime = ctx.gameStartDateTime.isValid()
        ? ctx.gameStartDateTime
        : QDateTime::currentDateTime();

    // 対局日
    if (style == HeaderStyle::Full) items.push_back({
        QStringLiteral("対局日"),
        startDateTime.toString(QStringLiteral("yyyy/MM/dd"))
    });

    // 開始日時（秒まで表示）
    items.push_back({
        QStringLiteral("開始日時"),
        startDateTime.toString(style == HeaderStyle::Full
            ? QStringLiteral("yyyy/MM/dd HH:mm:ss") : QStringLiteral("yyyy/MM/dd HH:mm"))
    });

    items.push_back({ QStringLiteral("先手"), black });
    items.push_back({ QStringLiteral("後手"), white });

    items.push_back({ QStringLiteral("手合割"), handicapLabel(ctx.startSfen) });

    // 持ち時間（時間制御が有効な場合のみ）
    if (ctx.hasTimeControl) {
        items.push_back({ QStringLiteral("持ち時間"),
                          timeControlText(ctx.initialTimeMs, ctx.byoyomiMs, ctx.fischerIncrementMs) });
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

void KifuExportMetadataBuilder::resolvePlayerNames(const KifuExportMetadata& ctx, QString& outBlack, QString& outWhite)
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

QString KifuExportMetadataBuilder::handicapLabel(const QString& startSfen)
{
    const QString trimmed = startSfen.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("startpos")) return QStringLiteral("平手");

    // 駒落ちの初期配置（持駒なし・正しい手番）なら手合割名を使う。
    // それ以外は局面図で表す「その他」にする。
    const QString label = NotationUtils::handicapLabelForSfen(trimmed);
    if (!label.isEmpty()) {
        const QString preset = NotationUtils::mapHandicapToSfen(label);
        const QString hands = trimmed.section(QLatin1Char(' '), 2, 2);
        const QString turn = trimmed.section(QLatin1Char(' '), 1, 1);
        if ((hands.isEmpty() || hands == QLatin1String("-"))
            && (turn.isEmpty() || turn == preset.section(QLatin1Char(' '), 1, 1))) {
            return label;
        }
    }
    return QStringLiteral("その他");
}

QString KifuExportMetadataBuilder::timeControlText(qint64 baseMs, qint64 byoyomiMs, qint64 incrementMs)
{
    const qint64 baseSec = baseMs / 1000;
    QString text = QStringLiteral("%1:%2")
        .arg(baseSec / 60, 2, 10, QLatin1Char('0'))
        .arg(baseSec % 60, 2, 10, QLatin1Char('0'));
    if (byoyomiMs >= 1000) {
        text += QStringLiteral("+%1").arg(byoyomiMs / 1000);
    } else if (incrementMs >= 1000) {
        text += QStringLiteral("+%1秒加算").arg(incrementMs / 1000);
    }
    return text;
}
