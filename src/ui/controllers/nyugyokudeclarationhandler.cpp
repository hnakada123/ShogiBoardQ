/// @file nyugyokudeclarationhandler.cpp
/// @brief 入玉宣言ハンドラクラスの実装

#include "nyugyokudeclarationhandler.h"
#include "dialogutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "matchcoordinator.h"
#include "nyugyokujudgement.h"
#include "playmode.h"

#include <QMessageBox>

namespace {

// 手番の対局者が人間か。人間対エンジンでは人間の側の手番だけ宣言できる
bool isHumanToMove(PlayMode mode, bool senteToMove)
{
    switch (mode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return senteToMove;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return !senteToMove;
    default:
        return false;
    }
}

} // namespace

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

    // 宣言できるのは手番の人間だけ。エンジンの手番で押してもエンジンの代わりには宣言しない
    const bool senteToMove = !m_gameController
        || m_gameController->currentPlayer() == ShogiGameController::Player1;
    if (!isHumanToMove(static_cast<PlayMode>(playMode), senteToMove)) {
        QMessageBox::information(parentWidget, tr("入玉宣言"), tr("入玉宣言は自分の手番で行います。"));
        return false;
    }

    // 持将棋ルール（対局ダイアログの設定）
    const int jishogiRule = NyugyokuJudgement::configuredRule();

    if (jishogiRule != NyugyokuJudgement::Rule24 && jishogiRule != NyugyokuJudgement::Rule27) {
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
    if (!DialogUtils::confirmAction(
            parentWidget,
            tr("入玉宣言確認"),
            tr("%1が入玉宣言を行います。\n\n"
               "宣言条件を満たさない場合は宣言側の負けとなります。\n"
               "本当に宣言しますか？").arg(declarerName),
            tr("宣言する"))) {
        return false;
    }

    // 宣言条件と点数を盤面から判定する（エンジンの宣言と共通）
    const NyugyokuJudgement::Result result = NyugyokuJudgement::judge(*board, isSenteTurn, jishogiRule);

    // 対局終了処理（MatchCoordinatorを使用）- 先に棋譜を更新
    if (m_match) {
        MatchCoordinator::Player declarer = isSenteTurn ? MatchCoordinator::P1 : MatchCoordinator::P2;
        m_match->handleNyugyokuDeclaration(declarer, result.success, result.isDraw);
    }

    // 結果ダイアログの表示
    QMessageBox::information(parentWidget, NyugyokuJudgement::resultTitle(), result.message);

    return true;
}
