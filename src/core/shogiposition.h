#ifndef SHOGIPOSITION_H
#define SHOGIPOSITION_H

/// @file shogiposition.h
/// @brief 局面の値（盤・持ち駒・手番・手数）。SFEN の読み書きと指し手の適用はここだけで行う

#include <QMap>
#include <QString>
#include <QStringView>
#include <array>
#include <optional>

#include "shogimove.h"
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 局面（盤81マス・両者の持ち駒・手番・手数）
 *
 * QObject ではない値の型。棋譜の読み書き・局面の計算・合法手の判定は、この型で局面を受け渡す。
 * 盤のマスは筋・段（1〜9）で指す。持ち駒は先後それぞれ生駒8種（玉を含む。局面編集で駒台に置ける）の枚数。
 * 表示用の盤（ShogiBoard）はこの値を1つ持ち、変更を知らせる包みにすぎない。
 */
class Position
{
public:
    static constexpr int kFiles = 9;
    static constexpr int kRanks = 9;
    static constexpr int kSquareCount = kFiles * kRanks;
    /// 持ち駒1種の枚数の上限（SFEN に書ける2桁。駒の数として正しいかは PositionValidator が見る）
    static constexpr int kMaxHandCount = 99;
    /// 手数の上限（読み込む SFEN・局面図の手数。これを超える値で桁あふれさせない）
    static constexpr int kMaxMoveNumber = 10000000;

    /// 空の盤・持ち駒なし・先手番・1手目
    Position();

    /// 平手の開始局面
    static Position hirate();

    /**
     * @brief SFEN（"盤 手番 持ち駒 手数"）を読む
     *
     * 4つの欄がそろっていなければ不正。行き所のない駒・成れない駒の "+" も不正とする。
     * 読めなければ nullopt を返し、error に理由を入れる。
     */
    static std::optional<Position> fromSfen(QStringView sfen, QString* error = nullptr);
    /// SFEN を読めたらこの局面をそれで置き換えて true。読めなければ変えずに false
    bool assignSfen(QStringView sfen, QString* error = nullptr);
    /// "startpos"・"sfen ..."・"position ..." の形と、手数を省いた3つの欄も受け付ける（moves 以降は指して進める。
    /// 読み方は UsiPosition::parse と同じ）
    static std::optional<Position> fromSfenLike(QStringView text, QString* error = nullptr);

    /// SFEN（"盤 手番 持ち駒 手数"）
    QString toSfen() const;
    /// 手番と手数だけを替えた SFEN（盤と持ち駒はこの局面）
    QString toSfen(Turn side, int moveNumber) const;
    /// SFEN の盤の欄
    QString boardSfen() const;
    /// SFEN の持ち駒の欄（先手→後手、それぞれ飛・角・金・銀・桂・香・歩の順。なければ "-"）
    QString handsSfen() const;
    /// 局面を比べるキー（"盤 手番 持ち駒"。手数を含めない）
    QString positionKey() const;

    // --- 盤 ---
    /// マスの駒（盤外は None）
    Piece pieceAt(BoardSquare square) const;
    Piece pieceAt(int file, int rank) const { return pieceAt(BoardSquare{file, rank}); }
    /// マスの駒を置き換える（盤外は無視する。持ち駒は増減しない）
    void setPieceAt(BoardSquare square, Piece piece);
    /// 81マスの駒（添字は (段 - 1) * 9 + (筋 - 1)）
    const std::array<Piece, kSquareCount>& squares() const { return m_squares; }
    /// 盤上の駒を数える
    int countOnBoard(Piece piece) const;
    /// 盤の上の側の玉のマス（なければ無効なマス）
    BoardSquare kingSquare(Turn side) const;

    // --- 持ち駒 ---
    /// 持ち駒の枚数（駒の色が持ち主。成駒は元の駒として数える）
    int handCount(Piece piece) const;
    void setHandCount(Piece piece, int count);
    void addToHand(Piece piece, int count = 1);
    /// 持ち駒を1枚減らす。持っていなければ false
    bool takeFromHand(Piece piece);
    /// 持ち駒が1枚もないか
    bool handsEmpty() const;
    /// 盤上・両者の持ち駒にない駒（駒箱）を、生駒・先手の駒で数える（標準の40枚との差。負にはしない）
    QMap<Piece, int> pieceBox() const;

    // --- 手番・手数 ---
    Turn sideToMove() const { return m_sideToMove; }
    void setSideToMove(Turn side) { m_sideToMove = side; }
    /// 次に指す手の手数（SFEN の4つ目の欄）
    int moveNumber() const { return m_moveNumber; }
    void setMoveNumber(int number) { m_moveNumber = number < 1 ? 1 : (number > kMaxMoveNumber ? kMaxMoveNumber : number); }

    // --- 指し手 ---
    /// 打つ駒が持ち駒にない手・駒のあるマスへ打つ手を許すか
    enum class DropRule {
        RequireHand,   ///< 持ち駒にあり、空いたマスへだけ打てる
        Lenient,       ///< 棋譜の読み込み用（持ち駒が合わなくても、駒のあるマスでも打つ）
    };

    /// 手番側の指し手として、動かす駒・取る駒を添える（移動元が空なら movingPiece は None）
    ShogiMove describe(const UsiMove& move) const;

    /**
     * @brief 手番側の指し手を指す（合法手かは確かめない）
     *
     * 取った駒は持ち駒に加え、行き所のない段へ動いた歩・香・桂は成る。手番を相手に渡し、手数を1つ進める。
     * 移動元が空の手・打つ駒がない手（RequireHand のとき）は指さずに false を返す。
     */
    bool apply(const UsiMove& move, DropRule rule = DropRule::RequireHand);
    /// 手番を mover にしてから指す（棋譜を書き出すときに、記録の手番で指す）
    bool applyAs(const UsiMove& move, Turn mover, DropRule rule = DropRule::RequireHand)
    {
        const Turn saved = m_sideToMove;
        m_sideToMove = mover;
        if (apply(move, rule)) return true;
        m_sideToMove = saved;
        return false;
    }

    friend bool operator==(const Position& a, const Position& b)
    {
        return a.m_squares == b.m_squares && a.m_hands == b.m_hands && a.m_sideToMove == b.m_sideToMove
            && a.m_moveNumber == b.m_moveNumber;
    }
    friend bool operator!=(const Position& a, const Position& b) { return !(a == b); }

    /// 盤のマスの添字（盤外は -1）
    static constexpr int indexOf(BoardSquare square)
    {
        return square.isValid() ? (square.rank - 1) * kFiles + (square.file - 1) : -1;
    }

private:
    static int handSlot(Piece piece);

    std::array<Piece, kSquareCount> m_squares;
    std::array<int, 2 * kHandKindCount> m_hands{};   ///< [先手8種, 後手8種]（PieceKind の値の順）
    Turn m_sideToMove = Turn::Black;
    int m_moveNumber = 1;
};

#endif // SHOGIPOSITION_H
