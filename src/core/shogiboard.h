#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

/// @file shogiboard.h
/// @brief 表示に使う盤（局面の値 Position を1つ持ち、変更を知らせる）

#include <QObject>
#include <QMap>

#include "shogiposition.h"
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 画面に出す盤（局面の値を1つ持ち、変更をシグナルで知らせる包み）
 *
 * 局面そのもの（盤・持ち駒・手番・手数・SFEN）は Position が持つ。計算のための一時的な局面には
 * Position を使い、この QObject は作らない。局面を変えるのは、局面の置き換え（setPosition・setSfen）、
 * 指し手（applyUsiMove）と、局面編集の3つの操作（updateBoardAndPieceStand・promoteOrDemotePiece・
 * resetGameBoard）だけ。表示の座標（駒台は筋10・11、駒箱は筋12）で読み出せる。
 */
class ShogiBoard : public QObject
{
    Q_OBJECT

public:
    explicit ShogiBoard(QObject* parent = nullptr);

    // --- 局面 ---

    /// 今の局面
    const Position& position() const { return m_position; }
    /// 局面を丸ごと置き換える（boardReset で知らせる）
    void setPosition(const Position& position);

    /**
     * @brief SFEN文字列で盤面全体を更新する
     *
     * 不正な場合は盤面を変更せず、エラーを通知する（ErrorBus）。
     */
    void setSfen(const QString& sfenStr);

    /**
     * @brief SFEN文字列で盤面全体を更新する（不正でも通知しない）
     * @param error 不正だったときの理由（任意）
     * @return 盤面を更新したら true。不正な場合は盤面を変更せず false
     */
    bool trySetSfen(const QString& sfenStr, QString* error = nullptr);

    /// 局面全体（盤・手番・持駒・手数）の SFEN
    QString toSfen() const { return m_position.toSfen(); }

    // --- 盤面アクセス ---

    /// 指定マスの駒を返す（盤上は筋1〜9。駒台の筋10・11は、そのセルに置く駒の種類）
    Piece pieceCharacter(const int file, const int rank) const;

    /// 盤上のマスの駒（盤外のマスは None）
    Piece pieceAt(BoardSquare square) const { return m_position.pieceAt(square); }

    /// 盤上のマスの駒を置き換える（局面の組み立て用。成りの補正も駒台の増減もしない。盤外のマスは無視する）
    void setPieceAt(BoardSquare square, Piece piece);

    /// 指定駒の駒台枚数を返す
    int pieceStandCount(Piece piece) const { return m_position.handCount(piece); }

    /// 盤上・両駒台にない未使用駒を、生駒・先手向きで数える（標準40枚との差分）。
    QMap<Piece, int> pieceBox() const;
    int pieceBoxCount(Piece piece) const;

    int ranks() const { return Position::kRanks; }
    int files() const { return Position::kFiles; }
    Turn currentPlayer() const { return m_position.sideToMove(); }

    /// 手番のみを更新する（盤面・持ち駒は変更しない。局面編集の手番変更などに使う）
    void setCurrentPlayer(Turn turn);

    /// 次に指す手の手数（SFEN の4つ目の欄）
    int moveNumber() const { return m_position.moveNumber(); }
    void setMoveNumber(int moveNumber) { m_position.setMoveNumber(moveNumber); }

    // --- 駒操作 ---

    /// 局面編集用: 盤面と駒台を同時に更新する
    void updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo);

    /**
     * @brief USI の指し手を盤と駒台に適用し、手番を相手に渡して手数を1つ進める
     *
     * 合法手かどうかは確かめない（対局で確かめてから指す経路は ShogiGameController::applyMove）。
     * 移動元が空、駒台に打つ駒がない、打つ先が埋まっている指し手は適用せず false を返す。
     * @param mover 指す側（手番をこの側にしてから指す）
     */
    bool applyUsiMove(const UsiMove& move, Turn mover);
    /// 手番側の手として適用する
    bool applyUsiMove(const UsiMove& move) { return applyUsiMove(move, currentPlayer()); }

    /// 局面編集用: 指定マスの駒を成/不成に切り替える
    void promoteOrDemotePiece(const int fileFrom, const int rankFrom);

    /// 駒台に打てる駒があるか判定する
    bool isPieceAvailableOnStand(const Piece source, const int fileFrom) const;

    // --- 盤面リセット ---

    /// 盤上・両駒台を空にして、全駒を駒箱に戻す（局面編集用）
    void resetGameBoard();

signals:
    /// 盤面全体リセット通知（→ ShogiView::repaint）
    void boardReset();

    /// 盤面の1マス変更通知（→ ShogiView::repaint）
    void dataChanged(int c, int r);

    /// 駒台の枚数の変更通知（→ ShogiView の駒台のセルの描き直し）。
    /// 局面を丸ごと替える setSfen は、この通知ではなく boardReset で知らせる
    void standChanged(Piece piece);

    /// 手番が変わった（指した・手番だけを変えた。局面を丸ごと替えたときは boardReset で知らせる）
    void turnChanged(Turn turn);

private:
    friend class TestShogiBoard;

    void setData(const int file, const int rank, const Piece value);
    /// 指定マスへ駒を移動する（配置データのみ更新。行き所のない段へ置いた歩・香・桂は成る）
    void movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                           const int fileTo, const int rankTo, const bool promote);
    /// 指定駒を駒台に加算する（delta>0 のときのみ）
    void addStandPiece(Piece piece, int delta = 1);
    /// 指定駒を駒台から1枚消費する（存在しない/0枚時は false）
    bool consumeStandPiece(Piece piece);
    /// 取った駒を駒台に追加する（成駒は元の駒に変換）
    void addPieceToStand(Piece dest);
    /// 駒台の駒を1枚増やす
    void incrementPieceOnStand(const Piece dest);
    /// 駒台の駒を1枚減らす（駒打ち時）。不足時は false。
    bool decrementPieceOnStand(Piece source);

    Position m_position;
};

#endif // SHOGIBOARD_H
