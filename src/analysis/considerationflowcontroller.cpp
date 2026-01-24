#include "considerationflowcontroller.h"

#include "considerationdialog.h"
#include "matchcoordinator.h"
#include "shogienginethinkingmodel.h"

#include <QObject>
#include <QDialog>

ConsiderationFlowController::ConsiderationFlowController(QObject* parent)
    : QObject(parent)
{
}

void ConsiderationFlowController::runWithDialog(const Deps& d, QWidget* parent, const QString& positionStr)
{
    if (!d.match) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: MatchCoordinator が未初期化です。"));
        return;
    }
    if (positionStr.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("検討対象の局面（position）が空です。"));
        return;
    }

    ConsiderationDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) return;

    const QList<ConsiderationDialog::Engine>& engines = dlg.getEngineList();
    const int idx = dlg.getEngineNumber();

    if (engines.isEmpty() || idx < 0 || idx >= engines.size()) {
        if (d.onError) d.onError(QStringLiteral("検討エンジンの選択が不正です。"));
        return;
    }

    const auto& engine = engines.at(idx);

    const bool unlimitedTime = dlg.unlimitedTimeFlag();
    const int byoyomiSec = dlg.getByoyomiSec();

    int byoyomiMs = 0;  // 0 は無制限
    if (!unlimitedTime) {
        byoyomiMs = byoyomiSec * 1000;  // 秒 → ms
    }

    // 時間設定コールバックを呼び出し
    if (d.onTimeSettingsReady) {
        d.onTimeSettingsReady(unlimitedTime, byoyomiSec);
    }

    // 表示名は engine.name を使用
    startAnalysis(d.match, engine.path, engine.name, positionStr, byoyomiMs, d.multiPV, d.considerationModel);
}

void ConsiderationFlowController::startAnalysis(MatchCoordinator* match,
                                                 const QString& enginePath,
                                                 const QString& engineName,
                                                 const QString& positionStr,
                                                 int byoyomiMs,
                                                 int multiPV,
                                                 ShogiEngineThinkingModel* considerationModel)
{
    MatchCoordinator::AnalysisOptions opt;
    opt.enginePath  = enginePath;
    opt.engineName  = engineName;
    opt.positionStr = positionStr;
    opt.byoyomiMs   = byoyomiMs;
    opt.multiPV     = multiPV;
    opt.mode        = PlayMode::ConsiderationMode;  // 既存の PlayMode
    opt.considerationModel = considerationModel;    // ★ 追加: 検討タブ用モデル

    match->startAnalysis(opt);
}
