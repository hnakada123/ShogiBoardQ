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

const QRegularExpression& resultPatternRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで[0-9０-９]+手"));
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

bool isResultLine(const QString& line)
{
    return resultPatternRe().match(line).hasMatch();
}

// === 結果行解析 ===

bool parseResultLine(const QString& line, TerminalType& terminal, int& moveCount)
{
    const QRegularExpressionMatch m = resultPatternCaptureRe().match(line);
    if (!m.hasMatch()) return false;
    moveCount = ShogiNotation::numberFromDigits(m.captured(1));

    // 「まで77手で千日手」などは終局語の表で読む。「先手の勝ち」だけの行は投了とみなす
    terminal = TerminalTypes::findWord(line);
    if (terminal == TerminalType::None && line.contains(QChar(u'勝'))) terminal = TerminalType::Resign;
    return terminal != TerminalType::None;
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
