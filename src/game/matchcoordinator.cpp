/// @file matchcoordinator.cpp
/// @brief 対局進行コーディネータ（司令塔）クラスの実装

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
#include "settingsservice.h"
#include "kifurecordlistmodel.h"
#include "sfenpositiontracer.h"
#include "sennichitedetector.h"

Q_LOGGING_CATEGORY(lcGame, "shogi.game")

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

    m_sfenRecord = d.sfenRecord;

    // デバッグ：どのリストを使うか明示
    qCDebug(lcGame).noquote()
        << "shared sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
        << " eveSfenRecord@=" << static_cast<const void*>(&m_eveSfenRecord);

    // 念のため NPE ガード（無いと困る設計なのでログだけ）
    if (!m_sfenRecord) {
        qCWarning(lcGame) << "sfenRecord is null! Presenterと同期できません。Deps.sfenRecordを渡してください。";
    }

    wireClock();
}

MatchCoordinator::~MatchCoordinator() = default;

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    m_usi1 = e1;
    m_usi2 = e2;
}

// ============================================================
// 投了・終局処理
// ============================================================

void MatchCoordinator::handleResign() {
    // すでに終局なら何もしない（中断後のタイムアウト等で呼ばれるのを防ぐ）
    if (m_gameOver.isOver) {
        qCDebug(lcGame) << "handleResign: already game over, ignoring";
        return;
    }

    GameEndInfo info;
    info.cause = Cause::Resignation;

    // 投了は「現在手番側」が行う：GCの現在手番から判定
    info.loser = (m_gc && m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
    // 勝者は敗者の逆（loserと同じソースから計算）
    const Player winner = (info.loser == P1) ? P2 : P1;

    // エンジンへの最終通知（HvE / EvE の両方に対応）
    // HvE/HvH は PlayMode から判定（m_usi2 の状態に依存しない）
    const bool isHvE = (m_playMode == PlayMode::EvenHumanVsEngine) ||
                       (m_playMode == PlayMode::EvenEngineVsHuman) ||
                       (m_playMode == PlayMode::HandicapHumanVsEngine) ||
                       (m_playMode == PlayMode::HandicapEngineVsHuman);
    const bool isHvH = (m_playMode == PlayMode::HumanVsHuman);

    if (m_hooks.sendRawToEngine) {
        if (isHvE) {
            // HvE：人間が投了＝エンジン勝ち。
            if (m_usi1) {
                m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
                m_hooks.sendRawToEngine(m_usi1, QStringLiteral("quit"));
            }
        } else if (!isHvH) {
            // EvE：勝者/敗者のエンジンそれぞれに通知
            Usi* winEng  = (winner     == P1) ? m_usi1 : m_usi2;
            Usi* loseEng = (info.loser == P1) ? m_usi1 : m_usi2;
            if (loseEng) m_hooks.sendRawToEngine(loseEng, QStringLiteral("gameover lose"));
            if (winEng)  {
                m_hooks.sendRawToEngine(winEng,  QStringLiteral("gameover win"));
                m_hooks.sendRawToEngine(winEng,  QStringLiteral("quit"));
            }
        }
        // HvH の場合はエンジンへの通知不要
    } else {
        // hooks未指定でも最低限の通知を司令塔内で実施
        if (isHvE) {
            // HvE：人間が投了＝エンジン勝ち。
            if (m_usi1) {
                sendRawTo(m_usi1, QStringLiteral("gameover win"));
                sendRawTo(m_usi1, QStringLiteral("quit"));
            }
        } else if (!isHvH) {
            // EvE：勝者/敗者のエンジンそれぞれに通知
            Usi* winEng  = (winner     == P1) ? m_usi1 : m_usi2;
            Usi* loseEng = (info.loser == P1) ? m_usi1 : m_usi2;
            if (loseEng) sendRawTo(loseEng, QStringLiteral("gameover lose"));
            if (winEng)  {
                sendRawTo(winEng,  QStringLiteral("gameover win"));
                sendRawTo(winEng,  QStringLiteral("quit"));
            }
        }
        // HvH の場合はエンジンへの通知不要
    }

    // 司令塔のゲームオーバー状態を確定（棋譜「投了」一意追記は appendMoveOnce=true で司令塔→UIへ）
    setGameOver(info, /*loserIsP1=*/(info.loser==P1), /*appendMoveOnce=*/true);

    // 投了時も結果ダイアログを表示
    displayResultsAndUpdateGui(info);
}

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

    // エンジン投了時も結果ダイアログを表示
    displayResultsAndUpdateGui(info);
}

void MatchCoordinator::handleEngineWin(int idx) {
    // エンジン入玉宣言勝ち＝宣言者がエンジン、成功=true、引き分け=false
    const Player declarer = (idx == 1 ? P1 : P2);
    handleNyugyokuDeclaration(declarer, /*success=*/true, /*isDraw=*/false);
}

void MatchCoordinator::handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw)
{
    // すでに終局なら何もしない
    if (m_gameOver.isOver) return;

    qCInfo(lcGame) << "handleNyugyokuDeclaration(): declarer=" << (declarer == P1 ? "P1" : "P2")
                    << " success=" << success << " isDraw=" << isDraw;

    // 進行系タイマを停止
    disarmHumanTimerIfNeeded();

    // 時計を停止
    if (m_clock) m_clock->stopClock();

    // GameEndInfo を構築
    GameEndInfo info;
    if (isDraw) {
        // 持将棋（引き分け）
        info.cause = Cause::Jishogi;
        info.loser = declarer;  // 持将棋は勝敗なし、宣言者のマーク表示用
    } else if (success) {
        // 入玉宣言勝ち
        info.cause = Cause::NyugyokuWin;
        info.loser = (declarer == P1) ? P2 : P1;  // 宣言者の相手が敗者
    } else {
        // 入玉宣言失敗（反則負け）
        info.cause = Cause::IllegalMove;
        info.loser = declarer;  // 宣言者が敗者
    }

    // エンジンへの終了通知
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);
    if (isEvE) {
        if (isDraw) {
            // 引き分け
            if (m_usi1) {
                m_usi1->sendGameOverCommand(QStringLiteral("draw"));
                m_usi1->sendQuitCommand();
                m_usi1->setSquelchResignLogging(true);
            }
            if (m_usi2) {
                m_usi2->sendGameOverCommand(QStringLiteral("draw"));
                m_usi2->sendQuitCommand();
                m_usi2->setSquelchResignLogging(true);
            }
        } else {
            // 勝敗あり
            Usi* winnerEng = (info.loser == P1) ? m_usi2 : m_usi1;
            Usi* loserEng  = (info.loser == P1) ? m_usi1 : m_usi2;
            if (winnerEng) {
                winnerEng->sendGameOverCommand(QStringLiteral("win"));
                winnerEng->sendQuitCommand();
                winnerEng->setSquelchResignLogging(true);
            }
            if (loserEng) {
                loserEng->sendGameOverCommand(QStringLiteral("lose"));
                loserEng->sendQuitCommand();
                loserEng->setSquelchResignLogging(true);
            }
        }
    } else {
        // HvE の場合は主エンジンへ通知
        if (Usi* eng = primaryEngine()) {
            if (isDraw) {
                eng->sendGameOverCommand(QStringLiteral("draw"));
            } else if (info.loser == P1) {
                // P1（人間）の負け
                eng->sendGameOverCommand(QStringLiteral("win"));
            } else {
                // P2（エンジン）の負け
                eng->sendGameOverCommand(QStringLiteral("lose"));
            }
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    // 終局状態を確定
    const bool loserIsP1 = (info.loser == P1);
    setGameOver(info, loserIsP1, /*appendMoveOnce=*/true);

    // 結果ダイアログは既にMainWindowで表示済みなので省略
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

void MatchCoordinator::displayResultsAndUpdateGui(const GameEndInfo& info) {
    // 対局中メニューのON/OFFなどUI側の状態を更新
    setGameInProgressActions(false);

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
    case Cause::Jishogi:
        // 持将棋（最大手数到達）
        msg = tr("最大手数に達しました。持将棋です。");
        break;
    case Cause::NyugyokuWin:
        // 入玉宣言勝ち
        msg = tr("%1の入玉宣言。%2の勝ちです。").arg(winnerJP, winnerJP);
        break;
    case Cause::IllegalMove:
        // 反則負け（入玉宣言失敗など）
        msg = tr("%1の反則負け。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::Sennichite:
        msg = tr("千日手が成立しました。");
        break;
    case Cause::OuteSennichite:
        msg = tr("%1の連続王手の千日手。%2の勝ちです。").arg(loserJP, winnerJP);
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

    // 棋譜自動保存
    if (m_autoSaveKifu && !m_kifuSaveDir.isEmpty() && m_hooks.autoSaveKifu) {
        qCInfo(lcGame) << "Calling autoSaveKifu hook: dir=" << m_kifuSaveDir;
        m_hooks.autoSaveKifu(m_kifuSaveDir, m_playMode,
                             m_humanName1, m_humanName2,
                             m_engineNameForSave1, m_engineNameForSave2);
    }

    emit gameEnded(info);
}

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

    if (!m_comm1)  { m_comm1  = comm1;  qCWarning(lcGame) << "EvE comm1 fallback created"; }
    if (!m_think1) { m_think1 = think1; qCWarning(lcGame) << "EvE think1 fallback created"; }
    if (!m_comm2)  { m_comm2  = comm2;  qCWarning(lcGame) << "EvE comm2 fallback created"; }
    if (!m_think2) { m_think2 = think2; qCWarning(lcGame) << "EvE think2 fallback created"; }

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName1 = engineName1.isEmpty() ? QStringLiteral("Engine") : engineName1;
    const QString dispName2 = engineName2.isEmpty() ? QStringLiteral("Engine") : engineName2;
    if (comm1) comm1->setEngineName(dispName1);
    if (comm2) comm2->setEngineName(dispName2);

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
// 対局開始フロー
// ============================================================

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    // 直前の対局で更新された m_positionStr1 を履歴に反映してから保存
    // （ゲーム中に指し手が追加されても m_positionStrHistory は更新されないため、
    //   次の対局開始時に最終状態を確定させる）
    if (!m_positionStr1.isEmpty() && m_positionStr1.startsWith(QLatin1String("position "))) {
        // m_positionStrHistory が空、または最後の要素と異なる場合は追加/更新
        if (m_positionStrHistory.isEmpty()) {
            m_positionStrHistory.append(m_positionStr1);
        } else if (m_positionStrHistory.constLast() != m_positionStr1) {
            // 最後の要素を最新の m_positionStr1 で置き換え
            m_positionStrHistory[m_positionStrHistory.size() - 1] = m_positionStr1;
        }
        qCDebug(lcGame).noquote() << "configureAndStart: synced m_positionStrHistory with m_positionStr1=" << m_positionStr1;
    }

    // 直前の対局履歴が残っていれば、それを確定した過去の対局として蓄積
    // (m_positionStrHistory はこの後クリアされるため、その前に保存)
    if (!m_positionStrHistory.isEmpty()) {
        m_allGameHistories.append(m_positionStrHistory);
        // メモリリーク防止：履歴数を制限
        while (m_allGameHistories.size() > kMaxGameHistories) {
            m_allGameHistories.removeFirst();
        }
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

    if (!m_allGameHistories.isEmpty()) {
        qCDebug(lcGame) << "=== Accumulated Game Histories (Count:" << m_allGameHistories.size() << ") ===";
        for (qsizetype i = 0; i < m_allGameHistories.size(); ++i) {
            qCDebug(lcGame) << " [Game" << (i + 1) << "]";
            const QStringList& rec = m_allGameHistories.at(i);
            for (const QString& pos : rec) {
                qCDebug(lcGame).noquote() << "  " << pos;
            }
        }
        qCDebug(lcGame) << "=======================================================";
        qCDebug(lcGame) << "--- Searching for start position in previous games ---";

        for (qsizetype i = 0; i < m_allGameHistories.size(); ++i) {
            const QStringList& hist = m_allGameHistories.at(i);
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
                const qsizetype mIdx = fullCmd.indexOf(QLatin1String(" moves "));
                const QString sfenPart = (mIdx == -1) ? fullCmd.mid(14)
                                                      : fullCmd.mid(14, mIdx - 14);
                tracer.setFromSfen(sfenPart);
                if (mIdx != -1) {
                    moves = fullCmd.mid(mIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
                }
            }

            // 2) 0手目（開始局面）の比較
            if (tracer.toSfenString() == targetSfen) {
                qCDebug(lcGame).noquote()
                << QString(" -> MATCH FOUND: [Game %1] Start Position (Move 0)").arg(i + 1);
                if (bestGameIdx == -1) { // 最初の一致を採用
                    bestGameIdx  = static_cast<int>(i);
                    bestBaseFull = fullCmd;
                    bestMatchPly = 0;
                }
            }

            // 3) 1手ずつ進めて比較
            for (qsizetype m = 0; m < moves.size(); ++m) {
                tracer.applyUsiMove(moves[m]);
                if (tracer.toSfenString() == targetSfen) {
                    qCDebug(lcGame).noquote()
                    << QString(" -> MATCH FOUND: [Game %1] Move %2").arg(i + 1).arg(m + 1);
                    if (bestGameIdx == -1) { // 最初の一致を採用
                        bestGameIdx  = static_cast<int>(i);
                        bestBaseFull = fullCmd;
                        bestMatchPly = static_cast<int>(m + 1);
                    }
                }
            }
        }
        qCDebug(lcGame) << "----------------------------------------------------";
    }

    // 前回の対局終了状態をクリア（再対局時に棋譜追加がブロックされる問題を修正）
    clearGameOverState();

    m_playMode = opt.mode;
    m_maxMoves = opt.maxMoves;

    // 棋譜自動保存設定を保存
    m_autoSaveKifu = opt.autoSaveKifu;
    m_kifuSaveDir = opt.kifuSaveDir;
    m_humanName1 = opt.humanName1;
    m_humanName2 = opt.humanName2;
    m_engineNameForSave1 = opt.engineName1;
    m_engineNameForSave2 = opt.engineName2;

    // 盤・名前などの初期化（GUI側へ委譲）
    qCDebug(lcGame).noquote() << "configureAndStart: calling hooks";
    qCDebug(lcGame).noquote() << "configureAndStart: opt.engineName1=" << opt.engineName1 << " opt.engineName2=" << opt.engineName2;
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
    if (m_hooks.setPlayersNames) {
        qCDebug(lcGame).noquote() << "configureAndStart: calling setPlayersNames(\"\", \"\")";
        m_hooks.setPlayersNames(QString(), QString());
    }
    if (m_hooks.setEngineNames) {
        qCDebug(lcGame).noquote() << "configureAndStart: calling setEngineNames(" << opt.engineName1 << "," << opt.engineName2 << ")";
        m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
    }
    if (m_hooks.setGameActions)    m_hooks.setGameActions(true);
    qCDebug(lcGame).noquote() << "configureAndStart: hooks done";

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
    updateTurnDisplay(m_cur);

    // ------------------------------------------------------------
    // 探索で見つけたゲームのヘッダをベースに moves だけをトリム
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
        const qsizetype pos = full.indexOf(movesKey);
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
    qCDebug(lcGame).noquote() << "configureAndStart: keepMoves(from sfenStart)=" << keepMoves;

    // --- ベース候補：探索一致ゲームのみ採用
    // マッチしなかった場合は履歴からの再利用をスキップ
    // （異なる初期局面の履歴を再利用すると、駒落ちなのに "position startpos" が送信される問題が発生するため）
    QString candidateBase;
    if (!bestBaseFull.isEmpty()) {
        candidateBase = bestBaseFull;
        qCDebug(lcGame).noquote()
            << "configureAndStart: base=matched game"
            << "gameIdx=" << (bestGameIdx + 1) << "matchPly=" << bestMatchPly
            << "full=" << candidateBase;
    }

    bool applied = false;
    if (!candidateBase.isEmpty() && keepMoves >= 0) {
        if (candidateBase.startsWith(QLatin1String("position "))) {
            const QString trimmed = trimMovesPreserveHeader(candidateBase, keepMoves);
            qCDebug(lcGame).noquote() << "configureAndStart: trimmed(base, keep=" << keepMoves << ")=" << trimmed;

            // trimmedがkeepMoves分の手を持っているか確認
            // candidateBase に十分な手数がない場合は、フォールバックに任せる
            auto countMoves = [](const QString& posStr) -> int {
                const qsizetype idx = posStr.indexOf(QLatin1String(" moves "));
                if (idx < 0) return 0;
                const QString after = posStr.mid(idx + 7).trimmed();
                if (after.isEmpty()) return 0;
                return static_cast<int>(after.split(QLatin1Char(' '), Qt::SkipEmptyParts).size());
            };
            const int actualMoves = countMoves(trimmed);

            if (actualMoves >= keepMoves) {
                m_positionStr1     = trimmed;
                m_positionPonder1  = trimmed;

                // 履歴はベースを差し替えて 1 件から再スタート（UNDO用）
                m_positionStrHistory.clear();
                m_positionStrHistory.append(trimmed);

                applied = true;
                qCDebug(lcGame).noquote() << "configureAndStart: applied=true (actualMoves=" << actualMoves << " >= keepMoves=" << keepMoves << ")";
            } else {
                qCDebug(lcGame).noquote() << "configureAndStart: candidateBase has insufficient moves (actualMoves=" << actualMoves << " < keepMoves=" << keepMoves << "), falling back to SFEN";
            }
        } else {
            qCWarning(lcGame).noquote()
            << "configureAndStart: candidateBase is not 'position ...' :" << candidateBase;
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
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine: {
        // エンジン同士は両方起動してから開始
        initEnginesForEvE(opt.engineName1, opt.engineName2);
        initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
        initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);
        startEngineVsEngine(opt);
        break;
    }

    // 平手：先手エンジン（P1エンジン固定）
    case PlayMode::EvenEngineVsHuman: {
        startHumanVsEngine(opt, /*engineIsP1=*/true);
        break;
    }

    // 平手：後手エンジン（P2エンジン固定）
    case PlayMode::EvenHumanVsEngine: {
        startHumanVsEngine(opt, /*engineIsP1=*/false);
        break;
    }

    // 駒落ち：先手エンジン（下手＝P1エンジン固定）
    case PlayMode::HandicapEngineVsHuman: {
        startHumanVsEngine(opt, /*engineIsP1=*/true);
        break;
    }

    // 駒落ち：後手エンジン（上手＝P2エンジン固定）
    case PlayMode::HandicapHumanVsEngine: {
        startHumanVsEngine(opt, /*engineIsP1=*/false);
        break;
    }

    case PlayMode::HumanVsHuman:
        startHumanVsHuman(opt);
        break;

    default:
        qCWarning(lcGame).noquote()
            << "configureAndStart: unexpected playMode="
            << static_cast<int>(m_playMode);
        break;
    }

    // ===== デバッグ: 退出時点の m_positionStr1 を記録 =====
    qCDebug(lcGame).noquote() << "configureAndStart: LEAVE m_positionStr1(final)=" << m_positionStr1
                       << "hist.size=" << m_positionStrHistory.size()
                       << "hist.last=" << (m_positionStrHistory.isEmpty() ? QString("<empty>") : m_positionStrHistory.constLast());
}

// ============================================================
// 対局モード別の開始処理
// ============================================================

void MatchCoordinator::startHumanVsHuman(const StartOptions& /*opt*/)
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
    updateTurnDisplay(m_cur);
}

// HvE（人間対エンジン）
//   engineIsP1 == true ならエンジンは先手座席、false なら後手座席
void MatchCoordinator::startHumanVsEngine(const StartOptions& opt, bool engineIsP1)
{
    if (m_hooks.log) {
        m_hooks.log(QStringLiteral("[Match] Start HvE (engineIsP1=%1)").arg(engineIsP1));
    }

    // 以前のエンジンは破棄（安全化）
    destroyEngines();

    // engineIsP1 に応じて適切なエンジン情報を選択する
    //    - engineIsP1=true（エンジン対人間）: エンジン1の情報を使う
    //    - engineIsP1=false（人間対エンジン）: エンジン2の情報を使う
    //    ただし、思考モデルは常にエンジン1スロット（m_comm1/m_think1）を使用
    const QString& enginePath = engineIsP1 ? opt.enginePath1 : opt.enginePath2;
    const QString& engineName = engineIsP1 ? opt.engineName1 : opt.engineName2;

    // HvEでは思考モデルは常にエンジン1スロット（m_comm1/m_think1）を使用
    UsiCommLogModel*          comm  = m_comm1;
    ShogiEngineThinkingModel* think = m_think1;
    if (!comm)  { comm  = new UsiCommLogModel(this);          m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(this); m_think1 = think; }

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    if (comm) comm->setEngineName(dispName);

    // USI を生成（この時点ではプロセス未起動）
    // HvEでは常に m_usi1 を使用
    m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);
    m_usi2 = nullptr;

    // 投了配線
    wireResignToArbiter(m_usi1, /*asP1=*/engineIsP1);
    // 入玉宣言勝ち配線
    wireWinToArbiter(m_usi1, /*asP1=*/engineIsP1);

    // ログ識別（UI 表示用）
    if (m_usi1) {
        m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), dispName);
        m_usi1->setSquelchResignLogging(false);
    }

    // USI エンジンを起動（path/name 必須）
    const Player engineSide = engineIsP1 ? P1 : P2;
    initializeAndStartEngineFor(engineSide, enginePath, engineName);

    // configureAndStart()で既にエンジン名が正しく設定されているため、
    // ここでの setEngineNames 呼び出しは不要（e2を空で上書きしてしまう問題があった）
    // if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName1, QString());

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
    updateTurnDisplay(m_cur);
}

// EvE の初手を開始する（起動・初期化済み前提）
void MatchCoordinator::startEngineVsEngine(const StartOptions& opt)
{
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    // EvE対局で初手からタイマーを動作させるため、ここで時計を開始する
    if (m_clock) {
        m_clock->startClock();
        qCDebug(lcGame) << "Clock started";
    }

    // 駒落ちの場合、SFENで手番が「w」（後手番）になっている
    // GCの currentPlayer() がその手番を持っているはず
    if (m_gc->currentPlayer() == ShogiGameController::NoPlayer) {
        // 平手なら先手から、駒落ちならSFENに従う
        m_gc->setCurrentPlayer(ShogiGameController::Player1);
    }
    m_cur = (m_gc->currentPlayer() == ShogiGameController::Player2) ? P2 : P1;
    updateTurnDisplay(m_cur);

    initPositionStringsForEvE(opt.sfenStart);

    // 駒落ちの場合は後手（上手）から開始
    const bool isHandicap = (m_playMode == PlayMode::HandicapEngineVsEngine);
    const bool whiteToMove = (m_gc->currentPlayer() == ShogiGameController::Player2);

    if (isHandicap && whiteToMove) {
        // 駒落ち：後手（上手 = m_usi2）が初手を指す
        startEvEFirstMoveByWhite();
    } else {
        // 平手：先手（下手 = m_usi1）が初手を指す
        startEvEFirstMoveByBlack();
    }
}

// 平手EvE：先手から開始
void MatchCoordinator::startEvEFirstMoveByBlack()
{
    const GoTimes t1 = computeGoTimes();
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

    // 先手1手目：次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p1From, p1To, rec1,
            pm,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
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
    updateTurnDisplay((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

    if (m_usi2) {
        m_usi2->setPreviousFileTo(p1To.x());
        m_usi2->setPreviousRankTo(p1To.y());
    }

    m_positionStr2     = m_positionStr1;
    m_positionPonder2.clear();

    const GoTimes t2 = computeGoTimes();
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

    // 後手1手目：次の手を渡す
    nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p2From, p2To, rec2,
            pm,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
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
    updateTurnDisplay((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

    // P2の手をP1のポジション文字列に同期
    m_positionStr1 = m_positionStr2;

    QTimer::singleShot(std::chrono::milliseconds(0), this, &MatchCoordinator::kickNextEvETurn);
}

// 駒落ちEvE：後手（上手）から開始
void MatchCoordinator::startEvEFirstMoveByWhite()
{
    // 後手（上手 = m_usi2）が初手を指す
    const GoTimes t2 = computeGoTimes();
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
    PlayMode pm = m_playMode;

    // 後手（上手）1手目
    int nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p2From, p2To, rec2,
            pm,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
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
    updateTurnDisplay((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

    if (m_usi1) {
        m_usi1->setPreviousFileTo(p2To.x());
        m_usi1->setPreviousRankTo(p2To.y());
    }

    m_positionStr1     = m_positionStr2;
    m_positionPonder1.clear();

    // 先手（下手 = m_usi1）が2手目を指す
    const GoTimes t1 = computeGoTimes();
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

    // 先手（下手）2手目
    nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(
            p1From, p1To, rec1,
            pm,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
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
    updateTurnDisplay((m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2);

    // P1の手をP2のポジション文字列に同期
    m_positionStr2 = m_positionStr1;

    QTimer::singleShot(std::chrono::milliseconds(0), this, &MatchCoordinator::kickNextEvETurn);
}

Usi* MatchCoordinator::primaryEngine() const
{
    return m_usi1;
}

Usi* MatchCoordinator::secondaryEngine() const
{
    return m_usi2;
}

void MatchCoordinator::initPositionStringsForEvE(const QString& sfenStart)
{
    m_positionStr1.clear();
    m_positionPonder1.clear();
    m_positionStr2.clear();
    m_positionPonder2.clear();

    // 平手の場合は startpos を使用、駒落ちの場合は sfen を使用
    static const QString kStartBoard =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    QString base;
    if (m_playMode == PlayMode::HandicapEngineVsEngine && !sfenStart.isEmpty()) {
        // SFENから盤面部分を抽出して平手かどうか判定
        QString checkSfen = sfenStart;
        if (checkSfen.startsWith(QLatin1String("position sfen "))) {
            checkSfen = checkSfen.mid(QStringLiteral("position sfen ").size()).trimmed();
        }
        const QStringList tok = checkSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const bool isStandardStart = (!tok.isEmpty() && tok[0] == kStartBoard);

        if (!isStandardStart) {
            // 駒落ち：position sfen <sfen> moves の形式を使用
            if (sfenStart.startsWith(QLatin1String("position "))) {
                base = sfenStart;
                // 末尾に " moves" がなければ追加
                if (!base.contains(QLatin1String(" moves"))) {
                    base += QStringLiteral(" moves");
                }
            } else {
                base = QStringLiteral("position sfen ") + sfenStart + QStringLiteral(" moves");
            }
        } else {
            base = QStringLiteral("position startpos moves");
        }
    } else {
        base = QStringLiteral("position startpos moves");
    }
    m_positionStr1 = base;
    m_positionStr2 = base;
}

void MatchCoordinator::kickNextEvETurn()
{
    if (m_playMode != PlayMode::EvenEngineVsEngine && m_playMode != PlayMode::HandicapEngineVsEngine) return;
    if (!m_usi1 || !m_usi2 || !m_gc) return;

    const bool p1ToMove = (m_gc->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? m_usi1 : m_usi2;
    Usi* receiver = p1ToMove ? m_usi2 : m_usi1;

    QString& pos    = p1ToMove ? m_positionStr1     : m_positionStr2;
    QString& ponder = p1ToMove ? m_positionPonder1  : m_positionPonder2;

    QPoint from(-1,-1), to(-1,-1);
    if (!engineThinkApplyMove(mover, pos, ponder, &from, &to))
        return;

    QString rec;

    // 次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_gc->validateAndMove(from, to, rec, m_playMode,
                               nextEve, sfenRecordForEvE(), gameMovesForEvE())) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    // 相手側のポジション文字列を同期
    if (p1ToMove) {
        m_positionStr2 = m_positionStr1;
    } else {
        m_positionStr1 = m_positionStr2;
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
    updateTurnDisplay(
        (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2
        );

    // 千日手チェック
    if (checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (m_maxMoves > 0 && m_eveMoveIndex >= m_maxMoves) {
        handleMaxMovesJishogi();
        return;
    }

    QTimer::singleShot(0, this, &MatchCoordinator::kickNextEvETurn);
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

// ============================================================
// 人間側の計測（HvEでの人間手）
// ============================================================

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
    m_bTimeStr = QString::number(b);
    m_wTimeStr = QString::number(w);
    emit timesForUSIUpdated(b, w);
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
    // これにより、エンジンからの "bestmove resign" が処理されても
    // 棋譜に「投了」が追加されることを防ぐ
    if (m_usi1) {
        QObject::disconnect(m_usi1, &Usi::bestMoveResignReceived, this, nullptr);
    }
    if (m_usi2) {
        QObject::disconnect(m_usi2, &Usi::bestMoveResignReceived, this, nullptr);
    }

    // すでに終局なら何もしない
    if (m_gameOver.isOver) return;

    // 終局フラグを立てる
    m_gameOver.isOver         = true;
    m_gameOver.when           = QDateTime::currentDateTime();
    m_gameOver.hasLast        = true;
    m_gameOver.lastInfo.cause = Cause::BreakOff;

    // 現在手番を loser に設定（中断時のマーク表示用、実際の勝敗はない）
    const ShogiGameController::Player gcTurn =
        (m_gc ? m_gc->currentPlayer() : ShogiGameController::NoPlayer);
    m_gameOver.lastInfo.loser = (gcTurn == ShogiGameController::Player1) ? P1 : P2;

    // 時計を即座に停止 & ゲームオーバーマーク（タイムアウト処理を防ぐ）
    if (m_clock) {
        m_clock->markGameOver();
        m_clock->stopClock();
    }

    // 進行系タイマを停止（人間用のみでOK）
    disarmHumanTimerIfNeeded();

    // EvE かどうかを判定（エンジン終了処理に必要）
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);

    // 思考中のエンジンを中断してから終了通知を送る
    if (isEvE) {
        // EvE: 両方のエンジンに stop を送って思考を中断
        if (m_usi1) m_usi1->sendStopCommand();
        if (m_usi2) m_usi2->sendStopCommand();

        // gameover 通知を送る（中断は引き分け扱い）
        if (m_usi1) {
            m_usi1->sendGameOverCommand(QStringLiteral("draw"));
            m_usi1->setSquelchResignLogging(true);
        }
        if (m_usi2) {
            m_usi2->sendGameOverCommand(QStringLiteral("draw"));
            m_usi2->setSquelchResignLogging(true);
        }
    } else {
        // HvE: 主エンジンに stop を送って思考を中断
        if (Usi* eng = primaryEngine()) {
            eng->sendStopCommand();
            eng->sendGameOverCommand(QStringLiteral("draw"));
            eng->setSquelchResignLogging(true);
        }
    }

    // UI側に終局を通知（対局者名・時間ラベルのスタイル維持のため）
    // appendBreakOffLineAndMark() より先に emit して setGameOverStyleLock(true) を設定し、
    // リプレイモード遷移時の clearTurnHighlight() で黄色ハイライトが消えないようにする
    emit gameEnded(m_gameOver.lastInfo);

    // 中断行の生成＋KIF追記＋一度だけの追記ブロック確定（内部で emit 済み）
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
    qCDebug(lcGame).noquote() << "startAnalysis ENTER:"
                       << "mode=" << static_cast<int>(opt.mode)
                       << "byoyomiMs=" << opt.byoyomiMs
                       << "multiPV=" << opt.multiPV
                       << "m_inConsiderationMode=" << m_inConsiderationMode
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
    UsiCommLogModel*          comm  = m_comm1;
    ShogiEngineThinkingModel* think = m_think1;
    if (!comm)  { comm  = new UsiCommLogModel(this);          m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(this); m_think1 = think; }

    // エンジン名をログモデルに設定（思考タブ表示用）
    if (comm) {
        comm->setEngineName(opt.engineName);
    }

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

    // 9) 詰み探索の配線（TsumiSearchMode のときのみ）
    m_inTsumeSearchMode = (opt.mode == PlayMode::TsumiSearchMode);
    if (m_inTsumeSearchMode && m_usi1) {
        connect(m_usi1, &Usi::checkmateSolved,
                this,  &MatchCoordinator::onCheckmateSolved,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateNoMate,
                this,  &MatchCoordinator::onCheckmateNoMate,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateNotImplemented,
                this,  &MatchCoordinator::onCheckmateNotImplemented,
                Qt::UniqueConnection);
        connect(m_usi1, &Usi::checkmateUnknown,
                this,  &MatchCoordinator::onCheckmateUnknown,
                Qt::UniqueConnection);
        // bestmove 受信時も通知（checkmate 非対応エンジン用）
        connect(m_usi1, &Usi::bestMoveReceived,
                this,   &MatchCoordinator::onTsumeBestMoveReceived,
                Qt::UniqueConnection);
    }

    // 10) 検討タブ用モデルを設定
    if (opt.considerationModel && opt.mode == PlayMode::ConsiderationMode) {
        m_usi1->setConsiderationModel(opt.considerationModel, opt.multiPV);
    }

    // 10.1) 前回の移動先を設定（「同」表記のため）
    qCDebug(lcGame).noquote() << "startAnalysis: opt.previousFileTo=" << opt.previousFileTo
                       << "opt.previousRankTo=" << opt.previousRankTo;
    if (opt.previousFileTo > 0 && opt.previousRankTo > 0) {
        m_usi1->setPreviousFileTo(opt.previousFileTo);
        m_usi1->setPreviousRankTo(opt.previousRankTo);
        qCDebug(lcGame).noquote() << "startAnalysis: setPreviousFileTo/RankTo:"
                           << opt.previousFileTo << "/" << opt.previousRankTo;
    } else {
        qCWarning(lcGame).noquote() << "startAnalysis: previousFileTo/RankTo not set (values are 0)";
    }

    // 10.5) 検討モードの場合、フラグを設定し bestmove を接続
    if (opt.mode == PlayMode::ConsiderationMode) {
        m_inConsiderationMode = true;
        // 検討の状態を保存（MultiPV変更時・ポジション変更時の再開用）
        m_considerationPositionStr = opt.positionStr;
        m_considerationByoyomiMs = opt.byoyomiMs;
        m_considerationMultiPV = opt.multiPV;
        m_considerationModelPtr = opt.considerationModel;
        m_considerationRestartPending = false;
        m_considerationWaiting = false;  // 待機フラグをリセット
        // 注意: m_considerationRestartInProgress はここではリセットしない
        // （restartConsiderationDeferred から呼ばれた場合は呼び出し元でリセットする）
        m_considerationEnginePath = opt.enginePath;
        m_considerationEngineName = opt.engineName;
        m_considerationPreviousFileTo = opt.previousFileTo;
        m_considerationPreviousRankTo = opt.previousRankTo;
        m_considerationLastUsiMove = opt.lastUsiMove;

        connect(m_usi1, &Usi::bestMoveReceived,
                this,   &MatchCoordinator::onConsiderationBestMoveReceived,
                Qt::UniqueConnection);
    }

    // 11) 解析/詰み探索の実行
    QString pos = opt.positionStr; // "position sfen <...>"
    qCDebug(lcGame).noquote() << "startAnalysis: about to start analysis communication, byoyomiMs=" << opt.byoyomiMs;
    if (opt.mode == PlayMode::TsumiSearchMode) {
        m_usi1->executeTsumeCommunication(pos, opt.byoyomiMs);
        qCDebug(lcGame).noquote() << "startAnalysis EXIT (executeTsumeCommunication returned)";
        return;
    }

    if (opt.mode == PlayMode::ConsiderationMode) {
        // 検討は非ブロッキングで開始し、UIフリーズを避ける
        m_usi1->sendAnalysisCommands(pos, opt.byoyomiMs, opt.multiPV);
        qCDebug(lcGame).noquote() << "startAnalysis EXIT (sendAnalysisCommands queued)";
        return;
    }

    m_usi1->executeAnalysisCommunication(pos, opt.byoyomiMs, opt.multiPV);
    qCDebug(lcGame).noquote() << "startAnalysis EXIT (executeAnalysisCommunication returned)";
}

void MatchCoordinator::stopAnalysisEngine()
{
    qCDebug(lcGame).noquote() << "stopAnalysisEngine called";

    // エンジン破棄中フラグをセット（検討再開の再入防止）
    m_engineShutdownInProgress = true;

    // 検討モード中なら終了シグナルを発火
    if (m_inConsiderationMode) {
        m_inConsiderationMode = false;
        m_considerationRestartInProgress = false;  // 再入防止フラグもリセット
        m_considerationWaiting = false;  // 待機フラグもリセット
        emit considerationModeEnded();
    }

    const bool wasTsumeSearch = m_inTsumeSearchMode;
    m_inTsumeSearchMode = false;  // フラグをリセット
    if (wasTsumeSearch) {
        emit tsumeSearchModeEnded();
    }
    destroyEngines(false);  // 思考内容を保持

    // エンジン破棄完了後にフラグをリセット
    m_engineShutdownInProgress = false;
}

void MatchCoordinator::updateConsiderationMultiPV(int multiPV)
{
    qCDebug(lcGame).noquote() << "updateConsiderationMultiPV called: multiPV=" << multiPV;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "updateConsiderationMultiPV: not in consideration mode, ignoring";
        return;
    }

    // 値が同じなら何もしない
    if (m_considerationMultiPV == multiPV) {
        qCDebug(lcGame).noquote() << "updateConsiderationMultiPV: same value, ignoring";
        return;
    }

    // 新しいMultiPV値を保存し、再開フラグを設定
    m_considerationMultiPV = multiPV;
    m_considerationRestartPending = true;

    // 検討タブ用モデルをクリア
    if (m_considerationModelPtr) {
        m_considerationModelPtr->clearAllItems();
    }

    // エンジンを停止（bestmove を受信後に再開する）
    if (m_usi1) {
        qCDebug(lcGame).noquote() << "updateConsiderationMultiPV: sending stop to restart with new MultiPV";
        m_usi1->sendStopCommand();
    }
}

bool MatchCoordinator::updateConsiderationPosition(const QString& newPositionStr,
                                                   int previousFileTo, int previousRankTo,
                                                   const QString& lastUsiMove)
{
    qCDebug(lcGame).noquote() << "updateConsiderationPosition called:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_considerationWaiting=" << m_considerationWaiting
                       << "m_considerationRestartPending=" << m_considerationRestartPending
                       << "m_usi1=" << (m_usi1 ? "valid" : "null")
                       << "previousFileTo=" << previousFileTo
                       << "previousRankTo=" << previousRankTo
                       << "lastUsiMove=" << lastUsiMove;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "updateConsiderationPosition: not in consideration mode, ignoring";
        return false;
    }

    // 同じポジションなら何もしない
    if (m_considerationPositionStr == newPositionStr) {
        qCDebug(lcGame).noquote() << "updateConsiderationPosition: same position, ignoring";
        return false;
    }

    // 新しいポジションと前回の移動先を保存
    m_considerationPositionStr = newPositionStr;
    m_considerationPreviousFileTo = previousFileTo;
    m_considerationPreviousRankTo = previousRankTo;
    m_considerationLastUsiMove = lastUsiMove;

    // 検討タブ用モデルをクリア
    if (m_considerationModelPtr) {
        m_considerationModelPtr->clearAllItems();
    }

    // エンジンが待機状態の場合、直接新しい局面で検討を再開（stop不要）
    if (m_considerationWaiting && m_usi1) {
        qCDebug(lcGame).noquote() << "updateConsiderationPosition: engine is waiting, resuming with new position";
        m_considerationWaiting = false;  // 待機状態を解除

        // 前回の移動先を設定（「同」表記のため）
        if (previousFileTo > 0 && previousRankTo > 0) {
            m_usi1->setPreviousFileTo(previousFileTo);
            m_usi1->setPreviousRankTo(previousRankTo);
        }

        // 最後の指し手を設定（読み筋表示ウィンドウのハイライト用）
        if (!lastUsiMove.isEmpty()) {
            m_usi1->setLastUsiMove(lastUsiMove);
        }

        // 既存エンジンに直接コマンドを送信（非ブロッキング）
        m_usi1->sendAnalysisCommands(newPositionStr, m_considerationByoyomiMs, m_considerationMultiPV);
        return true;
    }

    // エンジンが稼働中の場合、停止して再開フラグを設定
    m_considerationRestartPending = true;
    qCDebug(lcGame).noquote() << "updateConsiderationPosition: set m_considerationRestartPending=true";

    // エンジンを停止（bestmove を受信後に再開する）
    if (m_usi1) {
        qCDebug(lcGame).noquote() << "updateConsiderationPosition: sending stop to restart with new position";
        m_usi1->sendStopCommand();
    }

    return true;
}

void MatchCoordinator::onCheckmateSolved(const QStringList& pv)
{
    m_inTsumeSearchMode = false;  // 完了したのでフラグをリセット
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        const QString msg = tr("詰みあり（手順 %1 手）").arg(pv.size());
        m_hooks.showGameOverDialog(tr("詰み探索"), msg);
    }
    destroyEngines(false);  // 探索完了後にエンジンを終了（思考内容を保持）
}

void MatchCoordinator::onCheckmateNoMate()
{
    m_inTsumeSearchMode = false;  // 完了したのでフラグをリセット
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("詰みなし"));
    }
    destroyEngines(false);  // 探索完了後にエンジンを終了（思考内容を保持）
}

void MatchCoordinator::onCheckmateNotImplemented()
{
    m_inTsumeSearchMode = false;  // 完了したのでフラグをリセット
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("（エンジン側）未実装"));
    }
    destroyEngines(false);  // 探索完了後にエンジンを終了（思考内容を保持）
}

void MatchCoordinator::onCheckmateUnknown()
{
    m_inTsumeSearchMode = false;  // 完了したのでフラグをリセット
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("不明（解析不能）"));
    }
    destroyEngines(false);  // 探索完了後にエンジンを終了（思考内容を保持）
}

void MatchCoordinator::onTsumeBestMoveReceived()
{
    // 詰み探索モード中でない場合は無視
    if (!m_inTsumeSearchMode) return;

    m_inTsumeSearchMode = false;  // 完了したのでフラグをリセット
    emit tsumeSearchModeEnded();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("詰み探索"), tr("探索が完了しました"));
    }
    destroyEngines(false);  // 探索完了後にエンジンを終了（思考内容を保持）
}

void MatchCoordinator::onConsiderationBestMoveReceived()
{
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived ENTER:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_considerationRestartPending=" << m_considerationRestartPending
                       << "m_considerationWaiting=" << m_considerationWaiting;

    // 検討モード中でない場合は無視
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: not in consideration mode, ignoring";
        return;
    }

    // 再開フラグがセットされている場合は、新しい設定で検討を再開
    // 注意: executeAnalysisCommunication はブロッキング処理なので、
    // シグナルハンドラ内から直接呼び出すとGUIがフリーズする。
    // QTimer::singleShot で次のイベントループに遅延させる。
    if (m_considerationRestartPending) {
        m_considerationRestartPending = false;
        qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: scheduling restart (restart was pending)";

        // 再開処理を次のイベントループに遅延
        QTimer::singleShot(0, this, &MatchCoordinator::restartConsiderationDeferred);
        return;
    }

    // 検討時間が経過した場合、エンジンを待機状態にして次の局面選択を待つ
    // エンジンを終了せず、検討モードも維持する
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived: entering waiting state (engine idle)";
    m_considerationWaiting = true;  // 待機状態に移行
    m_considerationRestartInProgress = false;  // 再入防止フラグをリセット
    // 検討モードは維持（m_inConsiderationMode = true のまま）
    // エンジンは終了しない（destroyEngines を呼ばない）
    // considerationModeEnded も発火しない（ボタンは「検討中止」のまま）
    emit considerationWaitingStarted();  // UIに待機開始を通知（経過タイマー停止用）
    qCDebug(lcGame).noquote() << "onConsiderationBestMoveReceived EXIT (waiting state)";
}

void MatchCoordinator::restartConsiderationDeferred()
{
    qCDebug(lcGame).noquote() << "restartConsiderationDeferred ENTER:"
                       << "m_inConsiderationMode=" << m_inConsiderationMode
                       << "m_considerationRestartPending=" << m_considerationRestartPending
                       << "m_considerationRestartInProgress=" << m_considerationRestartInProgress
                       << "m_usi1=" << (m_usi1 ? "valid" : "null");

    // 検討モード中でなければ何もしない
    if (!m_inConsiderationMode) {
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: not in consideration mode, ignoring";
        return;
    }

    // エンジンがなければ何もしない
    if (!m_usi1) {
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: no engine, ignoring";
        return;
    }

    // 既存エンジンに直接コマンドを送信（非ブロッキング）
    // エンジンは bestmove 送信後アイドル状態なので、そのまま新しいコマンドを送れる
    qCDebug(lcGame).noquote() << "restartConsiderationDeferred: sending commands to existing engine"
                       << "position=" << m_considerationPositionStr
                       << "byoyomiMs=" << m_considerationByoyomiMs
                       << "multiPV=" << m_considerationMultiPV;

    // モデルをクリア
    if (m_considerationModelPtr) {
        m_considerationModelPtr->clearAllItems();
    }

    // 待機状態を解除
    m_considerationWaiting = false;

    // 前回の移動先を設定（「同」表記のため）
    if (m_considerationPreviousFileTo > 0 && m_considerationPreviousRankTo > 0) {
        m_usi1->setPreviousFileTo(m_considerationPreviousFileTo);
        m_usi1->setPreviousRankTo(m_considerationPreviousRankTo);
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: setPreviousFileTo/RankTo:"
                           << m_considerationPreviousFileTo << "/" << m_considerationPreviousRankTo;
    }

    // 最後の指し手を設定（読み筋表示ウィンドウのハイライト用）
    if (!m_considerationLastUsiMove.isEmpty()) {
        m_usi1->setLastUsiMove(m_considerationLastUsiMove);
        qCDebug(lcGame).noquote() << "restartConsiderationDeferred: setLastUsiMove:"
                           << m_considerationLastUsiMove;
    }

    // 既存エンジンにコマンドを送信
    m_usi1->sendAnalysisCommands(m_considerationPositionStr, m_considerationByoyomiMs, m_considerationMultiPV);

    qCDebug(lcGame).noquote() << "restartConsiderationDeferred EXIT";
}

void MatchCoordinator::onUsiError(const QString& msg)
{
    // ログへ（あれば）
    if (m_hooks.log) m_hooks.log(QStringLiteral("[USI-ERROR] ") + msg);
    // 実行中の USI オペを明示的に打ち切る
    if (m_usi1) m_usi1->cancelCurrentOperation();
    if (m_usi2) m_usi2->cancelCurrentOperation();
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

// ---------------------------------------------
// 初手がエンジン手番なら 1手だけ起動
// ---------------------------------------------
void MatchCoordinator::startInitialEngineMoveIfNeeded()
{
    if (!m_gc) return;

    const bool engineIsP1 = (m_playMode == PlayMode::EvenEngineVsHuman) || (m_playMode == PlayMode::HandicapEngineVsHuman);
    const bool engineIsP2 = (m_playMode == PlayMode::EvenHumanVsEngine) || (m_playMode == PlayMode::HandicapHumanVsEngine);

    const auto sideToMove = m_gc->currentPlayer();

    if (engineIsP1 && sideToMove == ShogiGameController::Player1) {
        startInitialEngineMoveFor(P1);
    } else if (engineIsP2 && sideToMove == ShogiGameController::Player2) {
        startInitialEngineMoveFor(P2);
    }
}

// （内部）指定したエンジン側で 1手だけ指す
void MatchCoordinator::startInitialEngineMoveFor(Player engineSide)
{
    Usi* eng = primaryEngine();
    if (!eng || !m_gc) return;

    auto extractMoveNumber = [](const QString& sfen) -> int {
        const QStringList tok = sfen.split(' ', Qt::SkipEmptyParts);
        if (tok.size() >= 5) return tok.last().toInt();
        return -1;
    };

    if (m_positionStr1.isEmpty()) {
        initPositionStringsFromSfen(QString()); // startpos moves
    }
    if (!m_positionStr1.startsWith(QLatin1String("position "))) {
        m_positionStr1 = QStringLiteral("position startpos moves");
    }

    const int mcCur = m_currentMoveIndex;
    const qsizetype recSizeBefore = m_sfenRecord ? m_sfenRecord->size() : -1;
    const QString recTailBefore = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    qCDebug(lcGame).noquote() << "HvE:init enter  mcCur=" << mcCur
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
    int nextIdx = mcCur + 1;
    const bool ok = m_gc->validateAndMove(
        eFrom, eTo, rec, m_playMode,
        nextIdx, m_sfenRecord, gameMovesRef());

    const QString recTailAfter = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qCDebug(lcGame).noquote() << "HvE:init v&m=" << ok
                             << " nextIdx=" << nextIdx
                             << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (!ok) return;

    // エンジン初手の手数インデックスを更新（同期漏れ防止）
    m_currentMoveIndex = nextIdx;

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
    updateTurnDisplay(m_cur);

    armHumanTimerIfNeeded();

    qCDebug(lcGame) << "about to call appendEval, engineSide=" << (engineSide == P1 ? "P1" : "P2");
    if (engineSide == P1) {
        qCDebug(lcGame) << "calling appendEvalP1, hook set=" << (m_hooks.appendEvalP1 ? "YES" : "NO");
        if (m_hooks.appendEvalP1) m_hooks.appendEvalP1();
    } else {
        qCDebug(lcGame) << "calling appendEvalP2, hook set=" << (m_hooks.appendEvalP2 ? "YES" : "NO");
        if (m_hooks.appendEvalP2) m_hooks.appendEvalP2();
    }

    // 千日手チェック
    if (checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (m_maxMoves > 0 && nextIdx >= m_maxMoves) {
        handleMaxMovesJishogi();
        return;
    }
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

    // sfenRecordと手数インデックスを同期
    int mcCur = m_currentMoveIndex;
    if (m_sfenRecord) {
        const int fromRec = static_cast<int>(qMax(qsizetype(0), m_sfenRecord->size() - 1));
        if (fromRec != mcCur) {
            qCDebug(lcGame) << "HvE sync mcCur" << mcCur << "->" << fromRec
                            << "(by recSize=" << m_sfenRecord->size() << ")";
            mcCur = fromRec;
            m_currentMoveIndex = fromRec;
        }
    }

    const qsizetype recSizeBefore = m_sfenRecord ? m_sfenRecord->size() : -1;
    const QString recTailBefore = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    qCDebug(lcGame).noquote() << "HvE enter  mcCur=" << mcCur
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
        (m_playMode == PlayMode::EvenEngineVsHuman) || (m_playMode == PlayMode::HandicapEngineVsHuman);
    const ShogiGameController::Player engineSeat =
        engineIsP1 ? ShogiGameController::Player1 : ShogiGameController::Player2;

    // 千日手チェック（人間の手の後）
    if (checkAndHandleSennichite()) return;

    const bool engineTurnNow = (m_gc && (m_gc->currentPlayer() == engineSeat));
    qCDebug(lcGame).noquote() << "HvE engineTurnNow=" << engineTurnNow
                             << " engineSeat=" << int(engineSeat);
    if (!engineTurnNow) { if (!gameOverState().isOver) armHumanTimerIfNeeded(); return; }

    Usi* eng = primaryEngine();
    if (!eng || !m_gc || !m_sfenRecord) { if (!gameOverState().isOver) armHumanTimerIfNeeded(); return; }

    if (m_positionStr1.isEmpty()) { initPositionStringsFromSfen(QString()); }
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
    int nextIdx = mcCur + 1;
    const bool ok = m_gc->validateAndMove(
        eFrom, eTo, rec, m_playMode,
        nextIdx, m_sfenRecord, gameMovesRef());

    const QString recTailAfter = (m_sfenRecord && !m_sfenRecord->isEmpty()) ? m_sfenRecord->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qCDebug(lcGame).noquote() << "HvE v&m=" << ok
                             << " argMove(nextIdx)=" << nextIdx
                             << " mcCur(before sync calc)=" << mcCur
                             << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (ok) {
        m_currentMoveIndex = nextIdx;
        qCDebug(lcGame).noquote() << "HvE mcCur ->" << m_currentMoveIndex;
    } else {
        if (!gameOverState().isOver) armHumanTimerIfNeeded();
        return;
    }

    // 千日手チェック（エンジンの手の後）
    if (checkAndHandleSennichite()) return;

    if (m_hooks.showMoveHighlights) m_hooks.showMoveHighlights(eFrom, eTo);

    // エンジンの考慮時間を確定してから棋譜に追記する
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
    updateTurnDisplay(m_cur);

    // エンジン着手後の評価値追加
    // engineIsP1: エンジンがP1（先手）の場合 → appendEvalP1
    //             エンジンがP2（後手）の場合 → appendEvalP2
    qCDebug(lcGame) << "onHumanMove: about to call appendEval, engineIsP1=" << engineIsP1;
    if (engineIsP1) {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP1, hook set=" << (m_hooks.appendEvalP1 ? "YES" : "NO");
        if (m_hooks.appendEvalP1) m_hooks.appendEvalP1();
    } else {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP2, hook set=" << (m_hooks.appendEvalP2 ? "YES" : "NO");
        if (m_hooks.appendEvalP2) m_hooks.appendEvalP2();
    }

    // 最大手数チェック
    if (m_maxMoves > 0 && m_currentMoveIndex >= m_maxMoves) {
        handleMaxMovesJishogi();
        return;
    }

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
            (m_playMode == PlayMode::EvenHumanVsEngine) || (m_playMode == PlayMode::HandicapHumanVsEngine);

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
        curSfenIdx = static_cast<int>(srec->size() - 1); // 末尾が現局面
    } else if (u_.currentMoveIndex) {
        curSfenIdx = *u_.currentMoveIndex + 1; // 行0始まり → SFENは開始局面含むので+1
    } else if (u_.gameMoves) {
        curSfenIdx = static_cast<int>(u_.gameMoves->size() + 1);
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
    if (u_.recordModel)              tryRemoveLastItems(u_.recordModel, 2);
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

    // 巻き戻し後の現在ベースを厳密に再構成（prevを先頭remainHands手にトリム）
    const QString startSfen0 = (srec && !srec->isEmpty()) ? srec->first() : QString();
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
    const auto stm = u_.gc ? u_.gc->currentPlayer() : ShogiGameController::NoPlayer;
    const bool humanNow = h_.isHumanSide ? h_.isHumanSide(stm) : false;
    if (u_.view) u_.view->setMouseClickMode(humanNow);

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
// 対局開始オプション構築
// ============================================================

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

        // 最大手数を取得
        opt.maxMoves = dlg->maxMoves();

        // 棋譜自動保存設定を取得
        opt.autoSaveKifu = dlg->isAutoSaveKifu();
        opt.kifuSaveDir = dlg->kifuSaveDir();

        // 対局者名を取得
        opt.humanName1 = dlg->humanName1();
        opt.humanName2 = dlg->humanName2();

        return opt;
    }

    // --- 対局ダイアログなし：INI から直近選択を復元（StartGameDialog と同じ仕様）
    {
        using namespace EngineSettingsConstants;

        QSettings settings(SettingsService::settingsFilePath(), QSettings::IniFormat);

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

// ============================================================
// 盤面反転・対局準備
// ============================================================

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1)
{
    if (!dlg) return;

    // 「人を手前に表示する」がチェックされていない場合は何もしない
    if (!dlg->isShowHumanInFront()) {
        return;
    }

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

// ============================================================
// 対局開始メインエントリ
// ============================================================

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

void MatchCoordinator::handleGameEnded()
{
    if (!m_gc) return;
    m_gc->finalizeGameResult();
    emit uiNotifyGameEnded();
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

    // 表記（KIF形式に準拠、▲/△は絶対座席）
    QString line;
    if (cause == Cause::Jishogi) {
        // 持将棋（入玉宣言で引き分け、または最大手数到達）
        // loserには宣言者が入っている（勝敗はないがマーク表示用）
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = QStringLiteral("%1持将棋").arg(mark);
    } else if (cause == Cause::NyugyokuWin) {
        // 入玉宣言で勝ち（loserは敗者＝宣言者の相手、勝者のマークを使用）
        const QString mark = (loser == P1) ? QStringLiteral("△") : QStringLiteral("▲");
        line = QStringLiteral("%1入玉勝ち").arg(mark);
    } else if (cause == Cause::IllegalMove) {
        // 反則負け（入玉宣言失敗など、敗者のマークを使用）
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = QStringLiteral("%1反則負け").arg(mark);
    } else if (cause == Cause::Sennichite) {
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = QStringLiteral("%1千日手").arg(mark);
    } else if (cause == Cause::OuteSennichite) {
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = QStringLiteral("%1連続王手の千日手").arg(mark);
    } else if (cause == Cause::BreakOff) {
        // 中断（loserには現在手番が入っている）
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = QStringLiteral("%1中断").arg(mark);
    } else {
        const QString mark = (loser == P1) ? QStringLiteral("▲") : QStringLiteral("△");
        line = (cause == Cause::Resignation)
                   ? QStringLiteral("%1投了").arg(mark)
                   : QStringLiteral("%1時間切れ").arg(mark);
    }

    // 「この手」の思考時間を確定（KIF 表示のため）
    // エポックが設定されている場合はそれを使用、未設定の場合はShogiClock内部の考慮時間を使用
    const qint64 epochMs = turnEpochFor(loser);
    qint64 considerMs = 0;
    
    if (epochMs > 0) {
        // エポックが設定されている場合：経過時間を計算
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        considerMs = now - epochMs;
        if (considerMs < 0) considerMs = 0;
    } else {
        // エポックが未設定の場合：ShogiClock内部で累積された考慮時間を使用
        considerMs = (loser == P1) ? m_clock->player1ConsiderationMs()
                                   : m_clock->player2ConsiderationMs();
    }
    
    if (loser == P1) m_clock->setPlayer1ConsiderationTime(int(considerMs));
    else             m_clock->setPlayer2ConsiderationTime(int(considerMs));

    // 秒読み適用と総考慮時間への加算を行い、表示用文字列を取得
    if (loser == P1) {
        m_clock->applyByoyomiAndResetConsideration1();
    } else {
        m_clock->applyByoyomiAndResetConsideration2();
    }

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
    // 千日手チェック（SFENは呼び出し前に追加済み）
    if (checkAndHandleSennichite()) return;

    const Player moverP = (moverBefore == ShogiGameController::Player1) ? P1 : P2;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsiderationFor(moverP);

    // HvH でも秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
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

    // 「この手」の考慮時間を確定（KIF用）
    if (m_clock) {
        // 残り時間を固定
        m_clock->stopClock();
        
        const qint64 epochMs = turnEpochFor(curP);
        qint64 considerMs = 0;
        
        if (epochMs > 0) {
            // エポックが設定されている場合：経過時間を計算
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            considerMs = now - epochMs;
            if (considerMs < 0) considerMs = 0;
        } else {
            // エポックが未設定の場合：ShogiClock内部で累積された考慮時間を使用
            considerMs = (curP == P1) ? m_clock->player1ConsiderationMs()
                                      : m_clock->player2ConsiderationMs();
        }

        if (curP == P1) m_clock->setPlayer1ConsiderationTime(int(considerMs));
        else            m_clock->setPlayer2ConsiderationTime(int(considerMs));
        
        // 秒読み適用と総考慮時間への加算
        if (curP == P1) {
            m_clock->applyByoyomiAndResetConsideration1();
        } else {
            m_clock->applyByoyomiAndResetConsideration2();
        }
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

void MatchCoordinator::handleMaxMovesJishogi()
{
    // すでに終局なら何もしない
    if (m_gameOver.isOver) return;

    qCInfo(lcGame) << "handleMaxMovesJishogi(): max moves reached";

    // 進行系タイマを停止
    disarmHumanTimerIfNeeded();

    // エンジンへの終了通知（EvE の場合）
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);
    if (isEvE) {
        if (m_usi1) {
            m_usi1->sendGameOverCommand(QStringLiteral("draw"));
            m_usi1->sendQuitCommand();
            m_usi1->setSquelchResignLogging(true);
        }
        if (m_usi2) {
            m_usi2->sendGameOverCommand(QStringLiteral("draw"));
            m_usi2->sendQuitCommand();
            m_usi2->setSquelchResignLogging(true);
        }
    } else {
        // HvE の場合は主エンジンへ通知
        if (Usi* eng = primaryEngine()) {
            eng->sendGameOverCommand(QStringLiteral("draw"));
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    // GameEndInfo を構築（持将棋は loser を便宜上 P1 とする）
    GameEndInfo info;
    info.cause = Cause::Jishogi;
    info.loser = P1;  // 持将棋は勝敗なし

    // 終局状態を確定
    m_gameOver.isOver        = true;
    m_gameOver.when          = QDateTime::currentDateTime();
    m_gameOver.hasLast       = true;
    m_gameOver.lastInfo      = info;
    m_gameOver.lastLoserIsP1 = true;

    // 棋譜欄に「持将棋」を追記
    appendGameOverLineAndMark(Cause::Jishogi, P1);

    // 結果ダイアログの表示
    displayResultsAndUpdateGui(info);
}

bool MatchCoordinator::checkAndHandleSennichite()
{
    if (m_gameOver.isOver) return false;

    // EvEの場合はEvE用のSFEN履歴を使用
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);
    const QStringList* rec = isEvE ? sfenRecordForEvE() : m_sfenRecord;
    if (!rec || rec->size() < 4) return false;

    const auto result = SennichiteDetector::check(*rec);
    switch (result) {
    case SennichiteDetector::Result::None:
        return false;
    case SennichiteDetector::Result::Draw:
        handleSennichite();
        return true;
    case SennichiteDetector::Result::ContinuousCheckByP1:
        handleOuteSennichite(true);   // P1（先手）の反則負け
        return true;
    case SennichiteDetector::Result::ContinuousCheckByP2:
        handleOuteSennichite(false);  // P2（後手）の反則負け
        return true;
    }
    return false;
}

void MatchCoordinator::handleSennichite()
{
    if (m_gameOver.isOver) return;

    qCInfo(lcGame) << "handleSennichite(): draw by repetition";

    disarmHumanTimerIfNeeded();

    // エンジンへの終了通知
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);
    if (isEvE) {
        if (m_usi1) {
            m_usi1->sendGameOverCommand(QStringLiteral("draw"));
            m_usi1->sendQuitCommand();
            m_usi1->setSquelchResignLogging(true);
        }
        if (m_usi2) {
            m_usi2->sendGameOverCommand(QStringLiteral("draw"));
            m_usi2->sendQuitCommand();
            m_usi2->setSquelchResignLogging(true);
        }
    } else {
        if (Usi* eng = primaryEngine()) {
            eng->sendGameOverCommand(QStringLiteral("draw"));
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    // GameEndInfo を構築（千日手は引き分け、loser を便宜上 P1 とする）
    GameEndInfo info;
    info.cause = Cause::Sennichite;
    info.loser = P1;

    m_gameOver.isOver        = true;
    m_gameOver.when          = QDateTime::currentDateTime();
    m_gameOver.hasLast       = true;
    m_gameOver.lastInfo      = info;
    m_gameOver.lastLoserIsP1 = true;

    appendGameOverLineAndMark(Cause::Sennichite, P1);
    displayResultsAndUpdateGui(info);
}

void MatchCoordinator::handleOuteSennichite(bool p1Loses)
{
    if (m_gameOver.isOver) return;

    const Player loser  = p1Loses ? P1 : P2;
    const Player winner = p1Loses ? P2 : P1;

    qCInfo(lcGame) << "handleOuteSennichite(): continuous check by"
                   << (p1Loses ? "P1(sente)" : "P2(gote)");

    disarmHumanTimerIfNeeded();

    // エンジンへの終了通知（勝敗が確定）
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);
    if (isEvE) {
        if (m_usi1) {
            const QString result = (winner == P1) ? QStringLiteral("win") : QStringLiteral("lose");
            m_usi1->sendGameOverCommand(result);
            m_usi1->sendQuitCommand();
            m_usi1->setSquelchResignLogging(true);
        }
        if (m_usi2) {
            const QString result = (winner == P2) ? QStringLiteral("win") : QStringLiteral("lose");
            m_usi2->sendGameOverCommand(result);
            m_usi2->sendQuitCommand();
            m_usi2->setSquelchResignLogging(true);
        }
    } else {
        if (Usi* eng = primaryEngine()) {
            // HvE: エンジンが勝者側なら win、負者側なら lose
            const bool engineIsP1 =
                (m_playMode == PlayMode::EvenEngineVsHuman) || (m_playMode == PlayMode::HandicapEngineVsHuman);
            const Player engineSide = engineIsP1 ? P1 : P2;
            const QString result = (winner == engineSide) ? QStringLiteral("win") : QStringLiteral("lose");
            eng->sendGameOverCommand(result);
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    GameEndInfo info;
    info.cause = Cause::OuteSennichite;
    info.loser = loser;

    m_gameOver.isOver        = true;
    m_gameOver.when          = QDateTime::currentDateTime();
    m_gameOver.hasLast       = true;
    m_gameOver.lastInfo      = info;
    m_gameOver.lastLoserIsP1 = p1Loses;

    appendGameOverLineAndMark(Cause::OuteSennichite, loser);
    displayResultsAndUpdateGui(info);
}

ShogiClock* MatchCoordinator::clock()
{
    return m_clock;
}

const ShogiClock* MatchCoordinator::clock() const
{
    return m_clock;
}
