/// @file nyugyokujudgement.cpp
/// @brief 入玉宣言の判定の実装

#include "nyugyokujudgement.h"

#include "enginemovevalidator.h"
#include "gamesettings.h"
#include "shogiposition.h"

std::optional<JishogiCalculator::Rule> NyugyokuJudgement::ruleFromSetting(int value)
{
    switch (value) {
    case 1: return JishogiCalculator::Rule::Points24;
    case 2: return JishogiCalculator::Rule::Points27;
    default: return std::nullopt;
    }
}

std::optional<JishogiCalculator::Rule> NyugyokuJudgement::configuredRule()
{
    return ruleFromSetting(GameSettings::StartGame::jishogiRule());
}

QString NyugyokuJudgement::declarerName(bool declarerIsSente, bool handicapNames)
{
    if (handicapNames) return declarerIsSente ? tr("下手") : tr("上手");
    return declarerIsSente ? tr("先手") : tr("後手");
}

NyugyokuJudgement::Result NyugyokuJudgement::judge(const Position& position, bool declarerIsSente,
                                                   JishogiCalculator::Rule rule, bool handicapNames)
{
    using Verdict = JishogiCalculator::Verdict;
    const auto points = JishogiCalculator::calculate(position);
    const auto& score = declarerIsSente ? points.sente : points.gote;
    const bool inCheck = EngineMoveValidator::isInCheck(position, declarerIsSente ? Turn::Black : Turn::White);
    const Verdict verdict = JishogiCalculator::judge(score, rule, declarerIsSente, inCheck);

    const bool kingInEnemyTerritory = score.kingInEnemyTerritory;
    const bool enoughPieces = score.piecesInEnemyTerritory >= 10;
    const bool noCheck = !inCheck;

    const QString checkMark = tr("○");
    const QString crossMark = tr("×");
    QString details = tr("【宣言条件の判定】\n"
                         "① 玉が敵陣にいる: %1\n"
                         "② 敵陣に10枚以上: %2 (%3枚)\n"
                         "③ 王手がかかっていない: %4\n"
                         "④ 宣言点数: %5点\n")
        .arg(kingInEnemyTerritory ? checkMark : crossMark, enoughPieces ? checkMark : crossMark)
        .arg(score.piecesInEnemyTerritory)
        .arg(noCheck ? checkMark : crossMark, QString::number(score.declarationPoints));

    Result result;
    result.success = verdict == Verdict::Win || verdict == Verdict::Draw;
    result.isDraw = verdict == Verdict::Draw;
    result.resultText = verdict == Verdict::Win ? tr("宣言勝ち")
                        : verdict == Verdict::Draw ? tr("持将棋（引き分け）") : tr("宣言失敗（負け）");
    QString reason;
    if (rule == JishogiCalculator::Rule::Points24) {
        details += tr("\n【24点法】\n");
        switch (verdict) {
        case Verdict::Win: reason = tr("31点以上: 勝ち"); break;
        case Verdict::Draw: reason = tr("24〜30点: 引き分け"); break;
        case Verdict::ShortOfPoints: reason = tr("24点未満: 宣言失敗"); break;
        case Verdict::NotDeclarable: reason = tr("条件未達: 宣言失敗"); break;
        }
    } else {
        details += tr("\n【27点法】\n");
        details += tr("必要点数: %1点以上\n").arg(JishogiCalculator::winningPoints(rule, declarerIsSente));
        switch (verdict) {
        case Verdict::Win: reason = tr("条件達成: 勝ち"); break;
        case Verdict::Draw:   // 27点法に持将棋はない
        case Verdict::ShortOfPoints: reason = tr("点数不足: 宣言失敗"); break;
        case Verdict::NotDeclarable: reason = tr("条件未達: 宣言失敗"); break;
        }
    }

    result.message = tr("%1の入玉宣言\n\n%2\n\n【結果】%3")
        .arg(declarerName(declarerIsSente, handicapNames), details + reason, result.resultText);
    return result;
}

NyugyokuJudgement::Result NyugyokuJudgement::judgeEngineDeclaration(const Position& position, bool declarerIsSente,
                                                                    bool handicapNames)
{
    const std::optional<JishogiCalculator::Rule> configured = configuredRule();
    Result result = judge(position, declarerIsSente, configured.value_or(JishogiCalculator::Rule::Points27),
                          handicapNames);
    if (!configured) {
        result.message = tr("持将棋ルールが「なし」のため、27点法で判定しました。") + QStringLiteral("\n\n")
                         + result.message;
    }
    return result;
}

QString NyugyokuJudgement::resultTitle()
{
    return tr("入玉宣言結果");
}
