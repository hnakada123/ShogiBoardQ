/// @file kifuclipboardservice.cpp
/// @brief 棋譜クリップボードサービスの実装

#include "kifuclipboardservice.h"
#include "gamerecordmodel.h"
#include "logcategories.h"

#include <QApplication>
#include <QClipboard>

namespace KifuClipboardService {

namespace {

/// クリップボードにテキストを設定
bool setClipboardText(const QString& text)
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
        return true;
    }
    return false;
}

} // anonymous namespace

bool copyKif(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx)
{
    const QStringList kifLines = model.toKifLines(ctx);

    if (kifLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyKif: no KIF data";
        return false;
    }

    const QString kifText = kifLines.join(QStringLiteral("\n"));
    if (setClipboardText(kifText)) {
        qCDebug(lcKifu).noquote() << "copyKif: copied" << kifText.size() << "chars";
        return true;
    }
    return false;
}

bool copyKi2(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx)
{
    const QStringList ki2Lines = model.toKi2Lines(ctx);

    if (ki2Lines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyKi2: no KI2 data";
        return false;
    }

    const QString ki2Text = ki2Lines.join(QStringLiteral("\n"));
    if (setClipboardText(ki2Text)) {
        qCDebug(lcKifu).noquote() << "copyKi2: copied" << ki2Text.size() << "chars";
        return true;
    }
    return false;
}

} // namespace KifuClipboardService
