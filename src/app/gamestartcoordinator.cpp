#include "gamestartcoordinator.h"
#include "kifurecordlistmodel.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "sfenpositiontracer.h"
#include "startgamedialog.h"
#include "kifudisplay.h"
#include "playernameservice.h"
#include "timecontrolutil.h"

#include <QPointer>
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

    // --- 2) TimeControl を正規化して適用（enabled 補正 / byoyomi 優先で inc を落とす） ---
    TimeControl tc = params.tc; // コピーして正規化

    const bool hasAny =
        (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
        (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
        (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    const bool useByoyomi = (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0);
    if (useByoyomi) {
        // 秒読み指定がある場合はフィッシャー増加は使わない（衝突回避）
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    }

    // 値が一つでも入っているのに enabled=false だったケースを救済
    if (!tc.enabled && hasAny) {
        tc.enabled = true;
    }

    qDebug().noquote()
        << "[GameStartCoordinator] normalized TimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byo=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byo=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // UI（時計）へ適用依頼：enabled か、もしくは何か値がある場合は必ず投げる
    if (tc.enabled || hasAny) {
        emit requestApplyTimeControl(tc);
        // 互換シグナル（どちらか一方だけ接続想定）
        emit applyTimeControlRequested(tc);
    }

    // 司令塔にも直に反映しておく（UIシグナルの非同期順序に影響されないように）
    if (m_match) {
        const bool loseOnTimeout = tc.enabled; // 持ち時間系が有効ならタイムアウト負け扱い
        m_match->setTimeControlConfig(
            useByoyomi,
            tc.p1.byoyomiMs, tc.p2.byoyomiMs,
            tc.p1.incrementMs, tc.p2.incrementMs,
            loseOnTimeout
            );
        m_match->refreshGoTimes();
    }

    // --- 3) 対局をセットアップ & 開始 ---
    emit willStart(params.opt);
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
    // --- 0) 前処理（UIのプレクリア） ---
    emit requestPreStartCleanup();

    // --- 1) ダイアログから時間設定を抽出 ---
    const TimeControl tc = extractTimeControlFromDialog(req.startDialog);

    // 互換のため従来のシグナルも両方飛ばす（他所で受けている場合がある）
    emit requestApplyTimeControl(tc);
    emit applyTimeControlRequested(tc);

    qInfo().noquote()
        << "[GameStartCoordinator] normalized TimeControl: "
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << "  byo=" << tc.p1.byoyomiMs << "  inc=" << tc.p1.incrementMs << " }"
        << " P2{base=" << tc.p2.baseMs << "  byo=" << tc.p2.byoyomiMs << "  inc=" << tc.p2.incrementMs << " }";

    // --- 2) 時計の取得（req.clock が無ければ m_match->clock() を使用） ---
    ShogiClock* clock = req.clock;
    if (!clock && m_match) {
        clock = m_match->clock();
    }
    if (!clock) {
        qWarning() << "[GSC] prepare: no ShogiClock (req.clock is null and m_match has no clock)";
        return;
    }

    // --- 3) 時間制御を適用（applyToClock は初期手番を 1/2 で設定する修正版を想定） ---
    TimeControlUtil::applyToClock(clock, tc, req.startSfen, QString());

    // 念のため SFEN から初期手番を明示（修正版が未適用でも安全）
    const int initialPlayer = (req.startSfen.contains(QLatin1String(" w ")) ? 2 : 1);
    clock->setCurrentPlayer(initialPlayer);

    // --- 4) 司令塔へ配線 → 起動（順序が重要） ---
    if (m_match) {
        m_match->setClock(clock);  // 先に配線
    }
    clock->startClock();           // その後で起動
}

// ===================================================================
// 段階実行 API：MainWindow 側の関数差し替え先
// ===================================================================
void GameStartCoordinator::prepareDataCurrentPosition(const Ctx& c)
{
    // 依存の軽いバリデーション
    if (!c.view || !m_match) {
        qWarning().noquote() << "[GameStartCoordinator] prepareDataCurrentPosition: missing deps:"
                             << "view=" << (c.view != nullptr) << " match=" << (m_match != nullptr);
        return;
    }

    // --- 0) 開始前クリーンアップを UI 層へ依頼（ハイライト/選択/解析UI などの掃除） ---
    // MainWindow では onPreStartCleanupRequested_() に接続済み
    emit requestPreStartCleanup();

    // --- 1) ベースSFENの決定（優先：開始SFEN → 現在SFEN） ---
    // もともとの MainWindow 実装では m_sfenRecord の有無で分岐していましたが、
    // Ctx では start/current の文字列を参照で受ける設計になっているため、
    // 実用上「開始SFENがあればそれを優先、無ければ現在SFEN」を使います。
    QString sfen;
    if (c.startSfenStr && !c.startSfenStr->isEmpty()) {
        sfen = *c.startSfenStr;
    } else if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
        sfen = *c.currentSfenStr;
    }

    // --- 2) SFEN 妥当性チェック（無効なら平手初期へフォールバック） ---
    if (!sfen.isEmpty()) {
        SfenPositionTracer tracer;
        if (tracer.setFromSfen(sfen)) {
            // --- 2a) 妥当な SFEN：盤モデルへ適用し、ハイライトはクリアのまま ---
            if (c.gc && c.gc->board()) {
                c.gc->board()->setSfen(sfen);
            } else if (c.view && c.view->board()) {
                c.view->board()->setSfen(sfen);
            }
            // 現在SFENの参照が来ていれば、最新の適用値を反映しておく
            if (c.currentSfenStr) *c.currentSfenStr = sfen;

            qDebug().noquote() << "[GameStartCoordinator] resume-from-current: SFEN applied.";
        } else {
            qWarning().noquote() << "[GameStartCoordinator] Invalid SFEN. Fallback to even (flat) start.";
            c.view->initializeToFlatStartingPosition();
            // start/current のどちらかが参照で来ていれば、妥当な既定値へ寄せておく
            if (c.startSfenStr && c.startSfenStr->isEmpty())
                *c.startSfenStr = QStringLiteral("startpos");
            if (c.currentSfenStr)
                *c.currentSfenStr = QStringLiteral("startpos");
        }
    } else {
        // --- 2b) SFEN が空：平手初期へフォールバック ---
        c.view->initializeToFlatStartingPosition();
        if (c.startSfenStr && c.startSfenStr->isEmpty())
            *c.startSfenStr = QStringLiteral("startpos");
        if (c.currentSfenStr)
            *c.currentSfenStr = QStringLiteral("startpos");
        qDebug().noquote() << "[GameStartCoordinator] no SFEN provided. Initialized to flat start.";
    }

    // --- 3) 直前の終局状態を必ずクリア（元実装の resetGameFlags() 相当のうち「終局フラグ掃除」） ---
    // 司令塔が保持する GameOverState をリセット（MainWindow::resetGameFlags の一部移行）
    m_match->clearGameOverState();

    // --- 4) ハイライトを確実に空へ（MainWindow::prepareDataCurrentPosition 内の clear→復元のうち clear 部分）
    // 復元（選択手番に応じた from/to ハイライト）は、GUI層（行選択変更ハンドラ）で行う設計に寄せます。
    c.view->removeHighlightAllData();

    // （任意）ログ
    qDebug().noquote() << "[GameStartCoordinator] prepareDataCurrentPosition: done.";
}

// 初期局面（平手／手合割）で開始する場合の準備を行う。
void GameStartCoordinator::prepareInitialPosition(const Ctx& c)
{
    // 1) 開始局面番号を取得（0=現在局面, 1..N=手合割）。0 の場合でも安全側で平手にフォールバック。
    int startingPosNumber = 1; // 1=平手を既定
    if (c.startDlg) {
        if (auto dlg = qobject_cast<StartGameDialog*>(c.startDlg)) {
            startingPosNumber = dlg->startingPositionNumber();
        } else {
            // 互換：property で持っていれば使う
            bool ok = false;
            const int v = c.startDlg->property("startingPositionNumber").toInt(&ok);
            if (ok) startingPosNumber = v;
        }
    }
    if (startingPosNumber <= 0) startingPosNumber = 1;

    // 2) 手合割 → 文字列テーブル（MainWindow 実装と同値を保持）
    static const QString kStartingPositionStr[14] = {
        //  1: 平手（元コードの配列は [0]=平手 でしたが、ここでは可読のため 1 起点の説明）
        QStringLiteral("startpos"),
        //  2: 香落ち
        QStringLiteral("sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  3: 右香落ち
        QStringLiteral("sfen 1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  4: 角落ち
        QStringLiteral("sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  5: 飛車落ち
        QStringLiteral("sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  6: 飛香落ち
        QStringLiteral("sfen lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  7: 二枚落ち
        QStringLiteral("sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  8: 三枚落ち
        QStringLiteral("sfen lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        //  9: 四枚落ち
        QStringLiteral("sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        // 10: 五枚落ち
        QStringLiteral("sfen 2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        // 11: 左五枚落ち
        QStringLiteral("sfen 1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        // 12: 六枚落ち
        QStringLiteral("sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        // 13: 八枚落ち
        QStringLiteral("sfen 3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
        // 14: 十枚落ち
        QStringLiteral("sfen 4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")
    };

    // 範囲ガード（1..14 を想定）
    const int idx = qBound(1, startingPosNumber, 14) - 1;
    const QString startPositionStr = kStartingPositionStr[idx];

    // 3) "startpos" / "sfen ..." → 純 SFEN へ正規化（MainWindow::parseStartPositionToSfen の移植）
    auto toPureSfen = [](QString s) -> QString {
        if (s == QLatin1String("startpos")) {
            // 平手の完全 SFEN へ正規化
            return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        }
        if (s.startsWith(QLatin1String("sfen "))) {
            s.remove(0, 5); // "sfen " を除去
            return s;
        }
        // それ以外（既に純 SFEN など）の場合は、そのまま返す
        return s;
    };

    const QString sfen = toPureSfen(startPositionStr);

    // 4) 参照を通して MainWindow 側の文字列を更新
    if (c.startSfenStr)   *c.startSfenStr   = sfen;
    if (c.currentSfenStr) *c.currentSfenStr = sfen; // 同期しておくと後段の利用が楽

    // 5) 棋譜欄に「=== 開始局面 ===」ヘッダを追加（存在時のみ）
    if (c.kifuModel) {
        c.kifuModel->appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                                QStringLiteral("（１手 / 合計）")));
    }

    // 6) SFEN 履歴に開始 SFEN を積む（存在時のみ）
    if (c.sfenRecord) {
        c.sfenRecord->append(sfen);
    }

    // 7) 見た目のノイズを避けるため、開幕時のハイライトはクリアしておく（存在時のみ）
    if (c.view) {
        c.view->removeHighlightAllData();
    }

    qDebug().noquote() << "[GameStartCoordinator] prepareInitialPosition: sfen=" << sfen;
}

void GameStartCoordinator::setTimerAndStart(const Ctx& c)
{
    // 依存チェック
    if (!c.clock) {
        qWarning().noquote() << "[GameStartCoordinator] setTimerAndStart: clock is null.";
        return;
    }

    // --- 1) ダイアログから各種設定値を取得 ---
    int basicTimeHour1 = 0, basicTimeMinutes1 = 0;
    int basicTimeHour2 = 0, basicTimeMinutes2 = 0;
    int byoyomi1 = 0, byoyomi2 = 0;
    int binc = 0, winc = 0;
    bool isLoseOnTimeout = true;

    if (c.startDlg) {
        if (auto dlg = qobject_cast<StartGameDialog*>(c.startDlg)) {
            basicTimeHour1    = dlg->basicTimeHour1();
            basicTimeMinutes1 = dlg->basicTimeMinutes1();
            basicTimeHour2    = dlg->basicTimeHour2();
            basicTimeMinutes2 = dlg->basicTimeMinutes2();
            byoyomi1          = dlg->byoyomiSec1();
            byoyomi2          = dlg->byoyomiSec2();
            binc              = dlg->addEachMoveSec1();
            winc              = dlg->addEachMoveSec2();
            isLoseOnTimeout   = dlg->isLoseOnTimeout();
        } else {
            auto getPropInt = [&](const char* name)->int {
                bool ok=false; int v = c.startDlg->property(name).toInt(&ok); return ok ? v : 0;
            };
            auto getPropBool = [&](const char* name)->bool {
                if (!c.startDlg->property(name).isValid()) return true;
                return c.startDlg->property(name).toBool();
            };
            basicTimeHour1    = getPropInt("basicTimeHour1");
            basicTimeMinutes1 = getPropInt("basicTimeMinutes1");
            basicTimeHour2    = getPropInt("basicTimeHour2");
            basicTimeMinutes2 = getPropInt("basicTimeMinutes2");
            byoyomi1          = getPropInt("byoyomiSec1");
            byoyomi2          = getPropInt("byoyomiSec2");
            binc              = getPropInt("addEachMoveSec1");
            winc              = getPropInt("addEachMoveSec2");
            isLoseOnTimeout   = getPropBool("isLoseOnTimeout");
        }
    }

    // --- 2) 秒へ統一（内部は秒でセット、UI表示は Clock 側でms等に変換） ---
    const int remainingTime1 = basicTimeHour1 * 3600 + basicTimeMinutes1 * 60;
    const int remainingTime2 = basicTimeHour2 * 3600 + basicTimeMinutes2 * 60;

    const bool hasTimeLimit =
        (basicTimeHour1*3600 + basicTimeMinutes1*60) > 0 ||
        (basicTimeHour2*3600 + basicTimeMinutes2*60) > 0 ||
        byoyomi1 > 0 || byoyomi2 > 0 || binc > 0 || winc > 0;

    // --- 3) 時計へ反映 ---
    c.clock->setLoseOnTimeout(isLoseOnTimeout);
    c.clock->setPlayerTimes(remainingTime1, remainingTime2,
                            byoyomi1, byoyomi2,
                            binc, winc,
                            hasTimeLimit);

    // --- 4) 初期msを出力（必要な場合のみ） ---
    if (c.initialTimeP1MsOut) *c.initialTimeP1MsOut = c.clock->getPlayer1TimeIntMs();
    if (c.initialTimeP2MsOut) *c.initialTimeP2MsOut = c.clock->getPlayer2TimeIntMs();

    // --- 5) 表示更新＆起動制御 ---
    emit requestUpdateTurnDisplay();
    c.clock->updateClock();
    if (!c.isReplayMode) {
        c.clock->startClock();
    }

    // ★★ 追加：司令塔に TimeControl（秒読み/増加/負け扱い）を設定
    if (m_match) {
        const bool useByoyomi = (byoyomi1 > 0 || byoyomi2 > 0);
        m_match->setTimeControlConfig(
            useByoyomi,
            byoyomi1 * 1000, byoyomi2 * 1000,
            binc     * 1000, winc     * 1000,
            isLoseOnTimeout
            );
    }

    // --- 6) MatchCoordinator へ時計を配線（初期表示も即時反映） ---
    if (m_match && c.clock) {
        m_match->setClock(c.clock);
        m_match->pokeTimeUpdateNow();
    }

    qDebug().noquote() << "[GameStartCoordinator] setTimerAndStart: "
                       << "t1=" << remainingTime1 << "s"
                       << " t2=" << remainingTime2 << "s"
                       << " byo=" << byoyomi1 << "/" << byoyomi2
                       << " inc=" << binc << "/" << winc
                       << " loseOnTimeout=" << isLoseOnTimeout
                       << " replay=" << c.isReplayMode;
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

PlayMode GameStartCoordinator::determinePlayMode(const int initPositionNumber,
                                                 const bool isPlayer1Human,
                                                 const bool isPlayer2Human) const
{
    // 平手（=1）と駒落ち（!=1）で分岐は同じ構造。元コードを忠実移植。
    const bool isEven = (initPositionNumber == 1);

    if (isEven) {
        if (isPlayer1Human && isPlayer2Human)  return HumanVsHuman;
        if (isPlayer1Human && !isPlayer2Human) return EvenHumanVsEngine;
        if (!isPlayer1Human && isPlayer2Human) return EvenEngineVsHuman;
        if (!isPlayer1Human && !isPlayer2Human) return EvenEngineVsEngine;
    } else {
        if (isPlayer1Human && isPlayer2Human)  return HumanVsHuman;
        if (isPlayer1Human && !isPlayer2Human) return HandicapHumanVsEngine;
        if (!isPlayer1Human && isPlayer2Human) return HandicapEngineVsHuman;
        if (!isPlayer1Human && !isPlayer2Human) return HandicapEngineVsEngine;
    }

    return PlayModeError;
}

PlayMode GameStartCoordinator::setPlayMode(const Ctx& c) const
{
    // StartGameDialog から値を取得（MainWindow::setPlayMode の移管）
    int  initPositionNumber = 1;
    bool isHuman1 = false, isHuman2 = false;
    bool isEngine1 = false, isEngine2 = false;

    if (c.startDlg) {
        if (auto dlg = qobject_cast<StartGameDialog*>(c.startDlg)) {
            initPositionNumber = dlg->startingPositionNumber();
            isHuman1           = dlg->isHuman1();
            isHuman2           = dlg->isHuman2();
            isEngine1          = dlg->isEngine1();
            isEngine2          = dlg->isEngine2();
        } else {
            // 互換: property 経由のフェールセーフ
            bool ok=false;
            const QVariant pn = c.startDlg->property("startingPositionNumber");
            initPositionNumber = pn.isValid() ? pn.toInt(&ok) : 1;
            if (!ok) initPositionNumber = 1;

            isHuman1  = c.startDlg->property("isHuman1").toBool();
            isHuman2  = c.startDlg->property("isHuman2").toBool();
            isEngine1 = c.startDlg->property("isEngine1").toBool();
            isEngine2 = c.startDlg->property("isEngine2").toBool();
        }
    }

    // 「Human と Engine の排他」を元コードと同じくここで整形
    const bool p1Human = (isHuman1  && !isEngine1);
    const bool p2Human = (isHuman2  && !isEngine2);

    const PlayMode mode = determinePlayMode(initPositionNumber, p1Human, p2Human);

    if (mode == PlayModeError) {
        // 元のメッセージに近い文面で、UI へ委譲（MainWindow::displayErrorMessage 相当）
        emit requestDisplayError(tr("An error occurred in GameStartCoordinator::determinePlayMode. "
                                    "There is a mistake in the game options."));
        qWarning().noquote() << "[GameStartCoordinator] setPlayMode: PlayModeError"
                             << "initPos=" << initPositionNumber
                             << " p1Human=" << p1Human << " p2Human=" << p2Human
                             << " (raw human/engine: "
                             << isHuman1 << "/" << isEngine1 << ", "
                             << isHuman2 << "/" << isEngine2 << ")";
    }

    return mode;
}

QChar GameStartCoordinator::turnFromSfen_(const QString& sfen)
{
    const QString s = sfen.trimmed();
    if (s.isEmpty()) return QChar();

    // 典型: "<board> b - 1" / "<board> w - 1"
    const QStringList toks = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.size() >= 2) {
        const QString t = toks.at(1).toLower();
        if (t == QLatin1String("b")) return QLatin1Char('b');
        if (t == QLatin1String("w")) return QLatin1Char('w');
    }
    // "startpos b 1" 形式の保険
    if (s.contains(QLatin1String(" b "))) return QLatin1Char('b');
    if (s.contains(QLatin1String(" w "))) return QLatin1Char('w');
    return QChar();
}

PlayMode GameStartCoordinator::determinePlayModeAlignedWithTurn(
    int initPositionNumber, bool isPlayer1Human, bool isPlayer2Human, const QString& startSfen)
{
    const QChar turn = turnFromSfen_(startSfen); // 'b' / 'w' / null
    const bool isEven = (initPositionNumber == 1);
    const bool hvh = (isPlayer1Human && isPlayer2Human);
    const bool eve = (!isPlayer1Human && !isPlayer2Human);
    const bool oneVsEngine = !hvh && !eve;

    // 「turn はどちらの座席が指すか」を表すだけなので、
    // 実際にその座席(P1 or P2)が Human か Engine かで HvE/EvH を決定する。
    if (isEven) {
        if (hvh) return HumanVsHuman;
        if (eve) return EvenEngineVsEngine;
        if (oneVsEngine) {
            if (turn == QLatin1Char('b')) {
                // 先手＝P1 の座席
                return isPlayer1Human ? EvenHumanVsEngine : EvenEngineVsHuman;
            }
            if (turn == QLatin1Char('w')) {
                // 後手＝P2 の座席
                return isPlayer2Human ? EvenHumanVsEngine : EvenEngineVsHuman;
            }
            // turn が取れない場合は座席だけで決める
            return (isPlayer1Human && !isPlayer2Human) ? EvenHumanVsEngine : EvenEngineVsHuman;
        }
        return NotStarted;
    } else {
        if (hvh) return HumanVsHuman;
        if (eve) return HandicapEngineVsEngine;
        if (oneVsEngine) {
            if (turn == QLatin1Char('b')) {
                return isPlayer1Human ? HandicapHumanVsEngine : HandicapEngineVsHuman;
            }
            if (turn == QLatin1Char('w')) {
                return isPlayer2Human ? HandicapHumanVsEngine : HandicapEngineVsHuman;
            }
            return (isPlayer1Human && !isPlayer2Human) ? HandicapHumanVsEngine : HandicapEngineVsHuman;
        }
        return NotStarted;
    }
}

void GameStartCoordinator::initializeGame(const Ctx& c)
{
    // --- 1) ダイアログ生成＆受付 ---
    StartGameDialog* dlg = new StartGameDialog;
    if (!dlg) return;

    if (dlg->exec() != QDialog::Accepted) {
        delete dlg;
        return;
    }

    // --- 2) ダイアログから必要情報を先に取得（この後の準備で使う） ---
    const int  initPosNo = dlg->startingPositionNumber(); // 平手=1, 現局面=0 など
    const bool p1Human   = dlg->isHuman1();
    const bool p2Human   = dlg->isHuman2();

    // --- 3) 開始SFENの決定（既存ロジック踏襲） ---
    const int startingPosNumber = initPosNo;

    QString startSfen;
    if (c.startSfenStr && !c.startSfenStr->isEmpty()) {
        startSfen = *(c.startSfenStr);
    }
    if (startingPosNumber == 0) {
        // 現在局面から開始
        Ctx c2 = c;
        c2.startDlg = dlg;
        prepareDataCurrentPosition(c2);

        if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
            startSfen = *(c.currentSfenStr);
        } else if (startSfen.isEmpty()) {
            startSfen = QStringLiteral("startpos");
        }
    } else if (startSfen.isEmpty()) {
        // 平手/駒落ちプリセット
        static const QVector<QString> presets = {
            QString(), // 0: 未使用（現在局面）
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"), // 1: 平手
            QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"), // 2: 香落ち
            QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"), // 3: 右香落ち
            QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),   // 4: 角落ち
            QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),   // 5: 飛車落ち
            QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),   // 6: 飛香落ち
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),     // 7: 二枚落ち
            QStringLiteral("lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),     // 8: 三枚落ち
            QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),     // 9: 四枚落ち
            QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),      // 10: 五枚落ち
            QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),      // 11: 左五枚落ち
            QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),       // 12: 六枚落ち
            QStringLiteral("3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),         // 13: 八枚落ち
            QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")            // 14: 十枚落ち
        };
        int idx = startingPosNumber;
        if (idx < 1) idx = 1;
        if (idx >= presets.size()) idx = presets.size() - 1;
        startSfen = presets.at(idx);

        // 駒落ち開始の内部状態整備（既存メソッド）
        Ctx c2 = c; c2.startDlg = dlg;
        prepareInitialPosition(c2);
    }

    // --- 4) PlayMode を SFEN手番と整合させて最終決定 ---
    PlayMode mode = determinePlayModeAlignedWithTurn(initPosNo, p1Human, p2Human, startSfen);
    qInfo() << "[GameStart] Final PlayMode =" << mode << "  startSfen=" << startSfen;

    // --- 5) StartOptions 構築（司令塔依存） ---
    if (!m_match) {
        delete dlg;
        return;
    }
    MatchCoordinator::StartOptions opt =
        m_match->buildStartOptions(mode, startSfen, c.sfenRecord, dlg);

    // 人を手前に（必要時のみ反転）
    m_match->ensureHumanAtBottomIfApplicable(dlg, c.bottomIsP1);

    // --- 6) TimeControl を ms で構築（ダイアログ値 → ミリ秒） ---
    auto hms_to_ms = [](int h, int m)->qint64 {
        const qint64 hh = qMax(0, h);
        const qint64 mm = qMax(0, m);
        return (hh*3600 + mm*60) * 1000;
    };
    auto sec_to_ms = [](int s)->qint64 {
        return qMax(0, s) * 1000ll;
    };

    const int h1   = dlg->basicTimeHour1();
    const int m1   = dlg->basicTimeMinutes1();
    const int h2   = dlg->basicTimeHour2();
    const int m2   = dlg->basicTimeMinutes2();
    const int byo1 = dlg->byoyomiSec1();
    const int byo2 = dlg->byoyomiSec2();
    const int inc1 = dlg->addEachMoveSec1();
    const int inc2 = dlg->addEachMoveSec2();

    TimeControl tc;
    tc.p1.baseMs      = hms_to_ms(h1, m1);
    tc.p2.baseMs      = hms_to_ms(h2, m2);
    tc.p1.byoyomiMs   = sec_to_ms(byo1);
    tc.p2.byoyomiMs   = sec_to_ms(byo2);
    tc.p1.incrementMs = sec_to_ms(inc1);
    tc.p2.incrementMs = sec_to_ms(inc2);

    // byoyomi 優先（併用指定なら inc は 0 扱い）
    const bool useByoyomi = (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0);
    if (useByoyomi) {
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    }

    // どれか値が入っていれば enabled = true
    const bool hasAny =
        (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
        (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
        (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);
    tc.enabled = hasAny;

    qDebug().noquote()
        << "[GSC] initializeGame tc:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byo=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byo=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // --- 7) 時計の準備と配線・起動は prepare(...) に委譲（★順序をここに移動） ---
    Request req;
    req.startDialog = dlg;
    req.startSfen   = startSfen;                            // ★ 手番確定に使用
    req.clock       = c.clock ? c.clock : m_match->clock(); // ★ ここで必ず渡す（null なら prepare 内で警告して return）

    prepare(req); // requestPreStartCleanup / 時間適用シグナル発火 / setClock→startClock（修正版の想定）

    // --- 8) 対局開始（時計設定 + 初手 go 設定） ---
    StartParams params;
    params.opt  = opt;
    params.tc   = tc;                 // 司令塔側の go 計算にも使用
    params.autoStartEngineMove = true;

    start(params);

    // --- 9) ★ここが今回の肝：時計を確実に起動（＆必要なら初手go） ---
    // prepare()/start() 内で未起動だった場合の保険として、司令塔ユーティリティを呼ぶ
    if (m_match) {
        // 必要なら clock を明示的に再セット（配線の取りこぼし対策）
        if (c.clock && m_match->clock() != c.clock) {
            m_match->setClock(c.clock);
        }
        // 1回で十分：内部で多重起動/多重goを避ける実装（UniqueConnection/フラグ）を想定
        m_match->startMatchTimingAndMaybeInitialGo();
    }

    delete dlg;
}

// ★追加：司令塔（MatchCoordinator）の生成と初期配線を一括で実施
MatchCoordinator* GameStartCoordinator::createAndWireMatch(const MatchCoordinator::Deps& deps,
                                                           QObject* parentForMatch)
{
    // 既存があれば破棄（親を MainWindow にぶら下げ直すため）
    if (m_match) {
        m_match->disconnect(this);
        if (m_match->parent() == parentForMatch) {
            delete m_match;
        } else {
            m_match->setParent(nullptr);
            delete m_match;
        }
        m_match = nullptr;
    }

    // 生成
    m_match = new MatchCoordinator(deps, parentForMatch);

    // --- 司令塔→Coordinator へ受け、Coordinator から re-emit ---
    // timeUpdated(p1ms, p2ms, p1turn, urgencyMs)
    QObject::connect(
        m_match,
        static_cast<void (MatchCoordinator::*)(qint64,qint64,bool,qint64)>(&MatchCoordinator::timeUpdated),
        this,
        static_cast<void (GameStartCoordinator::*)(qint64,qint64,bool,qint64)>(&GameStartCoordinator::timeUpdated),
        Qt::UniqueConnection
        );

    // requestAppendGameOverMove(const GameEndInfo&)
    QObject::connect(
        m_match, &MatchCoordinator::requestAppendGameOverMove,
        this,    &GameStartCoordinator::requestAppendGameOverMove,
        Qt::UniqueConnection
        );

    // boardFlipped(bool)
    QObject::connect(
        m_match, &MatchCoordinator::boardFlipped,
        this,    &GameStartCoordinator::boardFlipped,
        Qt::UniqueConnection
        );

    // gameOverStateChanged(const GameOverState&)
    QObject::connect(
        m_match, &MatchCoordinator::gameOverStateChanged,
        this,    &GameStartCoordinator::gameOverStateChanged,
        Qt::UniqueConnection
        );

    // gameEnded(const GameEndInfo&)
    QObject::connect(
        m_match,
        static_cast<void (MatchCoordinator::*)(const MatchCoordinator::GameEndInfo&)>(&MatchCoordinator::gameEnded),
        this,
        static_cast<void (GameStartCoordinator::*)(const MatchCoordinator::GameEndInfo&)>(&GameStartCoordinator::matchGameEnded),
        Qt::UniqueConnection
        );

    // USI ポインタの初期注入（nullptr 可）
    m_match->updateUsiPtrs(deps.usi1, deps.usi2);

    // デバッグ：シグナル存在確認（既存ログと同等）
    qDebug() << "[DBG] signal index:"
             << m_match->metaObject()->indexOfSignal("timeUpdated(long long,long long,bool,long long)");

    return m_match;
}

void GameStartCoordinator::applyResumePositionIfAny(ShogiGameController* gc,
                                                    ShogiView* view,
                                                    const QString& resumeSfen)
{
    if (!gc || resumeSfen.isEmpty()) return;

    if (auto* b = gc->board()) {
        b->setSfen(resumeSfen);
        if (view) view->applyBoardAndRender(b);
    }
}

void GameStartCoordinator::applyPlayersNamesForMode(ShogiView* view,
                                                    PlayMode mode,
                                                    const QString& human1,
                                                    const QString& human2,
                                                    const QString& engine1,
                                                    const QString& engine2) const
{
    if (!view) return;
    const PlayerNameMapping names =
        PlayerNameService::computePlayers(mode, human1, human2, engine1, engine2);
    view->setBlackPlayerName(names.p1);
    view->setWhitePlayerName(names.p2);
}

// src/app/gamestartcoordinator.cpp

// ダイアログから TimeControl を組み立てるユーティリティ。
// ※ StartGameDialog が無い場合でも QObject::property でフォールバックします。
GameStartCoordinator::TimeControl
GameStartCoordinator::buildTimeControlFromDialog_(QDialog* startDlg) const
{
    TimeControl tc; // すべて 0 で初期化される前提の構造体

    int h1=0, m1=0, h2=0, m2=0;
    int byo1=0, byo2=0;
    int inc1=0, inc2=0;

    auto propInt = [&](const char* name)->int {
        if (!startDlg) return 0;
        bool ok=false; const int v = startDlg->property(name).toInt(&ok);
        return ok ? v : 0;
    };

    // StartGameDialog の型が使えるなら直接 getter を呼ぶ
    if (auto dlg = qobject_cast<StartGameDialog*>(startDlg)) {
        h1   = dlg->basicTimeHour1();
        m1   = dlg->basicTimeMinutes1();
        h2   = dlg->basicTimeHour2();
        m2   = dlg->basicTimeMinutes2();
        byo1 = dlg->byoyomiSec1();
        byo2 = dlg->byoyomiSec2();
        inc1 = dlg->addEachMoveSec1();
        inc2 = dlg->addEachMoveSec2();
    } else {
        // フォールバック：プロパティ名で取得
        h1   = propInt("basicTimeHour1");
        m1   = propInt("basicTimeMinutes1");
        h2   = propInt("basicTimeHour2");
        m2   = propInt("basicTimeMinutes2");
        byo1 = propInt("byoyomiSec1");
        byo2 = propInt("byoyomiSec2");
        inc1 = propInt("addEachMoveSec1");
        inc2 = propInt("addEachMoveSec2");
    }

    // (時間, 分) → ms
    const qint64 base1Ms = qMax<qint64>(0, (static_cast<qint64>(h1)*3600 + m1*60) * 1000);
    const qint64 base2Ms = qMax<qint64>(0, (static_cast<qint64>(h2)*3600 + m2*60) * 1000);

    const qint64 byo1Ms  = qMax<qint64>(0, static_cast<qint64>(byo1) * 1000);
    const qint64 byo2Ms  = qMax<qint64>(0, static_cast<qint64>(byo2) * 1000);

    const qint64 inc1Ms  = qMax<qint64>(0, static_cast<qint64>(inc1) * 1000);
    const qint64 inc2Ms  = qMax<qint64>(0, static_cast<qint64>(inc2) * 1000);

    // byoyomi と increment は排他運用（byoyomi 優先）。両方入っていたら inc は無視。
    const bool useByoyomi = (byo1Ms > 0) || (byo2Ms > 0);

    tc.p1.baseMs      = base1Ms;
    tc.p2.baseMs      = base2Ms;
    tc.p1.byoyomiMs   = useByoyomi ? byo1Ms : 0;
    tc.p2.byoyomiMs   = useByoyomi ? byo2Ms : 0;
    tc.p1.incrementMs = useByoyomi ? 0      : inc1Ms;
    tc.p2.incrementMs = useByoyomi ? 0      : inc2Ms;

    // どれか一つでも値が入っていれば enabled = true
    tc.enabled = (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
                 (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
                 (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    qDebug().noquote()
        << "[GSC] buildTimeControlFromDialog_:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byo=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byo=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    return tc;
}
