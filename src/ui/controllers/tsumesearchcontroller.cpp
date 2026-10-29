/// @file tsumesearchcontroller.cpp
/// @brief 詰み探索（条件ダイアログと探索の開始）の実装

#include "tsumesearchcontroller.h"

#include "analysissession.h"
#include "errorbus.h"
#include "kifunavigationstate.h"
#include "matesearchposition.h"
#include "tsumeshogisearchdialog.h"

#include <QTimer>

TsumeSearchController::TsumeSearchController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.navState && m_deps.session);
}

std::optional<UsiPosition> TsumeSearchController::positionForMate() const
{
    // 途中局面は開始局面からの指し手で、開始局面（詰将棋の問題など）は玉の有無から決めた攻方の手番で送る
    return MateSearchPosition::forLine(m_deps.navState->currentRecordLine(), m_deps.navState->currentSfen());
}

void TsumeSearchController::showDialogAndStart()
{
    const std::optional<UsiPosition> position = positionForMate();
    if (!position) {
        ErrorBus::instance().postMessage(
            ErrorBus::ErrorLevel::Warning,
            tr("詰み探索用の局面（SFEN）が取得できません。棋譜を読み込むか局面を指定してください。"));
        return;
    }

    AnalysisOptions options;
    {
        TsumeShogiSearchDialog dialog(m_deps.parentWidget);
        if (dialog.exec() != QDialog::Accepted) return;
        const std::optional<EngineEntry> engine = dialog.engine();
        if (!engine) {
            ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, tr("詰み探索エンジンの選択が不正です。"));
            return;
        }
        options.mode = AnalysisOptions::Mode::Tsume;
        options.enginePath = engine->path;
        options.engineName = engine->name;
        options.position = *position;
        options.byoyomiMs = dialog.unlimitedTimeFlag() ? 0 : dialog.byoyomiSec() * 1000;   // 0 は無制限
    }
    // ダイアログの終了処理を終えてから、次のイベントループで探索を始める
    m_pending = options;
    QTimer::singleShot(0, this, &TsumeSearchController::startPendingSearch);
}

void TsumeSearchController::startPendingSearch()
{
    m_deps.session->start(m_pending);
}
