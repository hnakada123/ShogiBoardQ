/// @file csalexer_position.cpp
/// @brief CSA形式の初期局面（PI・P行・00AL）を盤（ShogiBoard）に組み立てる実装

#include "csalexer.h"
#include "sfenutils.h"
#include "shoginotation.h"
#include "shogiboard.h"

#include <QMap>
#include <QString>
#include <QStringList>

namespace CsaLexer {

// --- P行（初期局面）解析ヘルパ ---

namespace {

// P行のトークン化（+XX/-XX/* 形式の9トークンに分割）
static QStringList tokenizePRow(const QString& raw)
{
    QString rest = raw.mid(2).trimmed();
    rest.replace(QLatin1Char('\t'), QLatin1Char(' '));

    // 正規化：+/-/* の前後にスペースを挿入
    QString norm; norm.reserve(rest.size() * 2);
    for (qsizetype i = 0; i < rest.size(); ++i) {
        const QChar c = rest.at(i);
        if (c == QLatin1Char('+') || c == QLatin1Char('-') || c == QLatin1Char('*')) {
            norm += QLatin1Char(' ');
            norm += c;
            if (c != QLatin1Char('*') && i + 2 < rest.size()) {
                norm += rest.at(i + 1);
                norm += rest.at(i + 2);
                i += 2;
            }
            norm += QLatin1Char(' ');
        } else {
            norm += c;
        }
    }
    QStringList toks = norm.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.size() == 9) return toks;

    // フォールバック：スペースなし密着形式を直接解析
    QStringList ts; ts.reserve(9);
    for (qsizetype j = 0; j < rest.size(); ++j) {
        const QChar c = rest.at(j);
        if (c == QLatin1Char('*')) {
            ts.append(QStringLiteral("*"));
        } else if (c == QLatin1Char('+') || c == QLatin1Char('-')) {
            if (j + 2 < rest.size()) {
                const QString t = QString(c) + rest.at(j + 1) + rest.at(j + 2);
                ts.append(t);
                j += 2;
            }
        }
    }
    return ts;
}

} // anonymous namespace

// --- P行（初期局面）解析 ---

bool applyPRowLine(const QString& raw, ShogiBoard& board)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const int row = raw.at(1).isDigit() ? raw.at(1).digitValue() : -1;
    if (row < 1 || row > 9) return false;

    const QStringList toks = tokenizePRow(raw);
    if (toks.size() != 9) return false;

    for (int i = 0; i < 9; ++i) {
        const QString& t = toks.at(i);
        const BoardSquare square{9 - i, row};
        if (t == QLatin1String("*")) {
            board.setPieceAt(square, Piece::None);
            continue;
        }
        if (t.size() != 3) return false;
        const Turn side = (t.at(0) == QLatin1Char('+')) ? Turn::Black : Turn::White;
        board.setPieceAt(square, ShogiNotation::pieceFromCsaCode(QStringView(t).mid(1, 2), side));
    }
    return true;
}

bool applyPPlusMinusLine(const QString& raw, ShogiBoard& board, std::optional<Turn>& allRemainingTo)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const QChar sign = raw.at(1);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    const Turn side = (sign == QLatin1Char('+')) ? Turn::Black : Turn::White;

    QString noSpace = raw.mid(2).trimmed();
    noSpace.remove(QLatin1Char(' '));

    int matched = 0;
    for (qsizetype pos = 0; pos + 3 < noSpace.size(); pos += 4) {
        const int file = noSpace.at(pos + 0).digitValue();
        const int rank = noSpace.at(pos + 1).digitValue();
        const QStringView code = QStringView(noSpace).mid(pos + 2, 2);

        if (file == 0 && rank == 0 && code == u"AL") {
            if (!allRemainingTo) allRemainingTo = side;
            ++matched;
            continue;
        }

        const Piece piece = ShogiNotation::pieceFromCsaCode(code, side);
        if (file == 0 && rank == 0) {
            // 持駒は生駒で数える（玉は持駒にしない）
            const Piece inHand = demote(piece);
            if (inHand != Piece::None && toBlack(inHand) != Piece::BlackKing) board.addStandPiece(inHand);
        } else {
            const BoardSquare square{file, rank};
            if (!square.isValid()) continue;
            board.setPieceAt(square, piece);
        }
        ++matched;
    }
    return matched > 0;
}

void giveRemainingPiecesTo(ShogiBoard& board, Turn side)
{
    const QMap<Piece, int> remaining = board.pieceBox();
    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
        if (it.key() == Piece::BlackKing || it.value() <= 0) continue;
        board.addStandPiece(side == Turn::Black ? it.key() : toWhite(it.key()), it.value());
    }
}

// --- 開始局面解析 ---

bool parseStartPos(const QStringList& lines, int& idx, QString& baseSfen, ShogiBoard& board)
{
    // CSAでは配置が省略された駒は駒箱に残る。PIだけが平手配置を指定する。
    board.resetGameBoard();
    Turn stm = Turn::Black;
    std::optional<Turn> allRemainingTo;

    int i = idx;
    for (; i < lines.size(); ++i) {
        QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) continue;
        if (isMetaLine(raw) || isCommentLine(raw)) continue;

        if (raw.startsWith(QLatin1String("PI"))) {
            // 平手から、指定された駒（駒落ち）を除く
            (void)board.trySetSfen(SfenUtils::hirateSfen());
            const QString removed = raw.mid(2);
            if (removed.size() % 4 != 0) return false;
            for (qsizetype pos = 0; pos < removed.size(); pos += 4) {
                const BoardSquare square{removed.at(pos).digitValue(), removed.at(pos + 1).digitValue()};
                const Piece piece = ShogiNotation::pieceFromCsaCode(QStringView(removed).mid(pos + 2, 2));
                if (!square.isValid() || piece == Piece::None || toBlack(board.pieceAt(square)) != piece) return false;
                board.setPieceAt(square, Piece::None);
            }
            continue;
        }

        if (raw.size() >= 2 && raw.at(0) == QLatin1Char('P')) {
            if (raw.at(1).isDigit()) {
                applyPRowLine(raw, board);
                continue;
            } else if (raw.at(1) == QLatin1Char('+') || raw.at(1) == QLatin1Char('-')) {
                applyPPlusMinusLine(raw, board, allRemainingTo);
                continue;
            }
        }

        if (raw == QLatin1String("+")) { stm = Turn::Black; ++i; break; }
        if (raw == QLatin1String("-")) { stm = Turn::White; ++i; break; }

        if (isMoveLine(raw) || isResultLine(raw)) {
            if (raw.startsWith(QLatin1Char('+'))) stm = Turn::Black;
            else if (raw.startsWith(QLatin1Char('-'))) stm = Turn::White;
            break;
        }
    }
    idx = i;

    if (allRemainingTo) giveRemainingPiecesTo(board, *allRemainingTo);

    board.setCurrentPlayer(stm);
    baseSfen = board.convertBoardToSfen() + QLatin1Char(' ') + turnToSfen(stm) + QLatin1Char(' ')
               + board.convertStandToSfen() + QStringLiteral(" 1");
    return true;
}

} // namespace CsaLexer
