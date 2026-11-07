/// @file automationcommands_kifu.cpp
/// @brief 自動化 API の kifu.* メソッドの実装

#include "automationcommands.h"
#include "recordjson.h"
#include "kifunavigationstate.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "operationpolicy.h"
#include "inputpath.h"
#include "kifufilecontroller.h"
#include "kifunavigationcontroller.h"
#include "kifusavefile.h"
#include "outputpath.h"
#include "sfenutils.h"
#include "notificationpresenter.h"

#include <QFileInfo>
#include <QJsonArray>

namespace {

int totalPlies(const AutomationContext& context)
{
    // 表示中の手順（分岐ツリー）の行数。終局行（投了など）も1手に数える
    const QStringList record = AutomationCommands::currentLinePositions(context);
    return record.isEmpty() ? 0 : static_cast<int>(record.size()) - 1;
}

KifuFileController* requireFileController(const AutomationContext& context)
{
    KifuFileController* controller = context.kifuFileController;
    if (!controller) {
        throw AutomationError(ErrorKind::InternalError, QStringLiteral("Kifu controller is unavailable"));
    }
    return controller;
}

QJsonObject loadResult(const AutomationContext& context)
{
    QJsonObject result;
    result[QStringLiteral("total_plies")] = totalPlies(context);
    const QStringList record = AutomationCommands::currentLinePositions(context);
    result[QStringLiteral("start_sfen")] = record.isEmpty() ? QString() : record.first();
    result[QStringLiteral("kifu_file")] = context.document ? context.document->filePath() : QString();
    return result;
}

} // namespace

KifuFileController* AutomationCommands::requireReplaceableRecord(const AutomationContext& context, bool discardUnsaved)
{
    KifuFileController* controller = requireFileController(context);
    if (!controller->canReplaceRecord()) {
        throw AutomationError(ErrorKind::InvalidState,
                              QStringLiteral("The record cannot be replaced during a game, analysis or position editing"),
                              QStringLiteral("Stop the game or analysis, or finish position editing first"));
    }
    if (controller->hasUnsavedChanges() && !discardUnsaved) {
        throw AutomationError(ErrorKind::UnsavedChanges,
                              QStringLiteral("The current record has unsaved changes"),
                              QStringLiteral("Save it with kifu.save or pass discard_unsaved=true"));
    }
    return controller;
}

void AutomationCommands::registerKifuCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("kifu.load"), [context](const QJsonObject& params) {
        const QString path = params.value(QStringLiteral("path")).toString();
        const QString text = params.value(QStringLiteral("text")).toString();
        if (path.isEmpty() == text.isEmpty()) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Pass exactly one of \"path\" or \"text\""));
        }
        // CLI の入力と同じ検査（FIFO・デバイスは読み終わらず、メインスレッドを止めるので通常のファイルだけ）
        if (!path.isEmpty()) {
            if (const std::optional<Failure> problem = InputPath::check(path)) throw AutomationError(*problem);
        }
        KifuFileController* controller = AutomationCommands::requireReplaceableRecord(
            context, params.value(QStringLiteral("discard_unsaved")).toBool());
        bool ok = false;
        // 読み込み失敗のダイアログは出さず、内容をエラー応答で返す
        const NotificationPresenter::ScopedCapture capture;
        if (!path.isEmpty()) {
            ok = controller->loadKifuFile(path);
        } else {
            ok = controller->loadKifuText(text);
        }
        if (!ok || !capture.errors().isEmpty()) {
            const QString detail = capture.errors().join(QLatin1Char('\n'));
            throw AutomationError(ErrorKind::InvalidContent,
                                  detail.isEmpty() ? QStringLiteral("The record could not be loaded")
                                                   : QStringLiteral("The record could not be loaded: %1").arg(detail),
                                  QStringLiteral("Check the format of the record"));
        }
        QJsonObject result = loadResult(context);
        if (!capture.notices().isEmpty()) {
            result[QStringLiteral("warnings")] = QJsonArray::fromStringList(capture.notices());
        }
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("kifu.save"), [context](const QJsonObject& params) {
        const QString path = params.value(QStringLiteral("path")).toString();
        const bool overwrite = params.value(QStringLiteral("overwrite")).toBool();
        if (const std::optional<Failure> problem = OutputPath::check(path, overwrite, QStringLiteral("overwrite=true")))
            throw AutomationError(*problem);
        // 棋譜の拡張子のファイルにだけ保存する（ほかの名前を KIF で書かない。セキュリティ監査 S15）
        if (!KifuSaveFile::hasKnownSaveExtension(path)) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Not a record file name: %1").arg(path),
                                  QStringLiteral("Use one of %1").arg(KifuSaveFile::knownSaveExtensionsText()));
        }
        // GUI の保存と同じ規則。対局中の途中の棋譜や、局面編集の前の棋譜を保存済みにしない
        if (context.operationPolicy && !context.operationPolicy->canSaveRecord()) {
            throw AutomationError(ErrorKind::InvalidState,
                                  QStringLiteral("The record cannot be saved during a game, analysis or position editing"),
                                  QStringLiteral("Save after the operation ends"));
        }
        const QFileInfo info(path);
        KifuFileController* controller = requireFileController(context);
        QString error;
        if (!controller->saveKifuToPath(path, &error)) {
            const QString message = error.isEmpty() ? QStringLiteral("Saving failed: %1").arg(path)
                                                    : QStringLiteral("Saving failed: %1: %2").arg(path, error);
            throw AutomationError(ErrorKind::FileError, message,
                                  KifuSaveFile::encodingForPath(path) == KifuEncoding::ShiftJis
                                      ? QStringLiteral("Use a .kifu or .ki2u path to save as UTF-8")
                                      : QString());
        }
        QJsonObject result;
        result[QStringLiteral("path")] = path;
        result[QStringLiteral("format")] = info.suffix().toLower();
        result[QStringLiteral("lost")] =
            RecordJson::lost(KifuExport::lossIn(*context.document->tree(), KifuSaveFile::saveFormatForPath(path)));
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("kifu.get"), [context](const QJsonObject& params) {
        // 引数は AutomationMethodSpecs の表で検査済み（既定値も補ってある）
        const QString format = params.value(QStringLiteral("format")).toString();
        const int fromPly = params.value(QStringLiteral("from_ply")).toInt();
        const int maxMoves = params.value(QStringLiteral("max_moves")).toInt();
        QJsonObject result;
        result[QStringLiteral("format")] = format;
        result[QStringLiteral("total_plies")] = totalPlies(context);

        if (format == QLatin1String("moves")) {
            const KifuBranchTree* tree = context.branchTree;
            QJsonArray moves;
            bool truncated = false;
            if (tree) {
                // 既定は表示中の手順（分岐を表示中なら分岐）。line="main" で本譜を返す。
                const QString lineParam = params.value(QStringLiteral("line")).toString();
                const int lineIndex = (lineParam == QLatin1String("main") || !context.navState)
                                          ? 0 : context.navState->currentLineIndex();
                const QList<BranchLine> lines = !tree->isEmpty() ? tree->allLines() : QList<BranchLine>();
                const BranchLine line = (lineIndex >= 0 && lineIndex < lines.size()) ? lines.at(lineIndex) : BranchLine();
                result[QStringLiteral("line")] = lineIndex >= 0 && lineIndex < lines.size() ? lineIndex : 0;
                // 手数はほかの形式・get_app_state と同じく手順の行数（終局の行も1手に数える）
                result[QStringLiteral("total_plies")] = qMax(0, line.length() - 1);
                // 開始局面のコメント・しおりは行にせず、CLI の convert-kifu と同じ欄に書く（from_ply は1から）
                if (const KifuBranchNode* root = tree->root())
                    RecordJson::addOpening(result, root->comment(), root->bookmark());
                int emitted = 0;
                const QList<KifuBranchNode*> nodes = line.nodesFrom(fromPly);
                for (const KifuBranchNode* node : nodes) {
                    if (emitted >= maxMoves) { truncated = true; break; }
                    moves.append(RecordJson::entry(node->ply(), node->record()));
                    ++emitted;
                }
            }
            result[QStringLiteral("moves")] = moves;
            result[QStringLiteral("truncated")] = truncated;
            return result;
        }

        // 形式の名前は表（KifuFormats::savableNames）の選択肢で検査済み
        const KifuFormat kifuFormat = *KifuFormats::fromName(format);
        if (!context.document) {
            throw AutomationError(ErrorKind::InternalError, QStringLiteral("Kifu document is unavailable"));
        }
        const QStringList lines = KifuExport::lines(*context.document, kifuFormat);
        result[QStringLiteral("text")] = lines.join(QLatin1Char('\n')) + (lines.isEmpty() ? QString() : QStringLiteral("\n"));
        result[QStringLiteral("truncated")] = false;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("kifu.goto"), [context](const QJsonObject& params) {
        const int ply = params.value(QStringLiteral("ply")).toInt();
        const int total = qMax(0, static_cast<int>(currentLinePositions(context).size()) - 1);
        if (ply > total) {
            throw AutomationError(ErrorKind::InvalidArgument,
                                  QStringLiteral("ply %1 is beyond the last move (%2)").arg(ply).arg(total));
        }
        KifuNavigationController* nav = context.navigationController;
        if (!nav || !context.branchTree || context.branchTree->isEmpty()) {
            throw AutomationError(ErrorKind::InvalidState, QStringLiteral("No record is loaded"),
                                  QStringLiteral("Load a record with kifu.load first"));
        }
        // 棋譜欄・分岐候補欄などの GUI の入口と同じ規則（対局の盤・解析・編集中の局面を表示中の手から外さない）
        if (context.operationPolicy && !context.operationPolicy->canBrowseRecord()) {
            throw AutomationError(ErrorKind::InvalidState,
                                  QStringLiteral("The record cannot be navigated during a game, kifu analysis or position editing"),
                                  QStringLiteral("Wait until app.state reports ui_state idle, consideration or tsume_search"));
        }
        nav->goToPly(ply);
        QJsonObject result;
        result[QStringLiteral("ply")] = context.navState ? context.currentPly() : ply;
        result[QStringLiteral("sfen")] = currentSfen(context);
        return result;
    });
}
