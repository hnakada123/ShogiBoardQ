/// @file automationcommands_kifu.cpp
/// @brief 自動化 API の kifu.* メソッドの実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "kifunavigationcontroller.h"
#include "kifusavecoordinator.h"
#include "sfenutils.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

namespace {

void requireAbsolutePath(const QString& path)
{
    if (!QFileInfo(path).isAbsolute()) {
        throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("Path must be absolute: %1").arg(path));
    }
}

int totalPlies(const AutomationContext& context)
{
    const QStringList* record = context.sfenRecord ? context.sfenRecord() : nullptr;
    return record && !record->isEmpty() ? static_cast<int>(record->size()) - 1 : 0;
}

KifuFileController* requireFileController(const AutomationContext& context)
{
    KifuFileController* controller = context.kifuFileController ? context.kifuFileController() : nullptr;
    if (!controller) {
        throw AutomationError(AutomationErrorCode::InternalError, QStringLiteral("Kifu controller is unavailable"));
    }
    return controller;
}

void requireSavedOrDiscard(KifuFileController* controller, bool discard)
{
    if (controller->hasUnsavedChanges() && !discard) {
        throw AutomationError(AutomationErrorCode::UnsavedChanges,
                              QStringLiteral("The current record has unsaved changes"),
                              QStringLiteral("Save it with kifu.save or pass discard_unsaved=true"));
    }
}

QJsonObject loadResult(const AutomationContext& context)
{
    QJsonObject result;
    result[QStringLiteral("total_plies")] = totalPlies(context);
    const QStringList* record = context.sfenRecord ? context.sfenRecord() : nullptr;
    result[QStringLiteral("start_sfen")] = record && !record->isEmpty() ? record->first() : QString();
    result[QStringLiteral("kifu_file")] = context.saveFileName ? *context.saveFileName : QString();
    return result;
}

} // namespace

void AutomationCommands::registerKifuCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("kifu.load"), [context](const QJsonObject& params) {
        const QString path = AutomationParams::optionalString(params, QStringLiteral("path"));
        const QString text = AutomationParams::optionalString(params, QStringLiteral("text"));
        if (path.isEmpty() == text.isEmpty()) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Pass exactly one of \"path\" or \"text\""));
        }
        KifuFileController* controller = requireFileController(context);
        requireSavedOrDiscard(controller, AutomationParams::optionalBool(params, QStringLiteral("discard_unsaved")));
        bool ok = false;
        if (!path.isEmpty()) {
            requireAbsolutePath(path);
            if (!QFileInfo::exists(path)) {
                throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("File not found: %1").arg(path));
            }
            ok = controller->loadKifuFile(path);
        } else {
            ok = controller->loadKifuText(text);
        }
        if (!ok) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("The record could not be loaded"),
                                  QStringLiteral("Check the format; use dialog.list to see any error dialog"));
        }
        return loadResult(context);
    });

    dispatcher.registerMethod(QStringLiteral("kifu.save"), [context](const QJsonObject& params) {
        const QString path = AutomationParams::requireString(params, QStringLiteral("path"));
        const bool overwrite = AutomationParams::optionalBool(params, QStringLiteral("overwrite"));
        requireAbsolutePath(path);
        const QFileInfo info(path);
        if (info.exists() && !overwrite) {
            throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("File already exists: %1").arg(path),
                                  QStringLiteral("Pass overwrite=true to replace it"));
        }
        if (!info.dir().exists()) {
            throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("Directory does not exist: %1").arg(info.dir().path()));
        }
        KifuFileController* controller = requireFileController(context);
        if (!controller->saveKifuToPath(path)) {
            throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("Saving failed: %1").arg(path));
        }
        QJsonObject result;
        result[QStringLiteral("path")] = path;
        result[QStringLiteral("format")] = KifuSaveCoordinator::hasKnownSaveExtension(path) ? info.suffix().toLower() : QStringLiteral("kif");
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("kifu.get"), [context](const QJsonObject& params) {
        const QString format = AutomationParams::optionalString(params, QStringLiteral("format"), QStringLiteral("moves")).toLower();
        const int fromPly = AutomationParams::optionalInt(params, QStringLiteral("from_ply"), 1, 1, 100000);
        const int maxMoves = AutomationParams::optionalInt(params, QStringLiteral("max_moves"), 200, 1, 100000);
        QJsonObject result;
        result[QStringLiteral("format")] = format;
        result[QStringLiteral("total_plies")] = totalPlies(context);

        if (format == QLatin1String("moves")) {
            GameRecordModel* model = context.gameRecordModel ? context.gameRecordModel() : nullptr;
            QJsonArray moves;
            if (model) {
                const QList<KifDisplayItem> items = model->collectMainlineForExport();
                const QStringList usi = model->collectMainlineUsiForExport();
                int emitted = 0;
                for (const KifDisplayItem& item : items) {
                    if (item.ply < fromPly) continue;
                    if (emitted >= maxMoves) break;
                    QJsonObject move;
                    move[QStringLiteral("ply")] = item.ply;
                    move[QStringLiteral("text")] = item.prettyMove;
                    if (item.ply >= 1 && item.ply - 1 < usi.size()) move[QStringLiteral("usi")] = usi.at(item.ply - 1);
                    if (!item.timeText.isEmpty()) move[QStringLiteral("time")] = item.timeText;
                    if (!item.comment.isEmpty()) move[QStringLiteral("comment")] = item.comment;
                    if (!item.bookmark.isEmpty()) move[QStringLiteral("bookmark")] = item.bookmark;
                    moves.append(move);
                    ++emitted;
                }
            }
            result[QStringLiteral("moves")] = moves;
            result[QStringLiteral("truncated")] = false;
            return result;
        }

        static const QHash<QString, KifuSaveCoordinator::SaveFormat> formats = {
            {QStringLiteral("kif"), KifuSaveCoordinator::SaveFormat::Kif},
            {QStringLiteral("ki2"), KifuSaveCoordinator::SaveFormat::Ki2},
            {QStringLiteral("csa"), KifuSaveCoordinator::SaveFormat::Csa},
            {QStringLiteral("jkf"), KifuSaveCoordinator::SaveFormat::Jkf},
            {QStringLiteral("usen"), KifuSaveCoordinator::SaveFormat::Usen},
            {QStringLiteral("usi"), KifuSaveCoordinator::SaveFormat::Usi},
        };
        if (!formats.contains(format)) {
            throw AutomationError(AutomationErrorCode::InvalidParams,
                                  QStringLiteral("format must be one of moves, kif, ki2, csa, jkf, usen, usi"));
        }
        KifuExportController* exporter = context.kifuExportController ? context.kifuExportController() : nullptr;
        if (!exporter) {
            throw AutomationError(AutomationErrorCode::InternalError, QStringLiteral("Kifu exporter is unavailable"));
        }
        const QStringList lines = exporter->exportLines(formats.value(format));
        result[QStringLiteral("text")] = lines.join(QLatin1Char('\n')) + (lines.isEmpty() ? QString() : QStringLiteral("\n"));
        result[QStringLiteral("truncated")] = false;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("kifu.goto"), [context](const QJsonObject& params) {
        const int ply = AutomationParams::requireInt(params, QStringLiteral("ply"), 0, 100000);
        const int total = totalPlies(context);
        if (ply > total) {
            throw AutomationError(AutomationErrorCode::InvalidParams,
                                  QStringLiteral("ply %1 is beyond the last move (%2)").arg(ply).arg(total));
        }
        KifuNavigationController* nav = context.navigationController ? context.navigationController() : nullptr;
        if (!nav || !context.branchTree || context.branchTree->isEmpty()) {
            throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("No record is loaded"),
                                  QStringLiteral("Load a record with kifu.load first"));
        }
        nav->goToPly(ply);
        QJsonObject result;
        result[QStringLiteral("ply")] = context.currentMoveIndex ? *context.currentMoveIndex : ply;
        result[QStringLiteral("sfen")] = currentSfen(context);
        return result;
    });
}
