/// @file ki2lexer.cpp
/// @brief KI2形式固有の字句解析の実装

#include "ki2lexer.h"
#include "parsecommon.h"
#include "shoginotation.h"

#include <QRegularExpression>

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
    return !t.isEmpty() && ShogiNotation::isTurnMark(t.at(0));
}

// === 結果行解析 ===

std::optional<ResultLine> parseResultLine(const QString& line)
{
    const QRegularExpressionMatch m = resultPatternCaptureRe().match(line);
    if (!m.hasMatch()) return std::nullopt;
    ResultLine result;
    result.moveCount = ShogiNotation::numberFromDigits(m.captured(1));

    const QString suffix = line.mid(m.capturedEnd()).trimmed();
    if (!suffix.startsWith(QChar(u'で')) || suffix.mid(1).trimmed().isEmpty()) return std::nullopt;
    const QString reason = suffix.mid(1).trimmed();
    QString word = reason;
    static const QRegularExpression namedRe(QStringLiteral("^(先手|下手|後手|上手)の"));
    const QRegularExpressionMatch named = namedRe.match(reason);
    if (named.hasMatch()) {
        const QString who = named.captured(1);
        result.named = (who == QStringLiteral("先手") || who == QStringLiteral("下手")) ? Turn::Black : Turn::White;
        word = reason.mid(named.capturedEnd());
    }
    result.terminal = TerminalTypes::fromWord(word);
    if (result.terminal == TerminalType::None && result.named && word == QStringLiteral("勝ち"))
        result.terminal = TerminalType::Resign;
    static const QRegularExpression timeoutRe(QStringLiteral("^時間切れにより(先手|下手|後手|上手)の勝ち$"));
    if (const auto timeout = timeoutRe.match(reason); timeout.hasMatch()) {
        result.terminal = TerminalType::Timeout;
        const QString who = timeout.captured(1);
        result.named = (who == QStringLiteral("先手") || who == QStringLiteral("下手")) ? Turn::Black : Turn::White;
    }
    if (result.terminal == TerminalType::None) {
        // 「までN手」だけなら未終局。「で」に続く独自の終局理由は捨てない。
        result.terminal = TerminalType::Custom;
        result.terminalText = reason;
        result.named.reset();
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
    case TerminalType::Try:
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
