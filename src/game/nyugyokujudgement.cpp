/// @file nyugyokujudgement.cpp
/// @brief 入玉宣言の判定の実装

#include "nyugyokujudgement.h"

#include "enginemovevalidator.h"
#include "jishogicalculator.h"
#include "settingscommon.h"
#include "shogiboard.h"

#include <QSettings>

int NyugyokuJudgement::configuredRule()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    return settings.value(QStringLiteral("GameSettings/jishogiRule"), RuleNone).toInt();
}

NyugyokuJudgement::Result NyugyokuJudgement::judge(const ShogiBoard& board, bool declarerIsSente, int rule)
{
    const auto points = JishogiCalculator::calculate(board.boardData(), board.pieceStand());
    const auto& score = declarerIsSente ? points.sente : points.gote;
    const EngineMoveValidator validator;
    const bool inCheck = validator.checkIfKingInCheck(
        declarerIsSente ? EngineMoveValidator::BLACK : EngineMoveValidator::WHITE, board.boardData()) > 0;

    const bool kingInEnemyTerritory = score.kingInEnemyTerritory;
    const bool enoughPieces = score.piecesInEnemyTerritory >= 10;
    const bool noCheck = !inCheck;
    const bool conditionsMet = kingInEnemyTerritory && enoughPieces && noCheck;
    const int declarationPoints = score.declarationPoints;

    const QString checkMark = tr("○");
    const QString crossMark = tr("×");
    QString details = tr("【宣言条件の判定】\n"
                         "① 玉が敵陣にいる: %1\n"
                         "② 敵陣に10枚以上: %2 (%3枚)\n"
                         "③ 王手がかかっていない: %4\n"
                         "④ 宣言点数: %5点\n")
        .arg(kingInEnemyTerritory ? checkMark : crossMark, enoughPieces ? checkMark : crossMark)
        .arg(score.piecesInEnemyTerritory)
        .arg(noCheck ? checkMark : crossMark, QString::number(declarationPoints));

    Result result;
    QString verdict;
    if (rule == Rule24) {
        details += tr("\n【24点法】\n");
        if (conditionsMet && declarationPoints >= 31) {
            result.success = true;
            result.resultText = tr("宣言勝ち");
            verdict = tr("31点以上: 勝ち");
        } else if (conditionsMet && declarationPoints >= 24) {
            result.success = true;
            result.isDraw = true;
            result.resultText = tr("持将棋（引き分け）");
            verdict = tr("24〜30点: 引き分け");
        } else {
            result.resultText = tr("宣言失敗（負け）");
            verdict = conditionsMet ? tr("24点未満: 宣言失敗") : tr("条件未達: 宣言失敗");
        }
    } else {
        const int requiredPoints = declarerIsSente ? 28 : 27;
        details += tr("\n【27点法】\n");
        details += tr("必要点数: %1点以上\n").arg(requiredPoints);
        if (conditionsMet && declarationPoints >= requiredPoints) {
            result.success = true;
            result.resultText = tr("宣言勝ち");
            verdict = tr("条件達成: 勝ち");
        } else {
            result.resultText = tr("宣言失敗（負け）");
            verdict = conditionsMet ? tr("点数不足: 宣言失敗") : tr("条件未達: 宣言失敗");
        }
    }

    result.message = tr("%1の入玉宣言\n\n%2\n\n【結果】%3")
        .arg(declarerIsSente ? tr("先手") : tr("後手"), details + verdict, result.resultText);
    return result;
}

NyugyokuJudgement::Result NyugyokuJudgement::judgeEngineDeclaration(const ShogiBoard& board, bool declarerIsSente)
{
    const int configured = configuredRule();
    Result result = judge(board, declarerIsSente, configured == Rule24 ? Rule24 : Rule27);
    if (configured != Rule24 && configured != Rule27) {
        result.message = tr("持将棋ルールが「なし」のため、27点法で判定しました。") + QStringLiteral("\n\n")
                         + result.message;
    }
    return result;
}

QString NyugyokuJudgement::resultTitle()
{
    return tr("入玉宣言結果");
}
