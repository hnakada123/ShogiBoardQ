/// @file automationcommands_position.cpp
/// @brief 自動化 API の position.* メソッドの実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "kifufilecontroller.h"
#include "sfenutils.h"
#include "sfenvalidationservice.h"

#include <QJsonArray>

void AutomationCommands::registerPositionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("position.get"), [context](const QJsonObject&) {
        QJsonObject result;
        const QStringList record = currentLinePositions(context);
        const int ply = context.currentPly();
        result[QStringLiteral("sfen")] = currentSfen(context);
        const QString start = record.isEmpty() ? SfenUtils::hirateSfen() : record.first();
        result[QStringLiteral("start_sfen")] = SfenUtils::normalizeStart(start);
        result[QStringLiteral("ply")] = ply;
        // 表示中の手数までの指し手（分岐ツリーの各手から取る）
        const RecordLine line = currentRecordLine(context);
        result[QStringLiteral("moves")] = QJsonArray::fromStringList(line.usiMoves(line.currentPly));
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("position.set"), [context](const QJsonObject& params) {
        const QString sfen = params.value(QStringLiteral("sfen")).toString();
        const bool discard = params.value(QStringLiteral("discard_unsaved")).toBool();
        const SfenValidation validation = SfenValidationService::validate(sfen);
        if (!validation.valid) {
            throw AutomationError(ErrorKind::InvalidArgument,
                                  QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));
        }
        KifuFileController* controller = context.kifuFileController;
        if (!controller) {
            throw AutomationError(ErrorKind::InternalError, QStringLiteral("Kifu controller is unavailable"));
        }
        if (controller->hasUnsavedChanges() && !discard) {
            throw AutomationError(ErrorKind::UnsavedChanges,
                                  QStringLiteral("The current record has unsaved changes"),
                                  QStringLiteral("Save it with kifu.save or pass discard_unsaved=true"));
        }
        if (!controller->applySfenPosition(validation.normalizedSfen)) {
            throw AutomationError(ErrorKind::InvalidState, QStringLiteral("The position could not be applied"));
        }
        QJsonObject result;
        result[QStringLiteral("sfen")] = currentSfen(context);
        return result;
    });
}
