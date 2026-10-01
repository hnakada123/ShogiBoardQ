/// @file playernameservice.cpp
/// @brief プレイヤー名解決サービスクラスの実装

#include "playernameservice.h"

// ============================================================
// 名前解決 API
// ============================================================

EngineNameMapping PlayerNameService::computeEngineModels(PlayMode mode,
                                                         const QString& engine1,
                                                         const QString& engine2)
{
    EngineNameMapping out;

    switch (mode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        // P2がエンジン。モデル1側に表示（既存のMainWindow実装に合わせる）
        out.model1 = engine2;
        out.model2.clear();
        break;

    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        out.model1 = engine1;
        out.model2.clear();
        break;

    case PlayMode::EvenEngineVsEngine:
        out.model1 = engine1;
        out.model2 = engine2;
        break;

    case PlayMode::HandicapEngineVsEngine:
        // 既存実装は入れ替え（model1=engine2, model2=engine1）
        out.model1 = engine2;
        out.model2 = engine1;
        break;

    default:
        out.model1.clear();
        out.model2.clear();
        break;
    }

    return out;
}
