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

bool isBlank(QChar ch)
{
    return ch.isSpace() || ch == QChar(u'　');
}

/// 半角・全角の数字（1〜9）。それ以外は0
int digitValue(QChar ch)
{
    const char16_t u = ch.unicode();
    if (u >= u'1' && u <= u'9') return u - u'0';
    return ShogiNotation::fileFromZenkaku(ch);
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

std::optional<MoveText> parseMoveText(QStringView text)
{
    qsizetype i = 0;
    const auto skipBlanks = [&text, &i] {
        while (i < text.size() && isBlank(text.at(i))) ++i;
    };
    skipBlanks();
    if (i < text.size() && isTurnMark(text.at(i))) ++i;
    skipBlanks();

    MoveText move;
    if (i < text.size() && text.at(i) == QChar(u'同')) {
        move.sameAsPrevious = true;
        ++i;
    } else {
        if (i + 1 >= text.size()) return std::nullopt;
        const int file = digitValue(text.at(i));
        int rank = ShogiNotation::rankFromKanji(text.at(i + 1));
        if (rank == 0) rank = digitValue(text.at(i + 1));
        if (file == 0 || rank == 0) return std::nullopt;
        move.to = BoardSquare{file, rank};
        i += 2;
        skipBlanks();
        // 新聞などの「７六同歩」は、移動先と「同」を両方書く
        if (i < text.size() && text.at(i) == QChar(u'同')) ++i;
    }
    skipBlanks();

    // 駒の名前（成香・成桂・成銀だけ2文字）
    if (i + 1 < text.size() && text.at(i) == QChar(u'成')) {
        move.piece = ShogiNotation::pieceFromName(text.mid(i, 2));
        i += 2;
    } else if (i < text.size()) {
        move.piece = ShogiNotation::pieceFromName(text.mid(i, 1));
        ++i;
    }
    if (move.piece == Piece::None) return std::nullopt;

    static constexpr QStringView kModifiers = u"右左直上引寄";
    for (; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QChar(u'不') && i + 1 < text.size() && text.at(i + 1) == QChar(u'成')) {
            move.promote = false;
            ++i;
        } else if (ch == QChar(u'生')) {
            move.promote = false;
        } else if (ch == QChar(u'成')) {
            move.promote = true;
        } else if (ch == QChar(u'打')) {
            move.drop = true;
        } else if (ch == QChar(u'行')) {
            move.modifiers += QChar(u'上');
        } else if (kModifiers.contains(ch)) {
            move.modifiers += ch;
        }
    }
    return move;
}

} // namespace Ki2Lexer
