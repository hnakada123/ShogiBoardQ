#ifndef GAMEBOARD_H
#define GAMEBOARD_H

/// @file gameboard.h
/// @brief 対局の盤（盤を持ち、合法手を確かめて指し、指した手を棋譜の1行にして着手の確定を知らせる）

#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "legalmovestatus.h"
#include "recordentry.h"
#include "shogitypes.h"
#include "usimove.h"

class ShogiBoard;

/**
 * @brief 対局で使う盤（ShogiBoard）を持ち、指し手を確かめて盤に指す
 *
 * 合法手の判定と、指した手を棋譜の1行（RecordEntry）にすることを受け持つ。手番は盤が持つ
 * （board()->currentPlayer()）。「同」表記の基準（局面に至った手の移動先）は手順を持つ呼び出し側が
 * 指すたびに渡す（盤の局面に写しを持たない）。対局の手順への追加は呼び出し側（司令塔・通信対局・
 * 変化の記録）が行う。盤のクリックを手にする処理と成りの尋ね方は、盤の操作（BoardInteractionController）が持つ。
 */
class GameBoard : public QObject
{
    Q_OBJECT

public:
    explicit GameBoard(QObject* parent = nullptr);
    ~GameBoard() override;

    /// 対局の盤（起動から終了まで同じ1つ。局面は newGame・setSfen で入れ替える）
    ShogiBoard* board() const;

    /// 盤を指定の局面にする（手番は SFEN の手番）
    void newGame(const QString& startSfen);

    // --- 指し手処理 ---

    /// 手番の側の合法手か（成り・不成も指し手どおりに判定する）
    bool isLegal(const UsiMove& move) const;

    /// 人間・エンジン・定跡の指し手を合法手か確かめて指す。反則手は盤を変えずに nullopt
    std::optional<RecordEntry> applyMove(const UsiMove& move, BoardSquare previousTo);

    /// 確かめ済みの手、またはサーバーが裁定する手を指す。盤面に適用できない手は変更せず nullopt
    std::optional<RecordEntry> applyRecordedMove(const UsiMove& move, BoardSquare previousTo);

    /**
     * @brief 着手の確定を知らせる（盤・手順・棋譜・時計を更新し終えた後に、対局の司令塔が呼ぶ）
     * @param ply 対局の手順での手数
     */
    void notifyMoveCommitted(Turn mover, int ply);

signals:
    /// 着手が確定した（→ PieceSoundPlayer）
    void moveCommitted(Turn mover, int ply);

private:
    std::unique_ptr<ShogiBoard> m_board; ///< 将棋盤（所有。起動から終了まで同じ1つ）

    /// 成り・不成それぞれで合法手か
    LegalMoveStatus legalStatus(const UsiMove& move, Turn mover) const;

    /// 盤に指して手番を渡し、指した手を棋譜の1行にする
    std::optional<RecordEntry> commitMove(const UsiMove& move, BoardSquare previousTo);
};

#endif // GAMEBOARD_H
