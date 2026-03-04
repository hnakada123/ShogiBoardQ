/// @file tsumeshogigenerator_sfen.cpp
/// @brief TsumeshogiGenerator のSFEN解析・再構築ロジック実装

#include "tsumeshogigenerator.h"

namespace {
constexpr int kPieceTypeCount = 7; // P,L,N,S,G,B,R
}

TsumeshogiGenerator::ParsedSfen TsumeshogiGenerator::parseSfen(const QString& sfen) const
{
    ParsedSfen result;

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return result;
    }

    // 盤面パース
    const QStringList ranks = parts[0].split(QLatin1Char('/'));
    if (ranks.size() != 9) {
        return result;
    }

    for (int r = 0; r < 9; ++r) {
        const QString& row = ranks[r];
        int c = 0;
        for (qsizetype i = 0; i < row.size() && c < 9; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                int n = ch.digitValue();
                while (n-- > 0 && c < 9) {
                    result.cells[r][c++].clear();
                }
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size()) {
                    break;
                }
                const QChar p = row.at(++i);
                result.cells[r][c++] = QString(QLatin1Char('+')) + p;
            } else {
                result.cells[r][c++] = QString(ch);
            }
        }
        while (c < 9) {
            result.cells[r][c++].clear();
        }
    }

    // 持ち駒パース
    const QString hands = parts[2];
    if (hands == QStringLiteral("-")) {
        return result;
    }

    int num = 0;
    for (QChar ch : hands) {
        if (ch.isDigit()) {
            num = num * 10 + ch.digitValue();
            continue;
        }

        const bool isSente = ch.isUpper();
        const int n = (num > 0) ? num : 1;
        num = 0;
        const int idx = pieceCharToIndex(ch.toUpper());
        if (idx < 0) {
            continue;
        }
        if (isSente) {
            result.senteHand[idx] += n;
        } else {
            result.goteHand[idx] += n;
        }
    }

    return result;
}

QString TsumeshogiGenerator::buildSfenFromParsed(const ParsedSfen& parsed) const
{
    QString board;
    for (int r = 0; r < 9; ++r) {
        if (r > 0) {
            board += QLatin1Char('/');
        }
        int empty = 0;
        for (int c = 0; c < 9; ++c) {
            if (parsed.cells[r][c].isEmpty()) {
                empty++;
            } else {
                if (empty > 0) {
                    board += QString::number(empty);
                    empty = 0;
                }
                board += parsed.cells[r][c];
            }
        }
        if (empty > 0) {
            board += QString::number(empty);
        }
    }

    // 持ち駒構築
    QString hand;
    // 先手持駒（大文字）: R,B,G,S,N,L,P の順（SFEN慣例: 価値の高い順）
    static constexpr int kHandOrder[kPieceTypeCount] = {6, 5, 4, 3, 2, 1, 0};
    for (int o = 0; o < kPieceTypeCount; ++o) {
        const int idx = kHandOrder[o];
        if (parsed.senteHand[idx] > 0) {
            if (parsed.senteHand[idx] > 1) {
                hand += QString::number(parsed.senteHand[idx]);
            }
            hand += indexToPieceChar(idx);
        }
    }
    // 後手持駒（小文字）
    for (int o = 0; o < kPieceTypeCount; ++o) {
        const int idx = kHandOrder[o];
        if (parsed.goteHand[idx] > 0) {
            if (parsed.goteHand[idx] > 1) {
                hand += QString::number(parsed.goteHand[idx]);
            }
            hand += indexToPieceChar(idx).toLower();
        }
    }
    if (hand.isEmpty()) {
        hand = QStringLiteral("-");
    }

    // 「先手番 手数1」固定（詰将棋は常に攻方=先手番）
    return board + QStringLiteral(" b ") + hand + QStringLiteral(" 1");
}

int TsumeshogiGenerator::pieceCharToIndex(QChar upper)
{
    switch (upper.toLatin1()) {
    case 'P':
        return 0;
    case 'L':
        return 1;
    case 'N':
        return 2;
    case 'S':
        return 3;
    case 'G':
        return 4;
    case 'B':
        return 5;
    case 'R':
        return 6;
    default:
        return -1;
    }
}

QChar TsumeshogiGenerator::indexToPieceChar(int idx)
{
    static constexpr char kChars[kPieceTypeCount] = {'P', 'L', 'N', 'S', 'G', 'B', 'R'};
    if (idx < 0 || idx >= kPieceTypeCount) {
        return QChar();
    }
    return QLatin1Char(kChars[idx]);
}

QList<TsumeshogiGenerator::TrimCandidate>
TsumeshogiGenerator::enumerateRemovablePieces(const QString& sfen) const
{
    QList<TrimCandidate> candidates;
    const ParsedSfen parsed = parseSfen(sfen);

    // 盤上の駒を列挙（玉以外）
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            const QString& cell = parsed.cells[r][c];
            if (cell.isEmpty()) {
                continue;
            }

            // 駒文字を取得（成駒の場合は '+' の次の文字）
            const bool isPromoted = cell.startsWith(QLatin1Char('+'));
            const QChar pieceChar = isPromoted ? cell.at(1) : cell.at(0);

            // 玉はスキップ（攻方玉は詰将棋では配置されないが念のため）
            const QChar upper = pieceChar.toUpper();
            if (upper == QLatin1Char('K')) {
                continue;
            }

            // 守方持駒は除去対象外だが、盤上の守方駒は除去可
            TrimCandidate tc;
            tc.location = TrimCandidate::Board;
            tc.file = c;
            tc.rank = r;
            tc.pieceChar = pieceChar;
            tc.promoted = isPromoted;
            candidates.append(tc);
        }
    }

    // 攻方（先手）持駒を列挙
    for (int i = 0; i < kPieceTypeCount; ++i) {
        if (parsed.senteHand[i] > 0) {
            TrimCandidate tc;
            tc.location = TrimCandidate::Hand;
            tc.pieceChar = indexToPieceChar(i);
            candidates.append(tc);
        }
    }

    // 守方持駒は除去対象外（守方の防御資源を減らすのは不正）
    return candidates;
}

QString TsumeshogiGenerator::removePieceFromSfen(
    const QString& sfen, const TrimCandidate& candidate) const
{
    ParsedSfen parsed = parseSfen(sfen);

    // 除去する駒の生駒インデックス（持駒移動用）
    const QChar upper = candidate.pieceChar.toUpper();
    const int pieceIdx = pieceCharToIndex(upper);

    if (candidate.location == TrimCandidate::Board) {
        // 盤上の駒を除去
        parsed.cells[candidate.rank][candidate.file].clear();

        // 40駒保存則: 除去した駒は後手持駒に移動（生駒に戻す）
        if (pieceIdx >= 0) {
            parsed.goteHand[pieceIdx]++;
        }
    } else if (pieceIdx >= 0 && parsed.senteHand[pieceIdx] > 0) {
        // 攻方持駒から1枚除去 → 後手持駒に移動
        parsed.senteHand[pieceIdx]--;
        parsed.goteHand[pieceIdx]++;
    }

    return buildSfenFromParsed(parsed);
}
