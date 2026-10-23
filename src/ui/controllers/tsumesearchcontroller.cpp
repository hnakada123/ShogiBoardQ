/// @file tsumesearchcontroller.cpp
/// @brief 詰み探索（条件ダイアログと探索の開始）の実装

#include "tsumesearchcontroller.h"

#include "analysissession.h"
#include "errorbus.h"
#include "kifunavigationstate.h"
#include "tsumepositionutil.h"
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
    const RecordLine line = m_deps.navState->currentRecordLine();
    // 途中局面は開始局面からの指し手で送る（エンジンに千日手の判断材料を渡すため）
    if (!line.isEmpty() && line.currentPly > 0) return line.usiPosition(line.currentPly);
    // 開始局面（詰将棋の問題など）は、玉の有無から攻方を決めて手番を合わせる
    const QString command =
        TsumePositionUtil::buildPositionForMate(line.isEmpty() ? m_deps.navState->currentSfen() : line.startSfen);
    return UsiPosition::parse(command);
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
        const std::optional<EngineListSettings::EngineEntry> engine = dialog.engine();
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
