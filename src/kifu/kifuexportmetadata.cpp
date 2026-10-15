/// @file kifuexportmetadata.cpp
/// @brief 棋譜形式間で共有する対局情報と対局者名の生成
#include "kifuexportmetadata.h"
#include "gameinfokeys.h"
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

    const QString handicap = handicapLabel(ctx.startSfen);
    items.push_back({ blackPlayerKey(handicap), black });
    items.push_back({ whitePlayerKey(handicap), white });

    items.push_back({ GameInfoKeys::kHandicap, handicap });

    // 持ち時間（時間制御が有効な場合のみ）
    if (!ctx.timeControl.isUnlimited()) {
        items.push_back({ GameInfoKeys::kTimeControl,
                          timeControlText(ctx.timeControl, isHandicap(handicap)) });
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
    const MatchPlayers& players = ctx.players;
    // 名前が無いときは、人間なら先手・後手、エンジンなら Engine（エンジン同士なら Engine1・Engine2）
    const auto fallback = [&players](Turn side) {
        if (!players.isEngine(side)) return side == Turn::Black ? QObject::tr("先手") : QObject::tr("後手");
        if (!players.engineVsEngine()) return QObject::tr("Engine");
        return side == Turn::Black ? QObject::tr("Engine1") : QObject::tr("Engine2");
    };
    outBlack = players.name(Turn::Black);
    outWhite = players.name(Turn::White);
    if (outBlack.isEmpty()) outBlack = fallback(Turn::Black);
    if (outWhite.isEmpty()) outWhite = fallback(Turn::White);
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

bool KifuExportMetadataBuilder::isHandicap(const QString& handicapLabel)
{
    const QString label = handicapLabel.trimmed();
    return !label.isEmpty() && label != QStringLiteral("平手") && label != QStringLiteral("その他");
}

QString KifuExportMetadataBuilder::blackPlayerKey(const QString& handicapLabel)
{
    return isHandicap(handicapLabel) ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer;
}

QString KifuExportMetadataBuilder::whitePlayerKey(const QString& handicapLabel)
{
    return isHandicap(handicapLabel) ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer;
}

bool KifuExportMetadataBuilder::usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen)
{
    // 読み込んだ棋譜の見出しが先手・後手なら、駒落ちでもその呼び方にそろえる（1つの棋譜で呼び方を混ぜない）
    for (const auto& item : header) {
        const QString key = item.key.trimmed();
        if (key == GameInfoKeys::kShitatePlayer || key == GameInfoKeys::kUwatePlayer) return true;
        if (key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kWhitePlayer) return false;
    }
    return isHandicap(handicapLabel(startSfen));
}

QString KifuExportMetadataBuilder::timeControlText(const TimeControl& timeControl, bool handicap)
{
    const QString blackText = timeControlText(timeControl.black);
    const QString whiteText = timeControlText(timeControl.white);
    if (blackText == whiteText) return blackText;
    // CsaFormatter::convertToCsaTimeLines と KifuPresentation::infoValue がこの書式を読む
    return QStringLiteral("%1 %2 / %3 %4")
        .arg(handicap ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer, blackText,
             handicap ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer, whiteText);
}

QString KifuExportMetadataBuilder::timeControlText(const TimeControlSide& side)
{
    const qint64 mainSec = side.mainMs / 1000;
    QString text = QStringLiteral("%1:%2")
        .arg(mainSec / 60, 2, 10, QLatin1Char('0'))
        .arg(mainSec % 60, 2, 10, QLatin1Char('0'));
    if (side.byoyomiMs >= 1000) {
        text += QStringLiteral("+%1").arg(side.byoyomiMs / 1000);
    } else if (side.incrementMs >= 1000) {
        text += QStringLiteral("+%1秒加算").arg(side.incrementMs / 1000);
    }
    return text;
}
