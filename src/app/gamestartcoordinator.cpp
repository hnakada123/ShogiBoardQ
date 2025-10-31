#include "gamestartcoordinator.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "sfenpositiontracer.h"

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
    // ★ StartOptions::mode は PlayMode。NotStarted を弾く
    if (p.opt.mode == PlayMode::NotStarted) {
        whyNot = QStringLiteral("対局モードが NotStarted のままです。");
        return false;
    }
    return true;
}

// ===================================================================
// メイン API: StartOptions を受け取り対局を開始
// ===================================================================
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
        // 互換シグナル（どちらか一方だけ接続想定）
        emit applyTimeControlRequested(params.tc);
    }

    // --- 3) 対局をセットアップ & 開始 ---
    emit willStart(params.opt);

    // 司令塔へ一任
    m_match->configureAndStart(params.opt);

    // --- 4) 初手がエンジン手番なら go を起動 ---
    if (params.autoStartEngineMove) {
        m_match->startInitialEngineMoveIfNeeded();
    }

    // --- 5) 完了通知 ---
    emit started(params.opt);
    qDebug().noquote() << "[GameStartCoordinator] started: mode="
                       << static_cast<int>(params.opt.mode);
}

// ===================================================================
// 追加：開始前適用 API（軽量リクエスト）
// ===================================================================
void GameStartCoordinator::prepare(const Request& req)
{
    Q_UNUSED(req.mode);
    Q_UNUSED(req.startSfen);
    Q_UNUSED(req.bottomIsP1);
    Q_UNUSED(req.clock);

    // 1) UI/内部状態のプレクリアを要求
    emit requestPreStartCleanup();

    // 2) ダイアログ（QWidget）から時間設定を抽出して依頼
    const TimeControl tc = extractTimeControlFromDialog(req.startDialog);

    // 片方のみ接続されることを前提に、互換のため両方 emit
    emit requestApplyTimeControl(tc);
    emit applyTimeControlRequested(tc);
}

// ===================================================================
// 段階実行 API：MainWindow 側の関数差し替え先
// ===================================================================
void GameStartCoordinator::prepareDataCurrentPosition(const Ctx& c)
{
    Q_UNUSED(c);
    // ここでは UI 側でのプレクリア（シグナル）を優先し、具体的な同期は必要最小限に留める。
    // 必要に応じて、ビュー側の「現在選択手番での局面反映」等を呼ぶ。
    // 例）if (c.view) { /* c.view->applySfenAtCurrentPlyIfAllowed(); */ }
}

void GameStartCoordinator::prepareInitialPosition(const Ctx& c)
{
    if (!c.view) return;

    // 優先順：開始SFEN → 現在SFEN
    const QString sfen =
        (c.startSfenStr   && !c.startSfenStr->isEmpty())   ? *c.startSfenStr :
            (c.currentSfenStr && !c.currentSfenStr->isEmpty()) ? *c.currentSfenStr :
            QString();

    if (!sfen.isEmpty()) {
        // ★ ここで妥当性チェックのみ。適用は既存フローに委譲する。
        SfenPositionTracer tracer;
        if (tracer.setFromSfen(sfen)) {
            qDebug().noquote() << "[GameStartCoordinator] SFEN validated.";
            return; // 有効ならここで終了（実適用は後段フローに任せる）
        } else {
            qWarning().noquote() << "[GameStartCoordinator] Invalid SFEN. Fallback to flat start.";
        }
    }

    // ★ SFENが空 or 無効 → 平手初期を盤に用意（このAPIは既存）
    c.view->initializeToFlatStartingPosition();
}


void GameStartCoordinator::initializeGame(const Ctx& c)
{
    Q_UNUSED(c);
    // 盤とコントローラの整合、先後・手番の初期表示などを司令塔外で済ませている場合は NO-OP。
    // 必要に応じて、ここに c.gc ↔ c.view の同期 API を追加してください。
    // 例）if (c.gc && c.view) { c.gc->syncBoardFromView(c.view); }
}

void GameStartCoordinator::setTimerAndStart(const Ctx& c)
{
    Q_UNUSED(c);
    if (!m_match) return;

    // 時計の具体的適用は UI 層（MainWindow の onApplyTimeControlRequested_）で実施済み想定。
    // ここでは司令塔にタイマー起動と初手 go の起動可否判断を一任。
    m_match->startMatchTimingAndMaybeInitialGo();
}

// ===================================================================
// プレイモード判定（StartOptions を司令塔で評価）
// ===================================================================
int GameStartCoordinator::determinePlayMode(const MatchCoordinator::StartOptions& opt) const
{
    return static_cast<int>(opt.mode);
}

// ===================================================================
// ダイアログ抽出ヘルパ
// ===================================================================
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
        tc.p1.byoyomiMs = qMax(0, inc1) * 1000LL;
        tc.p2.byoyomiMs = qMax(0, inc2) * 1000LL;
        tc.p1.byoyomiMs = 0;
        tc.p2.byoyomiMs = 0;
    } else {
        tc.p1.byoyomiMs = 0;
        tc.p2.byoyomiMs = 0;
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
