#include "gamestartcoordinator.h"
#include "shogiview.h"

#include <QDebug>
#include <QWidget>
#include <QObject>
#include <QVariant>
#include <QMetaType>
#include <QtGlobal>

GameStartCoordinator::GameStartCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_match(d.match)
    , m_clock(d.clock)
    , m_gc(d.gc)
    , m_view(d.view)
{
    // MainWindow との queued 接続でも安全に受け渡せるように登録
    qRegisterMetaType<GameStartCoordinator::TimeControl>("GameStartCoordinator::TimeControl");
    qRegisterMetaType<GameStartCoordinator::Request>("GameStartCoordinator::Request");
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
        // 互換のため、コメント記述に合わせた別名シグナルも併発
        emit applyTimeControlRequested(params.tc);
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

// ========================= 追加：開始前適用 API =========================

void GameStartCoordinator::prepare(const Request& req)
{
    Q_UNUSED(req.mode);
    Q_UNUSED(req.startSfen);
    Q_UNUSED(req.bottomIsP1);
    Q_UNUSED(req.clock);

    // 1) UI/内部状態のプレクリアを要求（必要に応じて MainWindow 側で処理）
    emit requestPreStartCleanup();

    // 2) ダイアログ（QWidget）から時間設定を抽出して依頼
    const TimeControl tc = extractTimeControlFromDialog(req.startDialog);

    // 片方のみ接続されることを前提に、互換のため両方 emit
    emit requestApplyTimeControl(tc);
    emit applyTimeControlRequested(tc);
}

// ========================= 追加：ダイアログ抽出ヘルパ =========================

int GameStartCoordinator::readIntProperty(const QObject* root,
                                          const char* objectName,
                                          const char* prop,
                                          int def)
{
    if (!root) return def;
    if (auto obj = root->findChild<const QObject*>(QLatin1String(objectName))) {
        bool ok = false;
        const int n = obj->property(prop).toInt(&ok);
        if (ok) return n;
    }
    return def;
}

bool GameStartCoordinator::readBoolProperty(const QObject* root,
                                            const char* objectName,
                                            const char* prop,
                                            bool def)
{
    if (!root) return def;
    if (auto obj = root->findChild<const QObject*>(QLatin1String(objectName))) {
        const QVariant v = obj->property(prop);
        if (v.isValid()) return v.toBool();
    }
    return def;
}

GameStartCoordinator::TimeControl
GameStartCoordinator::extractTimeControlFromDialog(const QWidget* dlg)
{
    TimeControl tc;

    // 既定の objectName（あなたのUIに合わせて必要ならここを調整）
    const int p1h  = readIntProperty (dlg, "p1HoursSpin");
    const int p1m  = readIntProperty (dlg, "p1MinutesSpin");
    const int p2h  = readIntProperty (dlg, "p2HoursSpin");
    const int p2m  = readIntProperty (dlg, "p2MinutesSpin");
    const int byo1 = readIntProperty (dlg, "byoyomiSec1");
    const int byo2 = readIntProperty (dlg, "byoyomiSec2");
    const int inc1 = readIntProperty (dlg, "addEachMoveSec1");
    const int inc2 = readIntProperty (dlg, "addEachMoveSec2");
    const bool limited = readBoolProperty(dlg, "limitedTimeCheck");

    auto toMs = [](int h, int m){ return (qMax(0,h)*3600 + qMax(0,m)*60) * 1000LL; };

    tc.p1.baseMs = toMs(p1h, p1m);
    tc.p2.baseMs = toMs(p2h, p2m);

    // byoyomi / increment は排他
    if (byo1 > 0 || byo2 > 0) {
        tc.p1.byoyomiMs   = qMax(0, byo1) * 1000LL;
        tc.p2.byoyomiMs   = qMax(0, byo2) * 1000LL;
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    } else if (inc1 > 0 || inc2 > 0) {
        tc.p1.byoyomiMs   = 0;
        tc.p2.byoyomiMs   = 0;
        tc.p1.incrementMs = qMax(0, inc1) * 1000LL;
        tc.p2.incrementMs = qMax(0, inc2) * 1000LL;
    } else {
        tc.p1.byoyomiMs   = 0;
        tc.p2.byoyomiMs   = 0;
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    }

    // 時計の有効/無制限フラグ
    if (dlg) {
        tc.enabled = limited;
    } else {
        tc.enabled = (tc.p1.baseMs > 0 || tc.p2.baseMs > 0);
    }

    return tc;
}
