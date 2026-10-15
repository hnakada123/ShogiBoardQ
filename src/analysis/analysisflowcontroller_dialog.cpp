#include "analysisflowcontroller.h"
#include "kifuanalysisdialog.h"

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    m_lastStartSucceeded = false;
    if (!dlg) return;
    if (d.line.isEmpty()) {
        if (d.displayError) d.displayError(tr("解析対象の棋譜データが不正です。"));
        return;
    }
    const auto engines = dlg->engineList();
    const int index = dlg->engineNumber();
    if (index < 0 || index >= engines.size()) {
        if (d.displayError) d.displayError(tr("エンジン選択が不正です。"));
        return;
    }
    AnalysisRequest request;
    request.line = d.line;
    request.lineIndex = d.lineIndex;
    request.enginePath = engines.at(index).path;
    request.engineName = dlg->engineName();
    const int last = d.line.moveCount();
    request.options.startPly = dlg->initPosition() ? 0 : qBound(0, dlg->startPly(), last);
    request.options.endPly = dlg->initPosition() ? last : qBound(request.options.startPly, dlg->endPly(), last);
    request.options.movetimeMs = dlg->byoyomiSec() * 1000;
    m_lastStartSucceeded = start(d, std::move(request));
}

bool AnalysisFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    if (d.line.isEmpty()) {
        if (d.displayError) d.displayError(tr("解析対象の棋譜データが不正です。"));
        return false;
    }
    KifuAnalysisDialog dlg(parent);
    dlg.setMaxPly(d.line.moveCount());
    if (dlg.exec() != QDialog::Accepted) return false;
    start(d, &dlg);
    return m_lastStartSucceeded;
}
