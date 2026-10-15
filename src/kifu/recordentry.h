#ifndef RECORDENTRY_H
#define RECORDENTRY_H

/// @file recordentry.h
/// @brief 棋譜の1行（指した手か終局）

#include <QMetaType>
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
    QString displayText;    ///< 棋譜欄の表記（"▲７六歩(77)"、"△投了"）
    QString sfen;           ///< この行の後の局面（終局の行は直前の局面）
    RecordTime time;
    TerminalType terminal = TerminalType::None;
    Turn mover = Turn::Black;   ///< 指した側（終局の行は終局を記録した側）
    QString comment;
    QString bookmark;

    bool isTerminal() const { return terminal != TerminalType::None; }

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

    /// 終局の行。表記は終局語の表（TerminalTypes::label）から作る
    static RecordEntry ending(TerminalType type, Turn side, const RecordTime& time = {})
    {
        RecordEntry entry;
        entry.terminal = type;
        entry.mover = side;
        entry.time = time;
        if (type != TerminalType::None)
            entry.displayText = (side == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△"))
                + TerminalTypes::label(type);
        return entry;
    }
};
Q_DECLARE_METATYPE(RecordEntry)

#endif // RECORDENTRY_H
