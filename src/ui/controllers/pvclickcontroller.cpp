/// @file pvclickcontroller.cpp
/// @brief 読み筋の行から読み筋ダイアログを開く

#include "pvclickcontroller.h"

#include "appstate.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "pvboarddialog.h"
#include "sfenutils.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "shogiview.h"

PvClickController::PvClickController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void PvClickController::onPvRowClicked(ShogiEngineThinkingModel* model, int row)
{
    const ShogiInfoRecord* record = model && row >= 0 && row < model->rowCount() ? model->recordAt(row) : nullptr;
    if (!record || record->pv().isEmpty()) return;

    // 局面に合った手までの USI の読み筋。局面と合わない手があって残っていなければ、
    // その行の info でエンジンが送った読み筋をそのまま使う
    QStringList usiMoves = record->usiPv().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (usiMoves.isEmpty()) usiMoves = record->enginePv();

    // 読み筋を出したときの局面。無ければ表示中の局面とみなす
    const RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
    QString baseSfen = record->baseSfen();
    if (baseSfen.isEmpty()) baseSfen = m_deps.navState ? m_deps.navState->currentSfen() : QString();
    if (baseSfen.isEmpty()) baseSfen = SfenUtils::hirateSfen();

    // 読み筋の局面が表示中の手順の何手目かを探し、その局面に至った手をハイライトに使う
    QString lastUsiMove = record->lastUsiMove();
    QString prevSfen;
    const QString baseKey = SfenUtils::normalizeSfenKey(baseSfen);
    for (int ply = 1; ply <= line.moveCount(); ++ply) {
        if (SfenUtils::normalizeSfenKey(line.sfenAt(ply)) != baseKey) continue;
        prevSfen = line.sfenAt(ply - 1);
        if (lastUsiMove.isEmpty()) lastUsiMove = line.usiMove(ply).toString();
        break;
    }
    qCDebug(lcUi) << "onPvRowClicked: row=" << row << "base=" << baseSfen;

    auto* dlg = new PvBoardDialog(baseSfen, usiMoves, qobject_cast<QWidget*>(parent()));
    dlg->setKanjiPv(record->pv());
    m_dialogSources.insert(dlg, model);
    connect(dlg, &QDialog::finished, this, &PvClickController::onPvDialogFinished);

    // 対局者名（最後に始めた対局の対局者。名前が無ければ先手・後手）
    const MatchPlayers players = m_deps.state ? m_deps.state->players : MatchPlayers{};
    const QString blackName = players.name(Turn::Black);
    const QString whiteName = players.name(Turn::White);
    dlg->setPlayerNames(blackName.isEmpty() ? tr("先手") : blackName, whiteName.isEmpty() ? tr("後手") : whiteName);
    dlg->setFlipMode(m_deps.shogiView && m_deps.shogiView->flipMode());
    if (!lastUsiMove.isEmpty()) dlg->setLastMove(lastUsiMove);
    if (!prevSfen.isEmpty()) dlg->setPrevSfenForHighlight(prevSfen);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void PvClickController::onPvDialogFinished()
{
    // 閉じたダイアログの読み筋の行の選択を解く
    const auto source = m_dialogSources.constFind(sender());
    if (source == m_dialogSources.cend()) return;
    const ShogiEngineThinkingModel* model = source.value();
    m_dialogSources.erase(source);
    emit pvDialogClosed(model);
}
