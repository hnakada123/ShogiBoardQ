#include "kifupresentation.h"
#include "handicaps.h"
#include "sfenreplay.h"
#include "shogiposition.h"
#include "terminaltype.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>
#include <cmath>

namespace KifuPresentation {
namespace {
Options current;
QString translated(const char* source)
{
    return QCoreApplication::translate("KifuPresentation", source);
}

/// マスの駒を SFEN のトークン（成駒は "+P" の形）にする。file: 1..9、row: 0..8（範囲外・空きは空）
QString tokenAt(const Position& board, int file, int row)
{
    const Piece piece = board.pieceAt(BoardSquare{file, row + 1});
    if (piece == Piece::None) return {};
    return pieceSfen(piece);
}

QString tokenAtFileRank(const Position& board, int file, QChar rankLetter)
{
    return tokenAt(board, file, rankLetter.toLower().unicode() - u'a');
}

// Candidate origins use movement geometry and intervening pieces. Including an
// extra origin for a pinned candidate is harmless and keeps notation unambiguous.
bool reaches(const Position& board, const QString& token,
             int file, int rank, int destFile, int destRank)
{
    const int dx = destFile - file;
    const int dy = (destRank - rank) * (token.back().isUpper() ? 1 : -1);
    const int ax = std::abs(dx), ay = std::abs(dy);
    const QChar kind = token.back().toUpper();
    bool slide = false, valid = false;
    if (kind == QLatin1Char('K')) valid = qMax(ax, ay) == 1;
    else if (kind == QLatin1Char('G') || (token.startsWith('+') && QStringLiteral("PLNS").contains(kind)))
        valid = (dy == -1 && ax <= 1) || (dy == 0 && ax == 1) || (dy == 1 && ax == 0);
    else if (kind == QLatin1Char('S')) valid = (dy == -1 && ax <= 1) || (dy == 1 && ax == 1);
    else if (kind == QLatin1Char('N')) valid = dy == -2 && ax == 1;
    else if (kind == QLatin1Char('P')) valid = dy == -1 && ax == 0;
    else if (kind == QLatin1Char('L')) slide = valid = dy < 0 && ax == 0;
    else if (kind == QLatin1Char('B')) {
        slide = valid = ax == ay && ax > 0;
        valid = valid || (token.startsWith('+') && ax + ay == 1);
    } else if (kind == QLatin1Char('R')) {
        slide = valid = (ax == 0) != (ay == 0);
        valid = valid || (token.startsWith('+') && ax == 1 && ay == 1);
    }
    if (!valid || !slide) return valid;
    const int sx = (dx > 0) - (dx < 0);
    const int rawDy = destRank - rank;
    const int sy = (rawDy > 0) - (rawDy < 0);
    for (int f = file + sx, r = rank + sy; f != destFile || r != destRank; f += sx, r += sy) {
        if (!tokenAt(board, f, r).isEmpty()) return false;
    }
    return true;
}

// USI の指し手の形（"7g7f"・"7g7f+"・"P*5e"）か。正規表現
// ^(?:[1-9][a-i][1-9][a-i]\+?|[PLNSGBR]\*[1-9][a-i])$ と同じ判定を、読み筋の1手ごとに軽く行う
// （$ は末尾の改行1つの前でも一致するので、それも同じく許す）。
bool isUsiMove(QStringView usi)
{
    if (usi.endsWith(u'\n')) usi.chop(1);
    const auto file = [](QChar c) { return c.unicode() >= u'1' && c.unicode() <= u'9'; };
    const auto rank = [](QChar c) { return c.unicode() >= u'a' && c.unicode() <= u'i'; };
    if (usi.size() == 4 && usi.at(1) == u'*')
        return QStringView(u"PLNSGBR").contains(usi.at(0)) && file(usi.at(2)) && rank(usi.at(3));
    if (usi.size() != 4 && !(usi.size() == 5 && usi.at(4) == u'+')) return false;
    return file(usi.at(0)) && rank(usi.at(1)) && file(usi.at(2)) && rank(usi.at(3));
}

QString format(const Position& board, const QString& usi, Options style, bool side)
{
    if (!isUsiMove(usi)) return status(usi);
    const bool drop = usi.at(1) == QLatin1Char('*');
    const int df = usi.at(2).digitValue(), dr = usi.at(3).unicode() - 'a';
    const QString token = drop ? QString(usi.at(0)) : tokenAtFileRank(board, usi.at(0).digitValue(), usi.at(1));
    if (token.isEmpty()) return usi; // Incomplete input: never invent a piece.
    const QString piece = token.toUpper();
    QString result = side ? (board.sideToMove() == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△")) : QString();
    if (style.notation == Notation::Japanese) {
        static const QStringList names = {QStringLiteral("歩"), QStringLiteral("香"), QStringLiteral("桂"), QStringLiteral("銀"), QStringLiteral("金"), QStringLiteral("角"), QStringLiteral("飛"), QStringLiteral("玉")};
        static const QStringList promoted = {QStringLiteral("と"), QStringLiteral("成香"), QStringLiteral("成桂"), QStringLiteral("成銀"), QStringLiteral("金"), QStringLiteral("馬"), QStringLiteral("龍"), QStringLiteral("玉")};
        const qsizetype index = QStringLiteral("PLNSGBRK").indexOf(piece.back());
        if (index < 0) return usi;
        result += QChar(0xff10 + df);
        result += QStringLiteral("一二三四五六七八九").at(dr);
        result += token.startsWith('+') ? promoted.at(index) : names.at(index);
        if (drop) result += QStringLiteral("打");
        else {
            if (usi.endsWith('+')) result += QStringLiteral("成");
            result += QStringLiteral("(%1%2)").arg(usi.at(0)).arg(usi.at(1).unicode() - 'a' + 1);
        }
        return result;
    }
    result += piece;
    if (drop) return result + QLatin1Char('*') + usi.mid(2, 2);
    // 同じ駒で同じマスへ行ける駒がほかにあれば、移動元を付ける。盤全体を指し手ごとに調べるので、
    // マスの駒は写さずに比べる
    bool origin = style.alwaysOrigin;
    const int fromFile = usi.at(0).digitValue();
    const int fromRow = usi.at(1).unicode() - 'a';
    for (int f = 1; !origin && f <= 9; ++f) {
        for (int r = 0; !origin && r < 9; ++r) {
            if (f == fromFile && r == fromRow) continue;
            if (tokenAt(board, f, r) == token && reaches(board, token, f, r, df, dr)) origin = true;
        }
    }
    if (origin) result += usi.left(2);
    result += tokenAtFileRank(board, df, usi.at(3)).isEmpty() ? QLatin1Char('-') : QLatin1Char('x');
    result += usi.mid(2, 2);
    if (usi.endsWith('+')) result += QLatin1Char('+');
    else if (!piece.startsWith('+') && QStringLiteral("PLNSBR").contains(piece)) {
        const int fromRank = usi.at(1).unicode() - 'a';
        if (board.sideToMove() == Turn::Black ? (fromRank <= 2 || dr <= 2) : (fromRank >= 6 || dr >= 6)) result += QLatin1Char('=');
    }
    return result;
}
}

QString resolveLanguage(const QString& setting, const QString& systemLocale)
{
    const QString requested = setting == QStringLiteral("system") ? systemLocale : setting;
    const QLocale locale(requested);
    if (locale.language() == QLocale::Japanese) return QStringLiteral("ja_JP");
    if (locale.language() == QLocale::Chinese)
        return locale.script() == QLocale::TraditionalHanScript ? QStringLiteral("zh_TW") : QStringLiteral("zh_CN");
    return QStringLiteral("en");
}

void configure(const QString& language, const QString& notation, bool alwaysOrigin)
{
    current.notation = notation == QStringLiteral("western") ||
        (notation != QStringLiteral("japanese") && language == QStringLiteral("en"))
        ? Notation::Western : Notation::Japanese;
    current.alwaysOrigin = alwaysOrigin;
}

Options options() { return current; }
QString rankLabel(int rank)
{
    if (rank < 1 || rank > 9) return {};
    return current.notation == Notation::Western ? QString(QChar('a' + rank - 1))
        : QString(QStringLiteral("一二三四五六七八九").at(rank - 1));
}

QString usiMove(const ShogiMove& move)
{
    if (move.movingPiece == Piece::None) return {};
    return move.move.toString();
}

QString move(const QString& beforeSfen, const QString& usi, Options style, bool includeSide)
{
    Position board;
    if (beforeSfen.isEmpty() || !board.assignSfen(beforeSfen)) return usi;
    return format(board, usi, style, includeSide);
}

QString pv(const QString& beforeSfen, const QString& usiMoves, const QString& japanese, bool fullOrigin)
{
    if (current.notation == Notation::Japanese && !japanese.isEmpty()) return japanese;
    Position board;
    if (beforeSfen.isEmpty() || !board.assignSfen(beforeSfen)) return usiMoves.isEmpty() ? status(japanese) : usiMoves;
    Options style = current;
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    QStringList result;
    const auto moves = usiMoves.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const auto& usi : moves) {
        result.append(format(board, usi, style, true));
        if (!SfenReplay::applyLenient(board, usi)) break;
    }
    return result.isEmpty() ? status(japanese) : result.join(QLatin1Char(' '));
}

QString label(const QString& canonical, const QString& beforeSfen, const QString& usi, bool fullOrigin)
{
    if (current.notation == Notation::Japanese || usi.isEmpty()) return status(canonical);
    Options style = current;
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    static const QRegularExpression number(QStringLiteral("^\\s*[0-9]+\\s+"));
    const QString prefix = number.match(canonical).captured();
    return prefix + move(beforeSfen, usi, style)
        + (canonical.trimmed().endsWith(QLatin1Char('+')) ? QStringLiteral(" [+]") : QString());
}

QString moveLabel(const QString& canonical, const QString& beforeSfen, const QString& usi, bool fullOrigin)
{
    if (current.notation == Notation::Japanese || usi.isEmpty()) return status(canonical);
    Options style = current;
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    return move(beforeSfen, usi, style);
}

QString branchMark()
{
    return current.notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+");
}

QString status(const QString& canonical)
{

    // Only a whole generated label (with optional move number/side), never free text.
    static const QRegularExpression prefix(QStringLiteral("^(\\s*[0-9]+\\s+)?([▲△☗☖]\\s*)?"));
    const auto match = prefix.match(canonical);
    QString tail = canonical.mid(match.capturedLength()).trimmed();
    const bool branch = tail.endsWith(QLatin1Char('+'));
    if (branch) tail.chop(1);
    QString text;
    if (const TerminalType type = TerminalTypes::fromWord(tail); type != TerminalType::None) {
        // 訳がなければ（日本語表示）、棋譜に書いてある語をそのまま出す
        text = TerminalTypes::displayLabel(type);
        if (text == TerminalTypes::label(type)) text = tail;
    } else if (tail == QStringLiteral("（定跡）")) {
        text = translated(QT_TRANSLATE_NOOP("KifuPresentation", "（定跡）"));
    } else {
        return canonical;
    }
    return match.captured() + text
        + (branch ? (current.notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+")) : QString());
}

QString infoKey(const QString& canonical)
{
    static const char* const keys[] = {
        QT_TRANSLATE_NOOP("KifuPresentation", "対局日"), QT_TRANSLATE_NOOP("KifuPresentation", "開始日時"),
        QT_TRANSLATE_NOOP("KifuPresentation", "終了日時"), QT_TRANSLATE_NOOP("KifuPresentation", "先手"),
        QT_TRANSLATE_NOOP("KifuPresentation", "後手"), QT_TRANSLATE_NOOP("KifuPresentation", "下手"),
        QT_TRANSLATE_NOOP("KifuPresentation", "上手"), QT_TRANSLATE_NOOP("KifuPresentation", "手合割"),
        QT_TRANSLATE_NOOP("KifuPresentation", "持ち時間"), QT_TRANSLATE_NOOP("KifuPresentation", "棋戦"),
        QT_TRANSLATE_NOOP("KifuPresentation", "場所"), QT_TRANSLATE_NOOP("KifuPresentation", "備考"),
        QT_TRANSLATE_NOOP("KifuPresentation", "戦型"), QT_TRANSLATE_NOOP("KifuPresentation", "表題")};
    for (const char* key : keys) if (canonical == QString::fromUtf8(key)) return translated(key);
    return canonical;
}

QString infoValue(const QString& key, const QString& raw)
{
    if (key == QStringLiteral("手合割")) {
        // 手合割の名前は core の表（Handicaps::kAll）と同じ語。表にない「局面指定」「その他」だけここに書く
        for (const Handicaps::Handicap& handicap : Handicaps::kAll)
            if (raw == Handicaps::labelOf(handicap)) return translated(handicap.label);
        static const char* const others[] = {
            QT_TRANSLATE_NOOP("KifuPresentation", "局面指定"), QT_TRANSLATE_NOOP("KifuPresentation", "その他")};
        for (const char* value : others) if (raw == QString::fromUtf8(value)) return translated(value);
    }
    if (key == QStringLiteral("持ち時間") && raw == QStringLiteral("無制限"))
        return translated(QT_TRANSLATE_NOOP("KifuPresentation", "無制限"));
    if (key == QStringLiteral("持ち時間")) {
        // 先後で違う持ち時間（"先手 mm:ss+秒 / 後手 mm:ss+秒"、駒落ちは下手・上手）は、見出しと各側を訳す
        static const QRegularExpression perSide(QStringLiteral("^(先手|下手) (.+) / (後手|上手) (.+)$"));
        const QRegularExpressionMatch side = perSide.match(raw);
        if (side.hasMatch())
            return QStringLiteral("%1 %2 / %3 %4").arg(infoKey(side.captured(1)), infoValue(key, side.captured(2)),
                                                       infoKey(side.captured(3)), infoValue(key, side.captured(4)));
        // フィッシャー加算の書式（KifuExportMetadataBuilder::timeControlText）を表示言語に合わせる
        static const QRegularExpression increment(QStringLiteral("^(\\d+:\\d{2})\\+(\\d+)秒加算$"));
        const QRegularExpressionMatch m = increment.match(raw);
        if (m.hasMatch())
            return translated(QT_TRANSLATE_NOOP("KifuPresentation", "%1+%2秒加算")).arg(m.captured(1), m.captured(2));
    }
    return raw;
}
}
