#ifndef KIFUNODEREPLAY_H
#define KIFUNODEREPLAY_H

/// @file kifunodereplay.h
/// @brief 棋譜の手順のノードを、指す前の局面と一緒に1手ずつたどる（KI2・JKF の書き出しで共有）

#include "usimove.h"
#include "shogiposition.h"

class KifuBranchNode;

/**
 * @brief 手順のノードを局面と一緒にたどる
 *
 * 表記（「同」・修飾語・打・不成）は指す前の局面と直前の手の移動先から決まるので、書き出す側は
 * 各ノードの手を書く前に board()・mover()・previousTo() を読み、書いた後に advance() で進める。
 * ノードの指し手が局面に合わない（読み込んだ棋譜の手が指せない）ときは、局面をノードの SFEN から取り直して続ける。
 */
class KifuNodeReplay
{
public:
    /// startSfen の局面から始める。読めなければ fallbackSfen（根のノードの局面など）から
    explicit KifuNodeReplay(const QString& startSfen, const QString& fallbackSfen = QString());
    /// node の手を指した後の局面から始める（変化を書くときに、分岐元までの手を指し直さない）。
    /// node の SFEN が読めなければ synced() が false
    static KifuNodeReplay after(const KifuBranchNode* node);

    const Position& board() const { return m_board; }
    /// 次の手を指す側
    Turn mover() const { return m_mover; }
    /// 直前の手の移動先（「同」の判定に使う）
    BoardSquare previousTo() const { return m_previousTo; }
    /// 局面がノードの手順と合っているか
    bool synced() const { return m_synced; }

    /// node の指し手を今の局面から書けるか（局面が合っていて、ノードが指し手を持つ）
    bool canWrite(const KifuBranchNode* node) const;
    /**
     * @brief node の指し手を指して進める（開始局面と終局のノードは何もしない）
     * @return 指せたら true。指せなければ局面をノードの SFEN から取り直して false
     */
    bool advance(const KifuBranchNode* node);

    /// parent の子の手順を別にたどるための写し（局面が合っていなければ parent の局面から取り直す）
    KifuNodeReplay branchFrom(const KifuBranchNode* parent) const;

private:
    void resync(const KifuBranchNode* node);

    Position m_board;
    Turn m_mover = Turn::Black;
    BoardSquare m_previousTo;
    bool m_synced = false;
};

#endif // KIFUNODEREPLAY_H
