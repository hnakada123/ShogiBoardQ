#ifndef UNDOFLOWSERVICE_H
#define UNDOFLOWSERVICE_H

/// @file undoflowservice.h
/// @brief 「待った」巻き戻し後処理（評価値グラフ・ply同期）を集約するサービスの定義

#include <QStringList>

enum class PlayMode;
class MatchCoordinator;
class EvaluationGraphController;

/**
 * @brief 「待った」実行時の巻き戻し後処理を集約するサービス
 *
 * MainWindow から undoLastTwoMoves の後処理（評価値グラフのプロット削除・
 * カーソル位置同期）を分離し、undo フローを一元管理する。
 */
class UndoFlowService
{
public:
    struct Deps {
        MatchCoordinator* match = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        const PlayMode* playMode = nullptr;
        QStringList* sfenRecord = nullptr;
    };

    void updateDeps(const Deps& deps);

    /**
     * @brief 直近2手を取り消し、評価値グラフを同期する
     *
     * MatchCoordinator::undoTwoPlies() を呼び出し、成功した場合に
     * PlayMode に応じた評価値グラフのプロット削除と ply カーソル同期を行う。
     */
    void undoLastTwoMoves();

private:
    Deps m_deps;
};

#endif // UNDOFLOWSERVICE_H
