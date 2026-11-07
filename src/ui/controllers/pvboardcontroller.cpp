/// @file pvboardcontroller.cpp
/// @brief 読み筋の行から読み筋ダイアログを開く

#include "pvboardcontroller.h"

#include "kifunavigationstate.h"
#include "logcategories.h"
#include "playerinfopresenter.h"
#include "pvboarddialog.h"
#include "pvtext.h"
#include "sfenutils.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "shogiview.h"

PvBoardController::PvBoardController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.activity);
}

void PvBoardController::onPvRowClicked(ShogiEngineThinkingModel* model, int row)
{
    const ShogiInfoRecord* record = model && row >= 0 && row < model->rowCount() ? model->recordAt(row) : nullptr;
    if (!record || record->pv().isEmpty()) return;

    // 読み筋を出したときの局面。無ければ表示中の局面の上で、エンジンが送った語を指し直す
    PvText pv = record->pv();
    const RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
    if (pv.baseSfen().isEmpty()) {
        QString current = m_deps.navState ? m_deps.navState->currentSfen() : QString();
        if (current.isEmpty()) current = SfenUtils::hirateSfen();
        pv = PvText::play(current, pv.rest());
    }
    const QString baseSfen = pv.baseSfen();

    // 至った手が思考の行に無ければ、読み筋の局面が表示中の手順の何手目かを探し、その局面に至った手を使う
    QString lastUsiMove = record->lastUsiMove();
    if (lastUsiMove.isEmpty()) {
        const QString baseKey = SfenUtils::normalizeSfenKey(baseSfen);
        for (int ply = 1; ply <= line.moveCount(); ++ply) {
            if (SfenUtils::normalizeSfenKey(line.sfenAt(ply)) != baseKey) continue;
            lastUsiMove = line.usiMove(ply).toString();
            break;
        }
    }
    qCDebug(lcUi) << "onPvRowClicked: row=" << row << "base=" << baseSfen;

    PvBoardDialog* dlg = showPvBoard(pv, lastUsiMove);
    m_dialogSources.insert(dlg, model);
    connect(dlg, &QDialog::finished, this, &PvBoardController::onPvDialogFinished);
}

PvBoardDialog* PvBoardController::showPvBoard(const PvText& pv, const QString& lastUsiMove)
{
    auto* dlg = new PvBoardDialog(pv, qobject_cast<QWidget*>(parent()));
    // 対局者名は本体の盤と同じ（対局中は対局者、対局していなければ対局情報の名前）
    dlg->setPlayerNames(PlayerInfoPresenter::boardName(*m_deps.document, *m_deps.activity, Turn::Black),
                        PlayerInfoPresenter::boardName(*m_deps.document, *m_deps.activity, Turn::White));
    // 本体の盤の今の向きと大きさで出す
    dlg->setFlipMode(m_deps.shogiView && m_deps.shogiView->flipMode());
    if (m_deps.shogiView) dlg->setSquareSize(m_deps.shogiView->squareSize());
    if (!lastUsiMove.isEmpty()) dlg->setLastMove(lastUsiMove);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    return dlg;
}

void PvBoardController::onPvDialogFinished()
{
    // 閉じたダイアログの読み筋の行の選択を解く
    const auto source = m_dialogSources.constFind(sender());
    if (source == m_dialogSources.cend()) return;
    const ShogiEngineThinkingModel* model = source.value();
    m_dialogSources.erase(source);
    emit pvDialogClosed(model);
}
