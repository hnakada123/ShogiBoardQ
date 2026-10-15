/// @file gamestartcoordinator_dialogflow.cpp
/// @brief 開始要求を検証し、選択した手順を一度だけ適用する
#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "sfenutils.h"

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
        } else if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
            sfen = *c.currentSfenStr;
        } else if (c.startSfenStr) {
            sfen = *c.startSfenStr;
        }
        line.sfens = {GameStartOptionsBuilder::canonicalizeSfen(sfen)};
    }
    const int ply = qBound(0, line.currentPly, qMin(line.moveCount(), static_cast<int>(line.sfens.size()) - 1));
    line.currentPly = ply;
    line.sfens = line.sfens.mid(0, ply + 1);
    line.moves = line.moves.mid(0, ply);
    const QString sfen = line.sfens.last();
    if (!validateKingsForGame(sfen, errorMessage)) return false;

    const PlayMode mode = determinePlayModeAlignedWithTurn(
        c.dialogData.startingPositionNumber, c.dialogData.isHuman1, c.dialogData.isHuman2, sfen);
    StartParams params;
    params.opt = m_match->buildStartOptions(mode, sfen, nullptr, &c.dialogData);
    params.opt.recordLine = line;
    params.tc = GameStartOptionsBuilder::buildTimeControl(c.dialogData);
    params.announcePlayerNames = true;
    params.selectKifuRow = ply;
    if (!validate(params, errorMessage)) return false;

    // ここまで共有状態に触れない。クリーンアップには開始方法とノードの識別子を渡す。
    GameStartPreparation preparation;
    preparation.mode = fromCurrent ? GameStartPreparation::Mode::CurrentPosition : GameStartPreparation::Mode::NewGame;
    preparation.rootSfen = line.startSfen();
    preparation.startNodeId = fromCurrent ? c.startNodeId : -1;
    emit requestPreStartCleanup(preparation);

    if (c.startSfenStr) *c.startSfenStr = line.startSfen();
    if (c.currentSfenStr) *c.currentSfenStr = sfen;
    if (c.clock) m_match->setClock(c.clock);
    m_match->ensureHumanAtBottomIfApplicable(&c.dialogData, c.bottomSide);
    emit consecutiveGamesConfigured(c.dialogData.consecutiveGames, c.dialogData.isSwitchTurnEachGame);
    start(params);
    return true;
}
