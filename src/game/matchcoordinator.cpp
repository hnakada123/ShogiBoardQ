/// @file matchcoordinator.cpp
/// @brief 対局進行コーディネータ（司令塔）クラスの実装

#include "matchcoordinator.h"
#include "gameendhandler.h"
#include "gamestartorchestrator.h"
#include "analysissessionhandler.h"
#include "strategycontext.h"
#include "gamemodestrategy.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "kifurecordlistmodel.h"
#include "logcategories.h"

#include <limits>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QtGlobal>
#include <QDateTime>
#include <QTimer>
#include <QMetaObject>
#include <QMetaMethod>
#include <QThread>
#include <QCoreApplication>

// 平手初期SFENの簡易判定（必要なら厳密化可）
static bool isStandardStartposSfen(const QString& sfen)
{
    const QString canon = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return (!sfen.isEmpty() && sfen.trimmed() == canon);
}

// 直前のフル position 文字列から、先頭 handCount 手だけ残したベースを作る。
// 例）prev="position startpos moves 7g7f 3c3d 2g2f 8c8d", handCount=2
//   → "position startpos moves 7g7f 3c3d"
static QString buildBasePositionUpToHands(const QString& prevFull, int handCount, const QString& startSfenHint)
{
    QString head;  // "position startpos" or "position sfen <...>"
    QStringList moves;

    // prevFull を優先して head/moves を抽出
    if (!prevFull.isEmpty()) {
        const QString trimmed = prevFull.trimmed();

        if (trimmed.startsWith(QStringLiteral("position startpos"))) {
            head = QStringLiteral("position startpos");
        } else if (trimmed.startsWith(QStringLiteral("position sfen "))) {
            // "position sfen " 以降の SFEN 先頭トークンまでを head として採用（末尾の moves 部は除外）
            // 単純に " moves " の前までを head とする
            const qsizetype idxMoves = trimmed.indexOf(QStringLiteral(" moves "));
            head = (idxMoves >= 0) ? trimmed.left(idxMoves) : trimmed; // moves 無ければ全文
        }

        // moves の抽出
        const qsizetype idxMoves2 = trimmed.indexOf(QStringLiteral(" moves "));
        if (idxMoves2 >= 0) {
            const QString after = trimmed.mid(idxMoves2 + 7); // 7 = strlen(" moves ")
            const QStringList toks = after.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString& t : toks) {
                if (!t.isEmpty()) moves.append(t);
            }
        }
    }

    // prevFull から head が取れなかった場合：SFENヒントで head を決める
    if (head.isEmpty()) {
        if (isStandardStartposSfen(startSfenHint)) {
            head = QStringLiteral("position startpos");
        } else if (!startSfenHint.isEmpty()) {
            head = QStringLiteral("position sfen %1").arg(startSfenHint);
        } else {
            head = QStringLiteral("position startpos"); // フォールバック
        }
    }

    // handCount でトリミング
    if (handCount <= 0) {
        return head; // moves なし
    }

    // moves が prevFull 由来で空（または prevFull が空）の場合は、そのまま head のみを返す
    if (moves.isEmpty()) {
        return head;
    }

    const qsizetype take = qMin(qsizetype(handCount), moves.size());
    QStringList headMoves = moves.mid(0, take);
    return QStringLiteral("%1 moves %2").arg(head, headMoves.join(QLatin1Char(' ')));
}

using P = MatchCoordinator::Player;

// ============================================================
// 初期化・破棄
// ============================================================

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

    m_sfenHistory = d.sfenRecord ? d.sfenRecord : &m_sharedSfenRecord;
    m_externalGameMoves = d.gameMoves;

    // デバッグ：どのリストを使うか明示
    qCDebug(lcGame).noquote()
        << "shared sfenRecord*=" << static_cast<const void*>(m_sfenHistory);

    // 共有ポインタが未指定の場合は内部履歴を使用
    if (!d.sfenRecord) {
        qCWarning(lcGame) << "Deps.sfenRecord is null. Using internal sfen history buffer.";
    }

    m_strategyCtx = std::make_unique<StrategyContext>(*this);

    wireClock();
}

MatchCoordinator::~MatchCoordinator() = default;

void MatchCoordinator::ensureAnalysisSession()
{
    if (m_analysisSession) return;
    m_analysisSession = std::make_unique<AnalysisSessionHandler>(this);

    // ハンドラのシグナルをMatchCoordinatorのシグナルに中継する
    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationModeEnded,
            this,                    &MatchCoordinator::considerationModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::tsumeSearchModeEnded,
            this,                    &MatchCoordinator::tsumeSearchModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationWaitingStarted,
            this,                    &MatchCoordinator::considerationWaitingStarted);

    // ハンドラ用コールバック設定
    AnalysisSessionHandler::Hooks hooks;
    hooks.showGameOverDialog = m_hooks.showGameOverDialog;
    hooks.destroyEnginesKeepModels = [this]() { destroyEngines(false); };
    m_analysisSession->setHooks(hooks);
}

void MatchCoordinator::ensureGameEndHandler()
{
    if (m_gameEndHandler) return;
    m_gameEndHandler = new GameEndHandler(this);

    // シグナル転送（GameEndHandler → MatchCoordinator）
    connect(m_gameEndHandler, &GameEndHandler::gameEnded,
            this,             &MatchCoordinator::gameEnded);

    // Refs 設定
    GameEndHandler::Refs refs;
    refs.gc        = m_gc;
    refs.clock     = m_clock;
    refs.usi1      = &m_usi1;
    refs.usi2      = &m_usi2;
    refs.playMode  = &m_playMode;
    refs.gameOver  = &m_gameOver;
    refs.strategyProvider = [this]() -> GameModeStrategy* { return m_strategy.get(); };
    refs.sfenHistory = m_sfenHistory;
    m_gameEndHandler->setRefs(refs);

    // Hooks 設定
    GameEndHandler::Hooks hooks;
    hooks.disarmHumanTimerIfNeeded = [this]() { disarmHumanTimerIfNeeded(); };
    hooks.primaryEngine     = [this]() -> Usi* { return primaryEngine(); };
    hooks.turnEpochFor      = [this](Player p) -> qint64 { return turnEpochFor(p); };
    hooks.setGameInProgressActions = [this](bool b) { setGameInProgressActions(b); };
    hooks.setGameOver       = [this](const GameEndInfo& info, bool loserIsP1, bool append) {
        setGameOver(info, loserIsP1, append);
    };
    hooks.markGameOverMoveAppended = [this]() { markGameOverMoveAppended(); };
    hooks.appendKifuLine    = m_hooks.appendKifuLine;
    hooks.showGameOverDialog = m_hooks.showGameOverDialog;
    hooks.log               = m_hooks.log;
    hooks.autoSaveKifuIfEnabled = [this]() {
        if (m_autoSaveKifu && !m_kifuSaveDir.isEmpty() && m_hooks.autoSaveKifu) {
            qCInfo(lcGame) << "Calling autoSaveKifu hook: dir=" << m_kifuSaveDir;
            m_hooks.autoSaveKifu(m_kifuSaveDir, m_playMode,
                                 m_humanName1, m_humanName2,
                                 m_engineNameForSave1, m_engineNameForSave2);
        }
    };
    m_gameEndHandler->setHooks(hooks);
}

void MatchCoordinator::ensureGameStartOrchestrator()
{
    if (m_gameStartOrchestrator) return;
    m_gameStartOrchestrator = std::make_unique<GameStartOrchestrator>();

    // Refs 設定
    GameStartOrchestrator::Refs refs;
    refs.gc                 = m_gc;
    refs.playMode           = &m_playMode;
    refs.maxMoves           = &m_maxMoves;
    refs.currentTurn        = &m_cur;
    refs.currentMoveIndex   = &m_currentMoveIndex;
    refs.autoSaveKifu       = &m_autoSaveKifu;
    refs.kifuSaveDir        = &m_kifuSaveDir;
    refs.humanName1         = &m_humanName1;
    refs.humanName2         = &m_humanName2;
    refs.engineNameForSave1 = &m_engineNameForSave1;
    refs.engineNameForSave2 = &m_engineNameForSave2;
    refs.positionStr1       = &m_positionStr1;
    refs.positionPonder1    = &m_positionPonder1;
    refs.positionStrHistory = &m_positionStrHistory;
    refs.allGameHistories   = &m_allGameHistories;
    m_gameStartOrchestrator->setRefs(refs);

    // Hooks 設定
    GameStartOrchestrator::Hooks hooks;
    hooks.initializeNewGame = m_hooks.initializeNewGame;
    hooks.setPlayersNames   = m_hooks.setPlayersNames;
    hooks.setEngineNames    = m_hooks.setEngineNames;
    hooks.setGameActions    = m_hooks.setGameActions;
    hooks.renderBoardFromGc = m_hooks.renderBoardFromGc;
    hooks.clearGameOverState = [this]() { clearGameOverState(); };
    hooks.updateTurnDisplay  = [this](Player p) { updateTurnDisplay(p); };
    hooks.initializePositionStringsForStart = [this](const QString& s) {
        initializePositionStringsForStart(s);
    };
    hooks.createAndStartModeStrategy = [this](const StartOptions& opt) {
        createAndStartModeStrategy(opt);
    };
    hooks.flipBoard = [this]() { flipBoard(); };
    hooks.startInitialEngineMoveIfNeeded = [this]() { startInitialEngineMoveIfNeeded(); };
    m_gameStartOrchestrator->setHooks(hooks);
}

MatchCoordinator::StrategyContext& MatchCoordinator::strategyCtx()
{
    return *m_strategyCtx;
}

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    m_usi1 = e1;
    m_usi2 = e2;
}

// ============================================================
// 投了・終局処理
// ============================================================

void MatchCoordinator::handleResign() {
    ensureGameEndHandler();
    m_gameEndHandler->handleResign();
}

void MatchCoordinator::handleEngineResign(int idx) {
    ensureGameEndHandler();
    m_gameEndHandler->handleEngineResign(idx);
}

void MatchCoordinator::handleEngineWin(int idx) {
    ensureGameEndHandler();
    m_gameEndHandler->handleEngineWin(idx);
}

void MatchCoordinator::handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw)
{
    ensureGameEndHandler();
    m_gameEndHandler->handleNyugyokuDeclaration(declarer, success, isDraw);
}

// ============================================================
// 盤面・表示ヘルパ
// ============================================================

void MatchCoordinator::flipBoard() {
    // 実際の反転は GUI 側で実施（レイアウト/ラベル入替等を考慮）
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    emit boardFlipped(true);
}

void MatchCoordinator::setGameInProgressActions(bool inProgress) {
    if (m_hooks.setGameActions) m_hooks.setGameActions(inProgress);
}

void MatchCoordinator::updateTurnDisplay(Player p) {
    m_cur = p;
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(p);
}

// displayResultsAndUpdateGui は GameEndHandler に移動済み

// ============================================================
// エンジン管理
// ============================================================

void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    Usi*& eng = (side == P1 ? m_usi1 : m_usi2);

    if (!eng) {
        // フォールバック: HvE のように m_usi1 のみの場合
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

    // 例外を投げない前提：失敗は内部でシグナル/ログ通知される
    eng->initializeAndStartEngineCommunication(path, name);

    // 投了シグナル配線（side に基づいて先手/後手を判断）
    wireResignToArbiter(eng, (side == P1));
    // 入玉宣言勝ちシグナル配線
    wireWinToArbiter(eng, (side == P1));
}

void MatchCoordinator::wireResignToArbiter(Usi* engine, bool asP1)
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

void MatchCoordinator::wireWinToArbiter(Usi* engine, bool asP1)
{
    if (!engine) return;

    // 既存接続の掃除は「該当シグナルのみ」を明示して行う
    QObject::disconnect(engine, &Usi::bestMoveWinReceived, this, nullptr);

    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &MatchCoordinator::onEngine1Win,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &MatchCoordinator::onEngine2Win,
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

void MatchCoordinator::onEngine1Win()
{
    // エンジン1が入玉宣言勝ち
    this->handleEngineWin(1);
}

void MatchCoordinator::onEngine2Win()
{
    this->handleEngineWin(2);
}

void MatchCoordinator::destroyEngine(int idx, bool clearThinking)
{
    Usi*& ref = (idx == 1 ? m_usi1 : m_usi2);
    if (ref) {
        // すべてのシグナル接続を切断して、削除中/削除後のコールバックを防ぐ
        ref->disconnect();
        // エンジンプロセスを同期的にクリーンアップ（quitコマンド送信とプロセス終了待ち）
        ref->cleanupEngineProcessAndThread(clearThinking);
        // deleteLater()を使用して安全に削除
        ref->deleteLater();
        ref = nullptr;
    }
}

void MatchCoordinator::destroyEngines(bool clearModels)
{
    qCDebug(lcGame).noquote() << "destroyEngines called, clearModels=" << clearModels;
    destroyEngine(1, clearModels);
    destroyEngine(2, clearModels);

    // モデルをクリア（削除はしない。次回対局で再利用するため）
    // これにより、対局を繰り返してもデータが蓄積しない
    // clearModels=false の場合はクリアしない（詰み探索完了後などで思考内容を保持したい場合）
    if (clearModels) {
        qCDebug(lcGame).noquote() << "destroyEngines clearing models";
        if (m_comm1)  m_comm1->clear();
        if (m_think1) m_think1->clearAllItems();
        if (m_comm2)  m_comm2->clear();
        if (m_think2) m_think2->clearAllItems();
    } else {
        qCDebug(lcGame).noquote() << "destroyEngines preserving models (clearModels=false)";
    }
}

void MatchCoordinator::setPlayMode(PlayMode m)
{
    m_playMode = m;
}

MatchCoordinator::EngineModelPair MatchCoordinator::ensureEngineModels(int engineIndex)
{
    UsiCommLogModel*&          commRef  = (engineIndex == 1) ? m_comm1  : m_comm2;
    ShogiEngineThinkingModel*& thinkRef = (engineIndex == 1) ? m_think1 : m_think2;

    if (!commRef) {
        commRef = new UsiCommLogModel(this);
        qCWarning(lcGame) << "comm" << engineIndex << "fallback created";
    }
    if (!thinkRef) {
        thinkRef = new ShogiEngineThinkingModel(this);
        qCWarning(lcGame) << "think" << engineIndex << "fallback created";
    }

    return { commRef, thinkRef };
}

void MatchCoordinator::initEnginesForEvE(const QString& engineName1,
                                         const QString& engineName2)
{
    // 既存エンジンの破棄
    destroyEngines();

    // モデル（GUI から貰えない場合はフォールバック生成）
    auto [comm1, think1] = ensureEngineModels(1);
    auto [comm2, think2] = ensureEngineModels(2);

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName1 = engineName1.isEmpty() ? QStringLiteral("Engine") : engineName1;
    const QString dispName2 = engineName2.isEmpty() ? QStringLiteral("Engine") : engineName2;
    comm1->setEngineName(dispName1);
    comm2->setEngineName(dispName2);

    // USI を生成（この時点ではプロセス未起動）
    m_usi1 = new Usi(comm1, think1, m_gc, m_playMode, this);
    m_usi2 = new Usi(comm2, think2, m_gc, m_playMode, this);

    // 状態初期化
    m_usi1->resetResignNotified(); m_usi1->clearHardTimeout(); m_usi1->resetWinNotified();
    m_usi2->resetResignNotified(); m_usi2->clearHardTimeout(); m_usi2->resetWinNotified();

    // 投了配線
    wireResignToArbiter(m_usi1, /*asP1=*/true);
    wireResignToArbiter(m_usi2, /*asP1=*/false);
    // 入玉宣言勝ち配線
    wireWinToArbiter(m_usi1, /*asP1=*/true);
    wireWinToArbiter(m_usi2, /*asP1=*/false);

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

    const GoTimes t = computeGoTimes();

    // byoyomi が設定されていれば USI 的には秒読みを使う
    const bool useByoyomi = (t.byoyomi > 0);

    // qint64 → int への安全な変換（オーバーフロー防止）
    auto clampMsToInt = [](qint64 ms) -> int {
        if (ms < 0) return 0;
        if (ms > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        return static_cast<int>(ms);
    };

    const QString btimeStr = QString::number(t.btime);
    const QString wtimeStr = QString::number(t.wtime);

    QPoint from(-1, -1), to(-1, -1);
    m_gc->setPromote(false);

    // 例外を投げない前提：失敗は内部でログ/シグナル通知済み
    engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionStr,              // 現局面（SFEN/position）
        ponderStr,                // ponder
        from, to,                 // 出力される移動先
        clampMsToInt(t.byoyomi),  // byoyomi ms (int)
        btimeStr,                 // btime (QString)
        wtimeStr,                 // wtime (QString)
        clampMsToInt(t.binc),     // 先手加算 (int)
        clampMsToInt(t.winc),     // 後手加算 (int)
        useByoyomi                // byoyomi 使用フラグ
        );

    if (outFrom) *outFrom = from;
    if (outTo)   *outTo   = to;

    // resign/win/draw 等では from/to が (-1,-1) のままになる想定 → false で中断
    auto isValidTo = [](const QPoint& p) {
        return (p.x() >= 1 && p.x() <= 9 && p.y() >= 1 && p.y() <= 9);
    };
    if (!isValidTo(to)) {
        qCDebug(lcGame) << "engineThinkApplyMove: no legal 'to' returned (resign/abort?). from="
                        << from << "to=" << to;
        if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] engineThinkApplyMove: no legal move (resign/abort?)"));
        return false;
    }

    // from は 1..9（盤上）または 10/11（打ち駒）を許容。細かい妥当性は validateAndMove に委譲。
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
    qCDebug(lcGame) << "engineMoveOnce enter"
                     << "engineIndex=" << engineIndex
                     << "moverBefore=" << int(moverBefore)
                     << "thread=" << QThread::currentThread();

    QPoint from, to;
    if (!engineThinkApplyMove(eng, positionStr, ponderStr, &from, &to)) {
        qCWarning(lcGame) << "engineThinkApplyMove FAILED";
        return false;
    }
    qCDebug(lcGame) << "engineThinkApplyMove OK from=" << from << "to=" << to;

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    switch (moverBefore) {
    case ShogiGameController::Player1:
        qCDebug(lcGame) << "calling appendEvalP1";
        if (m_hooks.appendEvalP1) m_hooks.appendEvalP1();
        else qCWarning(lcGame) << "appendEvalP1 NOT set";
        break;
    case ShogiGameController::Player2:
        qCDebug(lcGame) << "calling appendEvalP2";
        if (m_hooks.appendEvalP2) m_hooks.appendEvalP2();
        else qCWarning(lcGame) << "appendEvalP2 NOT set";
        break;
    default:
        qCWarning(lcGame) << "moverBefore=NoPlayer -> skip eval append";
        break;
    }

    if (outTo) *outTo = to;
    return true;
}

// ============================================================
// 対局開始フロー（GameStartOrchestrator へ委譲）
// ============================================================

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->configureAndStart(opt);
}

// createAndStartModeStrategy は Strategy 生成が MC に密結合のため残留
void MatchCoordinator::createAndStartModeStrategy(const StartOptions& opt)
{
    m_strategy.reset();
    switch (m_playMode) {
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine: {
        // エンジン同士は両方起動してから開始
        initEnginesForEvE(opt.engineName1, opt.engineName2);
        initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
        initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);
        m_strategy = std::make_unique<EngineVsEngineStrategy>(*m_strategyCtx, opt, this);
        m_strategy->start();
        break;
    }

    // 平手：先手エンジン（P1エンジン固定）
    case PlayMode::EvenEngineVsHuman: {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, /*engineIsP1=*/true, opt.enginePath1, opt.engineName1);
        m_strategy->start();
        break;
    }

    // 平手：後手エンジン（P2エンジン固定）
    case PlayMode::EvenHumanVsEngine: {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, /*engineIsP1=*/false, opt.enginePath2, opt.engineName2);
        m_strategy->start();
        break;
    }

    // 駒落ち：先手エンジン（下手＝P1エンジン固定）
    case PlayMode::HandicapEngineVsHuman: {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, /*engineIsP1=*/true, opt.enginePath1, opt.engineName1);
        m_strategy->start();
        break;
    }

    // 駒落ち：後手エンジン（上手＝P2エンジン固定）
    case PlayMode::HandicapHumanVsEngine: {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, /*engineIsP1=*/false, opt.enginePath2, opt.engineName2);
        m_strategy->start();
        break;
    }

    case PlayMode::HumanVsHuman:
        m_strategy = std::make_unique<HumanVsHumanStrategy>(*m_strategyCtx);
        m_strategy->start();
        break;

    default:
        qCWarning(lcGame).noquote()
            << "configureAndStart: unexpected playMode="
            << static_cast<int>(m_playMode);
        break;
    }
}

Usi* MatchCoordinator::primaryEngine() const
{
    return m_usi1;
}

Usi* MatchCoordinator::secondaryEngine() const
{
    return m_usi2;
}

// ============================================================
// 時間制御の設定／照会
// ============================================================

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

// ============================================================
// エポック管理（1手の開始時刻）
// ============================================================

void MatchCoordinator::markTurnEpochNowFor(Player side, qint64 nowMs /*=-1*/) {
    if (nowMs < 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    if (side == P1) m_turnEpochP1Ms = nowMs; else m_turnEpochP2Ms = nowMs;
}

qint64 MatchCoordinator::turnEpochFor(Player side) const {
    return (side == P1) ? m_turnEpochP1Ms : m_turnEpochP2Ms;
}

// ============================================================
// ターン計測（HvH用の簡易ストップウォッチ）
// ============================================================

void MatchCoordinator::armTurnTimerIfNeeded() {
    if (m_strategy) m_strategy->armTurnTimerIfNeeded();
}

void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player mover) {
    if (m_strategy) m_strategy->finishTurnTimerAndSetConsideration(static_cast<int>(mover));
}

void MatchCoordinator::disarmHumanTimerIfNeeded() {
    if (m_strategy) m_strategy->disarmTurnTimer();
}

// ============================================================
// USI用 残時間算出
// ============================================================


MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const {
    GoTimes t;

    const bool hasRemainHook = static_cast<bool>(m_hooks.remainingMsFor);
    const bool hasIncHook    = static_cast<bool>(m_hooks.incrementMsFor);
    const bool hasByoHook    = static_cast<bool>(m_hooks.byoyomiMs);

    // 残り（ms）
    const qint64 rawB = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P1)) : 0;
    const qint64 rawW = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P2)) : 0;

    // デバッグ（入力値）
    qCDebug(lcGame).noquote()
        << "computeGoTimes_: hooks{remain=" << hasRemainHook
        << ", inc=" << hasIncHook
        << ", byo=" << hasByoHook
        << "} rawB=" << rawB << " rawW=" << rawW
        << " useByoyomi=" << m_tc.useByoyomi;

    if (m_tc.useByoyomi) {
        // 秒読み：btime/wtime はメイン残のみ。秒読み“適用中”なら 0 を送る。
        const bool bApplied = m_clock ? m_clock->byoyomi1Applied() : false;
        const bool wApplied = m_clock ? m_clock->byoyomi2Applied() : false;

        t.btime = bApplied ? 0 : rawB;
        t.wtime = wApplied ? 0 : rawW;
        t.byoyomi = (hasByoHook ? m_hooks.byoyomiMs() : 0);
        t.binc = t.winc = 0;

        qCDebug(lcGame).noquote()
            << "computeGoTimes_: BYO"
            << " bApplied=" << bApplied << " wApplied=" << wApplied
            << " => btime=" << t.btime << " wtime=" << t.wtime
            << " byoyomi=" << t.byoyomi;
    } else {
        // フィッシャー：btime/wtime は残り。inc は m_tc で保持。
        t.btime = rawB;
        t.wtime = rawW;
        t.byoyomi = 0;
        t.binc = m_tc.incMs1;
        t.winc = m_tc.incMs2;

        // （ポリシー）送信直前に増加分を引いてから渡す
        if (t.binc > 0) t.btime = qMax<qint64>(0, t.btime - t.binc);
        if (t.winc > 0) t.wtime = qMax<qint64>(0, t.wtime - t.winc);

        qCDebug(lcGame).noquote()
            << "computeGoTimes_: FISCHER"
            << " => btime=" << t.btime << " wtime=" << t.wtime
            << " binc=" << t.binc << " winc=" << t.winc;
    }

    return t;
}

void MatchCoordinator::computeGoTimesForUSI(qint64& outB, qint64& outW) const {
    const GoTimes t = computeGoTimes();
    outB = t.btime;
    outW = t.wtime;
}

void MatchCoordinator::refreshGoTimes() {
    qint64 b=0, w=0;
    computeGoTimesForUSI(b, w);
}

void MatchCoordinator::setClock(ShogiClock* clock)
{
    if (m_clock == clock) return;
    unwireClock();
    m_clock = clock;
    wireClock();
}

void MatchCoordinator::onClockTick()
{
    // デバッグ：ここが動いていれば Coordinator は時計を受信できている
    qCDebug(lcGame) << "onClockTick()";
    emitTimeUpdateFromClock();
}

void MatchCoordinator::pokeTimeUpdateNow()
{
    emitTimeUpdateFromClock();
}

void MatchCoordinator::emitTimeUpdateFromClock()
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
    qCDebug(lcGame) << "emit timeUpdated p1ms=" << p1ms << " p2ms=" << p2ms
             << " p1turn=" << p1turn << " urgencyMs=" << urgencyMs;

    emit timeUpdated(p1ms, p2ms, p1turn, urgencyMs);
}

void MatchCoordinator::wireClock()
{
    if (!m_clock) return;
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }

    // 明示シグネチャにしておくと安心（ShogiClock 側の timeUpdated が多態でも解決）
    m_clockConn = connect(m_clock, &ShogiClock::timeUpdated,
                          this, &MatchCoordinator::onClockTick,
                          Qt::UniqueConnection);
    Q_ASSERT(m_clockConn);
}

void MatchCoordinator::unwireClock()
{
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }
}

void MatchCoordinator::clearGameOverState()
{
    const bool wasOver = m_gameOver.isOver;
    m_gameOver = GameOverState{}; // 全クリア
    if (wasOver) {
        emit gameOverStateChanged(m_gameOver);
        qCDebug(lcGame) << "clearGameOverState()";
    }
}

// 司令塔が終局を確定させる唯一の入口
void MatchCoordinator::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    if (m_gameOver.isOver) {
        qCDebug(lcGame) << "setGameOver() ignored: already over";
        return;
    }

    qCDebug(lcGame).nospace()
        << "setGameOver cause="
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
        qCDebug(lcGame) << "emit requestAppendGameOverMove";
        emit requestAppendGameOverMove(info);
    }
}

void MatchCoordinator::markGameOverMoveAppended()
{
    if (!m_gameOver.isOver) return;
    if (m_gameOver.moveAppended) return;

    m_gameOver.moveAppended = true;
    emit gameOverStateChanged(m_gameOver);
    qCDebug(lcGame) << "markGameOverMoveAppended()";
}

// 投了と同様に"対局の実体"として中断を一元処理
void MatchCoordinator::handleBreakOff()
{
    // エンジンの投了シグナルを切断（レースコンディション防止）
    if (m_usi1) QObject::disconnect(m_usi1, &Usi::bestMoveResignReceived, this, nullptr);
    if (m_usi2) QObject::disconnect(m_usi2, &Usi::bestMoveResignReceived, this, nullptr);

    ensureGameEndHandler();
    m_gameEndHandler->handleBreakOff();
}

// 検討を開始する（単発エンジンセッション）
// - 既存の HvE と同じく m_usi1 を使用し、表示モデルは #1 スロットに流す。
// - resign シグナルは P1 扱いで司令塔（Arbiter）に配線する（検討でも安全側）。
void MatchCoordinator::startAnalysis(const AnalysisOptions& opt)
{
    ensureAnalysisSession();

    qCDebug(lcGame).noquote() << "startAnalysis ENTER:"
                       << "mode=" << static_cast<int>(opt.mode)
                       << "byoyomiMs=" << opt.byoyomiMs
                       << "multiPV=" << opt.multiPV
                       << "inConsideration=" << m_analysisSession->isInConsiderationMode()
                       << "m_engineShutdownInProgress=" << m_engineShutdownInProgress;

    // エンジン破棄中の場合は検討開始を拒否（再入防止）
    if (m_engineShutdownInProgress) {
        qCDebug(lcGame).noquote() << "startAnalysis: engine shutdown in progress, ignoring request";
        return;
    }

    // 1) モード設定（検討 / 詰み探索）
    setPlayMode(opt.mode); // PlayMode::ConsiderationMode or PlayMode::TsumiSearchMode

    // 2) 既存エンジンを破棄して新規作成（毎回エンジンを起動する）
    qCDebug(lcGame).noquote() << "startAnalysis: destroying old engines and starting new:" << opt.enginePath;
    destroyEngines();

    // deleteLater()で予約された削除を処理してから新エンジンを作成
    // これにより、古いエンジンオブジェクトが完全に破棄されてからメモリを再利用する
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // 3) 表示モデル（無ければ生成して保持）
    auto [comm, think] = ensureEngineModels(1);
    comm->setEngineName(opt.engineName);

    // 4) 単発エンジン生成（常に m_usi1 を使用）
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);

    // 既存の接続（bestmove等）はそのまま。エラー用だけスロット接続を追加。
    connect(m_usi1, &Usi::errorOccurred,
            this,   &MatchCoordinator::onUsiError,
            Qt::UniqueConnection);

    // 5) 投了配線
    wireResignToArbiter(m_usi1, /*asP1=*/true);
    // 入玉宣言勝ち配線
    wireWinToArbiter(m_usi1, /*asP1=*/true);

    // 6) ログ識別
    m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), opt.engineName);
    m_usi1->setSquelchResignLogging(false);

    // 7) USI 初期化＆起動（例外非使用前提）
    initializeAndStartEngineFor(P1, opt.enginePath, opt.engineName);

    // 8) UI 側にエンジン名を通知（必要時）
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName, QString());

    // 8.5) 開始局面に至った最後の指し手を設定（読み筋表示ウィンドウのハイライト用）
    if (!opt.lastUsiMove.isEmpty()) {
        m_usi1->setLastUsiMove(opt.lastUsiMove);
    }

    // 9) モード固有の配線と状態保存（AnalysisSessionHandler へ委譲）
    m_analysisSession->setupModeSpecificWiring(m_usi1, opt);

    // 10) 解析/詰み探索の通信開始（AnalysisSessionHandler へ委譲）
    m_analysisSession->startCommunication(m_usi1, opt);

    qCDebug(lcGame).noquote() << "startAnalysis EXIT";
}

void MatchCoordinator::stopAnalysisEngine()
{
    qCDebug(lcGame).noquote() << "stopAnalysisEngine called";

    // エンジン破棄中フラグをセット（検討再開の再入防止）
    m_engineShutdownInProgress = true;

    // 検討/詰み探索モードのフラグリセットとシグナル発火（AnalysisSessionHandler へ委譲）
    ensureAnalysisSession();
    m_analysisSession->handleStop();

    destroyEngines(false);  // 思考内容を保持

    // エンジン破棄完了後にフラグをリセット
    m_engineShutdownInProgress = false;
}

void MatchCoordinator::updateConsiderationMultiPV(int multiPV)
{
    ensureAnalysisSession();
    m_analysisSession->updateMultiPV(m_usi1, multiPV);
}

bool MatchCoordinator::updateConsiderationPosition(const QString& newPositionStr,
                                                   int previousFileTo, int previousRankTo,
                                                   const QString& lastUsiMove)
{
    ensureAnalysisSession();
    return m_analysisSession->updatePosition(m_usi1, newPositionStr,
                                             previousFileTo, previousRankTo, lastUsiMove);
}

void MatchCoordinator::onUsiError(const QString& msg)
{
    // ログへ（あれば）
    if (m_hooks.log) m_hooks.log(QStringLiteral("[USI-ERROR] ") + msg);
    // 実行中の USI オペを明示的に打ち切る
    if (m_usi1) m_usi1->cancelCurrentOperation();
    if (m_usi2) m_usi2->cancelCurrentOperation();

    // 詰み探索・検討モード中のエラー復旧（AnalysisSessionHandler へ委譲）
    ensureAnalysisSession();
    if (m_analysisSession->handleEngineError(msg)) {
        return;
    }

    // 対局中にエンジンがクラッシュした場合の復旧処理
    // （handleBreakOffは m_gameOver.isOver ガードがあるので重複呼び出しも安全）
    if (!m_gameOver.isOver) {
        switch (m_playMode) {
        case PlayMode::EvenHumanVsEngine:
        case PlayMode::EvenEngineVsHuman:
        case PlayMode::EvenEngineVsEngine:
        case PlayMode::HandicapHumanVsEngine:
        case PlayMode::HandicapEngineVsHuman:
        case PlayMode::HandicapEngineVsEngine:
            handleBreakOff();
            if (m_hooks.showGameOverDialog) {
                m_hooks.showGameOverDialog(tr("対局中断"), tr("エンジンエラー: %1").arg(msg));
            }
            return;
        default:
            break;
        }
    }
}

// ============================================================
// USI position文字列の初期化
// ============================================================

void MatchCoordinator::initializePositionStringsForStart(const QString& sfenStart)
{
    initPositionStringsFromSfen(sfenStart);
}

void MatchCoordinator::initPositionStringsFromSfen(const QString& sfenBase)
{
    // m_positionStr1/m_positionPonder1 だけ使う（単発エンジン系）
    m_positionStr1.clear();
    m_positionPonder1.clear();

    // USIのposition履歴はSFENと混ぜないため、対局開始ごとにクリア
    m_positionStrHistory.clear();

    QString base = sfenBase;
    if (base.isEmpty()) {
        // フォールバックは startpos
        m_positionStr1    = QStringLiteral("position startpos moves");
        m_positionPonder1 = m_positionStr1;
        return;
    }

    // "position sfen ..." 形式に正規化
    // 既に "position " で始まっていればそのまま使う
    if (base.startsWith(QLatin1String("position "))) {
        m_positionStr1    = base;
        m_positionPonder1 = base;
    }
    // 平手初期局面のSFENと一致する場合は "position startpos" に正規化
    else if (isStandardStartposSfen(base)) {
        m_positionStr1    = QStringLiteral("position startpos");
        m_positionPonder1 = m_positionStr1;
    }
    else {
        m_positionStr1    = QStringLiteral("position sfen %1").arg(base);
        m_positionPonder1 = m_positionStr1;
    }
}

void MatchCoordinator::startInitialEngineMoveIfNeeded()
{
    if (m_strategy) m_strategy->startInitialMoveIfNeeded();
}

void MatchCoordinator::setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks) {
    u_ = refs;
    h_ = hooks;
}

bool MatchCoordinator::undoTwoPlies()
{
    if (!m_gc) return false;

    // --- ロールバック前のフル position を退避 ---
    QString prevFullPosition;
    if (u_.positionStrList && !u_.positionStrList->isEmpty()) {
        prevFullPosition = u_.positionStrList->last();
    } else if (!m_positionStrHistory.isEmpty()) {
        prevFullPosition = m_positionStrHistory.constLast();
    }

    // --- SFEN履歴の現在位置を把握 ---
    // m_sfenHistory はコンストラクタで必ず有効（null なら内部バッファにフォールバック済み）
    QStringList* srec = m_sfenHistory;
    int curSfenIdx = -1;

    if (!srec->isEmpty()) {
        curSfenIdx = static_cast<int>(srec->size() - 1); // 末尾が現局面
    } else if (u_.currentMoveIndex) {
        curSfenIdx = *u_.currentMoveIndex + 1; // 行0始まり → SFENは開始局面含むので+1
    } else {
        curSfenIdx = static_cast<int>(gameMovesRef().size() + 1);
    }

    // 2手戻し後の SFEN 添字（= 残すべき手数）
    const int targetSfenIdx = qMax(0, curSfenIdx - 2);   // 0 = 開始局面
    const int remainHands   = qMax(0, targetSfenIdx);    // 残すべき手数（開始=0→手数=N）

    // --- 盤面を targetSfenIdx に復元 ---
    if (targetSfenIdx < srec->size()) {
        const QString sfen = srec->at(targetSfenIdx);
        if (m_gc->board()) {
            m_gc->board()->setSfen(sfen);
        }
        const bool sideToMoveIsBlack = sfen.contains(QStringLiteral(" b "));
        m_gc->setCurrentPlayer(sideToMoveIsBlack ? ShogiGameController::Player1
                                                 : ShogiGameController::Player2);
    }

    // --- 棋譜/モデル/履歴を末尾2件ずつ削除 ---
    if (u_.recordModel) tryRemoveLastItems(u_.recordModel, 2);
    auto& gm = gameMovesRef();
    if (gm.size() >= 2) {
        gm.remove(gm.size() - 2, 2);
    }
    if (u_.positionStrList && u_.positionStrList->size() >= 2) {
        u_.positionStrList->remove(u_.positionStrList->size() - 2, 2);
    }
    if (srec->size() >= 2) {
        srec->remove(srec->size() - 2, 2);
    }

    // --- USI用 position 履歴も2手ぶん巻き戻す ---
    if (m_positionStrHistory.size() >= 2) {
        m_positionStrHistory.remove(m_positionStrHistory.size() - 2, 2);
    } else {
        m_positionStrHistory.clear();
    }

    // 巻き戻し後の現在ベースを厳密に再構成（prevを先頭remainHands手にトリム）
    const QString startSfen0 = !srec->isEmpty() ? srec->first() : QString();
    const QString nextBase   = buildBasePositionUpToHands(prevFullPosition, remainHands, startSfen0);

    // 現在値と履歴に反映
    m_positionStr1    = nextBase;
    m_positionPonder1 = nextBase;

    if (u_.positionStrList) {
        // GUI側履歴の末尾を nextBase に整合（末尾が無い/異なる場合は置き換え）
        if (u_.positionStrList->isEmpty()) {
            u_.positionStrList->append(nextBase);
        } else {
            // 末尾を差し替え（「待った」直後の基底を明示的に保持）
            (*u_.positionStrList)[u_.positionStrList->size() - 1] = nextBase;
        }
    }

    if (m_positionStrHistory.isEmpty() || m_positionStrHistory.constLast() != nextBase) {
        m_positionStrHistory.clear();
        m_positionStrHistory.append(nextBase);
    }

    // --- 現在行（0始まり）を同期 ---
    const int targetMoveRow = qMax(0, targetSfenIdx - 1);
    if (u_.currentMoveIndex) {
        *u_.currentMoveIndex = targetMoveRow;
    }

    // --- 表示/ハイライトの同期 ---
    if (u_.boardCtl) u_.boardCtl->clearAllHighlights();
    if (h_.updateHighlightsForPly) h_.updateHighlightsForPly(targetSfenIdx);
    if (h_.updateTurnAndTimekeepingDisplay) h_.updateTurnAndTimekeepingDisplay();

    // --- 入力許可（人間手番なら盤クリックOK） ---
    const auto stm = m_gc->currentPlayer();
    const bool humanNow = h_.isHumanSide ? h_.isHumanSide(stm) : false;
    if (m_view) m_view->setMouseClickMode(humanNow);

    return true;
}

bool MatchCoordinator::tryRemoveLastItems(QObject* model, int n) {
    if (!model) return false;

    // まずは直接呼び出し（ダウンキャスト）
    if (auto* km = qobject_cast<KifuRecordListModel*>(model)) {
        return km->removeLastItems(n);
    }

    // フォールバック：メタ呼び出し（Q_INVOKABLE/slot を拾える）
    const QMetaObject* mo = model->metaObject();
    const int idx = mo->indexOfMethod("removeLastItems(int)");
    if (idx < 0) return false;

    bool ok = QMetaObject::invokeMethod(model, "removeLastItems", Q_ARG(int, n));
    return ok;
}

// ============================================================
// 対局開始オプション構築（GameStartOrchestrator へ委譲）
// ============================================================

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialog* dlg) const
{
    return GameStartOrchestrator::buildStartOptions(mode, startSfenStr, sfenRecord, dlg);
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->ensureHumanAtBottomIfApplicable(dlg, bottomIsP1);
}

void MatchCoordinator::prepareAndStartGame(PlayMode mode,
                                           const QString& startSfenStr,
                                           const QStringList* sfenRecord,
                                           const StartGameDialog* dlg,
                                           bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->prepareAndStartGame(mode, startSfenStr, sfenRecord, dlg, bottomIsP1);
}

void MatchCoordinator::startMatchTimingAndMaybeInitialGo()
{
    // タイマー起動
    if (m_clock) m_clock->startClock();

    // 初手がエンジンなら go
    startInitialEngineMoveIfNeeded();
}

void MatchCoordinator::handleTimeUpdated()
{
}

void MatchCoordinator::handlePlayerTimeOut(int player)
{
    if (!m_gc) return;
    // 負け処理を司令塔で集約
    m_gc->applyTimeoutLossFor(player);
    handleGameEnded();
}

void MatchCoordinator::handleGameEnded()
{
    if (!m_gc) return;
    m_gc->finalizeGameResult();
}

// これに置き換え（該当関数のみ）
void MatchCoordinator::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    turnText.clear(); p1.clear(); p2.clear();
    if (!m_clock) return;

    // 手番テキスト：GCがあれば currentPlayer() で判定
    if (m_gc) {
        const bool p1Turn = (m_gc->currentPlayer() == ShogiGameController::Player1);
        turnText = p1Turn ? QObject::tr("先手番") : QObject::tr("後手番");
    }

    // 残り時間：ShogiClockの既存ゲッターを利用
    const qint64 t1ms = qMax<qint64>(0, m_clock->getPlayer1TimeIntMs());
    const qint64 t2ms = qMax<qint64>(0, m_clock->getPlayer2TimeIntMs());

    auto mmss = [](qint64 ms) {
        const qint64 s = ms / 1000;
        const qint64 m = s / 60;
        const qint64 r = s % 60;
        return QStringLiteral("%1:%2")
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(r, 2, 10, QLatin1Char('0'));
    };

    p1 = mmss(t1ms);
    p2 = mmss(t2ms);
}

PlayMode MatchCoordinator::playMode() const
{
    return m_playMode;
}

void MatchCoordinator::appendGameOverLineAndMark(Cause cause, Player loser)
{
    ensureGameEndHandler();
    m_gameEndHandler->appendGameOverLineAndMark(cause, loser);
}

void MatchCoordinator::onHumanMove(const QPoint& from, const QPoint& to,
                                   const QString& prettyMove)
{
    if (m_strategy) m_strategy->onHumanMove(from, to, prettyMove);
}

void MatchCoordinator::forceImmediateMove()
{
    if (!m_gc) return;

    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) || (m_playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        // 現在手番のエンジンへ stop
        const Player turn =
            (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
        Usi* eng = (turn == P1) ? m_usi1 : m_usi2;
        if (eng && m_hooks.sendStopToEngine) {
            m_hooks.sendStopToEngine(eng);
        }
        return;
    }

    // HvE の場合は主エンジンへ stop
    if (Usi* eng = primaryEngine()) {
        if (m_hooks.sendStopToEngine) {
            m_hooks.sendStopToEngine(eng);
        }
    }
}

void MatchCoordinator::sendRawTo(Usi* which, const QString& cmd)
{
    if (!which) return;
    which->sendRaw(cmd);   // ← sendRawCommand ではなく sendRaw を呼ぶ
}

namespace {
// qint64 → int の安全な縮小（オーバーフロー防止）
inline int clampMsToInt(qint64 v) {
    if (v > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
    if (v < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
    return static_cast<int>(v);
}
}

void MatchCoordinator::sendGoToEngine(Usi* which, const GoTimes& t)
{
    if (!which) return;

    // byoyomi と increment は通常どちらか。両方指定なら increment 優先など
    // ポリシーは必要に応じて調整。ここでは「両方ゼロでないなら increment 優先」にします。
    const bool useByoyomi = (t.byoyomi > 0 && t.binc == 0 && t.winc == 0);

    which->sendGoCommand(
        clampMsToInt(t.byoyomi),        // byoyomi(ms)
        QString::number(t.btime),       // btime(ms)
        QString::number(t.wtime),       // wtime(ms)
        clampMsToInt(t.binc),           // 先手 increment(ms)
        clampMsToInt(t.winc),           // 後手 increment(ms)
        useByoyomi                      // byoyomi を使うか
        );
}

void MatchCoordinator::sendStopToEngine(Usi* which)
{
    if (!which) return;
    which->sendStopCommand();
}

void MatchCoordinator::sendRawToEngine(Usi* which, const QString& cmd)
{
    if (!which) return;
    which->sendRaw(cmd);
}

void MatchCoordinator::appendBreakOffLineAndMark()
{
    ensureGameEndHandler();
    m_gameEndHandler->appendBreakOffLineAndMark();
}

void MatchCoordinator::handleMaxMovesJishogi()
{
    ensureGameEndHandler();
    m_gameEndHandler->handleMaxMovesJishogi();
}

bool MatchCoordinator::checkAndHandleSennichite()
{
    ensureGameEndHandler();
    return m_gameEndHandler->checkAndHandleSennichite();
}

void MatchCoordinator::handleSennichite()
{
    ensureGameEndHandler();
    m_gameEndHandler->handleSennichite();
}

void MatchCoordinator::handleOuteSennichite(bool p1Loses)
{
    ensureGameEndHandler();
    m_gameEndHandler->handleOuteSennichite(p1Loses);
}

ShogiClock* MatchCoordinator::clock()
{
    return m_clock;
}

const ShogiClock* MatchCoordinator::clock() const
{
    return m_clock;
}
