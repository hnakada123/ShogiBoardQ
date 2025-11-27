#include "matchcoordinator.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "startgamedialog.h"
#include "enginesettingsconstants.h"
#include "kifurecordlistmodel.h"
#include "sfenpositiontracer.h"

#include <limits>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QtGlobal>
#include <QDateTime>
#include <QTimer>
#include <QMetaObject>
#include <QMetaMethod>
#include <QSettings>

static inline int clampMsToIntLocal(qint64 v) {
    if (v > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
    if (v < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
    return static_cast<int>(v);
}

// 平手初期SFENの簡易判定（必要なら厳密化可）
static bool isStandardStartposSfen_(const QString& sfen)
{
    const QString canon = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return (!sfen.isEmpty() && sfen.trimmed() == canon);
}

// 直前のフル position 文字列から、先頭 handCount 手だけ残したベースを作る。
// 例）prev="position startpos moves 7g7f 3c3d 2g2f 8c8d", handCount=2
//   → "position startpos moves 7g7f 3c3d"
static QString buildBasePositionUpToHands_(const QString& prevFull, int handCount, const QString& startSfenHint)
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
            const int idxMoves = trimmed.indexOf(QStringLiteral(" moves "));
            head = (idxMoves >= 0) ? trimmed.left(idxMoves) : trimmed; // moves 無ければ全文
        }

        // moves の抽出
        const int idxMoves = trimmed.indexOf(QStringLiteral(" moves "));
        if (idxMoves >= 0) {
            const QString after = trimmed.mid(idxMoves + 7); // 7 = strlen(" moves ")
            const QStringList toks = after.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString& t : toks) {
                if (!t.isEmpty()) moves.append(t);
            }
        }
    }

    // prevFull から head が取れなかった場合：SFENヒントで head を決める
    if (head.isEmpty()) {
        if (isStandardStartposSfen_(startSfenHint)) {
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

    const int take = qMin(handCount, moves.size());
    QStringList headMoves = moves.mid(0, take);
    return QStringLiteral("%1 moves %2").arg(head, headMoves.join(QLatin1Char(' ')));
}

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

    // ★共有SFENリストを受け取る
    m_sfenRecord = d.sfenRecord;

    // デバッグ：どのリストを使うか明示
    qInfo().noquote()
        << "[MC][init] shared sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
        << " eveSfenRecord@=" << static_cast<const void*>(&m_eveSfenRecord);

    // 念のため NPE ガード（無いと困る設計なのでログだけ）
    if (!m_sfenRecord) {
        qWarning() << "[MC][init] sfenRecord is null! Presenterと同期できません。Deps.sfenRecordを渡してください。";
    }

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
    ShogiGameController::Player start = ShogiGameController::Player1;
    if (!sfenStart.isEmpty()) {
        const auto parts = sfenStart.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2 && (parts[1] == QLatin1String("w") || parts[1] == QLatin1String("W"))) {
            start = ShogiGameController::Player2;
        }
    }

    // GC に反映（currentPlayerChanged → TurnManager は恒常接続で自動伝播）
    if (m_gc) m_gc->setCurrentPlayer(start);

    // 司令塔の手番も同期（既存のまま）
    m_cur = (m_gc && m_gc->currentPlayer() == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay_(m_cur);

    if (m_hooks.log) m_hooks.log(QStringLiteral("MatchCoordinator: startNewGame done"));
    emit gameStarted();
}

// 1) 人間側の投了
void MatchCoordinator::handleResign() {
    GameEndInfo info;
    info.cause = Cause::Resignation;

    // 投了は「現在手番側」が行う：GCの現在手番から判定
    info.loser = (m_gc && m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
    //const Player winner = (m_cur == P1 ? P2 : P1);

    // エンジンへの最終通知（HvE / EvE の両方に対応）
    if (m_hooks.sendRawToEngine) {
        if (m_usi1 && !m_usi2) {
            // HvE：エンジンは m_usi1。人間が投了＝エンジン勝ち。
            m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
            m_hooks.sendRawToEngine(m_usi1, QStringLiteral("quit")); // 再戦しないなら送る
        } else {
            // ★ 追加：hooks 未指定でも最低限の通知を司令塔内で実施
            if (m_usi1 && !m_usi2) {
                // HvE：エンジンは m_usi1。人間が投了＝エンジン勝ち。
                sendRawTo_(m_usi1, QStringLiteral("gameover win"));
                sendRawTo_(m_usi1, QStringLiteral("quit"));
            } else {
                // EvE：勝者/敗者のエンジンそれぞれに通知
                const Player winner = (m_cur == P1 ? P2 : P1);
                Usi* winEng  = (winner     == P1) ? m_usi1 : m_usi2;
                Usi* loseEng = (info.loser == P1) ? m_usi1 : m_usi2;
                if (loseEng) sendRawTo_(loseEng, QStringLiteral("gameover lose"));
                if (winEng)  {
                    sendRawTo_(winEng,  QStringLiteral("gameover win"));
                    sendRawTo_(winEng,  QStringLiteral("quit"));
                }
            }
        }
    }

    // 司令塔のゲームオーバー状態を確定（棋譜「投了」一意追記は appendMoveOnce=true で司令塔→UIへ）
    setGameOver(info, /*loserIsP1=*/(info.loser==P1), /*appendMoveOnce=*/true);

    // ★ 追加：投了時も結果ダイアログを表示
    displayResultsAndUpdateGui_(info);
}

// 2) エンジン側の投了
void MatchCoordinator::handleEngineResign(int idx) {
    // エンジン投了時はまず時計だけ停止（stop は送らない）
    if (m_clock) m_clock->stopClock();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (idx == 1 ? P1 : P2);

    // 負け側には lose + quit、勝ち側には win + quit を送る
    Usi* loserEng  = (info.loser == P1) ? m_usi1 : m_usi2;
    Usi* winnerEng = (info.loser == P1) ? m_usi2 : m_usi1;

    if (loserEng) {
        loserEng->sendGameOverLoseAndQuitCommands();
        loserEng->setSquelchResignLogging(true); // 任意：終局後の雑音ログ抑制
    }
    if (winnerEng) {
        winnerEng->sendGameOverWinAndQuitCommands();
        winnerEng->setSquelchResignLogging(true);
    }

    // 司令塔のゲームオーバー状態を確定（棋譜「投了」一意追記は appendMoveOnce=true で司令塔→UIへ）
    const bool loserIsP1 = (info.loser == P1);
    setGameOver(info, loserIsP1, /*appendMoveOnce=*/true);

    // ★ 追加：エンジン投了時も結果ダイアログを表示
    displayResultsAndUpdateGui_(info);
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
    } else {
        sendGoToCurrentEngine_(t); // ★ 追加：hooks 未指定なら司令塔内で送る
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
    } else {
        sendStopAllEngines_(); // ★ 追加
    }
}

void MatchCoordinator::displayResultsAndUpdateGui_(const GameEndInfo& info) {
    // 対局中メニューのON/OFFなどUI側の状態を更新
    setGameInProgressActions_(false);

    // 先後の文字列（日本語）
    const bool loserIsP1  = (info.loser == P1);
    const QString loserJP = loserIsP1 ? tr("先手") : tr("後手");
    const QString winnerJP= loserIsP1 ? tr("後手") : tr("先手");

    // メッセージ本文（日本語）
    QString msg;
    switch (info.cause) {
    case Cause::Resignation:
        // 例）「先手の投了。後手の勝ちです。」
        msg = tr("%1の投了。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::Timeout:
        // 例）「先手の時間切れ。後手の勝ちです。」
        msg = tr("%1の時間切れ。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::BreakOff:
    default:
        // 念のためのフォールバック
        msg = tr("対局が終了しました。");
        break;
    }

    // ダイアログ表示（MainWindow 側フックで QMessageBox を出します）
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("対局終了"), msg);
    }

    if (m_hooks.log) m_hooks.log(QStringLiteral("Game ended"));

    // UI 全体へも通知（既存のUI処理と整合）
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

    // 例外を投げない前提：失敗は内部でシグナル/ログ通知される
    eng->initializeAndStartEngineCommunication(path, name);

    // 投了シグナル配線（m_usi1=先手扱い）
    wireResignToArbiter_(eng, (eng == m_usi1));
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
        qInfo() << "[Match] engineThinkApplyMove: no legal 'to' returned (resign/abort?). from="
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
    // ★ デバッグ: 起動からの対局履歴を蓄積して出力
    static QVector<QStringList> allGameHistories;

    // 直前の対局履歴が残っていれば、それを確定した過去の対局として蓄積
    // (m_positionStrHistory はこの後クリアされるため、その前に保存)
    if (!m_positionStrHistory.isEmpty()) {
        allGameHistories.append(m_positionStrHistory);
    }

    // --- 探索対象SFENを正規化 ---
    QString targetSfen = opt.sfenStart.trimmed();
    if (targetSfen == QLatin1String("startpos")) {
        // 比較のため「平手初期SFEN」に正規化
        targetSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    } else if (targetSfen.startsWith(QLatin1String("position sfen "))) {
        targetSfen = targetSfen.mid(14).trimmed();
    }

    // --- 過去対局のどれをベースに切り詰めるかを探索 ---
    //     （最初に一致したゲームを優先＝例のログでは [Game 1] が選ばれる）
    int      bestGameIdx  = -1;
    QString  bestBaseFull;                // これをヘッダ保持のまま moves だけトリムして使う
    int      bestMatchPly = -1;           // 見つかった手数（デバッグ用）

    if (!allGameHistories.isEmpty()) {
        qDebug() << "=== Accumulated Game Histories (Count:" << allGameHistories.size() << ") ===";
        for (int i = 0; i < allGameHistories.size(); ++i) {
            qDebug() << " [Game" << (i + 1) << "]";
            const QStringList& rec = allGameHistories.at(i);
            for (const QString& pos : rec) {
                qDebug().noquote() << "  " << pos;
            }
        }
        qDebug() << "=======================================================";
        qDebug() << "--- Searching for start position in previous games ---";

        for (int i = 0; i < allGameHistories.size(); ++i) {
            const QStringList& hist = allGameHistories.at(i);
            if (hist.isEmpty()) continue;

            const QString fullCmd = hist.last(); // 例: "position startpos moves 7g7f 3c3d ..."
            SfenPositionTracer tracer;
            QStringList moves;

            // 1) 初期局面セットアップ
            if (fullCmd.startsWith(QLatin1String("position startpos"))) {
                tracer.resetToStartpos();
                if (fullCmd.contains(QLatin1String(" moves "))) {
                    moves = fullCmd.section(QLatin1String(" moves "), 1)
                    .split(QLatin1Char(' '), Qt::SkipEmptyParts);
                }
            } else if (fullCmd.startsWith(QLatin1String("position sfen "))) {
                const int mIdx = fullCmd.indexOf(QLatin1String(" moves "));
                const QString sfenPart = (mIdx == -1) ? fullCmd.mid(14)
                                                      : fullCmd.mid(14, mIdx - 14);
                tracer.setFromSfen(sfenPart);
                if (mIdx != -1) {
                    moves = fullCmd.mid(mIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
                }
            }

            // 2) 0手目（開始局面）の比較
            if (tracer.toSfenString() == targetSfen) {
                qDebug().noquote()
                << QString(" -> MATCH FOUND: [Game %1] Start Position (Move 0)").arg(i + 1);
                if (bestGameIdx == -1) { // 最初の一致を採用
                    bestGameIdx  = i;
                    bestBaseFull = fullCmd;
                    bestMatchPly = 0;
                }
            }

            // 3) 1手ずつ進めて比較
            for (int m = 0; m < moves.size(); ++m) {
                tracer.applyUsiMove(moves[m]);
                if (tracer.toSfenString() == targetSfen) {
                    qDebug().noquote()
                    << QString(" -> MATCH FOUND: [Game %1] Move %2").arg(i + 1).arg(m + 1);
                    if (bestGameIdx == -1) { // 最初の一致を採用
                        bestGameIdx  = i;
                        bestBaseFull = fullCmd;
                        bestMatchPly = m + 1;
                    }
                }
            }
        }
        qDebug() << "----------------------------------------------------";
    }

    m_playMode = opt.mode;

    // 盤・名前などの初期化（GUI側へ委譲）
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
    if (m_hooks.setPlayersNames)   m_hooks.setPlayersNames(QString(), QString());
    if (m_hooks.setEngineNames)    m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
    if (m_hooks.setGameActions)    m_hooks.setGameActions(true);

    // ---- 開始手番の決定（SFEN 解析：position sfen ... / 素のSFEN の両対応）
    auto decideStartSideFromSfen = [](const QString& sfen) -> ShogiGameController::Player {
        ShogiGameController::Player start = ShogiGameController::Player1;
        if (sfen.isEmpty()) return start;
        const QStringList tok = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tok.size() < 2) return start;

        int sideIdx = -1;
        if (tok.size() >= 4 && tok[0] == QLatin1String("position") && tok[1] == QLatin1String("sfen")) {
            sideIdx = 3; // "position sfen <board> <side> ..."
        } else if (tok.size() >= 2) {
            sideIdx = 1; // "<board> <side> ..."
        }

        if (sideIdx >= 0 && sideIdx < tok.size()) {
            const QString side = tok[sideIdx];
            if (side.compare(QLatin1String("w"), Qt::CaseInsensitive) == 0) {
                start = ShogiGameController::Player2; // 後手番
            } else {
                start = ShogiGameController::Player1; // 先手番
            }
        }
        return start;
    };

    const ShogiGameController::Player startSide = decideStartSideFromSfen(opt.sfenStart);
    if (m_gc) m_gc->setCurrentPlayer(startSide);
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

    // EvE 用の内部棋譜コンテナを初期化
    m_eveSfenRecord.clear();
    m_eveGameMoves.clear();
    m_eveMoveIndex = 0;

    // HvE/HvH 共通の安全策
    m_currentMoveIndex = 0;

    // 司令塔の内部手番も GC に同期（既存互換）
    m_cur = (startSide == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay_(m_cur);

    // ------------------------------------------------------------
    // ★★ 「探索で見つけたゲームのヘッダ」をベースに moves だけをトリム ★★
    //     ・ヘッダ（"position startpos" / "position sfen ..."）は絶対に書き換えない
    //     ・opt.sfenStart 末尾の手数(N) → 残す手数 = N-1
    // ------------------------------------------------------------
    auto parseKeepMovesFromSfen = [](const QString& sfenLike) -> int {
        QString s = sfenLike.trimmed();
        if (s.startsWith(QLatin1String("position sfen "))) {
            s = s.mid(QStringLiteral("position sfen ").size()).trimmed();
        }
        const QStringList tok = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tok.size() >= 4) {
            bool ok = false;
            const int mv = tok.last().toInt(&ok); // SFENの第4フィールド
            if (ok) return qMax(0, mv - 1);       // " ... b - 1" → 0手, " ... b - 3" → 2手
        }
        return -1;
    };

    auto trimMovesPreserveHeader = [](const QString& full, int keep) -> QString {
        const QString movesKey = QStringLiteral(" moves ");
        const int pos = full.indexOf(movesKey);
        QString head = full.trimmed();
        QString tail;
        if (pos >= 0) {
            head = full.left(pos).trimmed();                  // "position startpos" / "position sfen <...>"
            tail = full.mid(pos + movesKey.size()).trimmed(); // "7g7f 3c3d ..."
        }
        if (keep <= 0 || tail.isEmpty()) return head;
        QStringList mv = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (keep < mv.size()) mv = mv.mid(0, keep);
        if (mv.isEmpty()) return head;
        return head + movesKey + mv.join(QLatin1Char(' '));
    };

    const int keepMoves = parseKeepMovesFromSfen(opt.sfenStart);
    qDebug().noquote() << "[MC][configureAndStart] keepMoves(from sfenStart)=" << keepMoves;

    // --- ベース候補：探索一致ゲーム > 直前ゲーム履歴 の順で採用
    QString candidateBase;
    if (!bestBaseFull.isEmpty()) {
        candidateBase = bestBaseFull;
        qDebug().noquote()
            << "[MC][configureAndStart] base=matched game"
            << "gameIdx=" << (bestGameIdx + 1) << "matchPly=" << bestMatchPly
            << "full=" << candidateBase;
    } else if (!m_positionStrHistory.isEmpty()) {
        candidateBase = m_positionStrHistory.constLast();
        qDebug().noquote()
            << "[MC][configureAndStart] base=last history (fallback)"
            << "full=" << candidateBase;
    }

    bool applied = false;
    if (!candidateBase.isEmpty() && keepMoves >= 0) {
        if (candidateBase.startsWith(QLatin1String("position "))) {
            const QString trimmed = trimMovesPreserveHeader(candidateBase, keepMoves);
            qDebug().noquote() << "[MC][configureAndStart] trimmed(base, keep=" << keepMoves << ")=" << trimmed;

            m_positionStr1     = trimmed;
            m_positionPonder1  = trimmed;

            // 履歴はベースを差し替えて 1 件から再スタート（UNDO用）
            m_positionStrHistory.clear();
            m_positionStrHistory.append(trimmed);

            applied = true;
        } else {
            qWarning().noquote()
            << "[MC][configureAndStart] candidateBase is not 'position ...' :" << candidateBase;
        }
    }

    // ---- 履歴／一致ベースが使えないときのフォールバック
    if (!applied) {
        // 従来の初期化（position sfen <sfen> ... をそのまま使う）
        initializePositionStringsForStart(opt.sfenStart);

        // さらに、平手startposかつ明示のkeepMoves>0 なら、startpos形式で再構築
        auto isStandardStartposSfen = [](const QString& sfenLike) -> bool {
            QString s = sfenLike.trimmed();
            if (s.startsWith(QLatin1String("position sfen "))) {
                s = s.mid(QStringLiteral("position sfen ").size()).trimmed();
            }
            const QStringList tok = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (tok.size() < 4) return false;
            static const QString kStartBoard =
                QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
            const QString board = tok[0], turn = tok[1], hand = tok[2];
            return (board == kStartBoard && turn == QLatin1String("b") && hand == QLatin1String("-"));
        };

        if (isStandardStartposSfen(opt.sfenStart) && keepMoves > 0) {
            // 「position startpos moves ...」の体裁でリセット
            m_positionStr1     = QStringLiteral("position startpos");
            m_positionPonder1  = m_positionStr1;
            m_positionStrHistory.clear();
            m_positionStrHistory.append(m_positionStr1);
        }
    }

    // ---- モード別の起動ルート
    switch (m_playMode) {
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine: {
        initEnginesForEvE(opt.engineName1, opt.engineName2);
        initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
        initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);
        startEngineVsEngine_(opt);
        break;
    }
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine: {
        const bool engineIsP1 = opt.engineIsP1;
        startHumanVsEngine_(opt, engineIsP1);
        break;
    }
    case HumanVsHuman:
        startHumanVsHuman_(opt);
        break;
    default:
        break;
    }

    // ===== デバッグ: 退出時点の m_positionStr1 を記録 =====
    qDebug().noquote() << "[MC][configureAndStart] LEAVE m_positionStr1(final)=" << m_positionStr1
                       << "hist.size=" << m_positionStrHistory.size()
                       << "hist.last=" << (m_positionStrHistory.isEmpty() ? QString("<empty>") : m_positionStrHistory.constLast());
}

// HvH（人間対人間）
void MatchCoordinator::startHumanVsHuman_(const StartOptions& /*opt*/)
{
    if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] Start HvH"));

    // --- 手番の単一ソースを確立：GC → TurnManager → m_cur → 表示
    ShogiGameController::Player side =
        m_gc ? m_gc->currentPlayer() : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        side = ShogiGameController::Player1;         // 既定は先手
        if (m_gc) m_gc->setCurrentPlayer(side);
    }

    m_cur = (side == ShogiGameController::Player2) ? P2 : P1;

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    updateTurnDisplay_(m_cur);
}

// HvE（人間対エンジン）
//   engineIsP1 == true ならエンジンは先手座席、false なら後手座席
void MatchCoordinator::startHumanVsEngine_(const StartOptions& opt, bool engineIsP1)
{
    if (m_hooks.log) {
        m_hooks.log(QStringLiteral("[Match] Start HvE (engineIsP1=%1)").arg(engineIsP1));
    }

    // 以前のエンジンは破棄（安全化）
    destroyEngines();

    // ★ 単発エンジン＝表示は常に #1 スロット（上段）へ流す
    //    → 座席が P1/P2 でも comm/think は #1 を使う
    UsiCommLogModel*          comm  = m_comm1;
    ShogiEngineThinkingModel* think = m_think1;
    if (!comm)  { comm  = new UsiCommLogModel(this);         m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(this); m_think1 = think; }

    // USI を生成（この時点ではプロセス未起動）
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);
    m_usi2 = nullptr; // HvE は単発

    // 投了配線（m_usi1=先手扱いでよい：内部で asP1=true として扱う）
    wireResignToArbiter_(m_usi1, /*asP1=*/true);

    // ログ識別（UI 表示用）
    if (m_usi1) {
        const QString dispName = opt.engineName1.isEmpty() ? QStringLiteral("Engine") : opt.engineName1;
        m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), dispName);
        m_usi1->setSquelchResignLogging(false);
    }

    // ★★★ ここが肝心：USI エンジンを起動（path/name 必須） ★★★
    initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);

    // UI 側にエンジン名を通知（必要時）
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName1, QString());

    // --- 手番の単一ソースを確立：GC → m_cur → 表示 ---
    ShogiGameController::Player side =
        m_gc ? m_gc->currentPlayer() : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        // 既定は先手（SFEN未指定時）
        side = ShogiGameController::Player1;
        if (m_gc) m_gc->setCurrentPlayer(side);
    }
    m_cur = (side == ShogiGameController::Player2) ? P2 : P1;

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    updateTurnDisplay_(m_cur);
}

// EvE の初手を開始する（起動・初期化済み前提）
void MatchCoordinator::startEngineVsEngine_(const StartOptions& /*opt*/)
{
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    if (m_gc->currentPlayer() == ShogiGameController::NoPlayer) {
        m_gc->setCurrentPlayer(ShogiGameController::Player1);
    }
    m_cur = (m_gc->currentPlayer() == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay_(m_cur);

    initPositionStringsForEvE_();

    const GoTimes t1 = computeGoTimes_();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    m_gc->setPromote(false);

    m_usi1->handleEngineVsHumanOrEngineMatchCommunication(
        m_positionStr1, m_positionPonder1,
        p1From, p1To,
        static_cast<int>(t1.byoyomi),
        btimeStr1, wtimeStr1,
        static_cast<int>(t1.binc), static_cast<int>(t1.winc),
        (t1.byoyomi > 0)
        );

    QString rec1;
    PlayMode pm = m_playMode;

    // ★ 先手1手目：次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p1From, p1To, rec1,
            pm,
            nextEve,
            &m_eveSfenRecord,
            m_eveGameMoves
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_clock) {
        const qint64 thinkMs = m_usi1 ? m_usi1->lastBestmoveElapsedMs() : 0;
        m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        m_clock->applyByoyomiAndResetConsideration1();
    }
    if (m_hooks.appendKifuLine && m_clock) {
        m_hooks.appendKifuLine(rec1, m_clock->getPlayer1ConsiderationAndTotalTime());
    }

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(p1From, p1To);
    updateTurnDisplay_((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

    if (m_usi2) {
        m_usi2->setPreviousFileTo(p1To.x());
        m_usi2->setPreviousRankTo(p1To.y());
    }

    m_positionStr2     = m_positionStr1;
    m_positionPonder2.clear();

    const GoTimes t2 = computeGoTimes_();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    m_gc->setPromote(false);

    m_usi2->handleEngineVsHumanOrEngineMatchCommunication(
        m_positionStr2, m_positionPonder2,
        p2From, p2To,
        static_cast<int>(t2.byoyomi),
        btimeStr2, wtimeStr2,
        static_cast<int>(t2.binc), static_cast<int>(t2.winc),
        (t2.byoyomi > 0)
        );

    QString rec2;

    // ★ 後手1手目：次の手を渡す
    nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p2From, p2To, rec2,
            pm,
            nextEve,
            &m_eveSfenRecord,
            m_eveGameMoves
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_clock) {
        const qint64 thinkMs = m_usi2 ? m_usi2->lastBestmoveElapsedMs() : 0;
        m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        m_clock->applyByoyomiAndResetConsideration2();
    }
    if (m_hooks.appendKifuLine && m_clock) {
        m_hooks.appendKifuLine(rec2, m_clock->getPlayer2ConsiderationAndTotalTime());
    }

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(p2From, p2To);
    updateTurnDisplay_((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

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
    if (m_playMode != EvenEngineVsEngine && m_playMode != HandicapEngineVsEngine) return;
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    const bool p1ToMove = (m_gc->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? m_usi1 : m_usi2;
    Usi* receiver = p1ToMove ? m_usi2 : m_usi1;

    QString& pos    = p1ToMove ? m_positionStr1     : m_positionStr2;
    QString& ponder = p1ToMove ? m_positionPonder1  : m_positionPonder2;
    if (p1ToMove) pos = m_positionStr2; else pos = m_positionStr1;

    QPoint from(-1,-1), to(-1,-1);
    if (!engineThinkApplyMove(mover, pos, ponder, &from, &to))
        return;

    QString rec;

    // ★ 次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(from, to, rec, m_playMode,
                               nextEve, &m_eveSfenRecord, m_eveGameMoves)) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_clock) {
        const qint64 thinkMs = mover ? mover->lastBestmoveElapsedMs() : 0;
        if (p1ToMove) {
            m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration1();
        } else {
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

    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(from, to);
    updateTurnDisplay_(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

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

    const bool hasRemainHook = static_cast<bool>(m_hooks.remainingMsFor);
    const bool hasIncHook    = static_cast<bool>(m_hooks.incrementMsFor);
    const bool hasByoHook    = static_cast<bool>(m_hooks.byoyomiMs);

    // 残り（ms）
    const qint64 rawB = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P1)) : 0;
    const qint64 rawW = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P2)) : 0;

    // デバッグ（入力値）
    qDebug().noquote()
        << "[Match] computeGoTimes_: hooks{remain=" << hasRemainHook
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

        qDebug().noquote()
            << "[Match] computeGoTimes_: BYO"
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

        qDebug().noquote()
            << "[Match] computeGoTimes_: FISCHER"
            << " => btime=" << t.btime << " wtime=" << t.wtime
            << " binc=" << t.binc << " winc=" << t.winc;
    }

    return t;
}

void MatchCoordinator::sendGoToCurrentEngine_(const GoTimes& t)
{
    Usi* target = (m_cur == P1) ? m_usi1 : m_usi2;
    if (!target) {
        qDebug() << "[Match] sendGoToCurrentEngine_: target null";
        return;
    }

    const bool useByoyomi = (t.byoyomi > 0 && t.binc == 0 && t.winc == 0);

    qDebug().noquote()
        << "[Match] sendGoToCurrentEngine_:"
        << "cur=" << (m_cur==P1?"P1":"P2")
        << " btime=" << t.btime
        << " wtime=" << t.wtime
        << " byoyomi=" << t.byoyomi
        << " binc=" << t.binc
        << " winc=" << t.winc
        << " useByoyomi=" << useByoyomi;

    target->sendGoCommand(
        clampMsToIntLocal(t.byoyomi),         // byoyomi(ms)
        QString::number(t.btime),             // btime(ms)
        QString::number(t.wtime),             // wtime(ms)
        clampMsToIntLocal(t.binc),            // 先手inc(ms)
        clampMsToIntLocal(t.winc),            // 後手inc(ms)
        useByoyomi
        );
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

// 投了と同様に“対局の実体”として中断を一元処理
void MatchCoordinator::handleBreakOff()
{
    // すでに終局なら何もしない
    if (m_gameOver.isOver) return;

    // 進行系タイマを停止（人間用のみでOK）
    disarmHumanTimerIfNeeded();

    // 司令塔として終局状態を確定（中断）
    m_gameOver.isOver         = true;
    m_gameOver.when           = QDateTime::currentDateTime();
    m_gameOver.hasLast        = true;
    m_gameOver.lastInfo.cause = Cause::BreakOff;

    // ★ 中断行の生成＋KIF追記＋一度だけの追記ブロック確定（内部で emit 済み）
    appendBreakOffLineAndMark();

    // 起動中エンジンに quit
    if (m_usi1) m_usi1->sendQuitCommand();
    if (m_usi2) m_usi2->sendQuitCommand();
}

// 検討を開始する（単発エンジンセッション）
// - 既存の HvE と同じく m_usi1 を使用し、表示モデルは #1 スロットに流す。
// - resign シグナルは P1 扱いで司令塔（Arbiter）に配線する（検討でも安全側）。
// 検討を開始する（単発エンジンセッション）
void MatchCoordinator::startAnalysis(const AnalysisOptions& opt)
{
    // 1) モード設定（検討 / 詰み探索）
    setPlayMode(opt.mode); // ConsidarationMode or TsumiSearchMode

    // 2) 以前の単発エンジンは破棄
    destroyEngines();

    // 3) 表示モデル（無ければ生成して保持）
    UsiCommLogModel*          comm  = m_comm1;
    ShogiEngineThinkingModel* think = m_think1;
    if (!comm)  { comm  = new UsiCommLogModel(this);          m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(this); m_think1 = think; }

    // 4) 単発エンジン生成（常に m_usi1 を使用）
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);

    // 既存の接続（bestmove等）はそのまま。エラー用だけスロット接続を追加。
    connect(m_usi1, &Usi::errorOccurred,
            this,   &MatchCoordinator::onUsiError_,
            Qt::UniqueConnection);

    // 5) 投了配線
    wireResignToArbiter_(m_usi1, /*asP1=*/true);

    // 6) ログ識別
    m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), opt.engineName);
    m_usi1->setSquelchResignLogging(false);

    // 7) USI 初期化＆起動（例外非使用前提）
    initializeAndStartEngineFor(P1, opt.enginePath, opt.engineName);

    // 8) UI 側にエンジン名を通知（必要時）
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName, QString());

    // 9) 詰み探索の配線（TsumiSearchMode のときのみ）
    if (opt.mode == TsumiSearchMode && m_usi1) {
        connect(m_usi1, &Usi::checkmateSolved,
                this,  &MatchCoordinator::onCheckmateSolved_,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateNoMate,
                this,  &MatchCoordinator::onCheckmateNoMate_,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateNotImplemented,
                this,  &MatchCoordinator::onCheckmateNotImplemented_,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateUnknown,
                this,  &MatchCoordinator::onCheckmateUnknown_,
                Qt::UniqueConnection);
    }

    // 10) 解析/詰み探索の実行
    QString pos = opt.positionStr; // "position sfen <...>"
    if (opt.mode == TsumiSearchMode) {
        m_usi1->executeTsumeCommunication(pos, opt.byoyomiMs);
    } else {
        m_usi1->executeAnalysisCommunication(pos, opt.byoyomiMs);
    }
}

void MatchCoordinator::onCheckmateSolved_(const QStringList& pv)
{
    if (m_hooks.showGameOverDialog) {
        const QString msg = tr("詰みあり（手順 %1 手）").arg(pv.size());
        m_hooks.showGameOverDialog(tr("詰み探索"), msg);
    }
}

void MatchCoordinator::onCheckmateNoMate_()
{
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("詰みなし"));
    }
}

void MatchCoordinator::onCheckmateNotImplemented_()
{
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("（エンジン側）未実装"));
    }
}

void MatchCoordinator::onCheckmateUnknown_()
{
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("不明（解析不能）"));
    }
}

// 検討の停止（stop コマンド送信のみ。プロセス終了は destroyEngines() に委譲）
void MatchCoordinator::stopAnalysis()
{
    if (m_playMode != ConsidarationMode) return;
    if (m_usi1) {
        m_usi1->sendStopCommand();
        // 必要なら：m_usi1->waitForStopOrPonderhitCommand();
    }
}

// 検討中かを返す（モードと m_usi1 の有無で判定）
bool MatchCoordinator::isAnalysisActive() const
{
    return (m_playMode == ConsidarationMode) && (m_usi1 != nullptr);
}

// 検討モードを手動終了する（quit送信→エンジン破棄）
//  - ConsidarationMode 以外では何もしない
//  - Usi::sendQuitCommand() は終了時のログ抑止などの安全策込み
//  - 送信後にプロセス/スレッドを片付け、Usi オブジェクトも破棄
//  - モードは NotStarted に戻す（isAnalysisActive() が偽になる）
void MatchCoordinator::handleBreakOffConsidaration()
{
    if (m_playMode != ConsidarationMode)
        return;

    // 単発検討は m_usi1 を利用している前提（存在すれば確実に止める）
    if (m_usi1) {
        m_usi1->cleanupEngineProcessAndThread(); // 読み取り側をドレインして安全終了
        destroyEngine(1);                       // Usi オブジェクト自体も破棄
    }

    // 念のため、片側だけの想定でも m_usi2 が残っていれば同様に止める
    if (m_usi2) {
        m_usi2->cleanupEngineProcessAndThread();
        destroyEngine(2);
    }

    // モードを通常状態へ戻す（以降 isAnalysisActive()==false）
    setPlayMode(NotStarted);

    // UI 側のボタン等を「対局中でない」状態へ（フック未設定なら何もしない）
    setGameInProgressActions_(false);

    // 手番表示などの軽い再描画（必要なければ削ってOK）
    updateTurnDisplay_(m_cur);
}

void MatchCoordinator::continueAnalysis(const QString& positionStr, int byoyomiMs)
{
    // 検討モードで単発エンジン m_usi1 が起動済みであることが前提
    if (m_playMode != ConsidarationMode || !m_usi1) {
        if (m_hooks.log) m_hooks.log(QStringLiteral("[Analysis] continueAnalysis skipped (no active engine)"));
        return;
    }

    // Usi::executeAnalysisCommunication は非常参照引数なのでコピーを渡す
    QString pos = positionStr;
    m_usi1->executeAnalysisCommunication(pos, byoyomiMs);
}

void MatchCoordinator::startTsumeSearch(const QString& sfen, int timeMs, bool infinite)
{
    Usi* eng = primaryEngine();
    if (!eng) return;

    // ここでは接続や未宣言シグナルのemitは行わない
    // （先にビルドを通すための最小実装。結果処理は後続で接続可能）
    eng->sendPositionAndGoMate(sfen, timeMs, infinite);
}

void MatchCoordinator::stopTsumeSearch()
{
    Usi* eng = primaryEngine();
    if (!eng) return;

    // 詰み探索の停止
    eng->sendStopForMate();
}

// 【新規/任意】詰み探索中にbestmoveが来た場合の保険
void MatchCoordinator::onUsiBestmoveDuringTsume_(const QString& bestmove)
{
    Q_UNUSED(bestmove);
    // 多くの場合は無視で良い。ログだけ残す。
    qInfo() << "[Tsume] bestmove during mate-search:" << bestmove;
}

void MatchCoordinator::onUsiError_(const QString& msg)
{
    // ログへ（あれば）
    if (m_hooks.log) m_hooks.log(QStringLiteral("[USI-ERROR] ") + msg);
    // 実行中の USI オペを明示的に打ち切る
    if (m_usi1) m_usi1->cancelCurrentOperation();
    if (m_usi2) m_usi2->cancelCurrentOperation();
}

// ---------------------------------------------
// 初期 "position ... moves" を SFEN から生成
// ---------------------------------------------
void MatchCoordinator::initializePositionStringsForStart(const QString& sfenStart)
{
    initPositionStringsFromSfen_(sfenStart);
}

void MatchCoordinator::initPositionStringsFromSfen_(const QString& sfenBase)
{
    // m_positionStr1/m_positionPonder1 だけ使う（単発エンジン系）
    m_positionStr1.clear();
    m_positionPonder1.clear();

    // ★追加：USIのposition履歴はSFENと混ぜないため、開始ごとにクリア
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
    // ▼▼▼【ここに追加】平手初期局面のSFENと一致する場合は startpos に戻す ▼▼▼
    else if (isStandardStartposSfen_(base)) {
        m_positionStr1    = QStringLiteral("position startpos");
        m_positionPonder1 = m_positionStr1;
    }
    // ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
    else {
        m_positionStr1    = QStringLiteral("position sfen %1").arg(base);
        m_positionPonder1 = m_positionStr1;
    }
}

// ---------------------------------------------
// 初手がエンジン手番なら 1手だけ起動
// ---------------------------------------------
void MatchCoordinator::startInitialEngineMoveIfNeeded()
{
    if (!m_gc) return;

    const bool engineIsP1 = (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);
    const bool engineIsP2 = (m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine);

    const auto sideToMove = m_gc->currentPlayer();

    if (engineIsP1 && sideToMove == ShogiGameController::Player1) {
        startInitialEngineMoveFor_(P1);
    } else if (engineIsP2 && sideToMove == ShogiGameController::Player2) {
        startInitialEngineMoveFor_(P2);
    }
}

// （内部）指定したエンジン側で 1手だけ指す
void MatchCoordinator::startInitialEngineMoveFor_(Player engineSide)
{
    Usi* eng = primaryEngine();
    if (!eng || !m_gc) return;

    auto extractMoveNumber = [](const QString& sfen) -> int {
        const QStringList tok = sfen.split(' ', Qt::SkipEmptyParts);
        if (tok.size() >= 5) return tok.last().toInt();
        return -1;
    };

    if (m_positionStr1.isEmpty()) {
        initPositionStringsFromSfen_(QString()); // startpos moves
    }
    if (!m_positionStr1.startsWith(QLatin1String("position "))) {
        m_positionStr1 = QStringLiteral("position startpos moves");
    }

    const int mcCur = m_currentMoveIndex;
    const int recSizeBefore = m_sfenRecord ? m_sfenRecord->size() : -1;
    const QString recTailBefore = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    qInfo().noquote() << "[IDX][HvE:init] enter  mcCur=" << mcCur
                      << " recSizeBefore=" << recSizeBefore
                      << " recTailBefore='" << recTailBefore << "'";

    qint64 bMs = 0, wMs = 0;
    computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const auto tc = timeControl();
    const int  byoyomiMs = (engineSide == P1) ? tc.byoyomiMs1 : tc.byoyomiMs2;

    QPoint eFrom(-1, -1), eTo(-1, -1);
    m_gc->setPromote(false);

    eng->handleEngineVsHumanOrEngineMatchCommunication(
        m_positionStr1,
        m_positionPonder1,
        eFrom, eTo,
        byoyomiMs,
        bTime, wTime,
        tc.incMs1, tc.incMs2,
        tc.useByoyomi
        );

    QString rec;
    int nextIdx = mcCur + 1; // ★ 「次の手」
    const bool ok = m_gc->validateAndMove(
        eFrom, eTo, rec, m_playMode,
        nextIdx, m_sfenRecord, m_gameMoves);

    const QString recTailAfter = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qInfo().noquote() << "[IDX][HvE:init] v&m=" << ok
                      << " nextIdx=" << nextIdx
                      << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (!ok) return;

    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    if (m_clock) {
        if (engineSide == P1) {
            m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration1(); // ← 条件を外して常に適用
        } else {
            m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration2(); // ← 条件を外して常に適用
        }
    }
    if (m_hooks.appendKifuLine && m_clock) {
        const QString elapsed = (engineSide == P1)
        ? m_clock->getPlayer1ConsiderationAndTotalTime()
        : m_clock->getPlayer2ConsiderationAndTotalTime();
        m_hooks.appendKifuLine(rec, elapsed);
    }

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    m_cur = (m_gc->currentPlayer() == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay_(m_cur);

    armHumanTimerIfNeeded();

    if (engineSide == P1) { if (m_hooks.appendEvalP1) m_hooks.appendEvalP1(); }
    else                  { if (m_hooks.appendEvalP2) m_hooks.appendEvalP2(); }
}

// ---------------------------------------------
// HvE: 人間が指した直後の 1手返しを司令塔で完結
// ---------------------------------------------
void MatchCoordinator::onHumanMove_HvE(const QPoint& humanFrom, const QPoint& humanTo)
{
    auto extractMoveNumber = [](const QString& sfen) -> int {
        const QStringList tok = sfen.split(' ', Qt::SkipEmptyParts);
        // SFEN は <board> <turn> <hands> <move> の 4 トークン
        if (tok.size() >= 4) return tok.last().toInt();
        return -1;
    };

    // ★★★ ここで同期 ★★★
    int mcCur = m_currentMoveIndex;
    if (m_sfenRecord) {
        const int fromRec = qMax(0, m_sfenRecord->size() - 1);
        if (fromRec != mcCur) {
            qInfo() << "[IDX][HvE] sync mcCur" << mcCur << "->" << fromRec
                    << "(by recSize=" << m_sfenRecord->size() << ")";
            mcCur = fromRec;
            m_currentMoveIndex = fromRec;
        }
    }

    const int recSizeBefore = m_sfenRecord ? m_sfenRecord->size() : -1;
    const QString recTailBefore = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    qInfo().noquote() << "[IDX][HvE] enter  mcCur=" << mcCur
                      << " recSizeBefore=" << recSizeBefore
                      << " recTailBefore='" << recTailBefore << "'"
                      << " humanFrom=" << humanFrom << " humanTo=" << humanTo;

    // 人間側のストップウォッチ締め＆考慮確定（既存）
    finishHumanTimerAndSetConsideration();

    if (Usi* eng = primaryEngine()) {
        eng->setPreviousFileTo(humanTo.x());
        eng->setPreviousRankTo(humanTo.y());
    }

    // USIに渡す残り時間
    qint64 bMs = 0, wMs = 0;
    computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const bool engineIsP1 =
        (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);
    const ShogiGameController::Player engineSeat =
        engineIsP1 ? ShogiGameController::Player1 : ShogiGameController::Player2;

    const bool engineTurnNow = (m_gc && (m_gc->currentPlayer() == engineSeat));
    qInfo().noquote() << "[IDX][HvE] engineTurnNow=" << engineTurnNow
                      << " engineSeat=" << int(engineSeat);
    if (!engineTurnNow) { if (!gameOverState().isOver) armHumanTimerIfNeeded(); return; }

    Usi* eng = primaryEngine();
    if (!eng || !m_gc || !m_sfenRecord) { if (!gameOverState().isOver) armHumanTimerIfNeeded(); return; }

    if (m_positionStr1.isEmpty()) { initPositionStringsFromSfen_(QString()); }
    if (!m_positionStr1.startsWith(QLatin1String("position "))) {
        m_positionStr1 = QStringLiteral("position startpos moves");
    }

    const auto tc = timeControl();
    const int byoyomiMs = engineIsP1 ? tc.byoyomiMs1 : tc.byoyomiMs2;

    QPoint eFrom = humanFrom, eTo = humanTo;
    m_gc->setPromote(false);

    eng->handleHumanVsEngineCommunication(
        m_positionStr1, m_positionPonder1,
        eFrom, eTo,
        byoyomiMs,
        bTime, wTime,
        m_positionStrHistory,
        tc.incMs1, tc.incMs2,
        tc.useByoyomi
        );

    QString rec;
    int nextIdx = mcCur + 1;              // ★ 同期後の mcCur から算出
    const bool ok = m_gc->validateAndMove(
        eFrom, eTo, rec, m_playMode,
        nextIdx, m_sfenRecord, m_gameMoves);

    const QString recTailAfter = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qInfo().noquote() << "[IDX][HvE] v&m=" << ok
                      << " argMove(nextIdx)=" << nextIdx
                      << " mcCur(before sync calc)=" << mcCur
                      << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (ok) {
        m_currentMoveIndex = nextIdx;
        qInfo().noquote() << "[IDX][HvE] mcCur ->" << m_currentMoveIndex;
    } else {
        if (!gameOverState().isOver) armHumanTimerIfNeeded();
        return;
    }

    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(eFrom, eTo);

    // ★ エンジンの考慮時間を“確定”してから棋譜に追記する（ここが不在だった）
    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    if (m_clock) {
        if (m_gc->currentPlayer() == ShogiGameController::Player1) {
            // 直前に指したのは後手(P2)
            m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration2(); // ← 追加
        } else {
            // 直前に指したのは先手(P1)
            m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            m_clock->applyByoyomiAndResetConsideration1(); // ← 追加
        }
    }

    if (m_hooks.appendKifuLine && m_clock) {
        const QString elapsed = (m_gc->currentPlayer() == ShogiGameController::Player1)
        ? m_clock->getPlayer2ConsiderationAndTotalTime()
        : m_clock->getPlayer1ConsiderationAndTotalTime();
        m_hooks.appendKifuLine(rec, elapsed);
    }

    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    m_cur = (m_gc->currentPlayer() == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay_(m_cur);

    if (!gameOverState().isOver) armHumanTimerIfNeeded();
}

// 人間手直後に「考慮時間確定 → byoyomi/inc 適用 → KIF追記 → 人間手ハイライト」を済ませ、
// その後のエンジン1手返し等は既存の2引数版へ委譲する。
void MatchCoordinator::onHumanMove_HvE(const QPoint& humanFrom, const QPoint& humanTo, const QString& prettyMove)
{
    // 0) 人間手のハイライト
    if (m_hooks.showMoveHighlights) {
        m_hooks.showMoveHighlights(humanFrom, humanTo);
    }

    // 1) 人間側の考慮時間を確定 → byoyomi/inc を適用 → KIF 追記
    if (m_clock) {
        const bool humanIsP1 =
            (m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine);

        if (humanIsP1) {
            const qint64 ms = m_clock->player1ConsiderationMs();
            m_clock->setPlayer1ConsiderationTime(static_cast<int>(ms));
            m_clock->applyByoyomiAndResetConsideration1();

            if (m_hooks.appendKifuLine) {
                m_hooks.appendKifuLine(prettyMove, m_clock->getPlayer1ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            m_clock->setPlayer1ConsiderationTime(0);
        } else {
            const qint64 ms = m_clock->player2ConsiderationMs();
            m_clock->setPlayer2ConsiderationTime(static_cast<int>(ms));
            m_clock->applyByoyomiAndResetConsideration2();

            if (m_hooks.appendKifuLine) {
                m_hooks.appendKifuLine(prettyMove, m_clock->getPlayer2ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            m_clock->setPlayer2ConsiderationTime(0);
        }

        // ラベルなど即時更新
        pokeTimeUpdateNow();
    }

    // 2) 以降（エンジン go → bestmove → 盤/棋譜反映）は既存の2引数版に委譲
    //    finishHumanTimerAndSetConsideration() は2引数版の先頭で呼ばれるが、二重でも実害が出ない想定。
    onHumanMove_HvE(humanFrom, humanTo);
}

void MatchCoordinator::setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks) {
    u_ = refs;
    h_ = hooks;
}

bool MatchCoordinator::undoTwoPlies()
{
    if (!u_.gc) return false;

    // --- ロールバック前のフル position を退避（今回の肝） ---
    QString prevFullPosition;
    if (u_.positionStrList && !u_.positionStrList->isEmpty()) {
        prevFullPosition = u_.positionStrList->last();
    } else if (!m_positionStrHistory.isEmpty()) {
        prevFullPosition = m_positionStrHistory.constLast();
    }

    // --- SFEN履歴の現在位置を把握 ---
    QStringList* srec = u_.sfenRecord ? u_.sfenRecord : m_sfenRecord;
    int curSfenIdx = -1;

    if (srec && !srec->isEmpty()) {
        curSfenIdx = srec->size() - 1; // 末尾が現局面
    } else if (u_.currentMoveIndex) {
        curSfenIdx = *u_.currentMoveIndex + 1; // 行0始まり → SFENは開始局面含むので+1
    } else if (u_.gameMoves) {
        curSfenIdx = u_.gameMoves->size() + 1;
    } else {
        return false;
    }

    // 2手戻し後の SFEN 添字（= 残すべき手数）
    const int targetSfenIdx = qMax(0, curSfenIdx - 2);   // 0 = 開始局面
    const int remainHands   = qMax(0, targetSfenIdx);    // 残すべき手数（開始=0→手数=N）

    // --- 盤面を targetSfenIdx に復元 ---
    if (u_.gc && srec && targetSfenIdx < srec->size()) {
        const QString sfen = srec->at(targetSfenIdx);
        if (u_.gc->board()) {
            u_.gc->board()->setSfen(sfen);
        }
        const bool sideToMoveIsBlack = sfen.contains(QStringLiteral(" b "));
        u_.gc->setCurrentPlayer(sideToMoveIsBlack ? ShogiGameController::Player1
                                                  : ShogiGameController::Player2);
    }

    // --- 棋譜/モデル/履歴を末尾2件ずつ削除 ---
    if (u_.recordModel)              tryRemoveLastItems_(u_.recordModel, 2);
    if (u_.gameMoves && u_.gameMoves->size() >= 2) {
        u_.gameMoves->remove(u_.gameMoves->size() - 2, 2);
    }
    if (u_.positionStrList && u_.positionStrList->size() >= 2) {
        u_.positionStrList->remove(u_.positionStrList->size() - 2, 2);
    }
    if (srec && srec->size() >= 2) {
        srec->remove(srec->size() - 2, 2);
    }

    // --- USI用 position 履歴も2手ぶん巻き戻す ---
    if (m_positionStrHistory.size() >= 2) {
        m_positionStrHistory.remove(m_positionStrHistory.size() - 2, 2);
    } else {
        m_positionStrHistory.clear();
    }

    // --- ★ 巻き戻し後の “現在ベース” を厳密に再構成（prev を 先頭 remainHands 手にトリム） ---
    const QString startSfen0 = (srec && !srec->isEmpty()) ? srec->first() : QString();
    const QString nextBase   = buildBasePositionUpToHands_(prevFullPosition, remainHands, startSfen0);

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
    const auto stm = u_.gc ? u_.gc->currentPlayer() : ShogiGameController::NoPlayer;
    const bool humanNow = h_.isHumanSide ? h_.isHumanSide(stm) : false;
    if (u_.view) u_.view->setMouseClickMode(humanNow);

    return true;
}

void MatchCoordinator::armTimerAfterUndo_() {
    if (!u_.gc || !u_.clock || !h_.isHumanSide || !h_.isHvH) return;

    const auto sideToMove = u_.gc->currentPlayer();
    if (!h_.isHumanSide(sideToMove)) return;

    if (h_.isHvH()) {
        // 人対人：共有ターンタイマ
        armTurnTimerIfNeeded();
    } else {
        // 人対エンジン系：人間用タイマ
        armHumanTimerIfNeeded();
    }
}

bool MatchCoordinator::tryRemoveLastItems_(QObject* model, int n) {
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

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialog* dlg) const
{
    StartOptions opt;
    opt.mode = mode;

    // --- 開始SFEN（空なら既定=司令塔側で startpos 扱い）
    if (!startSfenStr.isEmpty()) {
        opt.sfenStart = startSfenStr;
    } else if (sfenRecord && !sfenRecord->isEmpty()) {
        opt.sfenStart = sfenRecord->first();
    } else {
        opt.sfenStart.clear();
    }

    // --- エンジン座席（PlayMode から）
    const bool engineIsP1 =
        (mode == PlayMode::EvenEngineVsHuman) ||
        (mode == PlayMode::HandicapEngineVsHuman);
    opt.engineIsP1 = engineIsP1;
    opt.engineIsP2 = !engineIsP1;

    // --- 対局ダイアログあり：そのまま採用
    if (dlg) {
        const auto engines = dlg->getEngineList();

        const int idx1 = dlg->engineNumber1();
        if (idx1 >= 0 && idx1 < engines.size()) {
            opt.engineName1 = dlg->engineName1();
            opt.enginePath1 = engines.at(idx1).path;
        }

        const int idx2 = dlg->engineNumber2();
        if (idx2 >= 0 && idx2 < engines.size()) {
            opt.engineName2 = dlg->engineName2();
            opt.enginePath2 = engines.at(idx2).path;
        }
        return opt;
    }

    // --- 対局ダイアログなし：INI から直近選択を復元（StartGameDialog と同じ仕様）
    {
        using namespace EngineSettingsConstants;

        QSettings settings(SettingsFileName, QSettings::IniFormat);

        // 1) エンジン一覧（name/path）の読み出し
        struct Eng { QString name; QString path; };
        QVector<Eng> list;
        int count = settings.beginReadArray("Engines");
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            Eng e;
            e.name = settings.value("name").toString();
            e.path = settings.value("path").toString();
            list.push_back(e);
        }
        settings.endArray();

        auto pathForName = [&](const QString& nm) -> QString {
            if (nm.isEmpty()) return {};
            for (const auto& e : list) {
                if (e.name == nm) return e.path;
            }
            return {};
        };

        // 2) 直近の対局設定（GameSettings）からエンジン名を取得
        settings.beginGroup("GameSettings");
        const QString name1 = settings.value("engineName1").toString();
        const QString name2 = settings.value("engineName2").toString();
        settings.endGroup();

        if (!name1.isEmpty()) {
            opt.engineName1 = name1;
            opt.enginePath1 = pathForName(name1);
        }
        if (!name2.isEmpty()) {
            opt.engineName2 = name2;
            opt.enginePath2 = pathForName(name2);
        }
        // パスが空でもここでは許容（initializeAndStartEngineCommunication 内で失敗は通知）
        return opt;
    }
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1)
{
    if (!dlg) return;

    const bool humanP1  = dlg->isHuman1();
    const bool humanP2  = dlg->isHuman2();
    const bool oneHuman = (humanP1 ^ humanP2); // HvE / EvH のときだけ true

    if (!oneHuman) {
        // HvH / EvE は対象外（仕様どおり）
        return;
    }

    // 「現在の向き（bottomIsP1）」と「人間が先手か？」が食い違っていたら1回だけ反転
    const bool needFlip = (humanP1 != bottomIsP1);
    if (needFlip) {
        // MainWindow::onActionFlipBoardTriggered(false) の代わりに司令塔から反転
        // 既存の flipBoard() が view と内部状態を適切に切り替える想定
        this->flipBoard();
        // この呼び出しにより、既存の onBoardFlipped シグナル連鎖で
        // MainWindow 側の m_bottomIsP1 も従来どおりトグルされます
    }
}

void MatchCoordinator::prepareAndStartGame(PlayMode mode,
                                           const QString& startSfenStr,
                                           const QStringList* sfenRecord,
                                           const StartGameDialog* dlg,
                                           bool bottomIsP1)
{
    // 1) オプション構築
    StartOptions opt = buildStartOptions(mode, startSfenStr, sfenRecord, dlg);

    // 2) 人間を手前へ（必要なら反転）
    ensureHumanAtBottomIfApplicable(dlg, bottomIsP1);

    // 3) 対局の構成と開始
    configureAndStart(opt);

    // 4) 初手がエンジン手番なら go→bestmove
    startInitialEngineMoveIfNeeded();
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
    // MainWindow::onMatchTimeUpdated → 司令塔へ
    emit timeTick(); // UI側はこの信号でリフレッシュをかける

    QString turn, p1, p2;
    recomputeClockSnapshot(turn, p1, p2);
    emit uiUpdateTurnAndClock(turn, p1, p2);
}

void MatchCoordinator::handlePlayerTimeOut(int player)
{
    if (!m_gc) return;
    // 負け処理を司令塔で集約
    m_gc->applyTimeoutLossFor(player);
    emit uiNotifyTimeout(player);
    handleGameEnded();
}

void MatchCoordinator::handleResignationRequest()
{
    if (!m_gc) return;
    m_gc->applyResignationOfCurrentSide();
    emit uiNotifyResign();
    handleGameEnded();
}

void MatchCoordinator::handleGameEnded()
{
    if (!m_gc) return;
    m_gc->finalizeGameResult();
    emit uiNotifyGameEnded();
}

void MatchCoordinator::handleGameOverStateChanged()
{
    // 元 MainWindow::onGameOverStateChanged のロジックをここへ
    // 例：go/stopの扱い・UI有効/無効トグルの司令塔視点の判断などを集約
    // 必要に応じて追加のUI信号を定義して通知
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
    if (!m_gameOver.isOver) return;
    if (m_gameOver.moveAppended) return;
    if (!m_clock || !m_hooks.appendKifuLine) {
        markGameOverMoveAppended();
        return;
    }

    // 残り時間を固定
    m_clock->stopClock();

    // 表記（▲/△は絶対座席）
    const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
    const QString line = (cause == Cause::Resignation)
                             ? QStringLiteral("%1投了").arg(mark)
                             : QStringLiteral("%1時間切れ").arg(mark);

    // 「この手」の思考時間を暫定的に反映（KIF 表示のため）
    const qint64 now     = QDateTime::currentMSecsSinceEpoch();
    const qint64 epochMs = turnEpochFor(loser);
    qint64 considerMs    = (epochMs > 0) ? (now - epochMs) : 0;
    if (considerMs < 0) considerMs = 0;
    if (loser == P1) m_clock->setPlayer1ConsiderationTime(int(considerMs));
    else             m_clock->setPlayer2ConsiderationTime(int(considerMs));

    const QString elapsed = (loser == P1)
                                ? m_clock->getPlayer1ConsiderationAndTotalTime()
                                : m_clock->getPlayer2ConsiderationAndTotalTime();

    // 1回だけ即時追記
    m_hooks.appendKifuLine(line, elapsed);

    // HvE/HvH の人間用ストップウォッチ解除
    disarmHumanTimerIfNeeded();

    // 重複追記ブロックを有効化
    markGameOverMoveAppended();
}

void MatchCoordinator::onHumanMove_HvH(ShogiGameController::Player moverBefore)
{
    const Player moverP = (moverBefore == ShogiGameController::Player1) ? P1 : P2;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsiderationFor(moverP);

    // ★ 追加：HvH でも秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
    if (m_clock) {
        if (moverP == P1) {
            m_clock->applyByoyomiAndResetConsideration1();
        } else {
            m_clock->applyByoyomiAndResetConsideration2();
        }
    }

    // 表示更新（時計ラベル等）
    if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] HvH: finalize previous turn"));
    if (m_clock)     handleTimeUpdated(); // 既存の UI 更新経路

    // 次手番の計測と UI 準備
    armTurnTimerIfNeeded();
}

void MatchCoordinator::forceImmediateMove()
{
    if (!m_gc) return;

    const bool isEvE =
        (m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine);

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

void MatchCoordinator::sendStopAllEngines_()
{
    if (m_usi1) m_usi1->sendStopCommand();
    if (m_usi2) m_usi2->sendStopCommand();
}

void MatchCoordinator::sendRawTo_(Usi* which, const QString& cmd)
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

void MatchCoordinator::sendGoTo(Usi* u, const GoTimes& t)
{
    if (!u) return;
    const bool useByoyomi = (t.byoyomi > 0 && t.binc == 0 && t.winc == 0);
    u->sendGoCommand(
        clampMsToInt(t.byoyomi),
        QString::number(t.btime),
        QString::number(t.wtime),
        clampMsToInt(t.binc),
        clampMsToInt(t.winc),
        useByoyomi
        );
}

void MatchCoordinator::sendStopTo(Usi* u)
{
    if (!u) return;
    u->sendStopCommand();
}

void MatchCoordinator::sendRawTo(Usi* u, const QString& cmd)
{
    if (!u) return;
    u->sendRaw(cmd);
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
    // 既に終局でなければ何もしない
    if (!m_gameOver.isOver) return;

    // すでに「中断」等が追記済みなら二重追記防止
    if (m_gameOver.moveAppended) return;

    // 現在手番（＝次に指す側）を GC から取得
    const ShogiGameController::Player gcTurn =
        (m_gc ? m_gc->currentPlayer() : ShogiGameController::NoPlayer);

    // ▲/△は「絶対座席」を表記（P1=▲, P2=△）
    const Player curP = (gcTurn == ShogiGameController::Player1) ? P1 : P2;
    const QString line = (curP == P1) ? QStringLiteral("▲中断") : QStringLiteral("△中断");

    // 「この手」の考慮時間を暫定確定（KIF用）
    // MatchCoordinator 内の turnEpochFor(...) を利用して今の経過msを算出
    if (m_clock) {
        const qint64 now     = QDateTime::currentMSecsSinceEpoch();
        const qint64 epochMs = turnEpochFor(curP);
        qint64 considerMs    = (epochMs > 0) ? (now - epochMs) : 0;
        if (considerMs < 0) considerMs = 0;

        if (curP == P1) m_clock->setPlayer1ConsiderationTime(int(considerMs));
        else            m_clock->setPlayer2ConsiderationTime(int(considerMs));
    }

    // "MM:SS/HH:MM:SS" を時計から取得
    QString elapsed;
    if (m_clock) {
        elapsed = (curP == P1)
        ? m_clock->getPlayer1ConsiderationAndTotalTime()
        : m_clock->getPlayer2ConsiderationAndTotalTime();
    }

    // 棋譜欄に 1 回だけ即時追記（MainWindow::appendKifuLine につながる Hook）
    if (m_hooks.appendKifuLine) {
        m_hooks.appendKifuLine(line, elapsed);
    }

    // 人間用ストップウォッチ解除（HvE/HvHに備える既存パターン）
    disarmHumanTimerIfNeeded();

    // 二重追記ブロック確定（以降は UI 側からも重複しない）
    markGameOverMoveAppended();
}

ShogiClock* MatchCoordinator::clock()
{
    return m_clock;
}

const ShogiClock* MatchCoordinator::clock() const
{
    return m_clock;
}
