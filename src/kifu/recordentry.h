#ifndef RECORDENTRY_H
#define RECORDENTRY_H

#include <QMetaType>
#include "shogimove.h"
#include "terminaltype.h"
#include "recordtime.h"

/// 確定した一手。対局・通信層から棋譜へ渡す値で、表示文字列の再解析を必要としない。
struct RecordEntry {
    ShogiMove move;
    QString displayText;
    QString sfen;
    RecordTime time;
    TerminalType terminal = TerminalType::None;
    Turn mover = Turn::Black;

    bool isTerminal() const { return terminal != TerminalType::None; }

    static RecordEntry ending(TerminalType type, Turn side, const RecordTime& time = {})
    {
        RecordEntry entry;
        entry.terminal = type;
        entry.mover = side;
        entry.time = time;
        QString label;
        switch (type) {
        case TerminalType::None: return entry;
        case TerminalType::Resign: label = QStringLiteral("投了"); break;
        case TerminalType::Checkmate: label = QStringLiteral("詰み"); break;
        case TerminalType::Repetition: label = QStringLiteral("千日手"); break;
        case TerminalType::PerpetualCheck: label = QStringLiteral("王手千日手"); break;
        case TerminalType::Impasse: label = QStringLiteral("持将棋"); break;
        case TerminalType::Timeout: label = QStringLiteral("時間切れ"); break;
        case TerminalType::IllegalWin: label = QStringLiteral("反則勝ち"); break;
        case TerminalType::IllegalLoss: label = QStringLiteral("反則負け"); break;
        case TerminalType::ForfeitWin: label = QStringLiteral("不戦勝"); break;
        case TerminalType::Forfeit: label = QStringLiteral("不戦敗"); break;
        case TerminalType::Interrupt: label = QStringLiteral("中断"); break;
        case TerminalType::NoCheckmate: label = QStringLiteral("不詰"); break;
        case TerminalType::DeclarationWin: label = QStringLiteral("入玉勝ち"); break;
        case TerminalType::Draw: label = QStringLiteral("引き分け"); break;
        case TerminalType::MaxMoves: label = QStringLiteral("最大手数到達"); break;
        case TerminalType::Error: label = QStringLiteral("エラー"); break;
        }
        entry.displayText = (side == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△")) + label;
        return entry;
    }

    /// 既存棋譜の取り込み境界。対局中は型付きの値を直接構築する。
    static RecordEntry imported(const ShogiMove& move, const QString& display,
                                const QString& sfen, const QString& time)
    {
        const TerminalType type = detectTerminalType(display);
        const bool whiteToMove = sfen.contains(QStringLiteral(" w "));
        // 終局手では局面は変わらない。通常手のSFENは着手後なので逆の側。
        Turn side = (type != TerminalType::None ? whiteToMove : !whiteToMove)
            ? Turn::White : Turn::Black;
        if (display.startsWith(QStringLiteral("▲"))) side = Turn::Black;
        if (display.startsWith(QStringLiteral("△"))) side = Turn::White;
        return {move, display, sfen, RecordTime::fromText(time), type, side};
    }
};

Q_DECLARE_METATYPE(RecordEntry)

#endif // RECORDENTRY_H
