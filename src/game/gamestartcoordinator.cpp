/// @file gamestartcoordinator.cpp
/// @brief 対局開始コーディネータクラスの実装

#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "logcategories.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "timecontrolutil.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QWidget>
#include <QMetaType>
#include "sfenutils.h"

// ============================================================
// 初期化
// ============================================================

GameStartCoordinator::GameStartCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_match(d.match)
{
    // MainWindow との queued 接続でも安全に受け渡せるように登録
    qRegisterMetaType<GameStartCoordinator::TimeSettings>("GameStartCoordinator::TimeSettings");
    qRegisterMetaType<GameStartPreparation>();
}

bool GameStartCoordinator::validate(const StartParams& p, QString& whyNot) const
{
    if (!m_match) {
        whyNot = QStringLiteral("MatchCoordinator が未設定です。");
        return false;
    }
    return validateKingsForGame(p.opt.sfenStart, whyNot);
}

bool GameStartCoordinator::validateKingsForGame(const QString& sfen, QString& whyNot) const
{
    // 駒箱に入っている王・玉は通常対局の盤上の玉として数えない。
    // 局面編集や詰将棋用の片玉局面は許可し、通常対局の開始時だけ検証する。
    const QString board = GameStartOptionsBuilder::canonicalizeSfen(sfen).section(QLatin1Char(' '), 0, 0);
    const auto blackKings = board.count(QLatin1Char('K'));
    const auto whiteKings = board.count(QLatin1Char('k'));
    if (blackKings == 1 && whiteKings == 1) return true;

    whyNot = tr("対局を開始できません。\n"
                "通常の対局には、先手と後手の王・玉を盤上に1枚ずつ配置してください。\n"
                "現在の枚数：先手 %1枚、後手 %2枚\n"
                "「編集」→「局面編集開始」で配置を修正してください。")
                 .arg(blackKings).arg(whiteKings);
    return false;
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

    // 時計の設定と開始はこの入口で一度だけ行う。UI側には適用済みの設定を通知する。
    TimeSettings tc = params.tc;
    tc.rules = tc.rules.normalized();
    tc.enabled = !tc.rules.isUnlimited();
    TimeControlUtil::applyToClock(m_match->clock(), tc, params.opt.sfenStart, {});
    emit requestApplyTimeControl(tc);

    // ダイアログを経ずに始める対局（連続対局の2局目以降）は、持ち時間と開始日時を記録したここで
    // 対局者名を通知し、盤の名前表示と対局情報を新しい対局に合わせる（ダイアログ経由と同じく開始前に）
    if (params.announcePlayerNames) {
        emit playerNamesResolved(params.opt.players);
    }

    // --- 3) 対局をセットアップ & 開始 ---
    m_match->start(params.opt);
    finishStart(params.opt, params.selectKifuRow, params.autoStartEngineMove);
}

void GameStartCoordinator::finishStart(const MatchCoordinator::StartOptions& opt, int selectKifuRow,
                                       bool startClockAndFirstMove)
{
    // 開始の通知 → 開始行の選択 → 時計 → 初手 の順。対局の開始はすべてここを通る
    emit started(opt);
    if (selectKifuRow >= 0) emit requestSelectKifuRow(selectKifuRow);
    if (!startClockAndFirstMove) return;
    // 時計を動かし、初手がエンジン手番なら go を起動（1回だけ）
    m_match->beginPlay();
}

// ============================================================
// ユーティリティ（GameStartOptionsBuilder へ委譲）
// ============================================================

bool GameStartCoordinator::resumeInterruptedGame()
{
    if (!m_match || !m_match->interruptedGame()) return false;
    const auto opt = m_match->interruptedGame()->options;
    const int ply = m_match->interruptedGame()->record.moveCount();
    if (!m_match->resumeInterruptedGame()) return false;
    finishStart(opt, ply, true);
    return true;
}

// ============================================================
// 開始要求を検証し、選択した手順を一度だけ適用する
// ============================================================

bool GameStartCoordinator::initializeGame(const Ctx& c, QString& errorMessage)
{
    errorMessage.clear();
    if (!m_match) return false;

    const bool fromCurrent = c.dialogData.startingPositionNumber == 0;
    RecordLine line = fromCurrent ? c.recordLine : RecordLine{};
    if (line.isEmpty()) {
        QString sfen;
        if (!fromCurrent) {
            sfen = GameStartOptionsBuilder::startingPositionSfen(c.dialogData.startingPositionNumber);
        } else {
            sfen = c.currentSfen.isEmpty() ? c.startSfen : c.currentSfen;
        }
        line.startSfen = GameStartOptionsBuilder::canonicalizeSfen(sfen);
    }
    const int ply = qBound(0, line.currentPly, line.moveCount());
    line = line.upTo(ply);
    const QString sfen = line.lastSfen();
    if (!validateKingsForGame(sfen, errorMessage)) return false;

    StartParams params;
    params.opt = GameStartOptionsBuilder::buildStartOptions(sfen, c.dialogData);
    params.opt.recordLine = line;
    params.tc = GameStartOptionsBuilder::buildTimeControl(c.dialogData);
    params.announcePlayerNames = true;
    params.selectKifuRow = ply;
    if (!validate(params, errorMessage)) return false;

    // ここまで共有状態に触れない。クリーンアップには開始方法とノードの識別子を渡す。
    GameStartPreparation preparation;
    preparation.mode = fromCurrent ? GameStartPreparation::Mode::CurrentPosition : GameStartPreparation::Mode::NewGame;
    preparation.rootSfen = line.startSfen;
    preparation.startNodeId = fromCurrent ? c.startNodeId : -1;
    emit requestPreStartCleanup(preparation);

    // 「人を手前に表示する」なら、人間の側が手前になるよう盤を反転させる
    if (GameStartOptionsBuilder::needsHumanAtBottomFlip(c.dialogData, c.bottomSide)) emit boardFlipRequested();
    emit consecutiveGamesConfigured(c.dialogData.consecutiveGames, c.dialogData.isSwitchTurnEachGame);
    start(params);
    return true;
}
