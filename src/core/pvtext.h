#ifndef PVTEXT_H
#define PVTEXT_H

/// @file pvtext.h
/// @brief 局面の上で指した読み筋（エンジンの読み筋・棋譜解析・詰みの手順・読み筋盤で共通の値）

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "shogitypes.h"
#include "terminaltype.h"
#include "usimove.h"

/**
 * @brief 開始局面の上で順に指した読み筋の値
 *
 * 先頭から指せる手（指す側の駒が移動元にある・持ち駒がある・行き先に自分の駒がない）だけを並べ、
 * 最初に指せなかった語から後は rest() に残す（エンジンの resign・win、末尾の "(57.54%)" のような注記もここ）。
 * 合法手かは確かめない（王手の放置や二歩も指す）。開始局面を読めなければ、すべての語が rest()。
 * 表記は KifuPresentation::pv が、その時の表記の設定で作る。
 */
class PvText
{
public:
    /// 指した1手
    struct Step {
        UsiMove move;
        Turn mover = Turn::Black;
        Piece moving = Piece::None;  ///< 動かした駒（成る前。駒打ちは打った駒）
        friend bool operator==(const Step& a, const Step& b)
        {
            return a.move == b.move && a.mover == b.mover && a.moving == b.moving;
        }
    };

    PvText() = default;

    /**
     * @brief 開始局面 baseSfen の上で tokens（USI の語）を順に指す
     * @param previousTo 開始局面に至った手の移動先（日本語の表記で最初の手を「同」と書く。無効なら書かない）
     */
    static PvText play(const QString& baseSfen, const QStringList& tokens, BoardSquare previousTo = {});
    /// usiText は空白で区切った USI の語
    static PvText play(const QString& baseSfen, const QString& usiText, BoardSquare previousTo = {});

    const QString& baseSfen() const { return m_baseSfen; }
    BoardSquare previousTo() const { return m_previousTo; }
    const QList<Step>& steps() const { return m_steps; }
    /// 指せなかった語から後（エンジンが送ったまま）
    const QStringList& rest() const { return m_rest; }

    bool isEmpty() const { return m_steps.isEmpty() && m_rest.isEmpty(); }
    /// 指せた手の数
    int moveCount() const { return static_cast<int>(m_steps.size()); }
    /// 最初の手（指せた手が無ければ nullopt）
    std::optional<UsiMove> firstMove() const;
    /// 指せた手の USI の語
    QStringList usiMoves() const;
    /// 指せた手を空白で区切った USI の読み筋
    QString usiText() const;
    /// 指せた手の後の終局の宣言（エンジンの resign は投了、win は入玉宣言勝ち。無ければ None）
    TerminalType declaration() const;
    /// 末尾の注記（"(57.54%)" のような指し手でない語が1つだけ残っていればその語。無ければ空）
    QString annotation() const;
    /// 表に出す読み筋があるか（指せた手か終局の宣言がある）
    bool isDisplayable() const { return !m_steps.isEmpty() || declaration() != TerminalType::None; }

    friend bool operator==(const PvText& a, const PvText& b)
    {
        return a.m_steps == b.m_steps && a.m_rest == b.m_rest && a.m_previousTo == b.m_previousTo
            && a.m_baseSfen == b.m_baseSfen;
    }
    friend bool operator!=(const PvText& a, const PvText& b) { return !(a == b); }

private:
    QString m_baseSfen;
    BoardSquare m_previousTo;
    QList<Step> m_steps;
    QStringList m_rest;
};

#endif // PVTEXT_H
