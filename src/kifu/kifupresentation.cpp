#include "kifupresentation.h"
#include "gameinfokeys.h"
#include "handicaps.h"
#include "ki2movenotation.h"
#include "kifmovetext.h"
#include "pvtext.h"
#include "recordentry.h"
#include "shoginotation.h"
#include "shogiposition.h"
#include "terminaltype.h"
#include "timecontroltext.h"

#include <QCoreApplication>

namespace KifuPresentation {
namespace {
QString translated(const char* source)
{
    return QCoreApplication::translate("KifuPresentation", source);
}

/// 英語表記の1手（board は指す前の局面、moving は動かす駒）
QString western(const Position& board, const UsiMove& move, Piece moving, Options style, bool side)
{
    const Turn mover = board.sideToMove();
    QString result = side ? QString(QChar(mover == Turn::Black ? u'▲' : u'△')) : QString();
    result += pieceSfen(toBlack(moving));
    const QString text = move.toString();
    const QStringView to = QStringView(text).mid(2, 2);
    if (move.isDrop()) {
        result += QLatin1Char('*');
        result += to;
        return result;
    }
    // 同じ駒で同じマスへ動ける駒がほかにあれば移動元を付ける（KI2 の修飾語と同じ判定）
    if (style.alwaysOrigin || Ki2MoveNotation::annotate(board, mover, move).sources.size() > 1)
        result += QStringView(text).left(2);
    result += board.pieceAt(move.to) == Piece::None ? QLatin1Char('-') : QLatin1Char('x');
    result += to;
    if (move.promote) result += QLatin1Char('+');
    else if (KifMoveText::couldPromote(moving, move)) result += QLatin1Char('=');
    return result;
}

/// 局面の上の1手の表記。読めない手はそのまま返し、駒のない移動元からは駒を作らない
QString format(const Position& board, const QString& usi, Options style, bool side)
{
    const std::optional<UsiMove> move = UsiMove::parse(usi.trimmed());
    if (!move) return usi;
    const Turn mover = board.sideToMove();
    const Piece moving = move->isDrop() ? move->dropPieceFor(mover) : board.pieceAt(move->from);
    if (moving == Piece::None) return usi; // Incomplete input: never invent a piece.

    if (style.notation == Notation::Japanese) {
        const QString text = KifMoveText::format(mover, moving, *move);
        return side ? text : text.mid(1);
    }
    return western(board, *move, moving, style, side);
}

/// 読み筋の表記の設定（fullOrigin なら移動元をすべて付ける）
Options pvStyle(bool fullOrigin)
{
    Options style = options();
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    return style;
}
} // namespace

QString rankLabel(int rank)
{
    if (rank < 1 || rank > 9) return {};
    return options().notation == Notation::Western ? QString(QChar('a' + rank - 1))
                                                   : ShogiNotation::rankKanji(rank);
}

QString move(const QString& beforeSfen, const QString& usi, Options style, bool includeSide)
{
    Position board;
    if (beforeSfen.isEmpty() || !board.assignSfen(beforeSfen)) return usi;
    return format(board, usi, style, includeSide);
}

QString pv(const PvText& line, bool fullOrigin)
{
    const QList<PvText::Step>& steps = line.steps();
    const TerminalType declaration = line.declaration();
    // 局面と合う手が無ければ、エンジンが送った語のまま出す
    if (steps.isEmpty() && declaration == TerminalType::None) return line.rest().join(QLatin1Char(' '));

    const Options style = pvStyle(fullOrigin);
    const bool japanese = style.notation == Notation::Japanese;
    QString text;
    if (japanese) {
        // 日本語表記は棋譜と同じ KIF の表記を続けて書き、同じマスへの手は「同」と書く
        text.reserve(steps.size() * 10);
        BoardSquare previousTo = line.previousTo();
        for (const PvText::Step& step : steps) {
            KifMoveText::append(text, step.mover, step.moving, step.move, previousTo, KifMoveText::SameSquare::Compact);
            previousTo = step.move.to;
        }
    } else {
        Position board;
        board.assignSfen(line.baseSfen());
        QStringList moves;
        moves.reserve(steps.size());
        for (const PvText::Step& step : steps) {
            moves.append(western(board, step.move, step.moving, style, true));
            board.apply(step.move);
        }
        text = moves.join(QLatin1Char(' '));
    }
    const QString separator = japanese || text.isEmpty() ? QString() : QStringLiteral(" ");
    if (declaration != TerminalType::None) {
        // resign・win は指せた手の次の手番の終局の宣言
        Turn side = Turn::Black;
        if (!steps.isEmpty()) side = opponent(steps.constLast().mover);
        else if (const std::optional<Position> base = Position::fromSfen(line.baseSfen())) side = base->sideToMove();
        text += separator + QChar(side == Turn::Black ? u'▲' : u'△') + TerminalTypes::displayLabel(declaration);
    } else if (const QString note = line.annotation(); !note.isEmpty()) {
        text += QLatin1Char(' ') + note;
    }
    return text;
}

QString pvMove(const PvText& line, int ply)
{
    const QList<PvText::Step>& steps = line.steps();
    if (ply < 1 || ply > steps.size()) return {};
    const PvText::Step& step = steps.at(ply - 1);
    const Options style = options();
    if (style.notation == Notation::Japanese) {
        const BoardSquare previousTo = ply == 1 ? line.previousTo() : steps.at(ply - 2).move.to;
        QString text;
        KifMoveText::append(text, step.mover, step.moving, step.move, previousTo, KifMoveText::SameSquare::Compact);
        return text;
    }
    Position board;
    board.assignSfen(line.baseSfen());
    for (int i = 0; i < ply - 1; ++i) board.apply(steps.at(i).move);
    return western(board, step.move, step.moving, style, true);
}

QString moveLabel(const QString& kifText, const QString& beforeSfen, const QString& usi, bool fullOrigin)
{
    if (options().notation == Notation::Japanese || usi.isEmpty()) return kifText;
    Options style = options();
    style.alwaysOrigin = fullOrigin || style.alwaysOrigin;
    return move(beforeSfen, usi, style);
}

QString entryLabel(const RecordEntry& entry, const QString& beforeSfen, bool fullOrigin)
{
    if (entry.isTerminal())
        return QChar(entry.mover == Turn::Black ? u'▲' : u'△') + entry.terminalLabel(true);
    if (entry.move.movingPiece == Piece::None) return entry.displayText;
    const QString usi = entry.move.move.toString();
    // 日本語表記は棋譜の表記（無ければ指し手から作る）
    if (entry.displayText.isEmpty() && options().notation == Notation::Japanese)
        return KifMoveText::format(entry.mover, entry.move.movingPiece, entry.move.move);
    return moveLabel(entry.displayText, beforeSfen, usi, fullOrigin);
}

QString numberedLabel(int ply, const QString& label, bool branch)
{
    return QString::number(ply).rightJustified(4, QLatin1Char(' ')) + QLatin1Char(' ') + label
        + (branch ? branchMark() : QString());
}

QString branchMark()
{
    return options().notation == Notation::Western ? QStringLiteral(" [+]") : QStringLiteral("+");
}

QString bookLabel()
{
    return translated(QT_TRANSLATE_NOOP("KifuPresentation", "（定跡）"));
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
    if (key == GameInfoKeys::kTimeControl && raw == QStringLiteral("無制限"))
        return translated(QT_TRANSLATE_NOOP("KifuPresentation", "無制限"));
    if (key == GameInfoKeys::kTimeControl) {
        // このアプリの持ち時間の書式（TimeControlText）を表示言語に合わせる。先後で違えば見出しと各側を訳す
        if (const std::optional<TimeControlText::Parsed> parsed = TimeControlText::parse(raw)) {
            const auto side = [](const TimeControlSide& value) {
                if (value.incrementMs < 1000 || value.byoyomiMs >= 1000) return TimeControlText::format(value);
                TimeControlSide main = value;
                main.incrementMs = 0;
                return translated(QT_TRANSLATE_NOOP("KifuPresentation", "%1+%2秒加算"))
                    .arg(TimeControlText::format(main)).arg(value.incrementMs / 1000);
            };
            const TimeControl& tc = parsed->timeControl;
            if (!parsed->perSide) return side(tc.black);
            const bool handicap = parsed->handicapNames;
            return QStringLiteral("%1 %2 / %3 %4")
                .arg(infoKey(handicap ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer), side(tc.black),
                     infoKey(handicap ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer), side(tc.white));
        }
    }
    return raw;
}
}
