#ifndef RECORDENTRY_H
#define RECORDENTRY_H

/// @file recordentry.h
/// @brief 棋譜の1行（指した手か終局）

#include <QMetaType>
#include <optional>
#include "shogimove.h"
#include "terminaltype.h"
#include "recordtime.h"

/**
 * @brief 棋譜の1行
 *
 * 指した手（move）か終局（terminal）のどちらか。手番・終局の種類は値で持ち、
 * 表示の文字列（displayText）から読み直さない。対局・通信・読み込みから棋譜（分岐ツリー）へ渡す。
 */
struct RecordEntry {
    ShogiMove move;
    QString displayText;    ///< 棋譜の表記（KIF の指し手 "▲７六歩(77)"、終局 "△投了"）。画面の表記は
                            ///< KifuPresentation::entryLabel が表記の設定に合わせて作り、ここには書き戻さない
    QString sfen;           ///< この行の後の局面（終局の行は直前の局面）
    RecordTime time;
    TerminalType terminal = TerminalType::None;
    Turn mover = Turn::Black;   ///< 指した側（終局の行は終局を記録した側）
    QString comment;
    QString bookmark;
    QString terminalText;  ///< 独自の終局理由。Custom のときだけ使い、表示・再保存でも原文を保つ

    bool isTerminal() const { return terminal != TerminalType::None; }

    QString terminalLabel(bool translated = false) const
    {
        if (terminal == TerminalType::Custom) return terminalText;
        return translated ? TerminalTypes::displayLabel(terminal) : TerminalTypes::label(terminal);
    }

    /// 指した手の行。指した側は動かした駒の色で決まる
    static RecordEntry played(const ShogiMove& move, const QString& displayText, const QString& sfen,
                              const RecordTime& time = {})
    {
        RecordEntry entry;
        entry.move = move;
        entry.displayText = displayText;
        entry.sfen = sfen;
        entry.time = time;
        entry.mover = move.mover();
        return entry;
    }

    /**
     * @brief 終局の行を記録する側（印を付け、その行の消費時間を数える側）
     * @param sideToMove 終局した局面の手番
     * @param loser 負けた側（勝敗のない終局では使わない）
     *
     * 投了・時間切れ・反則負けは負けた側、入玉宣言勝ち・反則勝ちは勝った側、
     * 千日手・持将棋・手数制限・中断などの勝敗のない終局は手番側。対局・通信対局・棋譜の読み込みで同じ規則
     */
    static Turn endingSide(TerminalType type, Turn sideToMove, Turn loser)
    {
        switch (type) {
        case TerminalType::Resign:
        case TerminalType::Timeout:
        case TerminalType::IllegalLoss:
        case TerminalType::Try:
            return loser;
        case TerminalType::DeclarationWin:
        case TerminalType::IllegalWin:
            return loser == Turn::Black ? Turn::White : Turn::Black;
        default:
            return sideToMove;
        }
    }

    /**
     * @brief 終局の行の勝った側（endingSide の逆。記録した側 mover と終局の種類から決める）
     *
     * 投了・詰み・時間切れ・反則負け・不戦敗は記録した側の負け、入玉宣言勝ち・反則勝ち・不戦勝は記録した側の勝ち。
     * 勝敗の付かない終局（千日手・持将棋・中断など）と指し手の行は nullopt
     */
    std::optional<Turn> winner() const
    {
        if (!isTerminal()) return std::nullopt;
        switch (terminal) {
        case TerminalType::Resign:
        case TerminalType::Checkmate:
        case TerminalType::Timeout:
        case TerminalType::IllegalLoss:
        case TerminalType::Forfeit:
        case TerminalType::Try: // 直前の着手でトライ成立。終局行の手番側は負けた側。
            return mover == Turn::Black ? Turn::White : Turn::Black;
        case TerminalType::IllegalWin:
        case TerminalType::DeclarationWin:
        case TerminalType::ForfeitWin:
            return mover;
        default:
            return std::nullopt;
        }
    }

    /// 終局の行の種類と、記録する側
    struct Ending {
        TerminalType terminal = TerminalType::None;
        Turn side = Turn::Black;
    };

    /**
     * @brief 終局の行を、勝敗を変えずに終局した局面の手番側の行にする
     * @param type 終局の種類
     * @param side 終局を記録する側（endingSide。KI2 の結果行なら名指しから決めた側）
     * @param sideToMove 終局した局面の手番
     *
     * KI2 の結果行のほかは、終局の行を手番側の行としてしか書けない（KIF の手数の行・CSA の符号・JKF の special・
     * USI・USEN）。手番でない側の行のまま書くと、読み直したときに勝敗が変わる。そこで、手番でない側の反則負けは
     * 手番側の反則勝ちに（反則勝ちは反則負けに）、不戦敗は不戦勝に（不戦勝は不戦敗に）し、勝敗のない終局は手番側の
     * 行にする。投了・詰み・時間切れ・入玉宣言勝ち・トライは、相手の行にすると意味が変わるので side のまま返す
     * （呼び出し側は side が手番側かを比べて知らせる）。
     * 対局の終局（連続王手の千日手は王手をかけていた側の反則負け）・通信対局の終局・CSA と JKF の
     * +/-ILLEGAL_ACTION・KI2 の結果行で同じ規則
     */
    static Ending forSideToMove(TerminalType type, Turn side, Turn sideToMove)
    {
        if (side == sideToMove) return {type, side};
        switch (type) {
        case TerminalType::IllegalLoss: return {TerminalType::IllegalWin, sideToMove};
        case TerminalType::IllegalWin: return {TerminalType::IllegalLoss, sideToMove};
        case TerminalType::Forfeit: return {TerminalType::ForfeitWin, sideToMove};
        case TerminalType::ForfeitWin: return {TerminalType::Forfeit, sideToMove};
        case TerminalType::Resign:
        case TerminalType::Checkmate:
        case TerminalType::Timeout:
        case TerminalType::DeclarationWin:
        case TerminalType::Try:
            return {type, side};
        default:
            return {type, sideToMove};
        }
    }

    /// 終局の行。表記は終局語の表（TerminalTypes::label）から作る
    static RecordEntry ending(TerminalType type, Turn side, const RecordTime& time = {}, const QString& text = {})
    {
        RecordEntry entry;
        entry.terminal = type;
        entry.mover = side;
        entry.time = time;
        if (type == TerminalType::Custom) entry.terminalText = text.trimmed();
        if (type != TerminalType::None)
            entry.displayText = (side == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△"))
                + entry.terminalLabel();
        return entry;
    }
};
Q_DECLARE_METATYPE(RecordEntry)

#endif // RECORDENTRY_H
