#include "matchcoordinator.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include <limits>
#include <QObject>
#include <QDebug>

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent) 
    , m_gc(d.gc)
    , m_clock(d.clock)
    , m_view(d.view)
    , m_usi1(d.usi1)
    , m_usi2(d.usi2)
    , m_hooks(d.hooks)
    , m_comm1(d.comm1)
    , m_think1(d.think1)
    , m_comm2(d.comm2)
    , m_think2(d.think2)
{
}

MatchCoordinator::~MatchCoordinator() = default;

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    m_usi1 = e1;
    m_usi2 = e2;
}

void MatchCoordinator::startNewGame(const QString& sfenStart)
{
    // 1) GUI固有の初期化
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(sfenStart);

    // 2) 表示/アクション
    setPlayersNamesForMode_();
    setEngineNamesBasedOnMode_();
    setGameInProgressActions_(true);

    // 3) 盤面描画
    renderShogiBoard_();

    // 4) 初手手番を SFEN から決定（なければ P1）
    ShogiGameController::Player first = ShogiGameController::Player1;
    if (!sfenStart.isEmpty()) {
        // “… b …”=先手番, “… w …”=後手番（SFEN準拠）
        if (sfenStart.contains(QStringLiteral(" w ")))
            first = ShogiGameController::Player2;
        else if (sfenStart.contains(QStringLiteral(" b ")))
            first = ShogiGameController::Player1;
    }

    // GC の手番を必ず確定してから UI 反映
    if (m_gc) m_gc->setCurrentPlayer(first);
    m_cur = (first == ShogiGameController::Player1 ? P1 : P2);
    updateTurnDisplay_(m_cur);

    if (m_hooks.log) m_hooks.log(QStringLiteral("MatchCoordinator: startNewGame done"));
    emit gameStarted();
}

void MatchCoordinator::handleResign() {
    stopClockAndSendStops_();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = m_cur; // 現在手番側が投了したとみなす（必要に応じて呼び出し側で明示管理）

    // エンジンへ通知（任意）
    if (m_hooks.sendRawToEngine) {
        if (info.loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::handleEngineResign(int idx) {
    stopClockAndSendStops_();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (idx == 1 ? P1 : P2);

    if (m_hooks.sendRawToEngine) {
        if (info.loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::notifyTimeout(Player loser) {
    stopClockAndSendStops_();

    // 任意：エンジンへ gameover 通知（生コマンドで十分）
    if (m_hooks.sendRawToEngine) {
        if (loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    GameEndInfo info;
    info.cause = Cause::Timeout;
    info.loser = loser;

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::flipBoard() {
    // 実際の反転は GUI 側で実施（レイアウト/ラベル入替等を考慮）
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    emit boardFlipped(true);
}

void MatchCoordinator::onTurnFinishedAndSwitch() {
    // 1) 現在の時間を読み、次手の go を計算
    const GoTimes t = computeGoTimes_();

    // 2) 手番を入れ替え
    m_cur = (m_cur == P1 ? P2 : P1);
    updateTurnDisplay_(m_cur);

    // 3) 次に指す側のエンジンへ go 送信（対人/対エンジン/エンジンvsエンジンは上位で制御可）
    if (m_hooks.sendGoToEngine) {
        if (m_cur == P1 && m_usi1) m_hooks.sendGoToEngine(m_usi1, t);
        if (m_cur == P2 && m_usi2) m_hooks.sendGoToEngine(m_usi2, t);
    }
}

// ---- private helpers ----

void MatchCoordinator::setPlayersNamesForMode_() {
    if (m_hooks.setPlayersNames) {
        // TODO: 実際の名前決定ロジックを移設
        m_hooks.setPlayersNames(QStringLiteral("Player1"), QStringLiteral("Player2"));
    }
}

void MatchCoordinator::setEngineNamesBasedOnMode_() {
    if (m_hooks.setEngineNames) {
        // TODO: 実際のエンジン名取得ロジックを移設（Usi から取得など）
        m_hooks.setEngineNames(QStringLiteral("Engine#1"), QStringLiteral("Engine#2"));
    }
}

void MatchCoordinator::setGameInProgressActions_(bool inProgress) {
    if (m_hooks.setGameActions) m_hooks.setGameActions(inProgress);
}

void MatchCoordinator::renderShogiBoard_() {
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
}

void MatchCoordinator::updateTurnDisplay_(Player p) {
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(p);
}

MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes_() const {
    GoTimes t;
    if (m_hooks.remainingMsFor) {
        t.btime = m_hooks.remainingMsFor(P1);
        t.wtime = m_hooks.remainingMsFor(P2);
    }
    if (m_hooks.incrementMsFor) {
        t.binc = m_hooks.incrementMsFor(P1);
        t.winc = m_hooks.incrementMsFor(P2);
    }
    if (m_hooks.byoyomiMs) {
        t.byoyomi = m_hooks.byoyomiMs();
    }
    return t;
}

void MatchCoordinator::stopClockAndSendStops_() {
    if (m_clock) m_clock->stopClock();
    if (m_hooks.sendStopToEngine) {
        if (m_usi1) m_hooks.sendStopToEngine(m_usi1);
        if (m_usi2) m_hooks.sendStopToEngine(m_usi2);
    }
}

void MatchCoordinator::displayResultsAndUpdateGui_(const GameEndInfo& info) {
    setGameInProgressActions_(false);

    if (m_hooks.showGameOverDialog) {
        const QString loserStr = (info.loser == P1) ? QStringLiteral("P1") : QStringLiteral("P2");
        const QString msg = (info.cause == Cause::Timeout)
                                ? tr("Timeout: %1 lost").arg(loserStr)
                                : tr("Resignation: %1 lost").arg(loserStr);
        m_hooks.showGameOverDialog(tr("Game Over"), msg);
    }

    if (m_hooks.log) m_hooks.log(QStringLiteral("Game ended"));

    emit gameEnded(info);
    emit gameOverShown();
}

void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    Usi*& eng = (side == P1 ? m_usi1 : m_usi2);

    if (!eng) {
        // HvE フォールバック：m_usi1 しか無い場合は m_usi1 を使う
        if (m_usi1 && !m_usi2) {
            if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] fallback to m_usi1 for HvE"));
            eng = m_usi1;
        } else {
            if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] engine ptr is null (side=%1)").arg(side == P1 ? "P1" : "P2"));
            return;
        }
    }

    QString path = enginePathIn;
    QString name = engineNameIn;

    try {
        eng->initializeAndStartEngineCommunication(path, name);
    } catch (...) {
        if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] init/start failed"));
        throw;
    }

    wireResignToArbiter_(eng, (eng == m_usi1)); // m_usi1=先手扱い（HvE でも OK）
}

void MatchCoordinator::wireResignSignals()
{
    if (m_usi1) wireResignToArbiter_(m_usi1, /*asP1=*/true);
    if (m_usi2) wireResignToArbiter_(m_usi2, /*asP1=*/false);
}

void MatchCoordinator::wireResignToArbiter_(Usi* engine, bool asP1)
{
    if (!engine) return;

    // 既存接続の掃除は「該当シグナルのみ」を明示して行う
    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, nullptr);

    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &MatchCoordinator::onEngine1Resign,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &MatchCoordinator::onEngine2Resign,
                         Qt::UniqueConnection);
    }
}

void MatchCoordinator::onEngine1Resign()
{
    // 既存のハンドラへ委譲（番号は 1 = P1）
    this->handleEngineResign(1);
}

void MatchCoordinator::onEngine2Resign()
{
    // 既存のハンドラへ委譲（番号は 2 = P2）
    this->handleEngineResign(2);
}

void MatchCoordinator::sendGameOverWinAndQuitTo(int idx)
{
    Usi* target = (idx == 1 ? m_usi1 : m_usi2);
    if (!target) return;

    // HvE のときは opponent が nullptr の可能性があるが、WIN 側だけ送れば良い。
    target->sendGameOverWinAndQuitCommands();
}

void MatchCoordinator::destroyEngine(int idx)
{
    Usi*& ref = (idx == 1 ? m_usi1 : m_usi2);
    if (ref) {
        delete ref;
        ref = nullptr;
    }
}

void MatchCoordinator::destroyEngines()
{
    destroyEngine(1);
    destroyEngine(2);
}

Usi* MatchCoordinator::enginePtr(int idx) const
{
    return (idx == 1 ? m_usi1 : (idx == 2 ? m_usi2 : nullptr));
}

int MatchCoordinator::indexForEngine_(const Usi* p) const
{
    if (!p) return 0;
    if (p == m_usi1) return 1;
    if (p == m_usi2) return 2;
    return 0;
}

void MatchCoordinator::setPlayMode(PlayMode m)
{
    m_playMode = m;
}

void MatchCoordinator::initEnginesForEvE(const QString& engineName1,
                                         const QString& engineName2)
{
    // 既存エンジンの破棄
    destroyEngines();

    // モデル（GUI から貰えない場合はフォールバック生成）
    UsiCommLogModel*          comm1  = m_comm1 ? m_comm1 : new UsiCommLogModel(this);
    ShogiEngineThinkingModel* think1 = m_think1 ? m_think1 : new ShogiEngineThinkingModel(this);
    UsiCommLogModel*          comm2  = m_comm2 ? m_comm2 : new UsiCommLogModel(this);
    ShogiEngineThinkingModel* think2 = m_think2 ? m_think2 : new ShogiEngineThinkingModel(this);

    if (!m_comm1)  { m_comm1  = comm1;  qWarning() << "[EvE] comm1 fallback created"; }
    if (!m_think1) { m_think1 = think1; qWarning() << "[EvE] think1 fallback created"; }
    if (!m_comm2)  { m_comm2  = comm2;  qWarning() << "[EvE] comm2 fallback created"; }
    if (!m_think2) { m_think2 = think2; qWarning() << "[EvE] think2 fallback created"; }

    // USI を生成（この時点ではプロセス未起動）
    m_usi1 = new Usi(comm1, think1, m_gc, m_playMode, this);
    m_usi2 = new Usi(comm2, think2, m_gc, m_playMode, this);

    // 状態初期化
    m_usi1->resetResignNotified(); m_usi1->clearHardTimeout();
    m_usi2->resetResignNotified(); m_usi2->clearHardTimeout();

    // 投了配線
    wireResignToArbiter_(m_usi1, /*asP1=*/true);
    wireResignToArbiter_(m_usi2, /*asP1=*/false);

    // ログ識別
    m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), engineName1);
    m_usi2->setLogIdentity(QStringLiteral("[E2]"), QStringLiteral("P2"), engineName2);
    m_usi1->setSquelchResignLogging(false);
    m_usi2->setSquelchResignLogging(false);

    // MainWindow 互換：司令塔が保有する USI を最新化
    updateUsiPtrs(m_usi1, m_usi2);
}

bool MatchCoordinator::engineThinkApplyMove(Usi* engine,
                                            QString& positionStr,
                                            QString& ponderStr,
                                            QPoint* outFrom,
                                            QPoint* outTo)
{
    if (!engine || !m_gc) return false;

    const GoTimes t = computeGoTimes_();

    // byoyomi が設定されていれば USI 的には秒読みを使う
    const bool useByoyomi = (t.byoyomi > 0);

    // qint64 → int への安全な変換（オーバーフロー防止）
    auto clampMsToInt = [](qint64 ms) -> int {
        if (ms < 0) return 0;
        if (ms > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        return static_cast<int>(ms);
    };

    // qint64 → QString（ミリ秒の数値文字列）
    const QString btimeStr = QString::number(t.btime);
    const QString wtimeStr = QString::number(t.wtime);

    QPoint from(-1, -1), to(-1, -1);
    m_gc->setPromote(false);

    engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionStr,                // 現局面（SFEN/position）
        ponderStr,                  // ponder
        from, to,                   // 出力される移動先
        clampMsToInt(t.byoyomi),    // byoyomi ms (int)
        btimeStr,                   // btime (QString)
        wtimeStr,                   // wtime (QString)
        clampMsToInt(t.binc),       // 先手加算 (int)
        clampMsToInt(t.winc),       // 後手加算 (int)
        useByoyomi                  // byoyomi 使用フラグ
        );

    if (outFrom) *outFrom = from;
    if (outTo)   *outTo   = to;
    return true;
}

bool MatchCoordinator::engineMoveOnce(Usi* eng,
                                      QString& positionStr,
                                      QString& ponderStr,
                                      bool /*useSelectedField2*/,
                                      int engineIndex,
                                      QPoint* outTo)
{
    QPoint from, to;
    if (!engineThinkApplyMove(eng, positionStr, ponderStr, &from, &to))
        return false;

    // ハイライト／評価グラフの更新は UI 側（Hookなど）に委譲するのが理想
    // ここでは盤描画だけを促す
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    if (outTo) *outTo = to;
    return true;
}

bool MatchCoordinator::playOneEngineTurn(Usi* mover,
                                         Usi* receiver,
                                         QString& positionStr,
                                         QString& ponderStr,
                                         int engineIndex)
{
    QPoint to;
    if (!engineMoveOnce(mover, positionStr, ponderStr,
                        /*useSelectedField2=*/false,
                        engineIndex, &to)) {
        return false;
    }

    // 次手ヒントを相手エンジンへ
    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    // ここでの終局判定は未定義メンバ m_gameIsOver を参照せず、
    // 司令塔の gameEnded() 発火側に委譲します。
    return true;
}

void MatchCoordinator::sendGameOverWinAndQuit()
{
    switch (m_playMode) {
    case HumanVsHuman:
        // 送信不要
        break;

    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();
        break;

    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
        break;

    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
        if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();
        break;

    default:
        // NotStarted / Analysis / Consideration / TsumiSearch / など
        break;
    }
}

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    m_playMode = opt.mode;

    // 盤・名前などの初期化（GUI側へ委譲）
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
    if (m_hooks.setPlayersNames)   m_hooks.setPlayersNames(QString(), QString());
    if (m_hooks.setEngineNames)    m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
    if (m_hooks.setGameActions)    m_hooks.setGameActions(true);
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    // EvE 用の内部棋譜コンテナを初期化
    m_eveSfenRecord.clear();
    m_eveGameMoves.clear();
    m_eveMoveIndex = 0;

    switch (m_playMode) {
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine: {
        // 1) USIインスタンスの生成（ログモデル等の配線もここで）
        initEnginesForEvE(opt.engineName1, opt.engineName2);

        // 2) 2台とも USI プロセスを起動・初期化（usi→isready→usinewgame）
        initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
        initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);

        // 3) 先手初手を開始（ここで position/go を送る）
        startEngineVsEngine_(opt);
        break;
    }

    case EvenHumanVsEngine:
    case HandicapHumanVsEngine: {
        // HvE/EvH は既存の単独エンジン起動ルートへ
        const bool engineIsP1 = opt.engineIsP1;
        startHumanVsEngine_(opt, engineIsP1);
        break;
    }

    case EvenEngineVsHuman:
    case HandicapEngineVsHuman: {
        const bool engineIsP1 = true; // 先手エンジン
        startHumanVsEngine_(opt, engineIsP1);
        break;
    }

    case HumanVsHuman:
        startHumanVsHuman_(opt);
        break;

    default:
        // NotStarted / Analysis 等
        break;
    }
}

void MatchCoordinator::startHumanVsHuman_(const StartOptions& /*opt*/)
{
    if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] Start HvH"));
    // HvH はエンジン不要。必要ならここで追加の初期化のみ。
}

void MatchCoordinator::startHumanVsEngine_(const StartOptions& opt, bool engineIsP1)
{
    if (m_hooks.log) {
        m_hooks.log(QStringLiteral("[Match] Start HvE (engineIsP1=%1)").arg(engineIsP1));
    }

    // 以前のエンジンが残っているとログモデル指し替えが難しいため一度クリーンに
    destroyEngines();

    // サイドに応じて GUI モデルを選択（P1側エンジンなら comm1/think1、P2側なら comm2/think2）
    UsiCommLogModel*          comm  = engineIsP1 ? m_comm1  : m_comm2;
    ShogiEngineThinkingModel* think = engineIsP1 ? m_think1 : m_think2;

    // GUI が該当側のモデルを用意していない場合のフォールバック（クラッシュ回避）
    if (!comm)  comm  = new UsiCommLogModel(this);
    if (!think) think = new ShogiEngineThinkingModel(this);

    // HvE は既存仕様どおり m_usi1 を使用（内部の先後は logIdentity で区別）
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);
    updateUsiPtrs(m_usi1, nullptr);

    m_usi1->resetResignNotified();
    m_usi1->clearHardTimeout();

    // 投了シグナル配線
    wireResignToArbiter_(m_usi1, /*asP1=*/true);

    // ログ識別（P1/P2 を表示に反映）
    const QString eName = engineIsP1 ? opt.engineName1 : opt.engineName2;
    m_usi1->setLogIdentity(QStringLiteral("[E]"),
                           engineIsP1 ? QStringLiteral("P1") : QStringLiteral("P2"),
                           eName);
    m_usi1->setSquelchResignLogging(false);

    // USI 起動（パス＋表示名）
    const QString ePath = engineIsP1 ? opt.enginePath1 : opt.enginePath2;
    const QString eDisp = engineIsP1 ? opt.engineName1 : opt.engineName2;
    initializeAndStartEngineFor(P1, ePath, eDisp); // HvE でも P1 側を使う（既存仕様）
}

// EvE の初手を開始する（起動・初期化済み前提）
void MatchCoordinator::startEngineVsEngine_(const StartOptions& /*opt*/)
{
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    // GC の手番が未設定なら先手にしておく
    if (m_gc->currentPlayer() == ShogiGameController::NoPlayer) {
        m_gc->setCurrentPlayer(ShogiGameController::Player1);
        m_cur = P1;
        updateTurnDisplay_(m_cur);
    }

    // "position ... moves" の初期化（startpos 前提。SFEN 始点に拡張するならここで分岐）
    initPositionStringsForEvE_();

    // 先手エンジン（P1）に初手思考を要求
    const GoTimes t1 = computeGoTimes_();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    m_gc->setPromote(false);

    try {
        m_usi1->handleEngineVsHumanOrEngineMatchCommunication(
            m_positionStr1, m_positionPonder1,
            p1From, p1To,
            static_cast<int>(t1.byoyomi),
            btimeStr1, wtimeStr1,
            static_cast<int>(t1.binc), static_cast<int>(t1.winc),
            (t1.byoyomi > 0)
            );
    } catch (const std::exception& e) {
        qWarning() << "[EvE] engine1 first move failed:" << e.what();
        return;
    }

    // bestmove を盤へ適用（EvE 専用の棋譜コンテナを使用）
    QString rec1;
    PlayMode pm = m_playMode;
    try {
        if (!m_gc->validateAndMove(
                p1From, p1To, rec1,
                pm,
                m_eveMoveIndex,
                &m_eveSfenRecord,
                m_eveGameMoves
                )) {
            return;
        }
    } catch (const std::exception& e) {
        qWarning() << "[EvE] validateAndMove(P1) failed:" << e.what();
        return;
    }

    // 盤描画＆手番表示
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    // ─────────────────────────────────────────────────────────────
    // ★ ここが今回の追加ポイント：
    //   先手の着手で更新された position を後手用にも反映し、P2 に 1手指させる
    // ─────────────────────────────────────────────────────────────

    // “同○”ヒントを P2 へ（必要なら）
    if (m_usi2) {
        m_usi2->setPreviousFileTo(p1To.x());
        m_usi2->setPreviousRankTo(p1To.y());
    }

    // 後手用 position は先手用をそのままコピー（"position ... moves ..." に先手着手が入っている）
    m_positionStr2     = m_positionStr1;
    m_positionPonder2.clear(); // 不要ならクリア

    const GoTimes t2 = computeGoTimes_();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    m_gc->setPromote(false);

    try {
        m_usi2->handleEngineVsHumanOrEngineMatchCommunication(
            m_positionStr2, m_positionPonder2,
            p2From, p2To,
            static_cast<int>(t2.byoyomi),
            btimeStr2, wtimeStr2,
            static_cast<int>(t2.binc), static_cast<int>(t2.winc),
            (t2.byoyomi > 0)
            );
    } catch (const std::exception& e) {
        qWarning() << "[EvE] engine2 first reply failed:" << e.what();
        return;
    }

    // 後手の着手を盤へ適用
    QString rec2;
    try {
        if (!m_gc->validateAndMove(
                p2From, p2To, rec2,
                pm,
                m_eveMoveIndex,
                &m_eveSfenRecord,
                m_eveGameMoves
                )) {
            return;
        }
    } catch (const std::exception& e) {
        qWarning() << "[EvE] validateAndMove(P2) failed:" << e.what();
        return;
    }

    // 反映
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    QTimer::singleShot(0, this, &MatchCoordinator::kickNextEvETurn_);
}

Usi* MatchCoordinator::primaryEngine() const
{
    return m_usi1;
}

Usi* MatchCoordinator::secondaryEngine() const
{
    return m_usi2;
}

void MatchCoordinator::initPositionStringsForEvE_()
{
    m_positionStr1.clear();
    m_positionPonder1.clear();
    m_positionStr2.clear();
    m_positionPonder2.clear();

    const QString base = QStringLiteral("position startpos moves");
    m_positionStr1 = base;
    m_positionStr2 = base;
}

void MatchCoordinator::kickNextEvETurn_()
{
    // EvE 以外・未初期化は何もしない
    if (m_playMode != EvenEngineVsEngine && m_playMode != HandicapEngineVsEngine) return;
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    // いま指す側と相手側を決める（GCの現手番に従う）
    const bool p1ToMove = (m_gc->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? m_usi1 : m_usi2;
    Usi* receiver = p1ToMove ? m_usi2 : m_usi1;

    // mover が使う position/ponder を選び、直前の相手側の position に同期
    QString& pos    = p1ToMove ? m_positionStr1     : m_positionStr2;
    QString& ponder = p1ToMove ? m_positionPonder1  : m_positionPonder2;
    if (p1ToMove) pos = m_positionStr2; else pos = m_positionStr1;

    // 1手思考 → bestmove 取得
    QPoint from(-1,-1), to(-1,-1);
    if (!engineThinkApplyMove(mover, pos, ponder, &from, &to))
        return;

    // 盤へ適用（EvE用の安全な記録バッファを使用）
    QString rec;
    try {
        if (!m_gc->validateAndMove(from, to, rec, m_playMode,
                                   m_eveMoveIndex, &m_eveSfenRecord, m_eveGameMoves)) {
            return;
        }
    } catch (const std::exception& e) {
        qWarning() << "[EvE] validateAndMove failed:" << e.what();
        return;
    }

    // 相手エンジンに “同○” ヒント
    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    // 盤の再描画＆手番表示
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    // 次の手をすぐ（イベントキュー経由で）回す
    QTimer::singleShot(0, this, &MatchCoordinator::kickNextEvETurn_);
}
