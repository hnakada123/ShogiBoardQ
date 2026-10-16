#ifndef PVNOTATIONFORMATTER_H
#define PVNOTATIONFORMATTER_H

/// @file pvnotationformatter.h
/// @brief USI の info（UsiInfo）の読み筋・探索中の手を漢字表記にするクラスの定義

#include <QString>
#include <QStringView>
#include "shogiposition.h"
#include "usiinfo.h"
#include "usimove.h"

/**
 * @brief 解析済みの info（UsiInfo）の読み筋を、思考を始めた局面の上で進めて漢字表記にするクラス
 *
 * info 行の解釈は UsiInfoParser が行う。このクラスは盤面に結び付いた表示用の変換だけを持つ。
 * 読み筋のうち局面に合った手までを USI 形式でも残す（読み筋ダイアログや候補手の矢印で使う）。
 */
class PvNotationFormatter
{
public:
    PvNotationFormatter() = default;

    /**
     * @brief 読み筋を日本語にするときの局面（思考を始めた局面）を設定する
     *
     * 読み筋の初手はこの局面の手番の手として書く。読めない SFEN なら読み筋を日本語にしない。
     */
    void setPosition(const QString& sfen);

    /// 局面に至った直前の手の移動先（読み筋の初手が同じマスへ行くなら「同」と書く。無効なマスなら書かない）
    void setPreviousMove(BoardSquare to);

    /**
     * @brief info の読み筋（pv）と探索中の手（currmove）を漢字表記にする
     *
     * USI形式の読み筋（例: "7g7h 2f2e 8e8f"）を、漢字表記（例: "△７八馬(77)▲２五歩(26)△８六歩(85)"）に変換する。
     * 前回の結果は捨てる。
     */
    void format(const UsiInfo& info);

    // --- 変換結果 ---

    /// 漢字表記の読み筋文字列を返す
    QString pvKanjiStr() const { return m_pvKanjiStr; }

    /// 局面に合った手までの、USI形式のスペース区切り読み筋文字列を返す
    QString pvUsiStr() const { return m_pvUsiStr; }

    /// GUIの「探索手」欄に表示する手（currmove、なければ読み筋の先頭手）
    QString searchedHand() const { return m_searchedHand; }
    QString searchedUsi() const { return m_searchedUsi; }

private:
    QString m_pvKanjiStr;       ///< 漢字表記の読み筋文字列
    QString m_pvUsiStr;         ///< USI形式の読み筋文字列
    QString m_searchedUsi;      ///< 探索手（USI形式）
    QString m_searchedHand;     ///< 探索手（漢字表記）

    Position m_position;            ///< 思考を始めた局面
    bool m_hasPosition = false;     ///< m_position を設定できたか
    Position m_pvBoard;             ///< 読み筋を進める局面（info ごとに m_position を写す）
    BoardSquare m_previousTo;       ///< 局面に至った直前の手の移動先

    /// board の上で指し手の動かす駒を返す（mover の駒でない、駒台にない、行き先が自分の駒などは None）
    static Piece movingPiece(const Position& board, const UsiMove& move, Turn mover);

    /// 読み筋の指し手を、盤の写しの上で進めながら漢字表記にする
    void formatPv(const QStringList& pv);

    /// currmove の指し手を漢字表記に変換する
    QString convertCurrMoveToKanjiNotation(QStringView str) const;
};

#endif // PVNOTATIONFORMATTER_H
