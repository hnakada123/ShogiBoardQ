#ifndef BOARDSYNCPRESENTER_H
#define BOARDSYNCPRESENTER_H

/// @file boardsyncpresenter.h
/// @brief 盤面同期プレゼンタクラスの定義


#include <QObject>
#include <QPoint>
#include <QString>

#include <functional>

#include "recordline.h"

class ShogiGameController;
class ShogiView;
class BoardInteractionController;

/**
 * @brief 棋譜の局面を盤に描き、直前の手をハイライトする
 *
 * 局面は棋譜の正本（分岐ツリーの表示中の手順）から取る。対局中も、対局の手順は
 * LiveGameSession が分岐ツリーに積んでいるので同じ経路で描ける。
 */
class BoardSyncPresenter : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        BoardInteractionController* bic = nullptr;
        std::function<RecordLine()> currentLine;   ///< 表示中の手順（KifuNavigationState::currentRecordLine）
    };

    explicit BoardSyncPresenter(const Deps& deps, QObject* parent=nullptr);

    /// 表示中の手順の ply 手目の局面を盤に描き、その手をハイライトする（ply=0 と終局行はハイライト無し）
    void syncBoardAndHighlightsAtRow(int ply) const;

    /**
     * @brief 指定した局面を盤に描き、直前の局面との差分から手をハイライトする（分岐ナビゲーション用）
     * @param currentSfen 現在の局面SFEN
     * @param prevSfen 直前の局面SFEN（空の場合はハイライトクリア）
     */
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen) const;

private:
    void applyBoard(const QString& sfen) const;
    void showMoveHighlight(const QString& prevSfen, const QString& currentSfen) const;

    ShogiGameController* m_gc;
    ShogiView* m_view;
    BoardInteractionController* m_bic;
    std::function<RecordLine()> m_currentLine;
};

#endif // BOARDSYNCPRESENTER_H
