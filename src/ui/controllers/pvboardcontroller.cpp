/// @file pvboardcontroller.cpp
/// @brief PV盤面コントローラクラスの実装

#include "pvboardcontroller.h"
#include "shogiboard.h"
#include "logcategories.h"

#include <QRegularExpression>

PvBoardController::PvBoardController(const QString& baseSfen, const QStringList& pvMoves)
    : m_baseSfen(baseSfen)
    , m_pvMoves(pvMoves)
{
    buildSfenHistory();
}

// --------------------------------------------------------
// Navigation
// --------------------------------------------------------

void PvBoardController::goFirst()
{
    m_currentPly = 0;
}

bool PvBoardController::goBack()
{
    if (m_currentPly > 0) {
        --m_currentPly;
        return true;
    }
    return false;
}

bool PvBoardController::goForward()
{
    if (m_currentPly < m_pvMoves.size()) {
        ++m_currentPly;
        return true;
    }
    return false;
}

void PvBoardController::goLast()
{
    m_currentPly = static_cast<int>(m_pvMoves.size());
}

// --------------------------------------------------------
// State queries
// --------------------------------------------------------

QString PvBoardController::currentSfen() const
{
    if (m_currentPly >= 0 && m_currentPly < m_sfenHistory.size()) {
        return m_sfenHistory.at(m_currentPly);
    }
    return {};
}

bool PvBoardController::isBlackTurn() const
{
    const QString sfen = currentSfen();
    return !sfen.contains(QStringLiteral(" w "));
}

QString PvBoardController::currentMoveText() const
{
    if (m_currentPly <= 0 || m_currentPly > m_pvMoves.size()) {
        return {};
    }
    const int moveIdx = m_currentPly - 1;
    if (moveIdx < m_kanjiMoves.size() && !m_kanjiMoves.at(moveIdx).isEmpty()) {
        return m_kanjiMoves.at(moveIdx);
    }
    return m_pvMoves.at(moveIdx);
}

// --------------------------------------------------------
// Property setters
// --------------------------------------------------------

void PvBoardController::setKanjiPv(const QString& kanjiPv)
{
    m_kanjiPv = kanjiPv;
    parseKanjiMoves();
}

// --------------------------------------------------------
// SFEN History
// --------------------------------------------------------

void PvBoardController::buildSfenHistory()
{
    m_sfenHistory.clear();
    m_sfenHistory.append(m_baseSfen);

    bool blackToMove = !m_baseSfen.contains(QStringLiteral(" w "));

    ShogiBoard tempBoard;
    tempBoard.setSfen(m_baseSfen);

    for (qsizetype i = 0; i < m_pvMoves.size(); ++i) {
        applyUsiMoveToBoard(&tempBoard, m_pvMoves.at(i), blackToMove);
        blackToMove = !blackToMove;
        QString nextTurn = blackToMove ? QStringLiteral("b") : QStringLiteral("w");
        QString nextSfen = tempBoard.convertBoardToSfen() + QStringLiteral(" ") +
                          nextTurn + QStringLiteral(" ") +
                          tempBoard.convertStandToSfen() + QStringLiteral(" 1");
        m_sfenHistory.append(nextSfen);
    }
}

// --------------------------------------------------------
// Kanji move parsing
// --------------------------------------------------------

void PvBoardController::parseKanjiMoves()
{
    m_kanjiMoves.clear();

    if (m_kanjiPv.isEmpty()) {
        return;
    }

    // 漢字表記の読み筋を個々の手に分割
    // 例: "△３四歩(33)▲２六歩(27)△８四歩(83)" → ["△３四歩(33)", "▲２六歩(27)", "△８四歩(83)"]
    static const QRegularExpression re(QStringLiteral("([▲△][^▲△]+)"));
    QRegularExpressionMatchIterator it = re.globalMatch(m_kanjiPv);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString move = match.captured(1).trimmed();
        if (!move.isEmpty()) {
            m_kanjiMoves.append(move);
        }
    }

    qCDebug(lcUi) << "parseKanjiMoves: parsed" << m_kanjiMoves.size() << "moves from kanjiPv";
}

// --------------------------------------------------------
// Highlight computation
// --------------------------------------------------------

PvHighlightCoords PvBoardController::computeHighlightCoords() const
{
    PvHighlightCoords result;

    QString usiMove;
    bool isBasePosition = false;

    if (m_currentPly == 0) {
        if (!m_lastMove.isEmpty() && m_lastMove.length() >= 4) {
            usiMove = m_lastMove;
            isBasePosition = true;
            qCDebug(lcUi) << "computeHighlightCoords: using m_lastMove=" << usiMove;
        } else if (!m_prevSfen.isEmpty()) {
            // Try diff-based highlight
            int fromFile = 0, fromRank = 0, toFile = 0, toRank = 0;
            QChar droppedPiece;
            if (diffSfenForHighlight(m_prevSfen, m_baseSfen, fromFile, fromRank, toFile, toRank, droppedPiece)) {
                qCDebug(lcUi) << "computeHighlightCoords: diff ok"
                         << " from=" << fromFile << fromRank
                         << " to=" << toFile << toRank
                         << " drop=" << droppedPiece;
                result.valid = true;
                if (fromFile > 0 && fromRank > 0) {
                    result.hasFrom = true;
                    result.fromFile = fromFile;
                    result.fromRank = fromRank;
                } else if (!droppedPiece.isNull()) {
                    const bool isBlack = droppedPiece.isUpper();
                    QPoint standCoord = getStandPseudoCoord(droppedPiece, isBlack);
                    if (!standCoord.isNull()) {
                        result.hasFrom = true;
                        result.fromFile = standCoord.x();
                        result.fromRank = standCoord.y();
                    }
                }
                if (toFile > 0 && toRank > 0) {
                    result.hasTo = true;
                    result.toFile = toFile;
                    result.toRank = toRank;
                }
                qCDebug(lcUi) << "computeHighlightCoords: diff highlight computed";
                return result;
            }
            qCDebug(lcUi) << "computeHighlightCoords: diff failed";
            // diff failed, try other fallbacks
            if (!m_pvMoves.isEmpty() && m_pvMoves.first().length() >= 4 && isStartposSfen(m_baseSfen)) {
                usiMove = m_pvMoves.first();
                isBasePosition = false;
                qCDebug(lcUi) << "computeHighlightCoords: using first pvMove(startpos)=" << usiMove;
            } else {
                qCDebug(lcUi) << "computeHighlightCoords: no usable move for base position";
                return result;
            }
        } else if (!m_pvMoves.isEmpty() && m_pvMoves.first().length() >= 4 && isStartposSfen(m_baseSfen)) {
            usiMove = m_pvMoves.first();
            isBasePosition = false;
            qCDebug(lcUi) << "computeHighlightCoords: using first pvMove(startpos)=" << usiMove;
        } else {
            qCDebug(lcUi) << "computeHighlightCoords: no usable move for base position";
            return result;
        }
    } else if (m_currentPly > m_pvMoves.size()) {
        qCDebug(lcUi) << "computeHighlightCoords: m_currentPly > m_pvMoves.size(), no highlight";
        return result;
    } else {
        usiMove = m_pvMoves.at(m_currentPly - 1);
        qCDebug(lcUi) << "computeHighlightCoords: using pvMove=" << usiMove;
    }

    if (usiMove.length() < 4) {
        qCDebug(lcUi) << "computeHighlightCoords: usiMove too short:" << usiMove;
        return result;
    }

    return computeHighlightFromUsiMove(usiMove, isBasePosition);
}

PvHighlightCoords PvBoardController::computeHighlightFromUsiMove(const QString& usiMove, bool isBasePosition) const
{
    PvHighlightCoords result;
    result.valid = true;

    if (usiMove.at(1) == '*') {
        // Drop move (e.g., "P*5e")
        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        qCDebug(lcUi) << "computeHighlightFromUsiMove: drop move, pieceChar=" << pieceChar
                 << " toFile=" << toFile << " toRank=" << toRank;

        bool isBlackMove = true;
        if (isBasePosition) {
            isBlackMove = m_baseSfen.contains(QStringLiteral(" w "));
        } else if (m_currentPly >= 1 && m_currentPly <= m_sfenHistory.size()) {
            const QString& prevSfen = m_sfenHistory.at(m_currentPly - 1);
            isBlackMove = !prevSfen.contains(QStringLiteral(" w "));
        }

        QPoint standCoord = getStandPseudoCoord(pieceChar, isBlackMove);
        if (!standCoord.isNull()) {
            result.hasFrom = true;
            result.fromFile = standCoord.x();
            result.fromRank = standCoord.y();
        }

        result.hasTo = true;
        result.toFile = toFile;
        result.toRank = toRank;
        qCDebug(lcUi) << "computeHighlightFromUsiMove: added drop highlights";
    } else {
        // Normal move (e.g., "7g7f" or "7g7f+")
        result.hasFrom = true;
        result.fromFile = usiMove.at(0).toLatin1() - '0';
        result.fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        result.hasTo = true;
        result.toFile = usiMove.at(2).toLatin1() - '0';
        result.toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        qCDebug(lcUi) << "computeHighlightFromUsiMove: normal move, fromFile=" << result.fromFile
                 << " fromRank=" << result.fromRank << " toFile=" << result.toFile << " toRank=" << result.toRank;
    }

    return result;
}

// --------------------------------------------------------
// Static helpers
// --------------------------------------------------------

void PvBoardController::applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isBlackToMove)
{
    if (!board || usiMove.isEmpty()) return;

    if (usiMove.length() < 4) return;

    auto isValidFile = [](QChar ch) { return ch >= '1' && ch <= '9'; };
    auto isValidRank = [](QChar ch) { return ch >= 'a' && ch <= 'i'; };

    // Drop move
    if (usiMove.at(1) == '*') {
        if (!isValidFile(usiMove.at(2)) || !isValidRank(usiMove.at(3))) return;

        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        if (!isBlackToMove) {
            pieceChar = pieceChar.toLower();
        }

        Piece piece = charToPiece(pieceChar);

        board->movePieceToSquare(piece, 0, 0, toFile, toRank, false);

        QMap<Piece, int>& stand = board->m_pieceStand;
        if (stand.contains(piece) && stand[piece] > 0) {
            stand[piece]--;
        }
        return;
    }

    // Normal move
    if (!isValidFile(usiMove.at(0)) || !isValidRank(usiMove.at(1)) ||
        !isValidFile(usiMove.at(2)) || !isValidRank(usiMove.at(3))) {
        return;
    }

    int fromFile = usiMove.at(0).toLatin1() - '0';
    int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
    int toFile = usiMove.at(2).toLatin1() - '0';
    int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
    bool promote = (usiMove.length() >= 5 && usiMove.at(4) == '+');

    Piece piece = board->getPieceCharacter(fromFile, fromRank);

    Piece captured = board->getPieceCharacter(toFile, toRank);
    if (captured != Piece::None) {
        Piece standPiece = demote(captured);
        standPiece = isBlackPiece(standPiece) ? toWhite(standPiece) : toBlack(standPiece);
        board->m_pieceStand[standPiece]++;
    }

    board->movePieceToSquare(piece, fromFile, fromRank, toFile, toRank, promote);
}

bool PvBoardController::isStartposSfen(QString sfen)
{
    sfen = sfen.trimmed();
    if (sfen.startsWith(QStringLiteral("position sfen "))) {
        sfen = sfen.mid(14).trimmed();
    } else if (sfen.startsWith(QStringLiteral("position "))) {
        sfen = sfen.mid(9).trimmed();
    }

    if (sfen == QLatin1String("startpos")) {
        return true;
    }

    static const QString startposSfen =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return sfen.startsWith(startposSfen.left(60));
}

bool PvBoardController::diffSfenForHighlight(const QString& prevSfen, const QString& currSfen,
                                              int& fromFile, int& fromRank,
                                              int& toFile, int& toRank,
                                              QChar& droppedPiece)
{
    fromFile = 0;
    fromRank = 0;
    toFile = 0;
    toRank = 0;
    droppedPiece = QChar();

    auto parseOneBoard = [](const QString& sfen, QString grid[9][9])->bool {
        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 9; ++x) grid[y][x].clear();
        }

        if (sfen.isEmpty()) return false;
        const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
        const QStringList rows = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (rows.size() != 9) return false;

        for (int r = 0; r < 9; ++r) {
            const QString& row = rows.at(r);
            const int y = r;
            int x = 8;
            for (qsizetype i = 0; i < row.size(); ++i) {
                const QChar ch = row.at(i);
                if (ch.isDigit()) {
                    x -= (ch.toLatin1() - '0');
                } else if (ch == QLatin1Char('+')) {
                    if (i + 1 >= row.size() || x < 0) return false;
                    grid[y][x] = QStringLiteral("+") + row.at(++i);
                    --x;
                } else {
                    if (x < 0) return false;
                    grid[y][x] = QString(ch);
                    --x;
                }
            }
            if (x != -1) return false;
        }
        return true;
    };

    QString ga[9][9], gb[9][9];
    if (!parseOneBoard(prevSfen, ga) || !parseOneBoard(currSfen, gb)) return false;

    int fromX = -1;
    int fromY = -1;
    int toX = -1;
    int toY = -1;
    int emptyCount = 0;
    int filledCount = 0;
    int changedCount = 0;

    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            if (ga[y][x] == gb[y][x]) continue;
            if (!ga[y][x].isEmpty() && gb[y][x].isEmpty()) {
                ++emptyCount;
                fromX = x;
                fromY = y;
            } else if (ga[y][x].isEmpty() && !gb[y][x].isEmpty()) {
                ++filledCount;
                toX = x;
                toY = y;
                droppedPiece = gb[y][x].at(0);
                if (droppedPiece == QLatin1Char('+') && gb[y][x].size() >= 2) {
                    droppedPiece = gb[y][x].at(1);
                }
            } else {
                ++changedCount;
                toX = x;
                toY = y;
            }
        }
    }

    bool validMove = (emptyCount == 1 && changedCount == 0 && filledCount == 1) ||
                     (emptyCount == 1 && changedCount == 1 && filledCount == 0) ||
                     (emptyCount == 0 && changedCount == 0 && filledCount == 1);

    if (!validMove) {
        qCDebug(lcUi) << "Invalid diff pattern:"
                 << " emptyCount=" << emptyCount
                 << " filledCount=" << filledCount
                 << " changedCount=" << changedCount;
        return false;
    }

    if (toX < 0 || toY < 0) return false;

    auto toFileRank = [](int x, int y, int& file, int& rank) {
        file = x + 1;
        rank = y + 1;
    };

    if (fromX >= 0 && fromY >= 0) {
        toFileRank(fromX, fromY, fromFile, fromRank);
    }
    toFileRank(toX, toY, toFile, toRank);
    return true;
}

QPoint PvBoardController::getStandPseudoCoord(QChar pieceChar, bool isBlack)
{
    QChar upperPiece = pieceChar.toUpper();

    if (isBlack) {
        switch (upperPiece.toLatin1()) {
            case 'P': return QPoint(10, 1);
            case 'L': return QPoint(10, 2);
            case 'N': return QPoint(10, 3);
            case 'S': return QPoint(10, 4);
            case 'G': return QPoint(10, 5);
            case 'B': return QPoint(10, 6);
            case 'R': return QPoint(10, 7);
            case 'K': return QPoint(10, 8);
            default: return QPoint();
        }
    } else {
        switch (upperPiece.toLatin1()) {
            case 'P': return QPoint(11, 9);
            case 'L': return QPoint(11, 8);
            case 'N': return QPoint(11, 7);
            case 'S': return QPoint(11, 6);
            case 'G': return QPoint(11, 5);
            case 'B': return QPoint(11, 4);
            case 'R': return QPoint(11, 3);
            case 'K': return QPoint(11, 2);
            default: return QPoint();
        }
    }
}
