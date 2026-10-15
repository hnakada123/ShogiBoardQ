#ifndef KIFUBRANCHNODE_H
#define KIFUBRANCHNODE_H

/// @file kifubranchnode.h
/// @brief 分岐ツリーノードクラスの定義

#include <QString>
#include <QList>

#include "recordentry.h"
#include "shogimove.h"

#include "terminaltype.h"
#include "recordtime.h"

/**
 * @brief 分岐ツリーのノードクラス
 *
 * 棋譜の1手（または開始局面）を表すノード。
 * ツリー構造で分岐を表現し、KifuBranchTreeが所有する。
 *
 */
class KifuBranchNode
{
public:
    KifuBranchNode();
    ~KifuBranchNode();

    // --- ノード識別 ---

    /// この行（指した手か終局）とコメント・しおり
    const RecordEntry& record() const { return m_record; }

    int nodeId() const { return m_nodeId; }
    void setNodeId(int id) { m_nodeId = id; }

    // --- 手数・指し手情報 ---

    /// 手数を取得（0=開始局面、1以降=指し手）
    int ply() const { return m_ply; }
    void setPly(int ply) { m_ply = ply; }
    void setRecord(const RecordEntry& record) { m_record = record; }

    /// 表示テキストを取得（例: "▲７六歩(77)"）
    QString displayText() const { return m_record.displayText; }
    void setDisplayText(const QString& text) { m_record.displayText = text; }

    /// この局面のSFENを取得
    QString sfen() const { return m_record.sfen; }
    void setSfen(const QString& sfen) { m_record.sfen = sfen; }

    /// この手を表すShogiMoveを取得（ply=0や終局手は無効）
    ShogiMove move() const { return m_record.move; }
    void setMove(const ShogiMove& move) { m_record.move = move; }

    QString comment() const { return m_record.comment; }
    void setComment(const QString& comment) { m_record.comment = comment; }

    QString bookmark() const { return m_record.bookmark; }
    void setBookmark(const QString& bookmark) { m_record.bookmark = bookmark; }

    QString timeText() const { return m_record.time.toText(); }
    const RecordTime& time() const { return m_record.time; }
    void setTime(const RecordTime& time) { m_record.time = time; }
    void setTimeText(const QString& time) { m_record.time = RecordTime::fromText(time); }

    TerminalType terminalType() const { return m_record.terminal; }
    void setTerminalType(TerminalType type) { m_record.terminal = type; }

    Turn terminalSide() const { return m_record.mover; }
    void setTerminalSide(Turn side) { m_record.mover = side; }

    // --- ツリー構造 ---

    /// 親ノードを取得（nullptr=ルート）
    KifuBranchNode* parent() const { return m_parent; }
    void setParent(KifuBranchNode* parent) { m_parent = parent; }

    /// 子ノード（分岐）を取得
    const QList<KifuBranchNode*>& children() const { return m_children; }
    void addChild(KifuBranchNode* child);
    void removeChild(KifuBranchNode* child);
    /// 子の並び順を変える（先頭ほど本譜側）。子でない場合や範囲外は何もしない。
    bool moveChild(KifuBranchNode* child, int index);
    int childCount() const { return static_cast<int>(m_children.size()); }
    KifuBranchNode* childAt(int index) const;

    // --- クエリ ---

    /// 親の最初の子かどうか（祖先は見ない。入れ子分岐内の先頭子でも true になる）
    /// 表示上の「本譜」判定には KifuNavigationState::isOnMainLine() を使うこと
    bool isMainLine() const;

    /// このノードが属するラインのインデックス（0=本譜、1以降=分岐）
    int lineIndex() const;

    /// 終局手かどうか
    bool isTerminal() const { return m_record.isTerminal(); }

    /// 盤面を変化させる指し手か（終局手やply=0でない）
    bool isActualMove() const { return !isTerminal() && m_ply > 0; }

    /// 分岐があるかどうか（子が2つ以上）
    bool hasBranch() const { return m_children.size() > 1; }

    /// このノードの兄弟ノードを取得（自分を除く）
    QList<KifuBranchNode*> siblings() const;

private:
    int m_nodeId = -1;                                  ///< ノードID（ツリー内で一意）
    int m_ply = 0;                                      ///< 手数（0=開始局面）
    RecordEntry m_record;                               ///< この行（指した手か終局）とコメント・しおり
    KifuBranchNode* m_parent = nullptr;                 ///< 親ノード（非所有）
    QList<KifuBranchNode*> m_children;                ///< 子ノードリスト（非所有、KifuBranchTreeが所有）
};

#endif // KIFUBRANCHNODE_H
