/// @file terminaltype.cpp
/// @brief 終局語の対応表

#include "terminaltype.h"

#include <QCoreApplication>
#include <array>

namespace {

struct Word {
    TerminalType type;
    const char* text;      ///< UTF-8
    bool canonical;        ///< 棋譜の行に書く語（種類ごとに1つ）
};

// 文中を探すとき前から順に調べるので、別の語を含む長い語を先に置く
// （「王手千日手」→「千日手」、「入玉宣言勝ち」→「宣言勝ち」、「不詰」→「詰み」→「詰」、「最大手数到達」→「最大手数」）。
const std::array<Word, 21> kWords = {{
    {TerminalType::PerpetualCheck, "王手千日手", true},
    {TerminalType::DeclarationWin, "入玉宣言勝ち", false},
    {TerminalType::DeclarationWin, "入玉勝ち", true},
    {TerminalType::DeclarationWin, "宣言勝ち", false},
    {TerminalType::Resign, "投了", true},
    {TerminalType::Interrupt, "中断", true},
    {TerminalType::Impasse, "持将棋", true},
    {TerminalType::Repetition, "千日手", true},
    {TerminalType::Timeout, "時間切れ", true},
    {TerminalType::Timeout, "切れ負け", false},
    {TerminalType::IllegalWin, "反則勝ち", true},
    {TerminalType::IllegalLoss, "反則負け", true},
    {TerminalType::ForfeitWin, "不戦勝", true},
    {TerminalType::Forfeit, "不戦敗", true},
    {TerminalType::NoCheckmate, "不詰", true},
    {TerminalType::Checkmate, "詰み", true},
    {TerminalType::Checkmate, "詰", false},
    {TerminalType::Draw, "引き分け", true},
    {TerminalType::MaxMoves, "最大手数到達", true},
    {TerminalType::MaxMoves, "最大手数", false},
    {TerminalType::Error, "エラー", true},
}};

// 表示の訳（lupdate が拾えるよう、訳す語をここに並べる）
const char* translatableLabel(TerminalType type)
{
    switch (type) {
    case TerminalType::None: return nullptr;
    case TerminalType::Resign: return QT_TRANSLATE_NOOP("TerminalType", "投了");
    case TerminalType::Checkmate: return QT_TRANSLATE_NOOP("TerminalType", "詰み");
    case TerminalType::Repetition: return QT_TRANSLATE_NOOP("TerminalType", "千日手");
    case TerminalType::PerpetualCheck: return QT_TRANSLATE_NOOP("TerminalType", "王手千日手");
    case TerminalType::Impasse: return QT_TRANSLATE_NOOP("TerminalType", "持将棋");
    case TerminalType::Timeout: return QT_TRANSLATE_NOOP("TerminalType", "時間切れ");
    case TerminalType::IllegalWin: return QT_TRANSLATE_NOOP("TerminalType", "反則勝ち");
    case TerminalType::IllegalLoss: return QT_TRANSLATE_NOOP("TerminalType", "反則負け");
    case TerminalType::Forfeit: return QT_TRANSLATE_NOOP("TerminalType", "不戦敗");
    case TerminalType::Interrupt: return QT_TRANSLATE_NOOP("TerminalType", "中断");
    case TerminalType::NoCheckmate: return QT_TRANSLATE_NOOP("TerminalType", "不詰");
    case TerminalType::DeclarationWin: return QT_TRANSLATE_NOOP("TerminalType", "入玉勝ち");
    case TerminalType::Draw: return QT_TRANSLATE_NOOP("TerminalType", "引き分け");
    case TerminalType::MaxMoves: return QT_TRANSLATE_NOOP("TerminalType", "最大手数到達");
    case TerminalType::Error: return QT_TRANSLATE_NOOP("TerminalType", "エラー");
    case TerminalType::ForfeitWin: return QT_TRANSLATE_NOOP("TerminalType", "不戦勝");
    }
    return nullptr;
}

} // namespace

namespace TerminalTypes {

QString label(TerminalType type)
{
    for (const Word& word : kWords) {
        if (word.type == type && word.canonical) return QString::fromUtf8(word.text);
    }
    return {};
}

QString displayLabel(TerminalType type)
{
    const char* source = translatableLabel(type);
    return source ? QCoreApplication::translate("TerminalType", source) : QString();
}

TerminalType fromWord(QStringView word)
{
    const QStringView trimmed = word.trimmed();
    for (const Word& entry : kWords) {
        if (trimmed == QString::fromUtf8(entry.text)) return entry.type;
    }
    return TerminalType::None;
}

TerminalType findWord(QStringView text, QString* matched)
{
    for (const Word& entry : kWords) {
        const QString candidate = QString::fromUtf8(entry.text);
        if (text.contains(candidate)) {
            if (matched) *matched = candidate;
            return entry.type;
        }
    }
    return TerminalType::None;
}

} // namespace TerminalTypes
