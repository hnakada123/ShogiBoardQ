#include "gamestartcoordinator.h"
#include "shogiview.h"
#include <QDebug>

GameStartCoordinator::GameStartCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_match(d.match)
    , m_clock(d.clock)
    , m_gc(d.gc)
    , m_view(d.view)
{
}

bool GameStartCoordinator::validate_(const StartParams& p, QString& whyNot) const
{
    if (!m_match) {
        whyNot = QStringLiteral("MatchCoordinator が未設定です。");
        return false;
    }
    // StartOptions の最低限チェック（必要に応じて追加）
    if (p.opt.mode == PlayMode::NotStarted) {
        whyNot = QStringLiteral("対局モードが NotStarted のままです。");
        return false;
    }
    return true;
}

void GameStartCoordinator::start(const StartParams& params)
{
    QString reason;
    if (!validate_(params, reason)) {
        emit startFailed(reason);
        qWarning().noquote() << "[GameStartCoordinator] startFailed:" << reason;
        return;
    }

    // --- 1) 開始前フック（UI/状態初期化） ---
    emit requestPreStartCleanup();

    // --- 2) 時計適用の依頼（具体的なセットは UI 層に任せる） ---
    if (params.tc.enabled) {
        emit requestApplyTimeControl(params.tc);
    }

    // --- 3) 対局をセットアップ & 開始 ---
    emit willStart(params.opt);

    // configure（司令塔へ一任）
    m_match->configureAndStart(params.opt);

    // --- 4) 初手がエンジン手番なら go を起動（司令塔側が内包していても重複起動ではなく安全） ---
    if (params.autoStartEngineMove) {
        m_match->startInitialEngineMoveIfNeeded();
    }

    // --- 5) 完了通知 ---
    emit started(params.opt);
    qDebug().noquote() << "[GameStartCoordinator] started: mode="
                       << static_cast<int>(params.opt.mode);
}
