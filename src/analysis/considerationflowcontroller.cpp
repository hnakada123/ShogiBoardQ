#include "considerationflowcontroller.h"

#include "matchcoordinator.h"
#include "shogienginethinkingmodel.h"
#include "enginesettingsconstants.h"

#include <QObject>
#include <QSettings>
#include <QDir>
#include <QApplication>

ConsiderationFlowController::ConsiderationFlowController(QObject* parent)
    : QObject(parent)
{
}

void ConsiderationFlowController::runDirect(const Deps& d, const DirectParams& params, const QString& positionStr)
{
    using namespace EngineSettingsConstants;

    if (!d.match) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: MatchCoordinator が未初期化です。"));
        return;
    }
    if (positionStr.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("検討対象の局面（position）が空です。"));
        return;
    }

    // 設定ファイルからエンジンパスを取得
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    const int size = settings.beginReadArray(EnginesGroupName);
    QString enginePath;
    QString engineName = params.engineName;

    if (params.engineIndex >= 0 && params.engineIndex < size) {
        settings.setArrayIndex(params.engineIndex);
        enginePath = settings.value(EnginePathKey).toString();
        if (engineName.isEmpty()) {
            engineName = settings.value(EngineNameKey).toString();
        }
    }
    settings.endArray();

    if (enginePath.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("検討エンジンの選択が不正です。"));
        return;
    }

    int byoyomiMs = 0;  // 0 は無制限
    if (!params.unlimitedTime) {
        byoyomiMs = params.byoyomiSec * 1000;  // 秒 → ms
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
    opt.considerationModel = considerationModel;    // ★ 追加: 検討タブ用モデル
    opt.previousFileTo = previousFileTo;            // ★ 追加: 前回の移動先の筋
    opt.previousRankTo = previousRankTo;            // ★ 追加: 前回の移動先の段
    opt.lastUsiMove = lastUsiMove;                  // ★ 追加: 最後の指し手（USI形式）

    match->startAnalysis(opt);
}
