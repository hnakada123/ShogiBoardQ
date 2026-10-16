/// @file gamestartoptionsbuilder.cpp
/// @brief 対局開始ダイアログの値の解釈（時間・対局者）と開始局面プリセットの実装

#include "gamestartoptionsbuilder.h"

#include <QCoreApplication>

#include "handicaps.h"
#include "logcategories.h"
#include "sfenutils.h"

// ============================================================
// 開始の計画
// ============================================================

std::optional<GameStartPlan> GameStartOptionsBuilder::plan(const GameStartRequest& request, QString* error)
{
    const StartGameDialogData& dlg = request.dialogData;
    const bool fromCurrent = dlg.startingPositionNumber == 0;
    RecordLine line = fromCurrent ? request.recordLine : RecordLine{};
    if (line.startSfen.isEmpty()) {
        line.startSfen = canonicalizeSfen(fromCurrent ? QString() : startingPositionSfen(dlg.startingPositionNumber));
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
    const auto minutesToMs = [](int hours, int minutes) {
        return qMax<qint64>(0, (static_cast<qint64>(hours) * 3600 + minutes * 60) * 1000);
    };
    const auto secondsToMs = [](int seconds) { return qMax<qint64>(0, static_cast<qint64>(seconds) * 1000); };

    TimeControl rules;
    rules.black.mainMs = minutesToMs(data.basicTimeHour1, data.basicTimeMinutes1);
    rules.white.mainMs = minutesToMs(data.basicTimeHour2, data.basicTimeMinutes2);
    rules.black.byoyomiMs = secondsToMs(data.byoyomiSec1);
    rules.white.byoyomiMs = secondsToMs(data.byoyomiSec2);
    rules.black.incrementMs = secondsToMs(data.addEachMoveSec1);
    rules.white.incrementMs = secondsToMs(data.addEachMoveSec2);

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
    MatchPlayers players;
    // 人間とエンジンの両方に印があるときは人間とする（ダイアログは排他だが、設定の読み違いに備える）
    players.blackIsEngine = data.isEngine1 && !data.isHuman1;
    players.whiteIsEngine = data.isEngine2 && !data.isHuman2;
    players.blackHumanName = data.humanName1;
    players.whiteHumanName = data.humanName2;
    const auto& engines = data.engineList;
    if (data.engineNumber1 >= 0 && data.engineNumber1 < engines.size()) players.blackEngineName = data.engineName1;
    if (data.engineNumber2 >= 0 && data.engineNumber2 < engines.size()) players.whiteEngineName = data.engineName2;
    return players;
}

// ============================================================
// 開始局面プリセット
// ============================================================

QString GameStartOptionsBuilder::startingPositionSfen(int index)
{
    // 対局開始ダイアログの「開始局面」の並び（1=平手, 2=香落ち, …, 14=十枚落ち）
    static constexpr const char16_t* kPositions[kStartingPositionCount] = {
        u"平手", u"香落ち", u"右香落ち", u"角落ち", u"飛車落ち", u"飛香落ち", u"二枚落ち",
        u"三枚落ち", u"四枚落ち", u"五枚落ち", u"左五枚落ち", u"六枚落ち", u"八枚落ち", u"十枚落ち",
    };
    const int idx = qBound(1, index, kStartingPositionCount) - 1;
    return QString::fromLatin1(Handicaps::byLabel(QStringView(kPositions[idx]))->sfen);
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
    const auto& engines = dlg.engineList;
    if (dlg.engineNumber1 >= 0 && dlg.engineNumber1 < engines.size()) opt.enginePath1 = engines.at(dlg.engineNumber1).path;
    if (dlg.engineNumber2 >= 0 && dlg.engineNumber2 < engines.size()) opt.enginePath2 = engines.at(dlg.engineNumber2).path;
    opt.maxMoves = dlg.maxMoves;
    opt.autoSaveKifu = dlg.autoSaveKifu;
    opt.kifuSaveDir = dlg.kifuSaveDir;
    return opt;
}

bool GameStartOptionsBuilder::needsHumanAtBottomFlip(const StartGameDialogData& dlg, Turn bottomSide)
{
    // 人間とエンジンの対局だけが対象（人間同士・エンジン同士はそのまま）
    if (!dlg.isShowHumanInFront || dlg.isHuman1 == dlg.isHuman2) return false;
    const Turn humanSide = dlg.isHuman1 ? Turn::Black : Turn::White;
    return humanSide != bottomSide;
}
