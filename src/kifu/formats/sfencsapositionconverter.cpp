#include "sfencsapositionconverter.h"

#include <QChar>
#include <QMap>
#include <optional>
#include <utility>

namespace SfenCsaPositionConverter {
namespace {

QString defaultSfen()
{
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

QChar csaPieceToSfenChar(const QString& piece)
{
    if (piece == QStringLiteral("FU")) return QLatin1Char('P');
    if (piece == QStringLiteral("KY")) return QLatin1Char('L');
    if (piece == QStringLiteral("KE")) return QLatin1Char('N');
    if (piece == QStringLiteral("GI")) return QLatin1Char('S');
    if (piece == QStringLiteral("KI")) return QLatin1Char('G');
    if (piece == QStringLiteral("KA")) return QLatin1Char('B');
    if (piece == QStringLiteral("HI")) return QLatin1Char('R');
    if (piece == QStringLiteral("OU")) return QLatin1Char('K');
    if (piece == QStringLiteral("TO")) return QLatin1Char('T');
    if (piece == QStringLiteral("NY")) return QLatin1Char('M');
    if (piece == QStringLiteral("NK")) return QLatin1Char('Q');
    if (piece == QStringLiteral("NG")) return QLatin1Char('V');
    if (piece == QStringLiteral("UM")) return QLatin1Char('H');
    if (piece == QStringLiteral("RY")) return QLatin1Char('D');
    return QChar();
}

QString sfenTokenFromCode(bool isSente, const QString& piece)
{
    const QChar base = csaPieceToSfenChar(piece);
    if (base.isNull()) {
        return QString();
    }

    const auto mapped = [&](QChar c) {
        return isSente ? c.toUpper() : c.toLower();
    };

    if (base == QLatin1Char('T')) return QStringLiteral("+") + mapped(QLatin1Char('P'));
    if (base == QLatin1Char('M')) return QStringLiteral("+") + mapped(QLatin1Char('L'));
    if (base == QLatin1Char('Q')) return QStringLiteral("+") + mapped(QLatin1Char('N'));
    if (base == QLatin1Char('V')) return QStringLiteral("+") + mapped(QLatin1Char('S'));
    if (base == QLatin1Char('H')) return QStringLiteral("+") + mapped(QLatin1Char('B'));
    if (base == QLatin1Char('D')) return QStringLiteral("+") + mapped(QLatin1Char('R'));

    return QString(mapped(base));
}

QString csaCodeFromSfenPiece(QChar piece, bool promoted)
{
    const QChar u = piece.toUpper();
    if (u == QLatin1Char('P')) return promoted ? QStringLiteral("TO") : QStringLiteral("FU");
    if (u == QLatin1Char('L')) return promoted ? QStringLiteral("NY") : QStringLiteral("KY");
    if (u == QLatin1Char('N')) return promoted ? QStringLiteral("NK") : QStringLiteral("KE");
    if (u == QLatin1Char('S')) return promoted ? QStringLiteral("NG") : QStringLiteral("GI");
    if (u == QLatin1Char('G')) return QStringLiteral("KI");
    if (u == QLatin1Char('B')) return promoted ? QStringLiteral("UM") : QStringLiteral("KA");
    if (u == QLatin1Char('R')) return promoted ? QStringLiteral("RY") : QStringLiteral("HI");
    if (u == QLatin1Char('K')) return QStringLiteral("OU");
    return QString();
}

QString buildHandsSfen(const QMap<QChar, int>& blackHands, const QMap<QChar, int>& whiteHands)
{
    const QChar order[] = {
        QLatin1Char('R'), QLatin1Char('B'), QLatin1Char('G'), QLatin1Char('S'),
        QLatin1Char('N'), QLatin1Char('L'), QLatin1Char('P')
    };

    QString out;
    for (QChar p : order) {
        const int count = blackHands.value(p, 0);
        if (count <= 0) continue;
        if (count > 1) out += QString::number(count);
        out += p;
    }
    for (QChar p : order) {
        const int count = whiteHands.value(p, 0);
        if (count <= 0) continue;
        if (count > 1) out += QString::number(count);
        out += p.toLower();
    }
    return out.isEmpty() ? QStringLiteral("-") : out;
}

bool parsePiLine(const QString& line, QStringList* ranks)
{
    if (!ranks) return false;
    *ranks = QStringList{
        QStringLiteral("lnsgkgsnl"),
        QStringLiteral("1r5b1"),
        QStringLiteral("ppppppppp"),
        QStringLiteral("9"),
        QStringLiteral("9"),
        QStringLiteral("9"),
        QStringLiteral("PPPPPPPPP"),
        QStringLiteral("1B5R1"),
        QStringLiteral("LNSGKGSNL")
    };

    if (line.size() <= 2) {
        return true;
    }

    // "PI82HI22KA" 形式（指定座標の駒を除去）に対応。
    for (int i = 2; i + 3 < line.size(); i += 4) {
        const int file = line.at(i).digitValue();
        const int rank = line.at(i + 1).digitValue();
        if (file < 1 || file > 9 || rank < 1 || rank > 9) continue;

        QString rankStr = ranks->at(rank - 1);
        QString expanded;
        for (QChar ch : std::as_const(rankStr)) {
            if (ch.isDigit()) {
                expanded += QString(ch.digitValue(), QLatin1Char('1'));
            } else {
                expanded += ch;
            }
        }
        const int idx = 9 - file;
        if (idx >= 0 && idx < expanded.size()) {
            expanded[idx] = QLatin1Char('1');
        }

        int empties = 0;
        QString compressed;
        for (QChar ch : std::as_const(expanded)) {
            if (ch == QLatin1Char('1')) {
                ++empties;
                continue;
            }
            if (empties > 0) {
                compressed += QString::number(empties);
                empties = 0;
            }
            compressed += ch;
        }
        if (empties > 0) {
            compressed += QString::number(empties);
        }
        (*ranks)[rank - 1] = compressed;
    }

    return true;
}

} // namespace

std::optional<QString> fromCsaPositionLines(const QStringList& csaLines, QString* outError)
{
    Q_UNUSED(outError)

    if (csaLines.isEmpty()) {
        return defaultSfen();
    }

    QStringList rankSfen(9, QStringLiteral("9"));
    QMap<QChar, int> blackHands;
    QMap<QChar, int> whiteHands;
    QString turn = QStringLiteral("b");
    bool hasBoardRows = false;

    for (const QString& raw : csaLines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("P1")) || line.startsWith(QStringLiteral("P2")) ||
            line.startsWith(QStringLiteral("P3")) || line.startsWith(QStringLiteral("P4")) ||
            line.startsWith(QStringLiteral("P5")) || line.startsWith(QStringLiteral("P6")) ||
            line.startsWith(QStringLiteral("P7")) || line.startsWith(QStringLiteral("P8")) ||
            line.startsWith(QStringLiteral("P9"))) {
            const int rank = line.mid(1, 1).toInt();
            if (rank < 1 || rank > 9) continue;

            QString row;
            int empties = 0;
            for (int i = 2; i + 2 < line.size(); i += 3) {
                const QString chunk = line.mid(i, 3);
                if (chunk == QStringLiteral(" * ")) {
                    ++empties;
                    continue;
                }
                const QChar side = chunk.at(0);
                const QString piece = chunk.mid(1, 2);
                const QString token = sfenTokenFromCode(side == QLatin1Char('+'), piece);
                if (token.isEmpty()) continue;
                if (empties > 0) {
                    row += QString::number(empties);
                    empties = 0;
                }
                row += token;
            }
            if (empties > 0) {
                row += QString::number(empties);
            }
            if (row.isEmpty()) {
                row = QStringLiteral("9");
            }
            rankSfen[rank - 1] = row;
            hasBoardRows = true;
            continue;
        }

        if (line.startsWith(QStringLiteral("P+")) || line.startsWith(QStringLiteral("P-"))) {
            const bool isSente = line.at(1) == QLatin1Char('+');
            QMap<QChar, int>& hands = isSente ? blackHands : whiteHands;
            for (int i = 2; i + 3 < line.size(); i += 4) {
                if (line.mid(i, 2) != QStringLiteral("00")) continue;
                const QString piece = line.mid(i + 2, 2);
                const QChar c = csaPieceToSfenChar(piece);
                if (!c.isNull()) {
                    QChar base = c;
                    if (base == QLatin1Char('T')) base = QLatin1Char('P');
                    if (base == QLatin1Char('M')) base = QLatin1Char('L');
                    if (base == QLatin1Char('Q')) base = QLatin1Char('N');
                    if (base == QLatin1Char('V')) base = QLatin1Char('S');
                    if (base == QLatin1Char('H')) base = QLatin1Char('B');
                    if (base == QLatin1Char('D')) base = QLatin1Char('R');
                    hands[base] = hands.value(base, 0) + 1;
                }
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("PI"))) {
            parsePiLine(line, &rankSfen);
            hasBoardRows = true;
            continue;
        }

        if (line == QStringLiteral("+")) {
            turn = QStringLiteral("b");
            continue;
        }
        if (line == QStringLiteral("-")) {
            turn = QStringLiteral("w");
            continue;
        }
    }

    if (!hasBoardRows) {
        return defaultSfen();
    }

    const QString board = rankSfen.join(QLatin1Char('/'));
    const QString hands = buildHandsSfen(blackHands, whiteHands);
    return QStringLiteral("%1 %2 %3 1").arg(board, turn, hands);
}

std::optional<QStringList> toCsaPositionLines(const QString& sfen, QString* outError)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        if (outError) *outError = QStringLiteral("sfen is empty");
        return std::nullopt;
    }

    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        if (outError) *outError = QStringLiteral("invalid sfen format");
        return std::nullopt;
    }

    const QStringList ranks = parts[0].split(QLatin1Char('/'));
    if (ranks.size() != 9) {
        if (outError) *outError = QStringLiteral("invalid rank count");
        return std::nullopt;
    }

    QStringList out;
    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("P%1").arg(rank + 1);
        const QString row = ranks.at(rank);
        int file = 9;
        for (int i = 0; i < row.size() && file >= 1; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                const int empties = ch.digitValue();
                for (int k = 0; k < empties; ++k) {
                    line += QStringLiteral(" * ");
                    --file;
                }
                continue;
            }

            bool promoted = false;
            QChar piece = ch;
            if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size()) {
                    if (outError) *outError = QStringLiteral("invalid promoted piece in sfen board");
                    return std::nullopt;
                }
                promoted = true;
                piece = row.at(++i);
            }

            const bool sente = piece.isUpper();
            const QString code = csaCodeFromSfenPiece(piece, promoted);
            if (code.isEmpty()) {
                if (outError) *outError = QStringLiteral("unsupported piece in sfen board");
                return std::nullopt;
            }
            line += sente ? QLatin1Char('+') : QLatin1Char('-');
            line += code;
            --file;
        }

        while (file >= 1) {
            line += QStringLiteral(" * ");
            --file;
        }
        out << line;
    }

    const QString hands = parts.at(2);
    if (hands != QStringLiteral("-")) {
        QString pPlus = QStringLiteral("P+");
        QString pMinus = QStringLiteral("P-");

        int count = 0;
        for (int i = 0; i < hands.size(); ++i) {
            const QChar ch = hands.at(i);
            if (ch.isDigit()) {
                count = count * 10 + ch.digitValue();
                continue;
            }

            int n = (count > 0) ? count : 1;
            count = 0;
            const bool sente = ch.isUpper();
            const QString code = csaCodeFromSfenPiece(ch, false);
            if (code.isEmpty()) {
                if (outError) *outError = QStringLiteral("unsupported hand piece in sfen");
                return std::nullopt;
            }
            QString& dst = sente ? pPlus : pMinus;
            for (int k = 0; k < n; ++k) {
                dst += QStringLiteral("00") + code;
            }
        }
        if (pPlus.size() > 2) out << pPlus;
        if (pMinus.size() > 2) out << pMinus;
    }

    out << ((parts.at(1) == QStringLiteral("w")) ? QStringLiteral("-") : QStringLiteral("+"));
    return out;
}

} // namespace SfenCsaPositionConverter
