#include "playernameservice.h"
#include <QDebug>

PlayerNameMapping PlayerNameService::computePlayers(PlayMode mode,
                                                    const QString& human1,
                                                    const QString& human2,
                                                    const QString& engine1,
                                                    const QString& engine2)
{
    qDebug().noquote() << "[PlayerNameService] ★ computePlayers: mode=" << static_cast<int>(mode)
                       << " human1=" << human1 << " human2=" << human2
                       << " engine1=" << engine1 << " engine2=" << engine2;
    
    PlayerNameMapping out;

    switch (mode) {
    case HumanVsHuman:
        out.p1 = human1;
        out.p2 = human2;
        break;

    case EvenHumanVsEngine:
        out.p1 = human1;
        out.p2 = engine2;
        break;

    case EvenEngineVsHuman:
        out.p1 = engine1;
        out.p2 = human2;
        break;

    case EvenEngineVsEngine:
        out.p1 = engine1;
        out.p2 = engine2;
        break;

    case HandicapHumanVsEngine: // 下手=Human(P1), 上手=Engine(P2)
        out.p1 = human1;
        out.p2 = engine2;
        break;

    case HandicapEngineVsHuman: // 下手=Engine(P1), 上手=Human(P2)
        out.p1 = engine1;
        out.p2 = human2;
        break;

    case HandicapEngineVsEngine: // 下手=Engine(P1), 上手=Engine(P2)
        out.p1 = engine1;
        out.p2 = engine2;
        break;

    // 解析/検討/詰み探索などは既存実装と同じくデフォルトラベル
    case AnalysisMode:
    case ConsidarationMode:
    case TsumiSearchMode:
    case NotStarted:
    case PlayModeError:
    default:
        out.p1 = QStringLiteral("先手");
        out.p2 = QStringLiteral("後手");
        break;
    }

    qDebug().noquote() << "[PlayerNameService] ★ computePlayers: result p1=" << out.p1 << " p2=" << out.p2;
    return out;
}

EngineNameMapping PlayerNameService::computeEngineModels(PlayMode mode,
                                                         const QString& engine1,
                                                         const QString& engine2)
{
    EngineNameMapping out;

    switch (mode) {
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        // P2 がエンジン。モデル1側に「使うエンジン」を表示（既存のMainWindow実装に合わせる）
        out.model1 = engine2;
        out.model2.clear();
        break;

    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        // P1 がエンジン
        out.model1 = engine1;
        out.model2.clear();
        break;

    case EvenEngineVsEngine:
        out.model1 = engine1;
        out.model2 = engine2;
        break;

    case HandicapEngineVsEngine:
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
