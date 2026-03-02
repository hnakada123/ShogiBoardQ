#ifndef MATCHUNDOHANDLER_H
#define MATCHUNDOHANDLER_H

/// @file matchundohandler.h
/// @brief 対局中の2手UNDO（待った）処理ハンドラの定義

#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

#include "shogimove.h"
#include "shogigamecontroller.h"

class ShogiView;
class KifuRecordListModel;
class BoardInteractionController;

/**
 * @brief 対局中の2手UNDO処理を担当するハンドラ
 *
 * MatchCoordinator から UNDO 関連のロジックを分離する。
 * 盤面の巻き戻し・棋譜/モデル/履歴の同期・USI position 文字列の再構成を行う。
 *
 * MatchCoordinator 内部状態への参照は Refs struct で受け取り、
 * UI通知は Hooks struct のコールバックで実行する。
 */
class MatchUndoHandler
{
public:
    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        QStringList* sfenHistory = nullptr;
        QString* positionStr1 = nullptr;
        QString* positionPonder1 = nullptr;
        QStringList* positionStrHistory = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
    };

    /// UNDO に必要な外部参照（棋譜UI固有）
    struct UndoRefs {
        KifuRecordListModel*        recordModel      = nullptr;
        QStringList*                positionStrList   = nullptr;
        int*                        currentMoveIndex  = nullptr;
        BoardInteractionController* boardCtl          = nullptr;
    };

    /**
     * @brief UNDO時のUIコールバック群
     *
     * 2手戻し操作後に盤面ハイライトと手番表示を更新するために使用。
     *
     * @note MatchCoordinatorHooksFactory::buildUndoHooks() で UndoDeps から生成。
     *       UndoDeps は MainWindowWiringAssembler::buildMatchWiringDeps() で設定。
     * @see MatchCoordinatorHooksFactory::buildUndoHooks
     */
    struct UndoHooks {
        /// @brief 指定手数のハイライトを表示する
        /// @note 配線元: UndoDeps → MainWindowMatchAdapter::updateHighlightsForPly
        std::function<void(int /*ply*/)> updateHighlightsForPly;

        /// @brief 手番表示と時計表示を現在の状態に更新する
        /// @note 配線元: UndoDeps → MainWindowMatchAdapter::updateTurnAndTimekeepingDisplay
        std::function<void()> updateTurnAndTimekeepingDisplay;

        /// @brief 指定プレイヤーが人間側かどうかを判定する
        /// @note 配線元: UndoDeps → PlayModePolicyService::isHumanSide
        std::function<bool(ShogiGameController::Player)> isHumanSide;
    };

    void setRefs(const Refs& refs);
    void setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks);

    /// 2手分のUNDOを実行する
    [[nodiscard]] bool undoTwoPlies();

    /// 平手初期SFENの簡易判定
    [[nodiscard]] static bool isStandardStartposSfen(const QString& sfen);

private:
    bool tryRemoveLastItems(QObject* model, int n);
    static QString buildBasePositionUpToHands(const QString& prevFull,
                                              int handCount,
                                              const QString& startSfenHint);

    Refs m_refs;
    UndoRefs u_;
    UndoHooks h_;
};

#endif // MATCHUNDOHANDLER_H
