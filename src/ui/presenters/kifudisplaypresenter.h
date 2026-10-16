#ifndef KIFUDISPLAYPRESENTER_H
#define KIFUDISPLAYPRESENTER_H

/// @file kifudisplaypresenter.h
/// @brief 棋譜欄・分岐候補欄・分岐ツリーに、表示中の手と手順を映す

#include <QObject>

class BranchTreeView;
class KifuBranchListModel;
class KifuNavigationState;
class KifuRecordListModel;
class LiveGameSession;
class QTableView;

/**
 * @brief 表示中の手（KifuNavigationState）が変わるたびに、棋譜欄・分岐候補欄・分岐ツリーを合わせる
 *
 * 表示する手順は、対局中は対局の最新の手まで（その先にある以前の手順は出さない）、
 * それ以外は選んでいる手順の終わりまで。状態の写しは持たず、毎回 KifuNavigationState から作る。
 * 分岐ツリーの図は、ツリーの変更を自分で受けて描き直す（ここでは表示中の手だけを伝える）。
 */
class KifuDisplayPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuNavigationState* navState = nullptr;      ///< 表示中の手（必須）
        LiveGameSession* liveSession = nullptr;       ///< 対局中の手順（必須）
        KifuRecordListModel* recordModel = nullptr;   ///< 棋譜欄（必須）
        KifuBranchListModel* branchModel = nullptr;   ///< 分岐候補欄（必須）
        QTableView* recordView = nullptr;             ///< 棋譜欄の表（選択と表示位置を合わせる）
        BranchTreeView* treeView = nullptr;           ///< 分岐ツリーの図
    };

    explicit KifuDisplayPresenter(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 表示中の手・手順・ツリーに合わせて描き直す
    void refresh();
    /// 表示（表記・言語）だけを描き直す
    void refreshLabels();

private:
    /// 分岐が増えた・減った・並びが変わったので、棋譜欄の分岐の印（「+」）を描き直す
    void refreshBranchMarks();
    void selectCurrentRow(int row);

    Deps m_deps;
};

#endif // KIFUDISPLAYPRESENTER_H
