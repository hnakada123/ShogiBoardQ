/// @file josekiregistrationcontroller.cpp
/// @brief エンジンの読み筋を定跡に登録する利用者操作の実装

#include "josekiregistrationcontroller.h"

#include "josekianalysisregisterdialog.h"
#include "josekibooksession.h"
#include "josekitablemodel.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisrow.h"
#include "kifupresentation.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

namespace {

/// SFEN の手番が後手か
bool whiteToMove(const QString& sfen)
{
    return sfen.section(QLatin1Char(' '), 1, 1) == QLatin1String("w");
}

} // namespace

JosekiRegistrationController::JosekiRegistrationController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.book);
}

bool JosekiRegistrationController::canRegister() const
{
    return m_deps.book->canEdit();
}

void JosekiRegistrationController::recordBoardMove(const QString& sfen, const QString& usiMove)
{
    if (!m_deps.recordBoardMoves || !m_deps.recordBoardMoves->isChecked() || sfen.isEmpty() || usiMove.isEmpty()) return;
    if (!canRegister()) {
        showStatus(tr("定跡の読み書き中のため、盤の手を登録できませんでした"));
        return;
    }
    if (m_deps.book->hasMove(sfen, usiMove)) return;
    if (m_deps.book->addMove(sfen, {usiMove, QStringLiteral("none"), 0, 0, 1, {}}))
        showStatus(tr("%1 を定跡に登録しました").arg(JosekiTableModel::moveText(usiMove, sfen)));
}

void JosekiRegistrationController::showStatus(const QString& text) const
{
    if (m_deps.statusBar) m_deps.statusBar->showMessage(text, 5000);
}

JosekiRegistrationController::Target JosekiRegistrationController::targetOf(const KifuAnalysisRow& row)
{
    Target target;
    target.sfen = row.sfen;
    target.pv = row.pv.usiMoves();
    // 結果の表は先手から見た評価値で持つので、局面の手番側から見た値に戻す
    target.score = whiteToMove(row.sfen) ? row.evaluation.negated() : row.evaluation;
    target.depth = row.depth;
    return target;
}

void JosekiRegistrationController::addRegisterAction(QMenu* menu, const Target& target)
{
    m_menuTarget = target;
    QString text = tr("読み筋の最初の手を定跡に登録");
    if (target.isValid()) {
        text = tr("読み筋の最初の手（%1）を定跡に登録")
                   .arg(KifuPresentation::move(target.sfen, target.pv.constFirst(), KifuPresentation::options()));
    }
    QAction* action = menu->addAction(text);
    action->setObjectName(QStringLiteral("josekiRegisterPv"));
    action->setEnabled(target.isValid() && canRegister());
    if (!target.isValid()) {
        action->setToolTip(tr("この行には局面に合った読み筋がありません"));
    } else if (!canRegister()) {
        action->setToolTip(tr("定跡ウィンドウで定跡を開くと登録できます（読み書き中は登録できません）"));
    } else {
        action->setToolTip(tr("読み筋の2手目を予想応手にします。登録済みの手は評価値・深さ・予想応手だけを書き換えます"));
    }
    connect(action, &QAction::triggered, this, &JosekiRegistrationController::registerMenuTarget);
}

void JosekiRegistrationController::onThinkingRowContextMenu(ShogiEngineThinkingModel* model, int row, QMenu* menu)
{
    const ShogiInfoRecord* record = model ? model->recordAt(row) : nullptr;
    if (!record || !menu) return;
    Target target;
    target.sfen = record->pv().baseSfen();
    target.pv = record->pv().usiMoves();
    target.score = record->score();
    target.depth = record->depth();
    addRegisterAction(menu, target);
}

void JosekiRegistrationController::onAnalysisRowContextMenu(int row, QMenu* menu)
{
    const KifuAnalysisRow* result = m_deps.analysisModel ? m_deps.analysisModel->row(row) : nullptr;
    if (!result || !menu) return;
    addRegisterAction(menu, targetOf(*result));
    QAction* all = menu->addAction(tr("結果をまとめて定跡に登録..."));
    all->setObjectName(QStringLiteral("josekiRegisterAnalysis"));
    all->setEnabled(canRegister());
    connect(all, &QAction::triggered, this, &JosekiRegistrationController::registerAnalysisResults);
}

void JosekiRegistrationController::registerMenuTarget()
{
    const Target target = m_menuTarget;
    if (!target.isValid() || !canRegister()) return;
    const QString move = KifuPresentation::move(target.sfen, target.pv.constFirst(), KifuPresentation::options());
    switch (m_deps.book->registerEnginePv(target.sfen, target.pv, target.score, target.depth)) {
    case JosekiBookSession::EngineRegistration::Added:
        showStatus(tr("%1 を定跡に登録しました").arg(move));
        break;
    case JosekiBookSession::EngineRegistration::Updated:
        showStatus(tr("定跡の %1 の評価値・深さ・予想応手を書き換えました").arg(move));
        break;
    case JosekiBookSession::EngineRegistration::Invalid:
        break;
    }
}

JosekiRegistrationController::BatchResult JosekiRegistrationController::registerAnalysisRows(int fromPly, int toPly,
                                                                                              bool black, bool white)
{
    BatchResult result;
    if (!m_deps.analysisModel || !canRegister()) return result;
    for (int i = 0; i < m_deps.analysisModel->rowCount(); ++i) {
        const KifuAnalysisRow* row = m_deps.analysisModel->row(i);
        if (!row || row->ply < fromPly || row->ply > toPly) continue;
        if (whiteToMove(row->sfen) ? !white : !black) continue;
        const Target target = targetOf(*row);
        if (!target.isValid()) continue;
        switch (m_deps.book->registerEnginePv(target.sfen, target.pv, target.score, target.depth)) {
        case JosekiBookSession::EngineRegistration::Added: ++result.added; break;
        case JosekiBookSession::EngineRegistration::Updated: ++result.updated; break;
        case JosekiBookSession::EngineRegistration::Invalid: break;
        }
    }
    return result;
}

void JosekiRegistrationController::registerAnalysisResults()
{
    if (!m_deps.analysisModel || !canRegister()) return;
    int minPly = -1;
    int maxPly = -1;
    for (int i = 0; i < m_deps.analysisModel->rowCount(); ++i) {
        const KifuAnalysisRow* row = m_deps.analysisModel->row(i);
        if (!row || row->ply < 0) continue;
        minPly = minPly < 0 ? row->ply : qMin(minPly, row->ply);
        maxPly = qMax(maxPly, row->ply);
    }
    if (minPly < 0) return;

    JosekiAnalysisRegisterDialog dialog(minPly, maxPly, m_deps.parentWidget);
    if (dialog.exec() != QDialog::Accepted) return;
    const JosekiAnalysisRegisterDialog::Range range = dialog.range();
    const BatchResult result = registerAnalysisRows(range.fromPly, range.toPly, range.black, range.white);
    const QString text = result.updated > 0
        ? tr("定跡に %1 手を登録し、%2 手を書き換えました（定跡ウィンドウで保存してください）").arg(result.added).arg(result.updated)
        : tr("定跡に %1 手を登録しました（定跡ウィンドウで保存してください）").arg(result.added);
    showStatus(text);
    QMessageBox::information(m_deps.parentWidget, tr("結果をまとめて定跡に登録"), text);
}
