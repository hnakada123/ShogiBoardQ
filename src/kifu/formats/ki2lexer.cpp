/// @file ki2lexer.cpp
/// @brief KI2形式固有の字句解析の実装

#include "ki2lexer.h"
#include "parsecommon.h"
#include "shoginotation.h"

#include <QRegularExpression>

namespace {

bool isTurnMark(QChar ch)
{
    return ch == QChar(u'▲') || ch == QChar(u'△') || ch == QChar(u'▽')
        || ch == QChar(u'☗') || ch == QChar(u'☖');
}

} // namespace

namespace Ki2Lexer {

// === 正規表現アクセサ ===

const QRegularExpression& resultPatternCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで([0-9０-９]+)手"));
        return &r;
    }();
    return re;
}

// === 行の種類判定 ===

bool isKi2MoveLine(const QString& line)
{
    const QString t = line.trimmed();
    return !t.isEmpty() && isTurnMark(t.at(0));
}

// === 結果行解析 ===

std::optional<ResultLine> parseResultLine(const QString& line)
{
    const QRegularExpressionMatch m = resultPatternCaptureRe().match(line);
    if (!m.hasMatch()) return std::nullopt;
    ResultLine result;
    result.moveCount = ShogiNotation::numberFromDigits(m.captured(1));

    // 「まで77手で千日手」などは終局語の表で読む。「先手の勝ち」だけの行は投了とみなす
    result.terminal = TerminalTypes::findWord(line);
    if (result.terminal == TerminalType::None && line.contains(QChar(u'勝'))) result.terminal = TerminalType::Resign;
    if (result.terminal == TerminalType::None) return std::nullopt;

    static const QRegularExpression namedRe(QStringLiteral("(先手|下手|後手|上手)の"));
    if (const QRegularExpressionMatch named = namedRe.match(line, m.capturedEnd(0)); named.hasMatch()) {
        const QString who = named.captured(1);
        result.named = (who == QStringLiteral("先手") || who == QStringLiteral("下手")) ? Turn::Black : Turn::White;
    }
    return result;
}

Turn ResultLine::endingSide(Turn sideToMove) const
{
    if (!named) return sideToMove;
    const Turn other = *named == Turn::Black ? Turn::White : Turn::Black;
    switch (terminal) {
    // 「先手の勝ち」「時間切れにより先手の勝ち」は勝った側を名指しする。記録するのは負けた側
    case TerminalType::Resign:
    case TerminalType::Timeout:
        return other;
    // 反則・入玉宣言・不戦の行は、記録した側をそのまま名指しする
    case TerminalType::IllegalWin:
    case TerminalType::IllegalLoss:
    case TerminalType::DeclarationWin:
    case TerminalType::ForfeitWin:
    case TerminalType::Forfeit:
        return *named;
    default:
        return sideToMove;
    }
}

// === 指し手テキスト解析 ===

QStringList extractMovesFromLine(const QString& line)
{
    QStringList moves;
    const QString t = line.trimmed();
    static const QRegularExpression moveRe(QStringLiteral("([▲△▽☗☖][^▲△▽☗☖]+)"));
    QRegularExpressionMatchIterator it = moveRe.globalMatch(t);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString move = m.captured(1).trimmed();
        qsizetype commentIdx = move.indexOf(QChar(u'*'));
        if (commentIdx < 0) commentIdx = move.indexOf(QChar(u'＊'));
        if (commentIdx >= 0) move = move.left(commentIdx).trimmed();
        if (!move.isEmpty()) moves << move;
    }
    return moves;
}

} // namespace Ki2Lexer
