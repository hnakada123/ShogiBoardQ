/// @file nyugyokudeclarationhandler.cpp
/// @brief 入玉宣言ハンドラクラスの実装

#include "nyugyokudeclarationhandler.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "matchcoordinator.h"
#include "jishogicalculator.h"
#include "fastmovevalidator.h"
#include "playmode.h"
#include "settingsservice.h"

#include <QSettings>
#include <QMessageBox>

NyugyokuDeclarationHandler::NyugyokuDeclarationHandler(QObject* parent)
    : QObject(parent)
{
}

void NyugyokuDeclarationHandler::setGameController(ShogiGameController* gc)
{
    m_gameController = gc;
}

void NyugyokuDeclarationHandler::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

QString NyugyokuDeclarationHandler::buildConditionDetails(
    bool kingInEnemyTerritory, int piecesInEnemyTerritory,
    bool noCheck, int declarationPoints, int jishogiRule, bool isSente) const
{
    const QString checkMark = tr("○");
    const QString crossMark = tr("×");

    QString details = tr("【宣言条件の判定】\n"
                          "① 玉が敵陣にいる: %1\n"
                          "② 敵陣に10枚以上: %2 (%3枚)\n"
                          "③ 王手がかかっていない: %4\n"
                          "④ 宣言点数: %5点\n")
        .arg(kingInEnemyTerritory ? checkMark : crossMark,
             piecesInEnemyTerritory >= 10 ? checkMark : crossMark)
        .arg(piecesInEnemyTerritory)
        .arg(noCheck ? checkMark : crossMark,
             QString::number(declarationPoints));

    if (jishogiRule == 1) {
        details += tr("\n【24点法】\n");
    } else {
        int requiredPoints = isSente ? 28 : 27;
        details += tr("\n【27点法】\n");
        details += tr("必要点数: %1点以上\n").arg(requiredPoints);
    }

    return details;
}

NyugyokuDeclarationHandler::DeclarationResult
NyugyokuDeclarationHandler::judge24PointRule(
    bool kingInEnemyTerritory, bool enoughPieces,
    bool noCheck, int declarationPoints) const
{
    DeclarationResult result;

    if (kingInEnemyTerritory && enoughPieces && noCheck) {
        if (declarationPoints >= 31) {
            result.success = true;
            result.isDraw = false;
            result.resultText = tr("宣言勝ち");
            result.conditionDetails = tr("31点以上: 勝ち");
        } else if (declarationPoints >= 24) {
            result.success = true;
            result.isDraw = true;
            result.resultText = tr("持将棋（引き分け）");
            result.conditionDetails = tr("24〜30点: 引き分け");
        } else {
            result.success = false;
            result.resultText = tr("宣言失敗（負け）");
            result.conditionDetails = tr("24点未満: 宣言失敗");
        }
    } else {
        result.success = false;
        result.resultText = tr("宣言失敗（負け）");
        result.conditionDetails = tr("条件未達: 宣言失敗");
    }

    return result;
}

NyugyokuDeclarationHandler::DeclarationResult
NyugyokuDeclarationHandler::judge27PointRule(
    bool kingInEnemyTerritory, bool enoughPieces,
    bool noCheck, int declarationPoints, bool isSente) const
{
    DeclarationResult result;
    int requiredPoints = isSente ? 28 : 27;

    if (kingInEnemyTerritory && enoughPieces && noCheck && declarationPoints >= requiredPoints) {
        result.success = true;
        result.isDraw = false;
        result.resultText = tr("宣言勝ち");
        result.conditionDetails = tr("条件達成: 勝ち");
    } else {
        result.success = false;
        result.resultText = tr("宣言失敗（負け）");
        if (!kingInEnemyTerritory || !enoughPieces || !noCheck) {
            result.conditionDetails = tr("条件未達: 宣言失敗");
        } else {
            result.conditionDetails = tr("点数不足: 宣言失敗");
        }
    }

    return result;
}

bool NyugyokuDeclarationHandler::handleDeclaration(QWidget* parentWidget, ShogiBoard* board, int playMode)
{
    // 対局中かどうかをチェック
    if (playMode == static_cast<int>(PlayMode::NotStarted)) {
        QMessageBox::warning(parentWidget, tr("入玉宣言"), tr("対局中ではありません。"));
        return false;
    }

    // 盤面データの確認
    if (!board) {
        QMessageBox::warning(parentWidget, tr("エラー"), tr("盤面データがありません。"));
        return false;
    }

    // 持将棋ルールの取得（QSettingsから読み込む）
    QSettings settings(SettingsService::settingsFilePath(), QSettings::IniFormat);
    int jishogiRule = settings.value("GameSettings/jishogiRule", 0).toInt();

    if (jishogiRule == 0) {
        QMessageBox::warning(parentWidget, tr("入玉宣言"),
            tr("持将棋ルールが「なし」に設定されています。\n"
               "対局ダイアログで「24点法」または「27点法」を選択してください。"));
        return false;
    }

    // 現在の手番を取得（宣言者）
    bool isSenteTurn = true;
    if (m_gameController) {
        isSenteTurn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    }
    QString declarerName = isSenteTurn ? tr("先手") : tr("後手");

    // 確認ダイアログ
    QMessageBox::StandardButton reply = QMessageBox::question(
        parentWidget,
        tr("入玉宣言確認"),
        tr("%1が入玉宣言を行います。\n\n"
           "宣言条件を満たさない場合は宣言側の負けとなります。\n"
           "本当に宣言しますか？").arg(declarerName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return false;
    }

    // 盤面データと点数を計算
    auto calcResult = JishogiCalculator::calculate(board->boardData(), board->getPieceStand());

    // 王手判定
    FastMoveValidator validator;
    bool declarerInCheck = false;
    const auto& score = isSenteTurn ? calcResult.sente : calcResult.gote;

    if (isSenteTurn) {
        declarerInCheck = validator.checkIfKingInCheck(FastMoveValidator::BLACK, board->boardData()) > 0;
    } else {
        declarerInCheck = validator.checkIfKingInCheck(FastMoveValidator::WHITE, board->boardData()) > 0;
    }

    // 宣言条件の判定
    bool kingInEnemyTerritory = score.kingInEnemyTerritory;
    bool enoughPieces = score.piecesInEnemyTerritory >= 10;
    bool noCheck = !declarerInCheck;
    int declarationPoints = score.declarationPoints;

    // 条件詳細を生成
    QString conditionDetails = buildConditionDetails(
        kingInEnemyTerritory, score.piecesInEnemyTerritory,
        noCheck, declarationPoints, jishogiRule, isSenteTurn);

    // 結果の判定
    DeclarationResult result;
    if (jishogiRule == 1) {
        result = judge24PointRule(kingInEnemyTerritory, enoughPieces, noCheck, declarationPoints);
    } else {
        result = judge27PointRule(kingInEnemyTerritory, enoughPieces, noCheck, declarationPoints, isSenteTurn);
    }

    conditionDetails += result.conditionDetails;

    // 対局終了処理（MatchCoordinatorを使用）- 先に棋譜を更新
    if (m_match) {
        MatchCoordinator::Player declarer = isSenteTurn ? MatchCoordinator::P1 : MatchCoordinator::P2;
        m_match->handleNyugyokuDeclaration(declarer, result.success, result.isDraw);
    }

    // 結果ダイアログの表示
    QString finalMessage = tr("%1の入玉宣言\n\n%2\n\n【結果】%3")
        .arg(declarerName, conditionDetails, result.resultText);

    QMessageBox::information(parentWidget, tr("入玉宣言結果"), finalMessage);

    // シグナル発行
    emit declarationCompleted(result);

    return true;
}
