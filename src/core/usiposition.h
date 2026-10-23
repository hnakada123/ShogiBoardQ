#ifndef USIPOSITION_H
#define USIPOSITION_H

/// @file usiposition.h
/// @brief エンジンに探索させる局面（開始局面と指し手の列）の値

#include <QMetaType>
#include <algorithm>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <optional>

#include "shogiposition.h"
#include "usimove.h"

/**
 * @brief エンジンに探索させる局面
 *
 * 開始局面・そこから指した手の列（position コマンドの moves）・指した後の局面・局面に至った直前の手を持つ。
 * 直前の手は読み筋の「同」の基準と盤の強調に使う。開始局面だけを送る局面（"position sfen ..."）でも、
 * そこに至った手が分かっていれば持てる。
 * 手番・探索する局面・先読みの当たり外れは、position コマンドの文字列を読み直さずにこの値から求める。
 */
class UsiPosition
{
public:
    /// 空（エンジンに局面を送らない）
    UsiPosition() = default;
    /// 開始局面だけの局面（lastMove はその局面に至った手。分からなければ省く）
    explicit UsiPosition(const Position& start, std::optional<UsiMove> lastMove = std::nullopt);

    /// 完全な SFEN か "startpos" の局面。読めなければ nullopt
    static std::optional<UsiPosition> fromSfen(QStringView sfen, std::optional<UsiMove> lastMove = std::nullopt);
    /// 開始局面（完全な SFEN）から手を順に指した局面。読めない・指せない手があれば nullopt
    static std::optional<UsiPosition> fromMoves(QStringView startSfen, const QList<UsiMove>& moves);
    /**
     * @brief 開始局面から moves を順に指した手順の、fromPly〜toPly 手目の後の各局面
     *
     * 指した手の列は全部の局面で共有する（局面ごとに手順を複製すると、手数の二乗のメモリになる）。
     * 範囲が手順の外か、読めない・指せない手があれば nullopt
     */
    static std::optional<QList<UsiPosition>> alongMoves(QStringView startSfen, const QList<UsiMove>& moves,
                                                        int fromPly, int toPly);
    /**
     * @brief position コマンド（"position startpos moves ..." / "position sfen ... moves ..."）を読む
     *
     * "position " を省いた形・SFEN だけの形（手数を省いた3つの欄も）も受け付ける。読めなければ nullopt を返し、error に理由を入れる
     */
    static std::optional<UsiPosition> parse(QStringView text, QString* error = nullptr);

    bool isEmpty() const { return !m_valid; }

    /// 開始局面（完全な SFEN）
    QString startSfen() const { return m_valid ? m_start.toSfen() : QString(); }
    /// 開始局面から指した手（USI 形式）
    QStringList moves() const { return m_moves.first(m_moveCount); }
    /// 指した後の局面（探索する局面）
    const Position& position() const { return m_position; }
    /// 探索する局面の SFEN
    QString sfen() const { return m_valid ? m_position.toSfen() : QString(); }
    /// 探索する局面の手番
    Turn sideToMove() const { return m_position.sideToMove(); }
    /// 探索する局面に至った直前の手（開始局面で、至った手が分からなければ nullopt）
    const std::optional<UsiMove>& lastMove() const { return m_lastMove; }

    /// この局面から1手指した局面（指せなければ nullopt）
    std::optional<UsiPosition> withMove(const UsiMove& move) const;

    /// エンジンに送る position コマンド（空なら空文字）
    QString command() const;

    /**
     * @brief 開始局面と指し手の列を position コマンドの文字列にする（指し手が指せるかは確かめない）
     *
     * 開始局面が空か平手（手数 1）なら "position startpos"、それ以外は書かれた SFEN のまま
     * "position sfen ..." にする。command() と棋譜の USI 形式の書き出しが共有する。
     */
    static QString commandFor(const QString& startSfen, const QStringList& moves);

    friend bool operator==(const UsiPosition& a, const UsiPosition& b)
    {
        if (a.m_valid != b.m_valid) return false;
        if (!a.m_valid) return true;
        return a.m_start == b.m_start && a.m_moveCount == b.m_moveCount
               && std::equal(a.m_moves.cbegin(), a.m_moves.cbegin() + a.m_moveCount, b.m_moves.cbegin())
               && a.m_lastMove == b.m_lastMove;
    }
    friend bool operator!=(const UsiPosition& a, const UsiPosition& b) { return !(a == b); }

private:
    /// この局面から1手指す（指せなければ false。そのときは使わない）
    bool advance(const UsiMove& move);

    bool m_valid = false;
    Position m_start;
    QStringList m_moves;          ///< 指した手。先頭の m_moveCount 手がこの局面の手順（alongMoves の局面どうしで共有する）
    qsizetype m_moveCount = 0;
    Position m_position;
    std::optional<UsiMove> m_lastMove;
};

Q_DECLARE_METATYPE(UsiPosition)

#endif // USIPOSITION_H
