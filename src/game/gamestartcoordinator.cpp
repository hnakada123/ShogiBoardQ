/// @file gamestartcoordinator.cpp
/// @brief 対局開始コーディネータクラスの実装

#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "logcategories.h"
#include "kifurecordlistmodel.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "startgamedialog.h"
#include "kifudisplay.h"
#include "timecontrolutil.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QWidget>
#include <QMetaType>

// ============================================================
// 初期化
// ============================================================

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

bool GameStartCoordinator::validate(const StartParams& p, QString& whyNot) const
{
    if (!m_match) {
        whyNot = QStringLiteral("MatchCoordinator が未設定です。");
        return false;
    }
    if (p.opt.mode == PlayMode::NotStarted) {
        whyNot = QStringLiteral("対局モードが PlayMode::NotStarted のままです。");
        return false;
    }
    return true;
}

// ============================================================
// メインAPI: StartOptionsを受け取り対局を開始
// ============================================================

void GameStartCoordinator::start(const StartParams& params)
{
    QString reason;
    if (!validate(params, reason)) {
        qCWarning(lcGame).noquote() << "start failed:" << reason;
        return;
    }

    // prepare()で既にクリーンアップ済みなので、ここでは呼ばない
    // （重複呼び出しにより、playerNamesResolvedで設定された名前が消えてしまう問題を修正）

    // --- 2) TimeControl を正規化して適用（enabled 補正 / byoyomi 優先で inc を落とす） ---
    TimeControl tc = params.tc;

    const bool hasAny =
        (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
        (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
        (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    // 秒読み指定がある場合はフィッシャー増加は使わない（衝突回避）
    const bool useByoyomi = (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0);
    if (useByoyomi) {
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    }

    // 値が一つでも入っているのに enabled=false だったケースを救済
    if (!tc.enabled && hasAny) {
        tc.enabled = true;
    }

    qCDebug(lcGame).noquote()
        << "normalized TimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byo=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byo=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // UI（時計）へ適用依頼
    if (tc.enabled || hasAny) {
        emit requestApplyTimeControl(tc);
    }

    // 司令塔にも直に反映（UIシグナルの非同期順序に影響されないように）
    if (m_match) {
        const bool loseOnTimeout = tc.enabled;
        m_match->setTimeControlConfig(
            useByoyomi,
            static_cast<int>(tc.p1.byoyomiMs), static_cast<int>(tc.p2.byoyomiMs),
            static_cast<int>(tc.p1.incrementMs), static_cast<int>(tc.p2.incrementMs),
            loseOnTimeout
            );
        m_match->refreshGoTimes();
    }

    // --- 3) 対局をセットアップ & 開始 ---
    m_match->configureAndStart(params.opt);

    // --- 4) 初手がエンジン手番なら go を起動 ---
    if (params.autoStartEngineMove) {
        m_match->startInitialEngineMoveIfNeeded();
    }

    // --- 5) 完了通知 ---
    emit started(params.opt);
    qCDebug(lcGame).noquote() << "started: mode="
                       << static_cast<int>(params.opt.mode);
}

// ============================================================
// 開始前適用API（軽量リクエスト）
// ============================================================

void GameStartCoordinator::prepare(const Request& req)
{
    // --- 0) 前処理（UIのプレクリア） ---
    // skipCleanup が true の場合はスキップ（既に prepareDataCurrentPosition 等で呼び出し済み）
    if (!req.skipCleanup) {
        emit requestPreStartCleanup();
    }

    // --- 1) ダイアログから時間設定を抽出 ---
    const TimeControl tc = GameStartOptionsBuilder::extractTimeControl(req.startDialog);

    emit requestApplyTimeControl(tc);

    qCDebug(lcGame).noquote()
        << "normalized TimeControl: "
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << "  byo=" << tc.p1.byoyomiMs << "  inc=" << tc.p1.incrementMs << " }"
        << " P2{base=" << tc.p2.baseMs << "  byo=" << tc.p2.byoyomiMs << "  inc=" << tc.p2.incrementMs << " }";

    // --- 2) 時計の取得（req.clock が無ければ m_match->clock() を使用） ---
    ShogiClock* clock = req.clock;
    if (!clock && m_match) {
        clock = m_match->clock();
    }
    if (!clock) {
        qCWarning(lcGame) << "prepare: no ShogiClock (req.clock is null and m_match has no clock)";
        return;
    }

    // --- 3) 時間制御を適用 ---
    TimeControlUtil::applyToClock(clock, tc, req.startSfen, QString());

    // SFENから初期手番を明示
    const int initialPlayer = (req.startSfen.contains(QLatin1String(" w ")) ? 2 : 1);
    clock->setCurrentPlayer(initialPlayer);

    // --- 4) 司令塔へ配線 → 起動（順序が重要） ---
    if (m_match) {
        m_match->setClock(clock);
    }
    clock->startClock();
}

// ============================================================
// 段階実行API: 現在局面からの開始準備
// ============================================================

void GameStartCoordinator::prepareDataCurrentPosition(const Ctx& c)
{
    qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: ENTER"
                       << " c.currentSfenStr=" << (c.currentSfenStr ? c.currentSfenStr->left(50) : "null")
                       << " c.startSfenStr=" << (c.startSfenStr ? c.startSfenStr->left(50) : "null");

    if (!c.view || !m_match) {
        qCWarning(lcGame).noquote() << "prepareDataCurrentPosition: missing deps:"
                             << "view=" << (c.view != nullptr) << " match=" << (m_match != nullptr);
        return;
    }

    // --- 1) ベースSFENの決定（優先: 現在SFEN → 開始SFEN → 平手） ---
    // 重要: requestPreStartCleanup より前に決定する。
    //       requestPreStartCleanup 内で棋譜欄の選択が変更され、
    //       m_currentSfenStr が上書きされる可能性があるため。
    QString baseSfen;
    if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
        baseSfen = *(c.currentSfenStr);
        qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: baseSfen from currentSfenStr=" << baseSfen.left(50);
    } else if (c.startSfenStr && !c.startSfenStr->isEmpty()) {
        baseSfen = *(c.startSfenStr);
        qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: baseSfen from startSfenStr=" << baseSfen.left(50);
    } else {
        baseSfen = QStringLiteral("startpos");
        qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: baseSfen FALLBACK to startpos";
    }

    // --- 0) 開始前クリーンアップをUI層へ依頼 ---
    // 注意: cleanup 内で selectStartRow(0) → syncBoardAndHighlightsAtRow(0) が
    //       m_currentSfenStr を初期局面（ply 0）のSFENに上書きしてしまう。
    //       baseSfen はこの上書きの影響を受けないよう、cleanup 前に確定済み。
    emit requestPreStartCleanup();

    // --- 2) ベースSFENの適用 ---
    if (baseSfen == QLatin1String("startpos")) {
        qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: applying startpos";
        c.view->initializeToFlatStartingPosition();
        if (c.startSfenStr && c.startSfenStr->isEmpty())
            *c.startSfenStr = QStringLiteral("startpos");
        if (c.currentSfenStr)
            *c.currentSfenStr = QStringLiteral("startpos");
    } else {
        qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: applying baseSfen=" << baseSfen.left(50);
        GameStartOptionsBuilder::applyResumePositionIfAny(c.gc, c.view, baseSfen);

        if (c.currentSfenStr) *c.currentSfenStr = baseSfen;
        if (c.startSfenStr   && c.startSfenStr->isEmpty())
            *c.startSfenStr = QString();
    }

    // --- 3) 直前の終局状態を必ずクリア ---
    m_match->clearGameOverState();

    // --- 4) ハイライトを確実に空へ ---
    c.view->removeHighlightAllData();

    qCDebug(lcGame).noquote() << "prepareDataCurrentPosition: done."
                       << " FINAL c.currentSfenStr=" << (c.currentSfenStr ? c.currentSfenStr->left(50) : "null");
}

// ============================================================
// 段階実行API: 初期局面（平手／手合割）からの開始準備
// ============================================================

void GameStartCoordinator::prepareInitialPosition(const Ctx& c)
{
    // 1) 開始局面番号を取得（0=現在局面, 1..N=手合割）
    int startingPosNumber = 1;
    if (c.startDlg) {
        if (auto dlg = qobject_cast<StartGameDialog*>(c.startDlg)) {
            startingPosNumber = dlg->startingPositionNumber();
        } else {
            bool ok = false;
            const int v = c.startDlg->property("startingPositionNumber").toInt(&ok);
            if (ok) startingPosNumber = v;
        }
    }
    if (startingPosNumber <= 0) startingPosNumber = 1;

    // 2) 手合割 → 純SFEN（GameStartOptionsBuilder のプリセットテーブルを使用）
    const QString sfen = GameStartOptionsBuilder::startingPositionSfen(startingPosNumber);

    qCDebug(lcGame).noquote()
        << "prepareInitial: startingPosNumber=" << startingPosNumber
        << " sfen=" << sfen
        << " sfenRecord*=" << static_cast<const void*>(c.sfenRecord);

    // 4) 参照を通して MainWindow 側の文字列を更新
    if (c.startSfenStr)   *c.startSfenStr   = sfen;
    if (c.currentSfenStr) *c.currentSfenStr = sfen;

    // 5) 棋譜欄に「=== 開始局面 ===」ヘッダを追加（重複防止）
    if (c.kifuModel) {
        const int rows = c.kifuModel->rowCount();
        bool need = true;
        if (rows > 0) {
            const QModelIndex idx0 = c.kifuModel->index(0, 0);
            const QString head = c.kifuModel->data(idx0, Qt::DisplayRole).toString();
            if (head == tr("=== 開始局面 ==="))
                need = false;
        }
        if (need) {
            if (rows == 0) {
                c.kifuModel->appendItem(
                    new KifuDisplay(tr("=== 開始局面 ==="),
                                    tr("（１手 / 合計）")));
            } else {
                c.kifuModel->prependItem(
                    new KifuDisplay(tr("=== 開始局面 ==="),
                                    tr("（１手 / 合計）")));
            }
        }
    }

    // 6) SFEN履歴に開始SFEN（0手目）を積む
    if (c.sfenRecord) {
        const int before = static_cast<int>(c.sfenRecord->size());
        qCDebug(lcGame).noquote() << "prepareInitial: sfenRecord BEFORE size=" << before;

        c.sfenRecord->clear();
        c.sfenRecord->append(sfen);

        qCDebug(lcGame).noquote() << "prepareInitial: sfenRecord AFTER  size=" << c.sfenRecord->size();
        if (!c.sfenRecord->isEmpty()) {
            qCDebug(lcGame).noquote() << "prepareInitial: head[0]=" << c.sfenRecord->first();
        }
    } else {
        qCDebug(lcGame).noquote() << "prepareInitial: sfenRecord is null";
    }

    // 7) 開幕時のハイライトはクリア
    if (c.view) {
        c.view->removeHighlightAllData();
    }

    qCDebug(lcGame).noquote() << "prepareInitialPosition: sfen=" << sfen
                       << " sfenRecord*=" << static_cast<const void*>(c.sfenRecord);
}

// ============================================================
// 段階実行API: 時計設定と対局開始
// ============================================================

void GameStartCoordinator::setTimerAndStart(const Ctx& c)
{
    if (!c.clock) {
        qCWarning(lcGame).noquote() << "setTimerAndStart: clock is null.";
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

    // --- 2) 秒へ統一 ---
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
    c.clock->updateClock();
    if (!c.isReplayMode) {
        c.clock->startClock();
    }

    // 司令塔に TimeControl（秒読み/増加/負け扱い）を設定
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

    qCDebug(lcGame).noquote() << "setTimerAndStart: "
                       << "t1=" << remainingTime1 << "s"
                       << " t2=" << remainingTime2 << "s"
                       << " byo=" << byoyomi1 << "/" << byoyomi2
                       << " inc=" << binc << "/" << winc
                       << " loseOnTimeout=" << isLoseOnTimeout
                       << " replay=" << c.isReplayMode;
}

// ============================================================
// ユーティリティ（GameStartOptionsBuilder へ委譲）
// ============================================================

PlayMode GameStartCoordinator::setPlayMode(const Ctx& c) const
{
    return GameStartOptionsBuilder::determinePlayModeFromDialog(c.startDlg);
}

PlayMode GameStartCoordinator::determinePlayModeAlignedWithTurn(
    int initPositionNumber, bool isPlayer1Human, bool isPlayer2Human, const QString& startSfen)
{
    return GameStartOptionsBuilder::determinePlayModeAlignedWithTurn(
        initPositionNumber, isPlayer1Human, isPlayer2Human, startSfen);
}

// ============================================================
// ユーティリティ（GameStartOptionsBuilder へ委譲）
// ============================================================

void GameStartCoordinator::applyResumePositionIfAny(ShogiGameController* gc,
                                                    ShogiView* view,
                                                    const QString& resumeSfen)
{
    GameStartOptionsBuilder::applyResumePositionIfAny(gc, view, resumeSfen);
}

void GameStartCoordinator::applyPlayersNamesForMode(ShogiView* view,
                                                    PlayMode mode,
                                                    const QString& human1,
                                                    const QString& human2,
                                                    const QString& engine1,
                                                    const QString& engine2) const
{
    GameStartOptionsBuilder::applyPlayersNamesForMode(view, mode, human1, human2, engine1, engine2);
}

void GameStartCoordinator::setMatch(MatchCoordinator* match)
{
    m_match = match;
}
