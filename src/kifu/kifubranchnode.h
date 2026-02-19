#ifndef KIFUBRANCHNODE_H
#define KIFUBRANCHNODE_H

/// @file kifubranchnode.h
/// @brief 分岐ツリーノードクラスの定義

#include <QString>
#include <QVector>

#include "shogimove.h"

/**
 * @brief 終局手の種類
 *
 * @todo remove コメントスタイルガイド適用済み
 */
enum class TerminalType {
    None,           ///< 通常の指し手（終局手ではない）
    Resign,         ///< 投了
    Checkmate,      ///< 詰み
    Repetition,     ///< 千日手
    Impasse,        ///< 持将棋
    Timeout,        ///< 切れ負け
    IllegalWin,     ///< 反則勝ち
    IllegalLoss,    ///< 反則負け
    Forfeit,        ///< 不戦敗
    Interrupt,      ///< 中断
    NoCheckmate     ///< 不詰（詰将棋用）
};

/**
 * @brief 終局手の種類を文字列から検出する
 * @param displayText 表示テキスト（例: "△投了"）
 * @return 検出された終局手の種類
 */
TerminalType detectTerminalType(const QString& displayText);

/**
 * @brief 分岐ツリーのノードクラス
 *
 * 棋譜の1手（または開始局面）を表すノード。
 * ツリー構造で分岐を表現し、KifuBranchTreeが所有する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class KifuBranchNode
{
public:
    KifuBranchNode();
    ~KifuBranchNode();

    // --- ノード識別 ---

    int nodeId() const { return m_nodeId; }
    void setNodeId(int id) { m_nodeId = id; }

    // --- 手数・指し手情報 ---

    /// 手数を取得（0=開始局面、1以降=指し手）
    int ply() const { return m_ply; }
    void setPly(int ply) { m_ply = ply; }

    /// 表示テキストを取得（例: "▲７六歩(77)"）
    QString displayText() const { return m_displayText; }
    void setDisplayText(const QString& text) { m_displayText = text; }

    /// この局面のSFENを取得
    QString sfen() const { return m_sfen; }
    void setSfen(const QString& sfen) { m_sfen = sfen; }

    /// この手を表すShogiMoveを取得（ply=0や終局手は無効）
    ShogiMove move() const { return m_move; }
    void setMove(const ShogiMove& move) { m_move = move; }

    QString comment() const { return m_comment; }
    void setComment(const QString& comment) { m_comment = comment; }

    QString bookmark() const { return m_bookmark; }
    void setBookmark(const QString& bookmark) { m_bookmark = bookmark; }

    QString timeText() const { return m_timeText; }
    void setTimeText(const QString& time) { m_timeText = time; }

    TerminalType terminalType() const { return m_terminalType; }
    void setTerminalType(TerminalType type) { m_terminalType = type; }

    // --- ツリー構造 ---

    /// 親ノードを取得（nullptr=ルート）
    KifuBranchNode* parent() const { return m_parent; }
    void setParent(KifuBranchNode* parent) { m_parent = parent; }

    /// 子ノード（分岐）を取得
    const QVector<KifuBranchNode*>& children() const { return m_children; }
    void addChild(KifuBranchNode* child);
    int childCount() const { return static_cast<int>(m_children.size()); }
    KifuBranchNode* childAt(int index) const;

    // --- クエリ ---

    /// 本譜かどうか（親の最初の子であるかどうかで判定）
    bool isMainLine() const;

    /// このノードが属するラインの名前（"本譜", "分岐1", "分岐2"...）
    QString lineName() const;

    /// このノードが属するラインのインデックス（0=本譜、1以降=分岐）
    int lineIndex() const;

    /// 終局手かどうか
    bool isTerminal() const { return m_terminalType != TerminalType::None; }

    /// 盤面を変化させる指し手か（終局手やply=0でない）
    bool isActualMove() const { return !isTerminal() && m_ply > 0; }

    /// 分岐があるかどうか（子が2つ以上）
    bool hasBranch() const { return m_children.size() > 1; }

    /// このノードの兄弟ノードを取得（自分を除く）
    QVector<KifuBranchNode*> siblings() const;

private:
    int m_nodeId = -1;                                  ///< ノードID（ツリー内で一意）
    int m_ply = 0;                                      ///< 手数（0=開始局面）
    QString m_displayText;                              ///< 表示テキスト
    QString m_sfen;                                     ///< この局面のSFEN
    ShogiMove m_move;                                   ///< 指し手データ
    QString m_comment;                                  ///< コメント
    QString m_bookmark;                                 ///< しおり
    QString m_timeText;                                 ///< 消費時間テキスト
    TerminalType m_terminalType = TerminalType::None;   ///< 終局手の種類

    KifuBranchNode* m_parent = nullptr;                 ///< 親ノード（非所有）
    QVector<KifuBranchNode*> m_children;                ///< 子ノードリスト（非所有、KifuBranchTreeが所有）
};

#endif // KIFUBRANCHNODE_H
