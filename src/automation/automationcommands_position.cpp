/// @file automationcommands_position.cpp
/// @brief 自動化 API の position.* メソッドの実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "kifufilecontroller.h"
#include "kifuloadparser.h"
#include "sfenutils.h"
#include "usiposition.h"

#include <QJsonArray>

void AutomationCommands::registerPositionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("position.get"), [context](const QJsonObject&) {
        QJsonObject result;
        const QStringList record = currentLinePositions(context);
        const int ply = context.currentPly();
        result[QStringLiteral("sfen")] = currentSfen(context);
        const QString start = record.isEmpty() ? SfenUtils::hirateSfen() : record.first();
        result[QStringLiteral("start_sfen")] = UsiPosition::sfenOf(start);
        result[QStringLiteral("ply")] = ply;
        // 表示中の手数までの指し手（分岐ツリーの各手から取る）
        const RecordLine line = currentRecordLine(context);
        result[QStringLiteral("moves")] = QJsonArray::fromStringList(line.usiMoves(line.currentPly));
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("position.set"), [context](const QJsonObject& params) {
        const QString sfen = params.value(QStringLiteral("sfen")).toString();
        const bool discard = params.value(QStringLiteral("discard_unsaved")).toBool();
        // GUI の局面の貼り付けと同じ読み方・同じ規則（KifuLoader::loadPositionFromSfen）。玉の無い図と、手番でない側に
        // 王手がかかった図も受け付ける。引数の誤りは棋譜を置き換えられるかより先に返す
        const std::optional<UsiPosition> position = UsiPosition::parse(sfen);
        if (!position) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Invalid SFEN: %1").arg(sfen));
        if (const QString reason = KifuLoadParser::startPositionProblem(position->position()); !reason.isEmpty())
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Invalid SFEN: %1").arg(reason));
        KifuFileController* controller = AutomationCommands::requireReplaceableRecord(context, discard);
        if (!controller->applySfenPosition(sfen)) {
            throw AutomationError(ErrorKind::InvalidState, QStringLiteral("The position could not be applied"));
        }
        QJsonObject result;
        result[QStringLiteral("sfen")] = currentSfen(context);
        return result;
    });
}
