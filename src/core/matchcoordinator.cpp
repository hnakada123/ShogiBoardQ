#include "matchcoordinator.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"

#include <limits>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QtGlobal>
#include <QDateTime>

using P = MatchCoordinator::Player;

static inline P toP(int gcPlayer) { return (gcPlayer == 1) ? MatchCoordinator::P1 : MatchCoordinator::P2; }

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
    // 既定値
    m_cur = P1;
    m_turnEpochP1Ms = m_turnEpochP2Ms = -1;

    wireClock_(); // ★CTOR でも配線
}

MatchCoordinator::~MatchCoordinator() = default;

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    m_usi1 = e1;
    m_usi2 = e2;
}

void MatchCoordinator::startNewGame(const QString& sfenStart) {
    // 既存
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(sfenStart);
    setPlayersNamesForMode_();
    setEngineNamesBasedOnMode_();
    setGameInProgressActions_(true);
    renderShogiBoard_();

    // ★ GC の手番を SFEN から決定（無ければ先手）
    if (m_gc) {
        ShogiGameController::Player start = ShogiGameController::Player1;
        if (!sfenStart.isEmpty()) {
            const auto parts = sfenStart.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                if (parts[1] == QLatin1String("w") || parts[1] == QLatin1String("W"))
                    start = ShogiGameController::Player2;
                else
                    start = ShogiGameController::Player1; // "b" or それ以外は先手扱い
            }
        }
        m_gc->setCurrentPlayer(start);
    }

    // 司令塔の手番も同期（既存のままでもOKですが念のため）
    m_cur = P1;
    updateTurnDisplay_(m_cur);

    if (m_hooks.log) m_hooks.log(QStringLiteral("MatchCoordinator: startNewGame done"));
    emit gameStarted();
}

// 1) 人間側の投了
void MatchCoordinator::handleResign() {
    GameEndInfo info;
    info.cause = Cause::Resignation;
    //info.loser = m_cur;                     // 投了したのは現在手番
    info.loser = (m_gc && m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
    const Player winner = (m_cur == P1 ? P2 : P1);

    if (m_hooks.sendRawToEngine) {
        if (m_usi1 && !m_usi2) {
            // HvE：エンジンは常に m_usi1。人間が投了＝エンジン勝ち。
            m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
            m_hooks.sendRawToEngine(m_usi1, QStringLiteral("quit")); // 再戦しないなら送る
        } else {
            // EvE：勝者/敗者のエンジンをそれぞれに通知
            Usi* winEng  = (winner    == P1) ? m_usi1 : m_usi2;
            Usi* loseEng = (info.loser== P1) ? m_usi1 : m_usi2;
            if (loseEng) m_hooks.sendRawToEngine(loseEng, QStringLiteral("gameover lose"));
            if (winEng)  {
                m_hooks.sendRawToEngine(winEng,  QStringLiteral("gameover win"));
                m_hooks.sendRawToEngine(winEng,  QStringLiteral("quit"));
            }
        }
    }

    // 終局の正規ルート（棋譜「投了」追記はこれが emit する requestAppendGameOverMove で）
    setGameOver(info, /*loserIsP1=*/(info.loser==P1), /*appendMoveOnce=*/true);
}

// 2) エンジン側の投了
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

    // ★ 同様に setGameOver(...) を使う
    const bool loserIsP1 = (info.loser == P1);
    setGameOver(info, loserIsP1, /*appendMoveOnce=*/true);
}

void MatchCoordinator::notifyTimeout(Player loser) {
    // 1) 時計停止（以降の進行を止める）
    if (m_clock) m_clock->stopClock();

    const Player winner = (loser == P1 ? P2 : P1);

    // 2) HvE の人間側判定（あれば利用）：人間が時間切れならエンジン勝ち
    Player humanSide = P1;
    const bool hasHumanSide = static_cast<bool>(m_hooks.humanPlayerSide);
    if (hasHumanSide) {
        humanSide = m_hooks.humanPlayerSide();
    }

    // 3) エンジンへ最終通知
    if (m_usi1 && !m_usi2) {
        // 片側エンジン（HvE）
        if (!hasHumanSide || loser == humanSide) {
            // 人間が時間切れ → エンジン勝ち
            m_usi1->sendGameOverWinAndQuitCommands();
        } else {
            // 例外的：エンジンが時間切れ（ほぼ無いが一応）
            m_usi1->sendGameOverLoseAndQuitCommands();
        }
        m_usi1->setSquelchResignLogging(true);
    } else {
        // EvE：両エンジンが居る
        Usi* loserEng  = (loser  == P1) ? m_usi1 : m_usi2;
        Usi* winnerEng = (winner == P1) ? m_usi1 : m_usi2;
        if (loserEng)  { loserEng->sendGameOverLoseAndQuitCommands();  loserEng->setSquelchResignLogging(true); }
        if (winnerEng) { winnerEng->sendGameOverWinAndQuitCommands();  winnerEng->setSquelchResignLogging(true); }
    }

    // 4) UI へ終局通知
    GameEndInfo info; info.cause = Cause::Timeout; info.loser = loser;
    setGameOver(info, /*loserIsP1=*/(loser == P1), /*appendMoveOnce=*/true);
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
    m_cur = p; // ★ 同期
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(p);
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

    // 評価値フックの呼出は呼び出し元（engineMoveOnce）で行う
    return true;
}

bool MatchCoordinator::engineMoveOnce(Usi* eng,
                                      QString& positionStr,
                                      QString& ponderStr,
                                      bool /*useSelectedField2*/,
                                      int engineIndex,
                                      QPoint* outTo)
{
    if (!m_gc) return false;

    const auto moverBefore = m_gc->currentPlayer();
    qDebug() << "[EVE] engineMoveOnce enter"
             << "engineIndex=" << engineIndex
             << "moverBefore=" << int(moverBefore)
             << "thread=" << QThread::currentThread();

    QPoint from, to;
    if (!engineThinkApplyMove(eng, positionStr, ponderStr, &from, &to)) {
        qDebug() << "[EVE] engineThinkApplyMove FAILED";
        return false;
    }
    qDebug() << "[EVE] engineThinkApplyMove OK from=" << from << "to=" << to;

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    switch (moverBefore) {
    case ShogiGameController::Player1:
        qDebug() << "[EVE] calling appendEvalP1";
        if (m_hooks.appendEvalP1) m_hooks.appendEvalP1();
        else qDebug() << "[EVE][WARN] appendEvalP1 NOT set";
        break;
    case ShogiGameController::Player2:
        qDebug() << "[EVE] calling appendEvalP2";
        if (m_hooks.appendEvalP2) m_hooks.appendEvalP2();
        else qDebug() << "[EVE][WARN] appendEvalP2 NOT set";
        break;
    default:
        qDebug() << "[EVE][WARN] moverBefore=NoPlayer -> skip eval append";
        break;
    }

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
        // ★ 単発は常に m_usi1 を使う
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
        break;

    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        // 単発は常に m_usi1
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
        break;

    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
        if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();
        break;

    default:
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

    // ★ 追加：SFEN から GC の手番を決定（無ければ先手）
    if (m_gc) {
        ShogiGameController::Player start = ShogiGameController::Player1;
        if (!opt.sfenStart.isEmpty()) {
            const auto parts = opt.sfenStart.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2 && (parts[1] == QLatin1String("w") || parts[1] == QLatin1String("W"))) {
                start = ShogiGameController::Player2;
            }
        }
        m_gc->setCurrentPlayer(start);
    }

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

    // 以前のエンジンは破棄
    destroyEngines();

    // ★ 単発エンジン＝表示は常に #1 スロット（上段）へ流す
    //    → 座席が P1 でも P2 でも、comm/think は #1 を使う
    UsiCommLogModel*          comm  = m_comm1;
    ShogiEngineThinkingModel* think = m_think1;

    // GUI が #1 モデルをまだ持っていなければフォールバック生成して保持
    if (!comm)  { comm  = new UsiCommLogModel(this);         m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(this); m_think1 = think; }

    // 単発は既存仕様どおり m_usi1 を使用（内部の先後識別は logIdentity で持つ）
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);
    updateUsiPtrs(m_usi1, nullptr);

    m_usi1->resetResignNotified();
    m_usi1->clearHardTimeout();

    // ★ 投了シグナルの配線は「実際の座席」に合わせる（P1/P2 を正しく扱う）
    wireResignToArbiter_(m_usi1, /*asP1=*/engineIsP1);

    // ログ識別：表示上は P1/P2 を正しく出す（ただし表示面は常に上段）
    const QString eName = engineIsP1 ? opt.engineName1 : opt.engineName2;
    m_usi1->setLogIdentity(QStringLiteral("[E]"),
                           engineIsP1 ? QStringLiteral("P1") : QStringLiteral("P2"),
                           eName);
    m_usi1->setSquelchResignLogging(false);

    // USI 起動（パス＋表示名）：座席に応じて正しいパス/名を渡す
    const QString ePath = engineIsP1 ? opt.enginePath1 : opt.enginePath2;
    const QString eDisp = engineIsP1 ? opt.engineName1 : opt.engineName2;

    // 既存の HvE 実装と互換を保つため、内部的には P1 側のスロットで起動する
    // （以降の go/stop はフック側で m_usi1 に送る運用）
    initializeAndStartEngineFor(P1, ePath, eDisp);
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

    // ★ EvE: 先手の1手目を棋譜欄へ反映
    if (m_clock) {
        const qint64 thinkMs = m_usi1 ? m_usi1->lastBestmoveElapsedMs() : 0;
        m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        m_clock->applyByoyomiAndResetConsideration1(); // byoyomi/Fischer適用 & 直近考慮秒の確定
    }
    if (m_hooks.appendKifuLine && m_clock) {
        m_hooks.appendKifuLine(rec1, m_clock->getPlayer1ConsiderationAndTotalTime());
    }

    // 盤描画＆手番表示
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(p1From, p1To);
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

    // ★ EvE: 後手の1手目を棋譜欄へ反映
    if (m_clock) {
        const qint64 thinkMs = m_usi2 ? m_usi2->lastBestmoveElapsedMs() : 0;
        m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        m_clock->applyByoyomiAndResetConsideration2();
    }
    if (m_hooks.appendKifuLine && m_clock) {
        m_hooks.appendKifuLine(rec2, m_clock->getPlayer2ConsiderationAndTotalTime());
    }

    // 反映
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(p2From, p2To);
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    QTimer::singleShot(std::chrono::milliseconds(0), this, &MatchCoordinator::kickNextEvETurn_);
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

    // ★ EvE: 今指した側（mover）を棋譜欄へ反映
    if (m_clock) {
        const qint64 thinkMs = mover ? mover->lastBestmoveElapsedMs() : 0;
        if (p1ToMove) {
            // いま指したのはP1
            m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration1();
        } else {
            // いま指したのはP2
            m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration2();
        }
    }
    if (m_hooks.appendKifuLine && m_clock) {
        const QString elapsed = p1ToMove
                                    ? m_clock->getPlayer1ConsiderationAndTotalTime()
                                    : m_clock->getPlayer2ConsiderationAndTotalTime();
        m_hooks.appendKifuLine(rec, elapsed);
    }

    // 相手エンジンに “同○” ヒント
    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    // 盤の再描画＆手番表示
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(from, to);
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    // 次の手をすぐ（イベントキュー経由で）回す
    QTimer::singleShot(0, this, &MatchCoordinator::kickNextEvETurn_);
}

// ===== 時間制御の設定／照会 =====

void MatchCoordinator::setTimeControlConfig(bool useByoyomi,
                                            int byoyomiMs1, int byoyomiMs2,
                                            int incMs1,     int incMs2,
                                            bool loseOnTimeout)
{
    m_tc.useByoyomi       = useByoyomi;
    m_tc.byoyomiMs1       = qMax(0, byoyomiMs1);
    m_tc.byoyomiMs2       = qMax(0, byoyomiMs2);
    m_tc.incMs1           = qMax(0, incMs1);
    m_tc.incMs2           = qMax(0, incMs2);
    m_tc.loseOnTimeout    = loseOnTimeout;
}

const MatchCoordinator::TimeControl& MatchCoordinator::timeControl() const {
    return m_tc;
}

// ===== エポック管理（1手の開始時刻） =====

void MatchCoordinator::markTurnEpochNowFor(Player side, qint64 nowMs /*=-1*/) {
    if (nowMs < 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    if (side == P1) m_turnEpochP1Ms = nowMs; else m_turnEpochP2Ms = nowMs;
}

qint64 MatchCoordinator::turnEpochFor(Player side) const {
    return (side == P1) ? m_turnEpochP1Ms : m_turnEpochP2Ms;
}

void MatchCoordinator::resetTurnEpochs() {
    m_turnEpochP1Ms = m_turnEpochP2Ms = -1;
}

// ===== ターン計測（HvH用の簡易ストップウォッチ） =====

void MatchCoordinator::armTurnTimerIfNeeded() {
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
    }
}

void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player mover) {
    if (!m_turnTimerArmed) return;
    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    if (m_clock) {
        if (mover == P1) m_clock->setPlayer1ConsiderationTime(static_cast<int>(ms));
        else             m_clock->setPlayer2ConsiderationTime(static_cast<int>(ms));
    }
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}

// ===== 人間側の計測（HvEでの人間手） =====

void MatchCoordinator::armHumanTimerIfNeeded() {
    if (!m_humanTimerArmed) {
        m_humanTurnTimer.start();
        m_humanTimerArmed = true;
    }
}

void MatchCoordinator::finishHumanTimerAndSetConsideration() {
    // どちらが「人間側」かは Main からのフックで取得（HvE想定）
    if (!m_hooks.humanPlayerSide) return;
    const Player side = m_hooks.humanPlayerSide();

    // ShogiClock 内部の考慮msをそのまま反映
    if (m_clock) {
        const qint64 clkMs = (side == P1) ? m_clock->player1ConsiderationMs()
                                          : m_clock->player2ConsiderationMs();
        if (side == P1) m_clock->setPlayer1ConsiderationTime(static_cast<int>(clkMs));
        else            m_clock->setPlayer2ConsiderationTime(static_cast<int>(clkMs));
    }
    if (m_humanTurnTimer.isValid()) m_humanTurnTimer.invalidate();
    m_humanTimerArmed = false;
}

void MatchCoordinator::disarmHumanTimerIfNeeded() {
    if (!m_humanTimerArmed) return;
    m_humanTimerArmed = false;
    m_humanTurnTimer.invalidate();
}

// ===== USI用 残時間算出 =====

MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes_() const {
    GoTimes t;

    // 残り（ms）
    const qint64 rawB = m_hooks.remainingMsFor ? qMax<qint64>(0, m_hooks.remainingMsFor(P1)) : 0;
    const qint64 rawW = m_hooks.remainingMsFor ? qMax<qint64>(0, m_hooks.remainingMsFor(P2)) : 0;

    if (m_tc.useByoyomi) {
        // 秒読み：btime/wtime はメイン残のみ。秒読み“適用中”なら 0 を送る。
        const bool bApplied = m_clock ? m_clock->byoyomi1Applied() : false;
        const bool wApplied = m_clock ? m_clock->byoyomi2Applied() : false;
        t.btime = bApplied ? 0 : rawB;
        t.wtime = wApplied ? 0 : rawW;
        t.byoyomi = (m_hooks.byoyomiMs ? m_hooks.byoyomiMs() : 0);
        t.binc = t.winc = 0;
    } else {
        // フィッシャー：pre-add に正規化（内部はpost-addなので1回だけ引く）
        t.btime = rawB;
        t.wtime = rawW;
        t.byoyomi = 0;
        t.binc = m_tc.incMs1;
        t.winc = m_tc.incMs2;
        if (t.binc > 0) t.btime = qMax<qint64>(0, t.btime - t.binc);
        if (t.winc > 0) t.wtime = qMax<qint64>(0, t.wtime - t.winc);
    }
    return t;
}

void MatchCoordinator::computeGoTimesForUSI(qint64& outB, qint64& outW) const {
    const GoTimes t = computeGoTimes_();
    outB = t.btime;
    outW = t.wtime;
}

void MatchCoordinator::refreshGoTimes() {
    qint64 b=0, w=0;
    computeGoTimesForUSI(b, w);
    m_bTimeStr = QString::number(b);
    m_wTimeStr = QString::number(w);
    emit timesForUSIUpdated(b, w);
}

int MatchCoordinator::computeMoveBudgetMsForCurrentTurn() const {
    const bool p1turn = (m_gc && m_gc->currentPlayer() == ShogiGameController::Player1);
    const int  mainMs = p1turn ? m_bTimeStr.toInt() : m_wTimeStr.toInt();
    const int  byoMs  = m_tc.useByoyomi ? (p1turn ? m_tc.byoyomiMs1 : m_tc.byoyomiMs2) : 0;
    return mainMs + byoMs;
}

void MatchCoordinator::setClock(ShogiClock* clock)
{
    if (m_clock == clock) return;
    unwireClock_();
    m_clock = clock;
    wireClock_();
}

void MatchCoordinator::onClockTick_()
{
    // デバッグ：ここが動いていれば Coordinator は時計を受信できている
    qDebug() << "[Match] onClockTick_()";
    emitTimeUpdateFromClock_();
}

void MatchCoordinator::pokeTimeUpdateNow()
{
    emitTimeUpdateFromClock_();
}

void MatchCoordinator::emitTimeUpdateFromClock_()
{
    if (!m_clock || !m_gc) return;

    const qint64 p1ms = m_clock->getPlayer1TimeIntMs();
    const qint64 p2ms = m_clock->getPlayer2TimeIntMs();
    const bool p1turn = (m_gc->currentPlayer() == ShogiGameController::Player1);

    const bool hasByoyomi = p1turn ? m_clock->hasByoyomi1()
                                   : m_clock->hasByoyomi2();
    const bool inByoyomi  = p1turn ? m_clock->byoyomi1Applied()
                                  : m_clock->byoyomi2Applied();
    const bool enableUrgency = (!hasByoyomi) || inByoyomi;

    const qint64 activeMs = p1turn ? p1ms : p2ms;
    const qint64 urgencyMs = enableUrgency ? activeMs
                                           : std::numeric_limits<qint64>::max();

    // デバッグ：UI へ送る値を確認
    qDebug() << "[Match] emit timeUpdated p1ms=" << p1ms << " p2ms=" << p2ms
             << " p1turn=" << p1turn << " urgencyMs=" << urgencyMs;

    emit timeUpdated(p1ms, p2ms, p1turn, urgencyMs);
}

void MatchCoordinator::wireClock_()
{
    if (!m_clock) return;
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }

    // 明示シグネチャにしておくと安心（ShogiClock 側の timeUpdated が多態でも解決）
    m_clockConn = connect(m_clock, &ShogiClock::timeUpdated,
                          this, &MatchCoordinator::onClockTick_,
                          Qt::UniqueConnection);
    Q_ASSERT(m_clockConn);
}

void MatchCoordinator::unwireClock_()
{
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }
}

void MatchCoordinator::clearGameOverState()
{
    const bool wasOver = m_gameOver.isOver;
    m_gameOver = GameOverState{}; // 全クリア
    if (wasOver) {
        emit gameOverStateChanged(m_gameOver);
        qDebug() << "[Match] clearGameOverState()";
    }
}

// 司令塔が終局を確定させる唯一の入口
void MatchCoordinator::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    if (m_gameOver.isOver) {
        qDebug() << "[MC] setGameOver() ignored: already over";
        return;
    }

    qDebug().nospace()
        << "[MC] setGameOver cause="
        << ((info.cause==Cause::Timeout)?"Timeout":"Resign")
        << " loser=" << ((info.loser==P1)?"P1":"P2")
        << " appendMoveOnce=" << appendMoveOnce;

    m_gameOver.isOver        = true;
    m_gameOver.hasLast       = true;
    m_gameOver.lastLoserIsP1 = loserIsP1;
    m_gameOver.lastInfo      = info;
    m_gameOver.when          = QDateTime::currentDateTime();

    emit gameOverStateChanged(m_gameOver);
    emit gameEnded(info);

    if (appendMoveOnce && !m_gameOver.moveAppended) {
        qDebug() << "[MC] emit requestAppendGameOverMove";
        emit requestAppendGameOverMove(info);
    }
}

void MatchCoordinator::markGameOverMoveAppended()
{
    if (!m_gameOver.isOver) return;
    if (m_gameOver.moveAppended) return;

    m_gameOver.moveAppended = true;
    emit gameOverStateChanged(m_gameOver);
    qDebug() << "[Match] markGameOverMoveAppended()";
}

void MatchCoordinator::sendGameOverWinAndQuitTo(int idx)
{
    Usi* target = (idx == 1 ? m_usi1 : m_usi2);
    if (!target) return;

    // HvE のときは opponent が nullptr の可能性があるが、WIN 側だけ送れば良い。
    target->sendGameOverWinAndQuitCommands();
}
