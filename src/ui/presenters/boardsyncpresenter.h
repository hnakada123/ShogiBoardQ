#ifndef BOARDSYNCPRESENTER_H
#define BOARDSYNCPRESENTER_H

/// @file boardsyncpresenter.h
/// @brief 盤を表示中の手の局面に合わせ、その手と手番を示す

#include <QObject>

class BoardInteractionController;
class KifuNavigationState;
class ShogiGameController;

/**
 * @brief 表示中の手（KifuNavigationState）が変わるたびに、盤をその局面に合わせる
 *
 * 盤（ShogiGameController の盤）は棋譜をたどるときも対局中も同じ1つで、表示中の手の局面と違うときだけ
 * 局面を入れ替える（対局中は対局の盤が先に最新の手を指しているので入れ替えない）。
 * 手のハイライトはノードの指し手から作る（手番の表示は盤のビューが盤の手番から作る）。
 */
class BoardSyncPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiGameController* gc = nullptr;             ///< 盤（必須）
        BoardInteractionController* bic = nullptr;     ///< ハイライトとクリック待ち（必須）

        const KifuNavigationState* navState = nullptr; ///< 表示中の手（必須）
    };

    explicit BoardSyncPresenter(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 盤・手のハイライト・手番の表示を表示中の手に合わせる
    void syncToCurrentNode();

private:
    Deps m_deps;
};

#endif // BOARDSYNCPRESENTER_H
