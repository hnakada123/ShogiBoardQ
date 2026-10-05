#include "kifupresentation.h"
#include "sfenpositiontracer.h"

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

// Candidate origins use movement geometry and intervening pieces. Including an
// extra origin for a pinned candidate is harmless and keeps notation unambiguous.
bool reaches(const SfenPositionTracer& board, const QString& token,
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
        if (!board.tokenAtFileRank(f, QChar('a' + r)).isEmpty()) return false;
    }
    return true;
}

QString format(const SfenPositionTracer& board, const QString& usi, Options style, bool side)
{
    static const QRegularExpression valid(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    if (!valid.match(usi).hasMatch()) return status(usi);
    const bool drop = usi.at(1) == QLatin1Char('*');
    const int df = usi.at(2).digitValue(), dr = usi.at(3).unicode() - 'a';
    const QString token = drop ? QString(usi.at(0)) : board.tokenAtFileRank(usi.at(0).digitValue(), usi.at(1));
    if (token.isEmpty()) return usi; // Incomplete input: never invent a piece.
    const QString piece = token.toUpper();
    QString result = side ? (board.blackToMove() ? QStringLiteral("▲") : QStringLiteral("△")) : QString();
    if (style.notation == Notation::Japanese) {
        const QStringList names = {QStringLiteral("歩"), QStringLiteral("香"), QStringLiteral("桂"), QStringLiteral("銀"), QStringLiteral("金"), QStringLiteral("角"), QStringLiteral("飛"), QStringLiteral("玉")};
        const QStringList promoted = {QStringLiteral("と"), QStringLiteral("成香"), QStringLiteral("成桂"), QStringLiteral("成銀"), QStringLiteral("金"), QStringLiteral("馬"), QStringLiteral("龍"), QStringLiteral("玉")};
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
    bool origin = style.alwaysOrigin;
    for (int f = 1; !origin && f <= 9; ++f) {
        for (int r = 0; !origin && r < 9; ++r) {
            if (f == usi.at(0).digitValue() && r == usi.at(1).unicode() - 'a') continue;
            if (board.tokenAtFileRank(f, QChar('a' + r)) == token && reaches(board, token, f, r, df, dr)) origin = true;
        }
    }
    if (origin) result += usi.left(2);
    result += board.tokenAtFileRank(df, usi.at(3)).isEmpty() ? QLatin1Char('-') : QLatin1Char('x');
    result += usi.mid(2, 2);
    if (usi.endsWith('+')) result += QLatin1Char('+');
    else if (!piece.startsWith('+') && QStringLiteral("PLNSBR").contains(piece)) {
        const int fromRank = usi.at(1).unicode() - 'a';
        if (board.blackToMove() ? (fromRank <= 2 || dr <= 2) : (fromRank >= 6 || dr >= 6)) result += QLatin1Char('=');
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
    const auto square = [](const QPoint& p) { return QString::number(p.x() + 1) + QChar('a' + p.y()); };
    if (move.fromSquare.x() >= 9)
        return QString(pieceToChar(demote(move.movingPiece)).toUpper()) + QLatin1Char('*') + square(move.toSquare);
    return square(move.fromSquare) + square(move.toSquare) + (move.isPromotion ? QStringLiteral("+") : QString());
}

QString move(const QString& beforeSfen, const QString& usi, Options style, bool includeSide)
{
    SfenPositionTracer board;
    if (beforeSfen.isEmpty() || !board.setFromSfen(beforeSfen)) return usi;
    return format(board, usi, style, includeSide);
}

QString pv(const QString& beforeSfen, const QString& usiMoves, const QString& japanese, bool fullOrigin)
{
    if (current.notation == Notation::Japanese && !japanese.isEmpty()) return japanese;
    SfenPositionTracer board;
    if (beforeSfen.isEmpty() || !board.setFromSfen(beforeSfen)) return usiMoves.isEmpty() ? status(japanese) : usiMoves;
    Options style = current;
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    QStringList result;
    const auto moves = usiMoves.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const auto& usi : moves) {
        result.append(format(board, usi, style, true));
        if (!board.applyUsiMove(usi)) break;
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

QString status(const QString& canonical)
{
    static const char* const names[] = {
        QT_TRANSLATE_NOOP("KifuPresentation", "投了"), QT_TRANSLATE_NOOP("KifuPresentation", "詰み"),
        QT_TRANSLATE_NOOP("KifuPresentation", "千日手"), QT_TRANSLATE_NOOP("KifuPresentation", "持将棋"),
        QT_TRANSLATE_NOOP("KifuPresentation", "切れ負け"), QT_TRANSLATE_NOOP("KifuPresentation", "反則勝ち"),
        QT_TRANSLATE_NOOP("KifuPresentation", "反則負け"), QT_TRANSLATE_NOOP("KifuPresentation", "不戦勝"),
        QT_TRANSLATE_NOOP("KifuPresentation", "不戦敗"), QT_TRANSLATE_NOOP("KifuPresentation", "中断"),
        QT_TRANSLATE_NOOP("KifuPresentation", "不詰"), QT_TRANSLATE_NOOP("KifuPresentation", "入玉勝ち"),
        QT_TRANSLATE_NOOP("KifuPresentation", "引き分け"), QT_TRANSLATE_NOOP("KifuPresentation", "最大手数"),
        QT_TRANSLATE_NOOP("KifuPresentation", "エラー"), QT_TRANSLATE_NOOP("KifuPresentation", "（定跡）")};
    // Only a whole generated label (with optional move number/side), never free text.
    static const QRegularExpression prefix(QStringLiteral("^(\\s*[0-9]+\\s+)?([▲△☗☖]\\s*)?"));
    const auto match = prefix.match(canonical);
    QString tail = canonical.mid(match.capturedLength()).trimmed();
    const bool branch = tail.endsWith(QLatin1Char('+'));
    if (branch) tail.chop(1);
    const QString original = tail;
    if (tail == QStringLiteral("時間切れ")) tail = QStringLiteral("切れ負け");
    if (tail == QStringLiteral("最大手数到達")) tail = QStringLiteral("最大手数");
    if (tail == QStringLiteral("宣言勝ち") || tail == QStringLiteral("入玉宣言勝ち")) tail = QStringLiteral("入玉勝ち");
    if (tail == QStringLiteral("詰")) tail = QStringLiteral("詰み");
    for (const char* name : names) {
        if (tail != QString::fromUtf8(name)) continue;
        const QString text = translated(name);
        return match.captured() + (text == tail ? original : text)
            + (branch ? (current.notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+")) : QString());
    }
    return canonical;
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
        static const char* const handicaps[] = {
            QT_TRANSLATE_NOOP("KifuPresentation", "平手"), QT_TRANSLATE_NOOP("KifuPresentation", "香落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "右香落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "角落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "飛車落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "飛香落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "二枚落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "四枚落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "三枚落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "五枚落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "左五枚落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "局面指定"),
            QT_TRANSLATE_NOOP("KifuPresentation", "六枚落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "八枚落ち"),
            QT_TRANSLATE_NOOP("KifuPresentation", "十枚落ち"), QT_TRANSLATE_NOOP("KifuPresentation", "その他")};
        for (const char* value : handicaps) if (raw == QString::fromUtf8(value)) return translated(value);
    }
    if (key == QStringLiteral("持ち時間") && raw == QStringLiteral("無制限"))
        return translated(QT_TRANSLATE_NOOP("KifuPresentation", "無制限"));
    if (key == QStringLiteral("持ち時間")) {
        // フィッシャー加算の書式（KifuExportMetadataBuilder::timeControlText）を表示言語に合わせる
        static const QRegularExpression increment(QStringLiteral("^(\\d+:\\d{2})\\+(\\d+)秒加算$"));
        const QRegularExpressionMatch m = increment.match(raw);
        if (m.hasMatch())
            return translated(QT_TRANSLATE_NOOP("KifuPresentation", "%1+%2秒加算")).arg(m.captured(1), m.captured(2));
    }
    return raw;
}
}
