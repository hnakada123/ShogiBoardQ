/// @file considerationflowcontroller.cpp
/// @brief 検討フローコントローラクラスの実装

#include "considerationflowcontroller.h"

#include "enginelistsettings.h"
#include "analysissession.h"
#include "shogienginethinkingmodel.h"

#include <QObject>

ConsiderationFlowController::ConsiderationFlowController(QObject* parent)
    : QObject(parent)
{
}

bool ConsiderationFlowController::runDirect(const Deps& d, const DirectParams& params, const QString& positionStr)
{
    if (!d.session) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: 検討のセッションが未初期化です。"));
        return false;
    }
    if (positionStr.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("検討対象の局面（position）が空です。"));
        return false;
    }

    // エンジンは名前（検討タブで選んだもの）で引く。名前が無いときだけ番号を使う。
    // 番号で引くと、タブの一覧を作った後に登録が変わったときに名前とパスが食い違う
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    int index = EngineListSettings::indexOfName(engines, params.engineName);
    if (index < 0 && params.engineName.isEmpty() && params.engineIndex >= 0 && params.engineIndex < engines.size()) {
        index = params.engineIndex;
    }
    const QString enginePath = index >= 0 ? engines.at(index).path : QString();
    const QString engineName = index >= 0 ? engines.at(index).name : params.engineName;

    if (enginePath.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("検討エンジンの選択が不正です。"));
        return false;
    }

    int byoyomiMs = 0;  // 0 は無制限
    if (!params.unlimitedTime) {
        byoyomiMs = params.byoyomiSec * 1000;  // 秒 → ms
    }

    if (d.onStarted) {
        d.onStarted();
    }

    // 時間設定コールバックを呼び出し
    if (d.onTimeSettingsReady) {
        d.onTimeSettingsReady(params.unlimitedTime, params.byoyomiSec);
    }

    // 候補手の数コールバックを呼び出し
    if (d.onMultiPVReady) {
        d.onMultiPVReady(params.multiPV);
    }

    AnalysisOptions opt;
    opt.mode        = AnalysisOptions::Mode::Consideration;
    opt.enginePath  = enginePath;
    opt.engineName  = engineName;
    opt.positionStr = positionStr;
    opt.byoyomiMs   = byoyomiMs;
    opt.multiPV     = params.multiPV;
    opt.considerationModel = d.considerationModel;
    opt.previousTo = params.previousTo;
    opt.lastUsiMove = params.lastUsiMove;
    d.session->start(opt);
    return true;
}
