#ifndef KIFUBRANCHNODE_H
#define KIFUBRANCHNODE_H

/// @file kifubranchnode.h
/// @brief 分岐ツリーノードクラスの定義

#include <QString>
#include <QList>

#include <memory>
#include <vector>

#include "recordentry.h"
#include "shogimove.h"

#include "terminaltype.h"
#include "recordtime.h"

/**
 * @brief 分岐ツリーのノードクラス
 *
 * 棋譜の1手（または開始局面）を表すノード。子（分岐）を所有し、ルートは KifuBranchTree が所有する。
 * 木の形（親子・並び・手数・ID）を変えられるのは KifuBranchTree だけ。
 */
class KifuBranchNode
{
public:
    KifuBranchNode();
    ~KifuBranchNode();
    KifuBranchNode(const KifuBranchNode&) = delete;
    KifuBranchNode& operator=(const KifuBranchNode&) = delete;

    // --- ノード識別 ---

    /// この行（指した手か終局）とコメント・しおり。書き換えるのは、ツリー（組み立て）と文書（コメント・しおりの編集。
    /// 版を上げて知らせる）だけ
    const RecordEntry& record() const { return m_record; }

    int nodeId() const { return m_nodeId; }

    // --- 手数・指し手情報 ---

    /// 手数を取得（0=開始局面、1以降=指し手）
    int ply() const { return m_ply; }

    // --- 行の値（record() の各項目を読む） ---

    /// 表示テキストを取得（例: "▲７六歩(77)"）
    QString displayText() const { return m_record.displayText; }

    /// この局面のSFENを取得
    QString sfen() const { return m_record.sfen; }

    /// この手を表すShogiMoveを取得（ply=0や終局手は無効）
    ShogiMove move() const { return m_record.move; }

    QString comment() const { return m_record.comment; }

    QString bookmark() const { return m_record.bookmark; }

    QString timeText() const { return m_record.time.toText(); }
    const RecordTime& time() const { return m_record.time; }

    TerminalType terminalType() const { return m_record.terminal; }

    Turn terminalSide() const { return m_record.mover; }

    // --- ツリー構造 ---

    /// 親ノードを取得（nullptr=ルート）
    KifuBranchNode* parent() const { return m_parent; }

    /// 子ノード（分岐。先頭ほど本譜側）の一覧
    QList<KifuBranchNode*> children() const;
    int childCount() const { return static_cast<int>(m_children.size()); }
    KifuBranchNode* childAt(int index) const;
    /// child が何番目の子か（子でなければ -1）
    int indexOfChild(const KifuBranchNode* child) const;

    // --- クエリ ---

    /// 親の最初の子かどうか（祖先は見ない。入れ子分岐内の先頭子でも true になる）
    /// 表示上の「本譜」判定には KifuNavigationState::isOnMainLine() を使うこと
    bool isMainLine() const;

    /// 終局手かどうか
    bool isTerminal() const { return m_record.isTerminal(); }

    /// 盤面を変化させる指し手か（終局手やply=0でない）
    bool isActualMove() const { return !isTerminal() && m_ply > 0; }

    /// 分岐があるかどうか（子が2つ以上）
    bool hasBranch() const { return m_children.size() > 1; }

private:
    friend class KifuBranchTree;
    friend class KifuBranchTreeBuilder;   ///< 読み込んだ棋譜の開始局面のコメント・しおりを置く
    friend class KifuDocument;            ///< コメント・しおりの編集
#ifdef SHOGIBOARDQ_TESTING
    friend struct KifuBranchNodeTestAccess;   ///< テストで行の値を直接置く（tests/branchtree_test_util.h）
#endif

    /// 行を書き換える（上の friend だけ）
    RecordEntry& mutableRecord() { return m_record; }

    /// 子を末尾に加える（child の親・手数をこのノードに合わせる）
    KifuBranchNode* appendChild(std::unique_ptr<KifuBranchNode> child);
    /// 子を外して返す（子でなければ nullptr）
    std::unique_ptr<KifuBranchNode> takeChild(const KifuBranchNode* child);
    /// 子の並び順を変える。子でない場合や範囲外は何もしない
    bool moveChild(const KifuBranchNode* child, int index);

    int m_nodeId = -1;                                  ///< ノードID（ツリー内で一意）
    int m_ply = 0;                                      ///< 手数（0=開始局面）
    RecordEntry m_record;                               ///< この行（指した手か終局）とコメント・しおり
    KifuBranchNode* m_parent = nullptr;                 ///< 親ノード（非所有）
    std::vector<std::unique_ptr<KifuBranchNode>> m_children;   ///< 子ノード（所有）
};

#endif // KIFUBRANCHNODE_H
