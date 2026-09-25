/// @file automationcommands_position.cpp
/// @brief 自動化 API の position.* メソッドの実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "kifufilecontroller.h"
#include "sfenutils.h"
#include "sfenvalidationservice.h"
#include "usimoveconverter.h"

#include <QJsonArray>

void AutomationCommands::registerPositionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("position.get"), [context](const QJsonObject&) {
        QJsonObject result;
        const QStringList* record = context.sfenRecord ? context.sfenRecord() : nullptr;
        const int ply = context.currentMoveIndex ? *context.currentMoveIndex : 0;
        result[QStringLiteral("sfen")] = currentSfen(context);
        QString start = record && !record->isEmpty() ? record->first()
                                                     : (context.startSfenStr ? *context.startSfenStr : QString());
        result[QStringLiteral("start_sfen")] = SfenUtils::normalizeStart(start);
        result[QStringLiteral("ply")] = ply;
        QStringList moves;
        if (record && ply > 0 && ply < record->size()) {
            moves = UsiMoveConverter::fromSfenRecord(record->mid(0, ply + 1));
        }
        result[QStringLiteral("moves")] = QJsonArray::fromStringList(moves);
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("position.set"), [context](const QJsonObject& params) {
        const QString sfen = AutomationParams::requireString(params, QStringLiteral("sfen"));
        const bool discard = AutomationParams::optionalBool(params, QStringLiteral("discard_unsaved"));
        const SfenValidation validation = SfenValidationService::validate(sfen);
        if (!validation.valid) {
            throw AutomationError(AutomationErrorCode::InvalidParams,
                                  QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));
        }
        KifuFileController* controller = context.kifuFileController ? context.kifuFileController() : nullptr;
        if (!controller) {
            throw AutomationError(AutomationErrorCode::InternalError, QStringLiteral("Kifu controller is unavailable"));
        }
        if (controller->hasUnsavedChanges() && !discard) {
            throw AutomationError(AutomationErrorCode::UnsavedChanges,
                                  QStringLiteral("The current record has unsaved changes"),
                                  QStringLiteral("Save it with kifu.save or pass discard_unsaved=true"));
        }
        if (!controller->applySfenPosition(validation.normalizedSfen)) {
            throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("The position could not be applied"));
        }
        QJsonObject result;
        result[QStringLiteral("sfen")] = currentSfen(context);
        return result;
    });
}
