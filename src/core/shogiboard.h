#ifndef SHOGIBOARD_H
#define SHOGIBOARD_H

/// @file shogiboard.h
/// @brief 表示に使う盤（局面の値 Position を1つ持ち、変更を知らせる）

#include <QObject>
#include <QMap>

#include "shogiposition.h"
#include "shogitypes.h"

/**
 * @brief 画面に出す盤（局面の値を1つ持ち、変更をシグナルで知らせる包み）
 *
 * 局面そのもの（盤・持ち駒・手番・手数・SFEN）は Position が持つ。計算のための一時的な局面には
 * Position を使い、この QObject は作らない。局面を変えるのは、局面の置き換え（丸ごと: setPosition・setSfen、
 * 変わった所だけを知らせる: replacePosition）と手番・手数の変更だけ。着手は GameBoard（MoveRecord::play）が、
 * 局面編集の操作は PositionEditRules が、次の局面を Position で作って replacePosition で渡す。
 * 表示の座標（駒台は筋10・11、駒箱は筋12）で読み出せる。
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
     * @brief 局面を next にし、変わったマス（dataChanged）・駒台（standChanged）・手番（turnChanged）だけを知らせる
     *
     * 着手と局面編集の1回の操作に使う（変わった所だけを描き直せる）。局面を丸ごと替えるときは setPosition。
     */
    void replacePosition(const Position& next);

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

    /// 駒台・駒箱の座標 fileFrom に source の駒があるか（盤上の筋は常に true）
    bool isPieceAvailableOnStand(const Piece source, const int fileFrom) const;

signals:
    /// 盤面全体リセット通知（→ ShogiView::repaint）
    void boardReset();

    /// 盤面の1マス変更通知（→ ShogiView::repaint）
    void dataChanged(int c, int r);

    /// 駒台の枚数の変更通知（→ ShogiView の駒台のセルの描き直し）。
    /// 局面を丸ごと替える setPosition・setSfen は、この通知ではなく boardReset で知らせる
    void standChanged(Piece piece);

    /// 手番が変わった（指した・手番だけを変えた。局面を丸ごと替えたときは boardReset で知らせる）
    void turnChanged(Turn turn);

private:
    Position m_position;
};

#endif // SHOGIBOARD_H
