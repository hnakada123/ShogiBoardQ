#include "kifupresentation.h"
#include "gameinfokeys.h"
#include "handicaps.h"
#include "ki2movenotation.h"
#include "kifmovetext.h"
#include "shoginotation.h"
#include "sfenreplay.h"
#include "shogiposition.h"
#include "terminaltype.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace KifuPresentation {
namespace {
QString translated(const char* source)
{
    return QCoreApplication::translate("KifuPresentation", source);
}

/// 局面の上の1手の表記。読めない手は status() に任せ、駒のない移動元からは駒を作らない
QString format(const Position& board, const QString& usi, Options style, bool side)
{
    const std::optional<UsiMove> move = UsiMove::parse(usi.trimmed());
    if (!move) return status(usi);
    const Turn mover = board.sideToMove();
    const Piece moving = move->isDrop() ? move->dropPieceFor(mover) : board.pieceAt(move->from);
    if (moving == Piece::None) return usi; // Incomplete input: never invent a piece.

    if (style.notation == Notation::Japanese) {
        const QString text = KifMoveText::format(mover, moving, *move);
        return side ? text : text.mid(1);
    }

    QString result = side ? QString(QChar(mover == Turn::Black ? u'▲' : u'△')) : QString();
    result += pieceSfen(toBlack(moving));
    const QString text = move->toString();
    const QStringView to = QStringView(text).mid(2, 2);
    if (move->isDrop()) {
        result += QLatin1Char('*');
        result += to;
        return result;
    }
    // 同じ駒で同じマスへ動ける駒がほかにあれば移動元を付ける（KI2 の修飾語と同じ判定）
    if (style.alwaysOrigin || Ki2MoveNotation::annotate(board, mover, *move).sources.size() > 1)
        result += QStringView(text).left(2);
    result += board.pieceAt(move->to) == Piece::None ? QLatin1Char('-') : QLatin1Char('x');
    result += to;
    if (move->promote) result += QLatin1Char('+');
    else if (KifMoveText::couldPromote(moving, *move)) result += QLatin1Char('=');
    return result;
}
} // namespace

QString rankLabel(int rank)
{
    if (rank < 1 || rank > 9) return {};
    return options().notation == Notation::Western ? QString(QChar('a' + rank - 1))
                                                   : ShogiNotation::rankKanji(rank);
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
    if (options().notation == Notation::Japanese && !japanese.isEmpty()) return japanese;
    Position board;
    if (beforeSfen.isEmpty() || !board.assignSfen(beforeSfen)) return usiMoves.isEmpty() ? status(japanese) : usiMoves;
    Options style = options();
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
    if (options().notation == Notation::Japanese || usi.isEmpty()) return status(canonical);
    Options style = options();
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    static const QRegularExpression number(QStringLiteral("^\\s*[0-9]+\\s+"));
    const QString prefix = number.match(canonical).captured();
    return prefix + move(beforeSfen, usi, style)
        + (canonical.trimmed().endsWith(QLatin1Char('+')) ? QStringLiteral(" [+]") : QString());
}

QString moveLabel(const QString& canonical, const QString& beforeSfen, const QString& usi, bool fullOrigin)
{
    if (options().notation == Notation::Japanese || usi.isEmpty()) return status(canonical);
    Options style = options();
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    return move(beforeSfen, usi, style);
}

QString branchMark()
{
    return options().notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+");
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
        + (branch ? (options().notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+")) : QString());
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
    if (key == GameInfoKeys::kHandicap) {
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
