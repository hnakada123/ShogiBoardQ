#ifndef LIVEGAMESESSION_H
#define LIVEGAMESESSION_H

/// @file livegamesession.h
/// @brief 対局中の手を分岐ツリーへ書き、表示中の手を最新の手に合わせる

#include <QObject>
#include <QList>
#include <QSet>

#include "recordentry.h"

class KifuBranchNode;
class KifuBranchTree;
class KifuNavigationState;

/**
 * @brief 対局（ローカル・通信）の手順を分岐ツリーへ即時に書く
 *
 * 対局の開始局面（分岐点）から指した手をツリーに足し、表示中の手（KifuNavigationState）を
 * 最新の手へ動かす。表示は KifuNavigationState の変化を受けた部品が合わせる。
 * 途中局面からの対局・待った・既存手の再利用を扱い、commit / discard で終わる。
 */
class LiveGameSession : public QObject
{
    Q_OBJECT

public:
    LiveGameSession(KifuBranchTree* tree, KifuNavigationState* navState, QObject* parent = nullptr);

    // === 開始 ===

    /// branchPoint（nullptr なら開始局面）から対局を始める。終局の行からは始められない
    void startFromNode(KifuBranchNode* branchPoint);
    void startFromRoot() { startFromNode(nullptr); }
    static bool canStartFrom(const KifuBranchNode* node);

    // === 状態 ===

    bool isActive() const { return m_active; }
    /// 対局を始めた分岐点（開始局面から始めたら nullptr）
    KifuBranchNode* branchPoint() const { return m_branchPoint; }
    /// 対局の最新の手（まだ指していなければ開始した局面のノード）
    KifuBranchNode* liveNode() const;
    /// 対局を始めた局面の手数
    int anchorPly() const;
    /// 対局で指した手の数（終局の行を含む）
    int moveCount() const { return m_moveCount; }
    /// 手を足せるか（対局中で、終局の行をまだ書いていない）
    bool canAddMove() const { return m_active && !m_hasTerminal; }

    // === 追記 ===

    /**
     * @brief 確定した行（指した手・終局）をツリーに足し、表示中の手をその行へ動かす
     *
     * 同じ手が既に子にあればそのノードを使う（指し直しで同じ手の分岐を作らない）。
     * 終局の行を足すと canAddMove() が false になる。
     */
    void appendRecord(const RecordEntry& entry);
    /**
     * @brief 時計の消費時間を、棋譜の累計に続けた時間へ直す
     *
     * 累計は、棋譜上で同じ側が直前に指した手の累計＋今回の消費時間にする。
     * 棋譜の途中から対局しても、累計が 0 から数え直されない。
     */
    RecordTime continuedTime(const RecordTime& time) const;
    /// このセッションで新しく作る最初のノードに付けるコメントを設定する（開始後に呼ぶ）
    void setFirstMoveNote(const QString& note);

    // === 待った ===

    /// 対局開始位置を越えず、指定手数を取り消せるか
    bool canUndoMoves(int count) const;
    /// 手を取り消す。取り消した手順はツリーに分岐として残し、次に別の手を指すとその手順より本譜側に並べる
    bool undoMoves(int count);

    // === 確定・破棄 ===

    /// 対局を終える（足した手はツリーに残る）。最後の手のノードを返す（手が無ければ nullptr）
    KifuBranchNode* commit();
    /// 対局をやめる（足した手はツリーに残る）
    void discard();

signals:
    /// 対局を始めた・終えた
    void activeChanged();

private slots:
    void onTreeChanged();

private:
    void placeBeforeUndoneSiblings(KifuBranchNode* node);
    void reset();

    KifuBranchTree* m_tree = nullptr;
    KifuNavigationState* m_navState = nullptr;
    bool m_active = false;
    bool m_hasTerminal = false;
    KifuBranchNode* m_branchPoint = nullptr;
    int m_anchorNodeId = -1;                 ///< 対局を始めた局面のノード（消えたら記録をやめる）
    KifuBranchNode* m_liveParent = nullptr;  ///< 最後に足した手（まだなければ nullptr）
    int m_moveCount = 0;                     ///< 対局で足した行の数（待ったで減らす）
    QSet<int> m_createdNodeIds;              ///< このセッションで新しく作ったノード
    QSet<int> m_undoneNodeIds;               ///< 待ったで取り消した手順の先頭のノード
    QString m_firstMoveNote;                 ///< 次に新しく作るノードへ付けるコメント（棋譜の途中から続けた対局の情報）
};

#endif // LIVEGAMESESSION_H
