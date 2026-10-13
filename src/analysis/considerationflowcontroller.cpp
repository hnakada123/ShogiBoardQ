/// @file considerationflowcontroller.cpp
/// @brief 検討フローコントローラクラスの実装

#include "considerationflowcontroller.h"

#include "enginelistsettings.h"
#include "matchcoordinator.h"
#include "shogienginethinkingmodel.h"

#include <QObject>

ConsiderationFlowController::ConsiderationFlowController(QObject* parent)
    : QObject(parent)
{
}

bool ConsiderationFlowController::runDirect(const Deps& d, const DirectParams& params, const QString& positionStr)
{
    if (!d.match) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: MatchCoordinator が未初期化です。"));
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

    startAnalysis(d.match, enginePath, engineName, positionStr, byoyomiMs, params.multiPV, d.considerationModel,
                  params.previousFileTo, params.previousRankTo, params.lastUsiMove);
    return true;
}

void ConsiderationFlowController::startAnalysis(MatchCoordinator* match,
                                                 const QString& enginePath,
                                                 const QString& engineName,
                                                 const QString& positionStr,
                                                 int byoyomiMs,
                                                 int multiPV,
                                                 ShogiEngineThinkingModel* considerationModel,
                                                 int previousFileTo,
                                                 int previousRankTo,
                                                 const QString& lastUsiMove)
{
    MatchCoordinator::AnalysisOptions opt;
    opt.enginePath  = enginePath;
    opt.engineName  = engineName;
    opt.positionStr = positionStr;
    opt.byoyomiMs   = byoyomiMs;
    opt.multiPV     = multiPV;
    opt.mode        = PlayMode::ConsiderationMode;  // 既存の PlayMode
    opt.considerationModel = considerationModel;
    opt.previousFileTo = previousFileTo;
    opt.previousRankTo = previousRankTo;
    opt.lastUsiMove = lastUsiMove;

    match->startAnalysis(opt);
}
