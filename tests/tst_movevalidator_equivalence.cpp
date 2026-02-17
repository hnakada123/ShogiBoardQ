#include <QtTest>
#include <QRandomGenerator>

#include "fastmovevalidator.h"
#include "movevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

namespace {

struct State {
    QVector<QChar> board;
    QMap<QChar, int> stand;
    MoveValidator::Turn turn = MoveValidator::BLACK;
};

bool isBlackPiece(const QChar piece)
{
    return piece >= QLatin1Char('A') && piece <= QLatin1Char('Z');
}

bool isWhitePiece(const QChar piece)
{
    return piece >= QLatin1Char('a') && piece <= QLatin1Char('z');
}

bool belongsToTurn(const MoveValidator::Turn turn, const QChar piece)
{
    if (turn == MoveValidator::BLACK) {
        return isBlackPiece(piece);
    }
    return isWhitePiece(piece);
}

QChar toBasePiece(const QChar piece)
{
    switch (piece.toLatin1()) {
    case 'Q': return QLatin1Char('P');
    case 'M': return QLatin1Char('L');
    case 'O': return QLatin1Char('N');
    case 'T': return QLatin1Char('S');
    case 'C': return QLatin1Char('B');
    case 'U': return QLatin1Char('R');
    case 'q': return QLatin1Char('p');
    case 'm': return QLatin1Char('l');
    case 'o': return QLatin1Char('n');
    case 't': return QLatin1Char('s');
    case 'c': return QLatin1Char('b');
    case 'u': return QLatin1Char('r');
    default: return piece;
    }
}

QChar toPromotedPiece(const QChar piece)
{
    switch (piece.toLatin1()) {
    case 'P': return QLatin1Char('Q');
    case 'L': return QLatin1Char('M');
    case 'N': return QLatin1Char('O');
    case 'S': return QLatin1Char('T');
    case 'B': return QLatin1Char('C');
    case 'R': return QLatin1Char('U');
    case 'p': return QLatin1Char('q');
    case 'l': return QLatin1Char('m');
    case 'n': return QLatin1Char('o');
    case 's': return QLatin1Char('t');
    case 'b': return QLatin1Char('c');
    case 'r': return QLatin1Char('u');
    default: return piece;
    }
}

int handRankForPiece(const MoveValidator::Turn turn, const QChar piece)
{
    const char p = toBasePiece(piece).toLatin1();

    if (turn == MoveValidator::BLACK) {
        switch (p) {
        case 'P': return 0;
        case 'L': return 1;
        case 'N': return 2;
        case 'S': return 3;
        case 'G': return 4;
        case 'B': return 5;
        case 'R': return 6;
        default: return 0;
        }
    }

    switch (p) {
    case 'r': return 2;
    case 'b': return 3;
    case 'g': return 4;
    case 's': return 5;
    case 'n': return 6;
    case 'l': return 7;
    case 'p': return 8;
    default: return 0;
    }
}

QVector<ShogiMove> enumerateCandidates(const State& state)
{
    QVector<ShogiMove> out;
    out.reserve(9000);

    for (int from = 0; from < 81; ++from) {
        const QChar piece = state.board[from];
        if (piece == QLatin1Char(' ') || !belongsToTurn(state.turn, piece)) {
            continue;
        }

        const QPoint fromPt(from % 9, from / 9);
        for (int to = 0; to < 81; ++to) {
            if (to == from) {
                continue;
            }
            const QPoint toPt(to % 9, to / 9);
            const QChar captured = state.board[to];
            out.append(ShogiMove(fromPt, toPt, piece, captured, false));
        }
    }

    const QList<QChar> handPieces = (state.turn == MoveValidator::BLACK)
        ? QList<QChar>{QLatin1Char('P'), QLatin1Char('L'), QLatin1Char('N'), QLatin1Char('S'), QLatin1Char('G'), QLatin1Char('B'), QLatin1Char('R')}
        : QList<QChar>{QLatin1Char('p'), QLatin1Char('l'), QLatin1Char('n'), QLatin1Char('s'), QLatin1Char('g'), QLatin1Char('b'), QLatin1Char('r')};

    const int handFile = (state.turn == MoveValidator::BLACK)
        ? MoveValidator::BLACK_HAND_FILE
        : MoveValidator::WHITE_HAND_FILE;

    for (const QChar piece : handPieces) {
        if (state.stand.value(piece, 0) <= 0) {
            continue;
        }
        for (int to = 0; to < 81; ++to) {
            const QPoint toPt(to % 9, to / 9);
            out.append(ShogiMove(QPoint(handFile, handRankForPiece(state.turn, piece)), toPt, piece, state.board[to], false));
        }
    }

    return out;
}

void applyLegalMove(State& state, const ShogiMove& move)
{
    const int to = move.toSquare.y() * 9 + move.toSquare.x();

    if (move.fromSquare.x() >= 0 && move.fromSquare.x() < 9
        && move.fromSquare.y() >= 0 && move.fromSquare.y() < 9) {
        const int from = move.fromSquare.y() * 9 + move.fromSquare.x();
        const QChar moving = state.board[from];
        const QChar captured = state.board[to];

        state.board[from] = QLatin1Char(' ');
        state.board[to] = move.isPromotion ? toPromotedPiece(moving) : moving;

        if (captured != QLatin1Char(' ')) {
            const QChar base = toBasePiece(captured);
            const QChar handPiece = (state.turn == MoveValidator::BLACK)
                ? QChar(base.toUpper())
                : QChar(base.toLower());
            state.stand[handPiece] = state.stand.value(handPiece, 0) + 1;
        }
    } else {
        const QChar moving = move.movingPiece;
        state.board[to] = moving;
        state.stand[moving] = state.stand.value(moving, 0) - 1;
    }

    state.turn = (state.turn == MoveValidator::BLACK) ? MoveValidator::WHITE : MoveValidator::BLACK;
}

} // namespace

class TestMoveValidatorEquivalence : public QObject
{
    Q_OBJECT

private slots:
    void compareHighRiskPositions_data()
    {
        QTest::addColumn<QString>("sfen");

        // 王手回避局面（先手玉が飛車で王手）
        QTest::newRow("risk-check-evasion")
            << QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");

        // 両王手局面（先手玉に角＋飛の両王手）
        QTest::newRow("risk-double-check")
            << QStringLiteral("4k4/9/9/9/9/9/9/3br4/4K4 b - 1");

        // 二歩候補局面
        QTest::newRow("risk-nifu")
            << QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1");

        // 強制成り（歩）
        QTest::newRow("risk-mandatory-pawn")
            << QStringLiteral("4k4/P8/9/9/9/9/9/9/4K4 b - 1");

        // 強制成り（桂）
        QTest::newRow("risk-mandatory-knight")
            << QStringLiteral("4k4/9/1N7/9/9/9/9/9/4K4 b - 1");

        // 打ち歩詰めを狙える最小構成局面
        QTest::newRow("risk-pawn-drop-mate")
            << QStringLiteral("3gkg3/3p1p3/4G4/9/9/9/9/9/4K4 b P 1");
    }

    void compareHighRiskPositions()
    {
        QFETCH(QString, sfen);

        ShogiBoard board;
        board.setSfen(sfen);

        State state;
        state.board = board.boardData();
        state.stand = board.getPieceStand();
        state.turn = (board.currentPlayer() == QStringLiteral("w"))
            ? MoveValidator::WHITE
            : MoveValidator::BLACK;

        MoveValidator ref;
        FastMoveValidator fast;

        const auto fastTurn = (state.turn == MoveValidator::BLACK)
            ? FastMoveValidator::BLACK
            : FastMoveValidator::WHITE;

        const int refCount = ref.generateLegalMoves(state.turn, state.board, state.stand);
        const int fastCount = fast.generateLegalMoves(fastTurn, state.board, state.stand);
        QCOMPARE(fastCount, refCount);

        const int refChecks = ref.checkIfKingInCheck(state.turn, state.board);
        const int fastChecks = fast.checkIfKingInCheck(fastTurn, state.board);
        QCOMPARE(fastChecks, refChecks);

        const QVector<ShogiMove> candidates = enumerateCandidates(state);
        for (const ShogiMove& cand : candidates) {
            ShogiMove r = cand;
            ShogiMove f = cand;
            const LegalMoveStatus rs = ref.isLegalMove(state.turn, state.board, state.stand, r);
            const LegalMoveStatus fs = fast.isLegalMove(fastTurn, state.board, state.stand, f);

            if (rs.nonPromotingMoveExists != fs.nonPromotingMoveExists
                || rs.promotingMoveExists != fs.promotingMoveExists) {
                qDebug() << "HighRisk mismatch sfen=" << sfen
                         << "move=" << cand
                         << "ref np/p=" << rs.nonPromotingMoveExists << rs.promotingMoveExists
                         << "fast np/p=" << fs.nonPromotingMoveExists << fs.promotingMoveExists;
            }

            QCOMPARE(fs.nonPromotingMoveExists, rs.nonPromotingMoveExists);
            QCOMPARE(fs.promotingMoveExists, rs.promotingMoveExists);
        }
    }

    void comparePawnDropMateMove()
    {
        const QString sfen = QStringLiteral("3gkg3/3p1p3/4G4/9/9/9/9/9/4K4 b P 1");

        ShogiBoard board;
        board.setSfen(sfen);

        MoveValidator ref;
        FastMoveValidator fast;

        // 5b への歩打ち（打ち歩詰め候補）
        ShogiMove probe(QPoint(MoveValidator::BLACK_HAND_FILE, handRankForPiece(MoveValidator::BLACK, QLatin1Char('P'))),
                        QPoint(4, 1), QLatin1Char('P'), QLatin1Char(' '), false);

        ShogiMove r = probe;
        ShogiMove f = probe;

        const auto rs = ref.isLegalMove(MoveValidator::BLACK, board.boardData(), board.getPieceStand(), r);
        const auto fs = fast.isLegalMove(FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), f);

        QCOMPARE(fs.nonPromotingMoveExists, rs.nonPromotingMoveExists);
        QCOMPARE(fs.promotingMoveExists, rs.promotingMoveExists);
    }

    void compareSpecialPositions_data()
    {
        QTest::addColumn<QString>("sfen");

        QTest::newRow("hirate-black")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QTest::newRow("hirate-white")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("king-only-black-hand")
            << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b P 1");
        QTest::newRow("king-only-white-hand")
            << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w p 1");
        QTest::newRow("double-check")
            << QStringLiteral("4k4/9/9/9/9/9/9/3br4/4K4 b - 1");
        QTest::newRow("nifu-candidate")
            << QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1");
        QTest::newRow("promotion-zone-pawn")
            << QStringLiteral("4k4/9/4p4/4P4/9/9/9/9/4K4 b - 1");
        QTest::newRow("mandatory-pawn-promotion")
            << QStringLiteral("4k4/P8/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("mandatory-knight-promotion")
            << QStringLiteral("4k4/9/1N7/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("drop-dead-square-knight")
            << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b N 1");
        QTest::newRow("promoted-gold-like-pieces")
            << QStringLiteral("4k4/9/9/9/4q4/9/4Q4/9/4K4 b R 1");
        QTest::newRow("check-position-from-existing-test")
            << QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
    }

    void compareSpecialPositions()
    {
        QFETCH(QString, sfen);

        ShogiBoard board;
        board.setSfen(sfen);

        State state;
        state.board = board.boardData();
        state.stand = board.getPieceStand();
        state.turn = (board.currentPlayer() == QStringLiteral("w"))
            ? MoveValidator::WHITE
            : MoveValidator::BLACK;

        MoveValidator ref;
        FastMoveValidator fast;

        const auto runForTurn = [&](const MoveValidator::Turn turn) {
            const auto fastTurn = (turn == MoveValidator::BLACK)
                ? FastMoveValidator::BLACK
                : FastMoveValidator::WHITE;
            const auto opponentTurn = (turn == MoveValidator::BLACK)
                ? MoveValidator::WHITE
                : MoveValidator::BLACK;

            State tstate = state;
            tstate.turn = turn;

            const int refOpponentChecks = ref.checkIfKingInCheck(opponentTurn, tstate.board);
            if (refOpponentChecks > 0) {
                qDebug() << "Skip special comparison due to opponent already-in-check state"
                         << "sfen=" << sfen
                         << "turn=" << (turn == MoveValidator::BLACK ? "b" : "w")
                         << "opponentChecks=" << refOpponentChecks;
                return;
            }

            const int refCount = ref.generateLegalMoves(turn, tstate.board, tstate.stand);
            const int fastCount = fast.generateLegalMoves(fastTurn, tstate.board, tstate.stand);
            QCOMPARE(fastCount, refCount);

            const int refChecks = ref.checkIfKingInCheck(turn, tstate.board);
            const int fastChecks = fast.checkIfKingInCheck(fastTurn, tstate.board);
            QCOMPARE(fastChecks, refChecks);

            const QVector<ShogiMove> candidates = enumerateCandidates(tstate);
            for (const ShogiMove& cand : candidates) {
                ShogiMove r = cand;
                ShogiMove f = cand;
                const LegalMoveStatus rs = ref.isLegalMove(turn, tstate.board, tstate.stand, r);
                const LegalMoveStatus fs = fast.isLegalMove(fastTurn, tstate.board, tstate.stand, f);

                if (rs.nonPromotingMoveExists != fs.nonPromotingMoveExists
                    || rs.promotingMoveExists != fs.promotingMoveExists) {
                    qDebug() << "Special mismatch sfen=" << sfen
                             << "turn=" << (turn == MoveValidator::BLACK ? "b" : "w")
                             << "move=" << cand
                             << "ref np/p=" << rs.nonPromotingMoveExists << rs.promotingMoveExists
                             << "fast np/p=" << fs.nonPromotingMoveExists << fs.promotingMoveExists;
                }

                QCOMPARE(fs.nonPromotingMoveExists, rs.nonPromotingMoveExists);
                QCOMPARE(fs.promotingMoveExists, rs.promotingMoveExists);
            }
        };

        runForTurn(MoveValidator::BLACK);
        runForTurn(MoveValidator::WHITE);
    }

    void compareInitialPositionFullScan()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        State state;
        state.board = board.boardData();
        state.stand = board.getPieceStand();
        state.turn = MoveValidator::BLACK;

        MoveValidator ref;
        FastMoveValidator fast;

        const int refCount = ref.generateLegalMoves(state.turn, state.board, state.stand);
        const int fastCount = fast.generateLegalMoves(FastMoveValidator::BLACK, state.board, state.stand);
        QCOMPARE(fastCount, refCount);

        const QVector<ShogiMove> candidates = enumerateCandidates(state);
        for (const ShogiMove& cand : candidates) {
            ShogiMove r = cand;
            ShogiMove f = cand;
            const LegalMoveStatus rs = ref.isLegalMove(state.turn, state.board, state.stand, r);
            const LegalMoveStatus fs = fast.isLegalMove(FastMoveValidator::BLACK, state.board, state.stand, f);

            if (rs.nonPromotingMoveExists != fs.nonPromotingMoveExists
                || rs.promotingMoveExists != fs.promotingMoveExists) {
                qDebug() << "Mismatch move=" << cand
                         << "ref np/p=" << rs.nonPromotingMoveExists << rs.promotingMoveExists
                         << "fast np/p=" << fs.nonPromotingMoveExists << fs.promotingMoveExists;
            }

            QCOMPARE(fs.nonPromotingMoveExists, rs.nonPromotingMoveExists);
            QCOMPARE(fs.promotingMoveExists, rs.promotingMoveExists);
        }
    }

    void compareRandomReachablePositions()
    {
        QRandomGenerator rng(20260217U);

        for (int game = 0; game < 8; ++game) {
            ShogiBoard board;
            board.setSfen(kHirateSfen);

            State state;
            state.board = board.boardData();
            state.stand = board.getPieceStand();
            state.turn = MoveValidator::BLACK;

            MoveValidator ref;
            FastMoveValidator fast;

            for (int ply = 0; ply < 30; ++ply) {
                const auto fastTurn = (state.turn == MoveValidator::BLACK)
                    ? FastMoveValidator::BLACK
                    : FastMoveValidator::WHITE;

                const int refCount = ref.generateLegalMoves(state.turn, state.board, state.stand);
                const int fastCount = fast.generateLegalMoves(fastTurn, state.board, state.stand);
                QCOMPARE(fastCount, refCount);

                const int refChecks = ref.checkIfKingInCheck(state.turn, state.board);
                const int fastChecks = fast.checkIfKingInCheck(fastTurn, state.board);
                QCOMPARE(fastChecks, refChecks);

                const QVector<ShogiMove> candidates = enumerateCandidates(state);
                QVector<ShogiMove> legalByRef;
                legalByRef.reserve(220);

                for (const ShogiMove& cand : candidates) {
                    ShogiMove r = cand;
                    ShogiMove f = cand;
                    const LegalMoveStatus rs = ref.isLegalMove(state.turn, state.board, state.stand, r);
                    const LegalMoveStatus fs = fast.isLegalMove(fastTurn, state.board, state.stand, f);

                    if (rs.nonPromotingMoveExists != fs.nonPromotingMoveExists
                        || rs.promotingMoveExists != fs.promotingMoveExists) {
                        qDebug() << "Mismatch game/ply=" << game << ply << "move=" << cand
                                 << "ref np/p=" << rs.nonPromotingMoveExists << rs.promotingMoveExists
                                 << "fast np/p=" << fs.nonPromotingMoveExists << fs.promotingMoveExists;
                    }

                    QCOMPARE(fs.nonPromotingMoveExists, rs.nonPromotingMoveExists);
                    QCOMPARE(fs.promotingMoveExists, rs.promotingMoveExists);

                    if (rs.nonPromotingMoveExists) {
                        ShogiMove nm = cand;
                        nm.isPromotion = false;
                        legalByRef.append(nm);
                    }
                    if (rs.promotingMoveExists) {
                        ShogiMove pm = cand;
                        pm.isPromotion = true;
                        legalByRef.append(pm);
                    }
                }

                QCOMPARE(legalByRef.size(), refCount);

                if (legalByRef.isEmpty()) {
                    break;
                }

                const int pick = static_cast<int>(rng.bounded(static_cast<quint32>(legalByRef.size())));
                applyLegalMove(state, legalByRef[pick]);
            }
        }
    }
};

QTEST_MAIN(TestMoveValidatorEquivalence)
#include "tst_movevalidator_equivalence.moc"
