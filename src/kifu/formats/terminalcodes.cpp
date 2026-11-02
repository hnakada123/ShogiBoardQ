/// @file terminalcodes.cpp
/// @brief 棋譜形式ごとの終局の符号の実装

#include "terminalcodes.h"

#include <QStringView>

namespace {

struct Code {
    TerminalType type;
    const char* code;
    bool readOnly = false;  ///< 読むだけで書かない符号
};

// 書くときは種類の最初の行（readOnly の行は除く）、読むときは符号の最初の行を使う（後ろの行は読むだけの別名）

const Code kCsaCodes[] = {
    {TerminalType::Resign, "TORYO"},
    {TerminalType::Interrupt, "CHUDAN"},
    {TerminalType::PerpetualCheck, "OUTE_SENNICHITE"},
    {TerminalType::Repetition, "SENNICHITE"},
    {TerminalType::Impasse, "JISHOGI"},
    {TerminalType::Timeout, "TIME_UP"},
    {TerminalType::IllegalLoss, "ILLEGAL_MOVE"},
    {TerminalType::DeclarationWin, "KACHI"},
    {TerminalType::Draw, "HIKIWAKE"},
    {TerminalType::MaxMoves, "MAX_MOVES"},
    {TerminalType::Checkmate, "TSUMI"},
    {TerminalType::NoCheckmate, "FUZUMI"},
    {TerminalType::Error, "ERROR"},
    {TerminalType::IllegalLoss, "ILLEGAL_ACTION"},
};

const Code kUsiCodes[] = {
    {TerminalType::Resign, "resign"},
    {TerminalType::Interrupt, "break"},
    {TerminalType::Repetition, "rep_draw"},
    {TerminalType::PerpetualCheck, "rep_draw"},
    {TerminalType::Draw, "draw"},
    {TerminalType::Impasse, "draw"},
    {TerminalType::Timeout, "timeout"},
    {TerminalType::DeclarationWin, "win"},
    {TerminalType::Checkmate, "checkmate"},
    {TerminalType::IllegalWin, "illegal_win"},
    {TerminalType::IllegalLoss, "lose"},
    {TerminalType::Repetition, "sennichite"},
    // 過去の ShogiBoardQ が書いた独自の符号
    {TerminalType::Checkmate, "mate"},
    {TerminalType::IllegalLoss, "illegal"},
};

// USEN の符号（作者の実装 mog-core-scala の SpecialMove に合わせる。r=投了, t=時間切れ, p=中断, j=持将棋）。
// 千日手は持将棋と同じく引き分けの j で書く。
// i は手番側の反則（手番側の負け）で、後ろに反則手の3文字が続く（UsenCodec が読む）。終局の行は反則手を持たないので
// 書かず、読むだけにする。反則勝ちの符号は無い
const Code kUsenCodes[] = {
    {TerminalType::Resign, "r"},
    {TerminalType::Timeout, "t"},
    {TerminalType::Interrupt, "p"},
    {TerminalType::Impasse, "j"},
    {TerminalType::PerpetualCheck, "j"},
    {TerminalType::Repetition, "j"},
    {TerminalType::IllegalLoss, "i", true},
};

template <size_t N>
QString codeOf(const Code (&table)[N], TerminalType type)
{
    for (const Code& entry : table) {
        if (entry.type == type && !entry.readOnly) return QString::fromLatin1(entry.code);
    }
    return {};
}

template <size_t N>
const Code* entryOf(const Code (&table)[N], QStringView code, Qt::CaseSensitivity cs = Qt::CaseSensitive)
{
    for (const Code& entry : table) {
        if (code.compare(QLatin1String(entry.code), cs) == 0) return &entry;
    }
    return nullptr;
}

} // namespace

namespace TerminalCodes {

QString csaSpecial(TerminalType type, Turn sideToMove, bool perpetualCheck)
{
    // 手番側の反則勝ちは、相手（手番でない側）の反則
    if (type == TerminalType::IllegalWin)
        return sideToMove == Turn::Black ? QStringLiteral("-ILLEGAL_ACTION") : QStringLiteral("+ILLEGAL_ACTION");
    if (type == TerminalType::PerpetualCheck && !perpetualCheck) type = TerminalType::Repetition;
    return codeOf(kCsaCodes, type);
}

TerminalType fromCsaSpecial(const QString& code, Turn sideToMove)
{
    QStringView key(code);
    if (key.startsWith(QLatin1Char('%'))) key = key.mid(1);
    // +ILLEGAL_ACTION / -ILLEGAL_ACTION は反則した側。手番側でなければ手番側の反則勝ち
    if ((key.startsWith(QLatin1Char('+')) || key.startsWith(QLatin1Char('-')))
        && key.mid(1).startsWith(QLatin1String("ILLEGAL_ACTION"))) {
        const Turn offender = key.startsWith(QLatin1Char('+')) ? Turn::Black : Turn::White;
        return offender == sideToMove ? TerminalType::IllegalLoss : TerminalType::IllegalWin;
    }
    // 後ろに続く語（"%TORYO,T10" の時間など）は見ない
    for (const Code& entry : kCsaCodes) {
        if (key.startsWith(QLatin1String(entry.code))) return entry.type;
    }
    return TerminalType::None;
}

QString usi(TerminalType type)
{
    return codeOf(kUsiCodes, type);
}

TerminalType fromUsi(const QString& code)
{
    const Code* entry = entryOf(kUsiCodes, code, Qt::CaseInsensitive);
    return entry ? entry->type : TerminalType::Error;
}

bool isUsiCode(const QString& code)
{
    return entryOf(kUsiCodes, code, Qt::CaseInsensitive) != nullptr;
}

QString usen(TerminalType type)
{
    return codeOf(kUsenCodes, type);
}

TerminalType fromUsen(const QString& code)
{
    const Code* entry = entryOf(kUsenCodes, code);
    return entry ? entry->type : TerminalType::Error;
}

bool isUsenCode(const QString& code)
{
    return entryOf(kUsenCodes, code) != nullptr;
}

} // namespace TerminalCodes
