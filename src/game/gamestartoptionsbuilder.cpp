/// @file gamestartoptionsbuilder.cpp
/// @brief 対局開始ダイアログの値の解釈（時間・対局者）と開始局面プリセットの実装

#include "gamestartoptionsbuilder.h"

#include <QCoreApplication>

#include "logcategories.h"
#include "sfenutils.h"

// ============================================================
// 開始の計画
// ============================================================

std::optional<GameStartPlan> GameStartOptionsBuilder::plan(const GameStartRequest& request, QString* error)
{
    const StartGameDialogData& dlg = request.dialogData;
    const bool fromCurrent = dlg.startsFromCurrentPosition();
    RecordLine line = fromCurrent ? request.recordLine : RecordLine{};
    if (line.startSfen.isEmpty()) {
        line.startSfen = canonicalizeSfen(fromCurrent ? QString() : QString::fromLatin1(dlg.startingHandicap->sfen));
    } else {
        line.startSfen = canonicalizeSfen(line.startSfen);
    }
    line = line.upTo(qBound(0, line.currentPly, line.moveCount()));
    const QString sfen = line.lastSfen();
    if (!validateKings(sfen, error)) return std::nullopt;

    GameStartPlan result;
    result.options = buildStartOptions(sfen, dlg);
    result.options.recordLine = line;
    result.time = buildTimeSettings(dlg);
    result.preparation.mode = fromCurrent ? GameStartPreparation::Mode::CurrentPosition
                                          : GameStartPreparation::Mode::NewGame;
    result.preparation.rootSfen = line.startSfen;
    result.preparation.startNodeId = fromCurrent ? request.startNodeId : -1;
    result.flipBoard = needsHumanAtBottomFlip(dlg, request.bottomSide);
    result.consecutiveGames = dlg.consecutiveGames;
    result.switchTurnEachGame = dlg.isSwitchTurnEachGame;
    return result;
}

bool GameStartOptionsBuilder::validateKings(const QString& sfen, QString* whyNot)
{
    // 駒箱に入っている王・玉は通常対局の盤上の玉として数えない。
    // 局面編集や詰将棋用の片玉局面は許可し、通常対局の開始時だけ検証する。
    const QString board = canonicalizeSfen(sfen).section(QLatin1Char(' '), 0, 0);
    const auto blackKings = board.count(QLatin1Char('K'));
    const auto whiteKings = board.count(QLatin1Char('k'));
    if (blackKings == 1 && whiteKings == 1) return true;
    if (whyNot) {
        *whyNot = QCoreApplication::translate("GameStartOptionsBuilder",
                                              "対局を開始できません。\n"
                                              "通常の対局には、先手と後手の王・玉を盤上に1枚ずつ配置してください。\n"
                                              "現在の枚数：先手 %1枚、後手 %2枚\n"
                                              "「編集」→「局面編集開始」で配置を修正してください。")
                      .arg(blackKings).arg(whiteKings);
    }
    return false;
}

// ============================================================
// TimeControl 構築
// ============================================================

MatchTimeSettings GameStartOptionsBuilder::buildTimeSettings(const StartGameDialogData& data)
{
    TimeControl rules;
    rules.black = data.players[0].time;
    rules.white = data.players[1].time;

    MatchTimeSettings tc;
    tc.loseOnTimeout = data.isLoseOnTimeout;
    tc.rules = rules.normalized();

    qCDebug(lcGame).noquote()
        << "buildTimeSettings:"
        << " limited=" << !tc.rules.isUnlimited()

        << " P1{base=" << tc.rules.black.mainMs << " byo=" << tc.rules.black.byoyomiMs << " inc=" << tc.rules.black.incrementMs << "}"
        << " P2{base=" << tc.rules.white.mainMs << " byo=" << tc.rules.white.byoyomiMs << " inc=" << tc.rules.white.incrementMs << "}";

    return tc;
}

// ============================================================
// 対局者
// ============================================================

MatchPlayers GameStartOptionsBuilder::playersFromDialog(const StartGameDialogData& data)
{
    const PlayerChoice& black = data.players[0];
    const PlayerChoice& white = data.players[1];
    MatchPlayers players;
    players.blackIsEngine = black.isEngine();
    players.whiteIsEngine = white.isEngine();
    players.blackHumanName = black.humanName;
    players.whiteHumanName = white.humanName;
    if (black.isEngine()) players.blackEngineName = black.engine.name;
    if (white.isEngine()) players.whiteEngineName = white.engine.name;
    return players;
}

// ============================================================
// SFEN 正規化
// ============================================================

QString GameStartOptionsBuilder::canonicalizeSfen(const QString& sfen)
{
    QString t = sfen.trimmed();
    if (t.isEmpty() || t == QLatin1String("startpos")) {
        return SfenUtils::hirateSfen();
    }
    return t;
}

// ============================================================
// 開始の設定
// ============================================================

MatchStartOptions GameStartOptionsBuilder::buildStartOptions(const QString& startSfen, const StartGameDialogData& dlg)
{
    MatchStartOptions opt;
    opt.players = playersFromDialog(dlg);
    opt.sfenStart = startSfen;
    if (dlg.players[0].isEngine()) opt.enginePath1 = dlg.players[0].engine.path;
    if (dlg.players[1].isEngine()) opt.enginePath2 = dlg.players[1].engine.path;
    // 定跡を使えるのはエンジンの対局者だけ（定跡そのものは対局を始める側が写して渡す）
    for (std::size_t i = 0; i < dlg.players.size(); ++i) {
        if (dlg.players[i].isEngine()) opt.josekiOptions[i] = dlg.players[i].joseki.normalized();
    }
    opt.maxMoves = dlg.maxMoves;
    opt.autoSave.enabled = dlg.autoSaveKifu;
    opt.autoSave.dir = dlg.kifuSaveDir;
    opt.autoSave.extension = QStringLiteral("kifu");   // ローカル対局は UTF-8 の KIF
    return opt;
}

bool GameStartOptionsBuilder::needsHumanAtBottomFlip(const StartGameDialogData& dlg, Turn bottomSide)
{
    // 人間とエンジンの対局だけが対象（人間同士・エンジン同士はそのまま）
    if (!dlg.isShowHumanInFront || dlg.players[0].isHuman() == dlg.players[1].isHuman()) return false;
    const Turn humanSide = dlg.players[0].isHuman() ? Turn::Black : Turn::White;
    return humanSide != bottomSide;
}
