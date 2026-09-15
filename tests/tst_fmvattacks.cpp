#include <QtTest>

#include <cstdlib>
#include <random>

#include "fmvattacks.h"

namespace {

using namespace fmv;

constexpr char kPieceChars[] = "PLNSGBRKQMOTCU";

char pieceChar(int piece, Color side)
{
    const char c = kPieceChars[piece];
    return side == Color::Black ? c : static_cast<char>(c + ('a' - 'A'));
}

// 表・占有ビットボードを参照せず、座標と盤面走査だけで判定する。
bool referenceAttack(const EnginePosition& pos, Color side, PieceType piece, int from, int to)
{
    if (from == to) {
        return false;
    }
    const int df = to % 9 - from % 9;
    const int dr = to / 9 - from / 9;
    const int forward = side == Color::Black ? -1 : 1;
    const bool diagonal = std::abs(df) == std::abs(dr);
    const bool orthogonal = df == 0 || dr == 0;
    bool sliding = false;
    switch (piece) {
    case PieceType::Pawn:
        return df == 0 && dr == forward;
    case PieceType::Knight:
        return std::abs(df) == 1 && dr == 2 * forward;
    case PieceType::Silver:
        return (dr == forward && std::abs(df) <= 1)
               || (dr == -forward && std::abs(df) == 1);
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        return (dr == forward && std::abs(df) <= 1)
               || (dr == 0 && std::abs(df) == 1)
               || (dr == -forward && df == 0);
    case PieceType::King:
        return std::abs(df) <= 1 && std::abs(dr) <= 1;
    case PieceType::Lance:
        sliding = df == 0 && dr * forward > 0;
        break;
    case PieceType::Bishop:
        sliding = diagonal;
        break;
    case PieceType::Rook:
        sliding = orthogonal;
        break;
    case PieceType::Horse:
        if (std::abs(df) + std::abs(dr) == 1) {
            return true;
        }
        sliding = diagonal;
        break;
    case PieceType::Dragon:
        if (std::abs(df) == 1 && std::abs(dr) == 1) {
            return true;
        }
        sliding = orthogonal;
        break;
    default:
        return false;
    }
    if (!sliding) {
        return false;
    }
    const int stepF = (df > 0) - (df < 0);
    const int stepR = (dr > 0) - (dr < 0);
    for (int f = from % 9 + stepF, r = from / 9 + stepR;
         f != to % 9 || r != to / 9; f += stepF, r += stepR) {
        if (pos.board[static_cast<std::size_t>(r * 9 + f)] != ' ') {
            return false;
        }
    }
    return true;
}

Bitboard81 referenceAttackers(const EnginePosition& pos, int to, Color side)
{
    Bitboard81 result;
    for (int from = 0; from < kSquareNb; ++from) {
        const char c = pos.board[static_cast<std::size_t>(from)];
        for (int pi = 0; pi < static_cast<int>(PieceType::PieceTypeNb); ++pi) {
            if (c == pieceChar(pi, side)) {
                if (referenceAttack(pos, side, static_cast<PieceType>(pi), from, to)) {
                    result.set(static_cast<Square>(from));
                }
                break;
            }
        }
    }
    return result;
}

void addPieceRows(bool slidersOnly)
{
    QTest::addColumn<int>("piece");
    QTest::addColumn<int>("color");
    for (int ci = 0; ci < 2; ++ci) {
        for (int pi = 0; pi < static_cast<int>(PieceType::PieceTypeNb); ++pi) {
            const auto pt = static_cast<PieceType>(pi);
            if (slidersOnly && pt != PieceType::Lance && pt != PieceType::Bishop
                && pt != PieceType::Rook && pt != PieceType::Horse && pt != PieceType::Dragon) {
                continue;
            }
            const QByteArray name = QByteArray::number(ci) + '-' + kPieceChars[pi];
            QTest::newRow(name.constData()) << pi << ci;
        }
    }
}

} // namespace

class TestFmvAttacks : public QObject
{
    Q_OBJECT

private slots:
    void allSquarePairs_data() { addPieceRows(false); }
    void allSquarePairs()
    {
        QFETCH(int, piece);
        QFETCH(int, color);
        const auto side = static_cast<Color>(color);
        const auto pt = static_cast<PieceType>(piece);
        EnginePosition pos;
        for (int from = 0; from < kSquareNb; ++from) {
            pos.clear();
            pos.board[static_cast<std::size_t>(from)] = pieceChar(piece, side);
            pos.rebuildBitboards();
            for (int to = 0; to < kSquareNb; ++to) {
                const auto fromSq = static_cast<Square>(from);
                const auto toSq = static_cast<Square>(to);
                const bool expected = referenceAttack(pos, side, pt, from, to);
                const Bitboard81 expectedSources = expected ? Bitboard81::squareBit(fromSq) : Bitboard81{};
                QVERIFY2(pieceAttacksSquare(pos, side, pt, fromSq, toSq) == expected
                             && attackersTo(pos, toSq, side) == expectedSources
                             && isSquareAttacked(pos, toSq, side) == expected,
                         qPrintable(QStringLiteral("from=%1 to=%2").arg(from).arg(to)));
            }
        }
    }

    void singleBlocker_data() { addPieceRows(true); }
    void singleBlocker()
    {
        QFETCH(int, piece);
        QFETCH(int, color);
        const auto side = static_cast<Color>(color);
        const auto pt = static_cast<PieceType>(piece);
        EnginePosition pos;
        for (int from = 0; from < kSquareNb; ++from) {
            pos.clear();
            pos.board[static_cast<std::size_t>(from)] = pieceChar(piece, side);
            for (int to = 0; to < kSquareNb; ++to) {
                if (!referenceAttack(pos, side, pt, from, to)) {
                    continue;
                }
                for (int blocker = 0; blocker < kSquareNb; ++blocker) {
                    if (blocker == from) {
                        continue;
                    }
                    // 両端・経路外・lo/hi境界を含めて遮蔽を確認する。
                    pos.board[static_cast<std::size_t>(blocker)] = pieceChar(0, opposite(side));
                    pos.rebuildBitboards();
                    const auto fromSq = static_cast<Square>(from);
                    const auto toSq = static_cast<Square>(to);
                    const bool expected = referenceAttack(pos, side, pt, from, to);
                    const Bitboard81 expectedSources = expected ? Bitboard81::squareBit(fromSq) : Bitboard81{};
                    QVERIFY2(pieceAttacksSquare(pos, side, pt, fromSq, toSq) == expected
                                 && attackersTo(pos, toSq, side) == expectedSources
                                 && isSquareAttacked(pos, toSq, side) == expected,
                             qPrintable(QStringLiteral("from=%1 to=%2 blocker=%3")
                                            .arg(from).arg(to).arg(blocker)));
                    pos.board[static_cast<std::size_t>(blocker)] = ' ';
                }
            }
        }
    }

    void mixedPositions()
    {
        std::mt19937 random(20260915U);
        for (int sample = 0; sample < 128; ++sample) {
            EnginePosition pos;
            pos.clear();
            const unsigned density = 10U + static_cast<unsigned>(sample % 4) * 20U;
            for (char& c : pos.board) {
                if (random() % 100U < density) {
                    const auto side = static_cast<Color>(random() % 2U);
                    c = pieceChar(static_cast<int>(random() % 14U), side);
                }
            }
            pos.rebuildBitboards();
            for (int ci = 0; ci < 2; ++ci) {
                const auto side = static_cast<Color>(ci);
                for (int to = 0; to < kSquareNb; ++to) {
                    const auto sq = static_cast<Square>(to);
                    const Bitboard81 expected = referenceAttackers(pos, to, side);
                    QVERIFY2(attackersTo(pos, sq, side) == expected
                                 && isSquareAttacked(pos, sq, side) == expected.any(),
                             qPrintable(QStringLiteral("sample=%1 color=%2 to=%3")
                                            .arg(sample).arg(ci).arg(to)));
                }
            }
        }
    }

    void invalidInputs()
    {
        EnginePosition pos;
        pos.clear();
        for (const Square sq : {static_cast<Square>(kSquareNb), kInvalidSquare}) {
            QVERIFY(attackersTo(pos, sq, Color::Black).none());
            QVERIFY(!isSquareAttacked(pos, sq, Color::Black));
            QVERIFY(!pieceAttacksSquare(pos, Color::Black, PieceType::Rook, sq, 0));
            QVERIFY(!pieceAttacksSquare(pos, Color::Black, PieceType::Rook, 0, sq));
        }
        QVERIFY(attackersTo(pos, 0, Color::ColorNb).none());
        QVERIFY(!isSquareAttacked(pos, 0, Color::ColorNb));
        QVERIFY(!pieceAttacksSquare(pos, Color::ColorNb, PieceType::Rook, 0, 1));
        QVERIFY(!pieceAttacksSquare(pos, Color::Black, PieceType::PieceTypeNb, 0, 1));
    }
};

QTEST_GUILESS_MAIN(TestFmvAttacks)
#include "tst_fmvattacks.moc"
