/// @file shogiengineinfoparser_board.cpp
/// @brief USIエンジンのinfo行パーサ - 座標解析・盤面操作・PVシミュレーション

#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "shogiutils.h"

#include "logcategories.h"

// ============================================================
// 座標解析ユーティリティ
// ============================================================

int ShogiEngineInfoParser::convertRankCharToInt(const QChar rankChar)
{
    if (rankChar.isLetter() && rankChar.toLatin1() <= 'i') {
        return rankChar.toLatin1() - 'a' + 1;
    }
    else {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::convertRankCharToInt. Invalid character conversion %1.").arg(rankChar);
        qCWarning(lcEngine).noquote() << errorMessage;
        return -1;
    }
}

int ShogiEngineInfoParser::convertPieceToStandRank(const QChar pieceChar)
{
    if (m_pieceCharToIntMap.contains(pieceChar)) {
        return m_pieceCharToIntMap.value(pieceChar);
    } else {
        return -1;
    }
}

bool ShogiEngineInfoParser::isBoardRankChar(const QChar rankChar) const
{
    return rankChar.isLetter() && rankChar.toLatin1() <= 'i';
}

int ShogiEngineInfoParser::parseMoveString(const QString& moveStr, int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, bool& promote)
{
    fileFrom = rankFrom = fileTo = rankTo = 0;
    promote = false;

    if (moveStr.length() < 4) {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The length of the move string %1 is insufficient.").arg(moveStr);
        qCWarning(lcEngine).noquote() << errorMessage;
        return -1;
    }

    const QChar* moveChars = moveStr.data();

    // fileFrom を取得
    if (moveChars[0].isDigit()) {
        fileFrom = moveChars[0].digitValue();
    } else {
        // 駒台の駒（例: "G*5b"）
        const int standPieceNumber = convertPieceToStandRank(moveChars[0]);
        if (standPieceNumber >= 1 && standPieceNumber <= 7) {
            fileFrom = STAND_FILE;
        } else {
            // 指し手以外（例: "(57.54%)"）
            fileFrom = rankFrom = fileTo = rankTo = 0;
            promote = false;
            return INFO_STRING_SPECIAL_CASE;
        }
    }

    // rankFrom を取得
    if (isBoardRankChar(moveChars[1])) {
        rankFrom = convertRankCharToInt(moveChars[1]);
        if (rankFrom <= 0) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert source rank.");
            qCWarning(lcEngine).noquote() << errorMessage;
            return -1;
        }
    } else if (moveChars[1] == QLatin1Char('*')) {
        // 駒打ち："G*5b" のようなケース
        const int standPieceNumber = convertPieceToStandRank(moveChars[0]);
        if (standPieceNumber >= 1 && standPieceNumber <= 7) {
            rankFrom = standPieceNumber;
        } else {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Invalid stand piece specification.");
            qCWarning(lcEngine).noquote() << errorMessage;
            return -1;
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the source square are invalid.");
        qCWarning(lcEngine).noquote() << errorMessage;
        return -1;
    }

    // fileTo を取得
    if (moveChars[2].isDigit()) {
        fileTo = moveChars[2].digitValue();
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination file are invalid.");
        qCWarning(lcEngine).noquote() << errorMessage;
        return -1;
    }

    // rankTo を取得
    if (isBoardRankChar(moveChars[3])) {
        rankTo = convertRankCharToInt(moveChars[3]);
        if (rankTo <= 0) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert destination rank.");
            qCWarning(lcEngine).noquote() << errorMessage;
            return -1;
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination square are invalid.");
        qCWarning(lcEngine).noquote() << errorMessage << "moveChars[3]=" << moveChars[3];
        return -1;
    }

    // 成/不成フラグ（5文字目が '+' のとき）
    promote = (moveStr.size() > 4 && moveChars[4] == QLatin1Char('+'));

    return 0;
}

// ============================================================
// 駒文字変換
// ============================================================

QString ShogiEngineInfoParser::getPieceKanjiName(QChar symbol)
{
    symbol = symbol.toUpper();

    if (m_pieceMapping.contains(symbol)) {
        return m_pieceMapping[symbol];
    }
    else {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceKanjiName. The piece character '%1' does not exist.").arg(symbol);
        qCWarning(lcEngine).noquote() << errorMessage;
        return QString();
    }
}

QChar ShogiEngineInfoParser::getPieceCharacter(const QVector<QChar>& boardData, const int file, const int rank)
{
    if ((file >= 1) && (file <= 9)) {
        return boardData.at((rank - 1) * BOARD_SIZE + (file - 1));
    }
    else if (file == STAND_FILE) {
        auto iter = m_pieceMap.find(rank);

        if (iter != m_pieceMap.end()) {
            return iter.value();
        } else {
            QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceCharacter. The rank value is invalid.");
            qCWarning(lcEngine).noquote() << errorMessage << "rank=" << rank;
            return QChar();
        }
    }
    else {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceCharacter. The file value is invalid.");
        qCWarning(lcEngine).noquote() << errorMessage << "file=" << file;
        return QChar();
    }
}

// ============================================================
// 盤面操作
// ============================================================

bool ShogiEngineInfoParser::setData(QVector<QChar>& boardData, const int file, const int rank, const QChar piece) const
{
    int index = (rank - 1) * BOARD_SIZE + (file - 1);

    if (boardData.at(index) == piece) return false;

    boardData[index] = piece;

    return true;
}

void ShogiEngineInfoParser::movePieceToSquare(QVector<QChar>& boardData, QChar movingPiece, int fileFrom, int rankFrom,
                                              int fileTo, int rankTo, bool promote) const
{
    if (promote) {
        QChar promotedPiece;

        if (m_promoteMap.contains(movingPiece)) {
            promotedPiece = m_promoteMap[movingPiece];
        }

        setData(boardData, fileTo, rankTo, promotedPiece);
        setData(boardData, fileFrom, rankFrom, ' ');
    }
    else {
        // 盤上の駒を動かす場合、元の位置を空白にする
        if ((fileFrom >= 1) && (fileFrom <= 9)) {
            setData(boardData, fileFrom, rankFrom, ' ');
        }

        setData(boardData, fileTo, rankTo, movingPiece);
    }
}

// ============================================================
// PV解析・盤面シミュレーション
// ============================================================

int ShogiEngineInfoParser::parsePvAndSimulateMoves(const QStringList& pvTokens, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                   const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    m_pvKanjiStr.clear();
    m_pvUsiStr.clear();

    QStringList validUsiMoves;
    for (qsizetype i = 0; i < pvTokens.size(); ++i) {
        const QString& token = pvTokens.at(i);

        const int rc = parseMoveString(token, fileFrom, rankFrom, fileTo, rankTo, promote);
        if (rc == INFO_STRING_SPECIAL_CASE) {
            if (i == pvTokens.size() - 1) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了
                m_pvKanjiStr += " " + token;
                // m_pvUsiStr には非指し手トークンを含めない
                m_pvUsiStr = validUsiMoves.join(QStringLiteral(" "));
                return 0;
            }
            m_pvUsiStr = validUsiMoves.join(QStringLiteral(" "));
            return -1;
        }
        if (rc < 0) {
            m_pvUsiStr = validUsiMoves.join(QStringLiteral(" "));
            return -1;
        }
        validUsiMoves.append(token);

        const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
        const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
        if (kanjiMovePiece.isEmpty()) {
            return -1;
        }

        const QString shogiStr = convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote,
                                                          algorithm, static_cast<int>(i), isPondering);

        setPreviousFileTo(fileTo);
        setPreviousRankTo(rankTo);

        m_pvKanjiStr += shogiStr;

        if (i == 0) m_searchedHand = shogiStr;

        movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
    }

    m_pvUsiStr = validUsiMoves.join(QStringLiteral(" "));
    return 0;
}

void ShogiEngineInfoParser::parseAndApplyMoveToClonedBoard(const QString& str, QVector<QChar>& clonedBoardData)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(str, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0) {
        return;
    }
    if (rc == INFO_STRING_SPECIAL_CASE) {
        return;
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
}

// ============================================================
// bestmove予想手の変換
// ============================================================

QString ShogiEngineInfoParser::convertPredictedMoveToKanjiString(const ShogiGameController* algorithm, QString& predictedOpponentMove, QVector<QChar>& clonedBoardData)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(predictedOpponentMove, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0 || rc == INFO_STRING_SPECIAL_CASE) {
        return QString();
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
    if (kanjiMovePiece.isEmpty()) {
        return QString();
    }

    const bool isPondering = true;
    const int i = 0;

    const QString shogiStr =
        convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote, algorithm, i, isPondering);

    setPreviousFileTo(fileTo);
    setPreviousRankTo(rankTo);

    return shogiStr;
}

// ============================================================
// デバッグ出力
// ============================================================

void ShogiEngineInfoParser::printShogiBoard(const QVector<QChar>& boardData) const
{
    if (boardData.size() != NUM_BOARD_SQUARES) {
        qCDebug(lcEngine) << "無効な盤面データ";
        return;
    }

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        QString row;
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            QChar piece = boardData[rank * BOARD_SIZE + file];
            if (piece == ' ') {
                row.append("  ");
            } else {
                row.append(piece.toLatin1()).append(' ');
            }
        }
        qCDebug(lcEngine) << row;
    }
    qCDebug(lcEngine) << "----------------------------------------";
}
