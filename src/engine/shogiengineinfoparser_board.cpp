/// @file shogiengineinfoparser_board.cpp
/// @brief USIエンジンのinfo行パーサ - 座標解析・盤面操作・PVシミュレーション

#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"

#include "logcategories.h"

// ============================================================
// 座標解析ユーティリティ
// ============================================================

std::optional<int> ShogiEngineInfoParser::convertRankCharToInt(const QChar rankChar)
{
    if (rankChar.isLetter() && rankChar.toLatin1() <= 'i') {
        return rankChar.toLatin1() - 'a' + 1;
    }
    else {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::convertRankCharToInt. Invalid character conversion %1.").arg(rankChar);
        qCWarning(lcEngine).noquote() << errorMessage;
        return std::nullopt;
    }
}

std::optional<int> ShogiEngineInfoParser::convertPieceToStandRank(const QChar pieceChar)
{
    switch (pieceChar.unicode()) {
    case u'P': return 1;
    case u'L': return 2;
    case u'N': return 3;
    case u'S': return 4;
    case u'G': return 5;
    case u'B': return 6;
    case u'R': return 7;
    default: return std::nullopt;
    }
}

bool ShogiEngineInfoParser::isBoardRankChar(const QChar rankChar) const
{
    return rankChar.isLetter() && rankChar.toLatin1() <= 'i';
}

int ShogiEngineInfoParser::parseMoveString(QStringView moveStr, int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, bool& promote)
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
        const auto standPieceNumber = convertPieceToStandRank(moveChars[0]);
        if (standPieceNumber.has_value() && *standPieceNumber >= 1 && *standPieceNumber <= 7) {
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
        const auto rankResult = convertRankCharToInt(moveChars[1]);
        if (!rankResult.has_value()) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert source rank.");
            qCWarning(lcEngine).noquote() << errorMessage;
            return -1;
        }
        rankFrom = *rankResult;
    } else if (moveChars[1] == QLatin1Char('*')) {
        // 駒打ち："G*5b" のようなケース
        const auto standPieceNumber = convertPieceToStandRank(moveChars[0]);
        if (standPieceNumber.has_value() && *standPieceNumber >= 1 && *standPieceNumber <= 7) {
            rankFrom = *standPieceNumber;
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
        const auto rankResult = convertRankCharToInt(moveChars[3]);
        if (!rankResult.has_value()) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert destination rank.");
            qCWarning(lcEngine).noquote() << errorMessage;
            return -1;
        }
        rankTo = *rankResult;
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

const QString& ShogiEngineInfoParser::pieceKanjiName(QChar symbol)
{
    static const QString kPawn = QStringLiteral("歩"), kLance = QStringLiteral("香"), kKnight = QStringLiteral("桂"),
        kSilver = QStringLiteral("銀"), kGold = QStringLiteral("金"), kBishop = QStringLiteral("角"),
        kRook = QStringLiteral("飛"), kKing = QStringLiteral("玉"), kTokin = QStringLiteral("と"),
        kPromotedLance = QStringLiteral("成香"), kPromotedKnight = QStringLiteral("成桂"),
        kPromotedSilver = QStringLiteral("成銀"), kHorse = QStringLiteral("馬"), kDragon = QStringLiteral("龍");
    static const QString kNone;

    symbol = symbol.toUpper();

    switch (symbol.unicode()) {
    case u'P': return kPawn;
    case u'L': return kLance;
    case u'N': return kKnight;
    case u'S': return kSilver;
    case u'G': return kGold;
    case u'B': return kBishop;
    case u'R': return kRook;
    case u'K': return kKing;
    case u'Q': return kTokin;
    case u'M': return kPromotedLance;
    case u'O': return kPromotedKnight;
    case u'T': return kPromotedSilver;
    case u'C': return kHorse;
    case u'U': return kDragon;
    default: {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceKanjiName. The piece character '%1' does not exist.").arg(symbol);
        qCWarning(lcEngine).noquote() << errorMessage;
        return kNone;
    }
    }
}

QChar ShogiEngineInfoParser::pieceCharacter(const QList<QChar>& boardData, const int file, const int rank)
{
    if ((file >= 1) && (file <= 9)) {
        return boardData.at((rank - 1) * BOARD_SIZE + (file - 1));
    }
    else if (file == STAND_FILE) {
        // 駒台の段番号 → 駒文字
        static constexpr char16_t kStandPieces[] = {u'P', u'L', u'N', u'S', u'G', u'B', u'R'};
        if (rank >= 1 && rank <= 7) {
            return QChar(kStandPieces[rank - 1]);
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

bool ShogiEngineInfoParser::setData(QList<QChar>& boardData, const int file, const int rank, const QChar piece) const
{
    int index = (rank - 1) * BOARD_SIZE + (file - 1);

    if (boardData.at(index) == piece) return false;

    boardData[index] = piece;

    return true;
}

void ShogiEngineInfoParser::movePieceToSquare(QList<QChar>& boardData, QChar movingPiece, int fileFrom, int rankFrom,
                                              int fileTo, int rankTo, bool promote) const
{
    if (promote) {
        // 駒文字 → 成駒の文字（成れない駒は空のまま）
        QChar promotedPiece;
        switch (movingPiece.unicode()) {
        case u'P': promotedPiece = QLatin1Char('Q'); break;
        case u'L': promotedPiece = QLatin1Char('M'); break;
        case u'N': promotedPiece = QLatin1Char('O'); break;
        case u'S': promotedPiece = QLatin1Char('T'); break;
        case u'B': promotedPiece = QLatin1Char('C'); break;
        case u'R': promotedPiece = QLatin1Char('U'); break;
        case u'p': promotedPiece = QLatin1Char('q'); break;
        case u'l': promotedPiece = QLatin1Char('m'); break;
        case u'n': promotedPiece = QLatin1Char('o'); break;
        case u's': promotedPiece = QLatin1Char('t'); break;
        case u'b': promotedPiece = QLatin1Char('c'); break;
        case u'r': promotedPiece = QLatin1Char('u'); break;
        default: break;
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

int ShogiEngineInfoParser::parsePvAndSimulateMoves(const QList<QStringView>& tokens, qsizetype first,
                                                   QList<QChar>& clonedBoardData, const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    // 読み筋は info 行ごとに全手を変換する。1手ずつ文字列を作らず、結果の文字列に直接足す
    const qsizetype moveCount = tokens.size() - first;
    m_pvKanjiStr.clear();
    m_pvKanjiStr.reserve(moveCount * 10);
    m_pvUsiStr.clear();
    m_pvUsiStr.reserve(moveCount * 6);

    for (qsizetype i = first; i < tokens.size(); ++i) {
        const QStringView token = tokens.at(i);
        const int moveIndex = static_cast<int>(i - first);

        // 詰んだ局面などでエンジンが返す resign / win は指し手ではなく終局の宣言。
        // 棋譜と同じ日本語表記にし、表示時に KifuPresentation が表記・言語に合わせる
        if (token == u"resign" || token == u"win") {
            m_pvKanjiStr += moveSymbol(moveIndex, isPondering);
            m_pvKanjiStr += (token == u"resign" ? QStringLiteral("投了") : QStringLiteral("入玉勝ち"));
            return 0;
        }

        const int rc = parseMoveString(token, fileFrom, rankFrom, fileTo, rankTo, promote);
        if (rc == INFO_STRING_SPECIAL_CASE) {
            if (i == tokens.size() - 1) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了
                // （m_pvUsiStr には非指し手トークンを含めない）
                m_pvKanjiStr += QLatin1Char(' ');
                m_pvKanjiStr += token;
                return 0;
            }
            return -1;
        }
        if (rc < 0) {
            return -1;
        }
        // USI形式の読み筋は、変換できた手までを空白区切りで並べる
        if (!m_pvUsiStr.isEmpty()) m_pvUsiStr += QLatin1Char(' ');
        m_pvUsiStr += token;

        const QChar movingPiece = pieceCharacter(clonedBoardData, fileFrom, rankFrom);
        const QString& kanjiMovePiece = pieceKanjiName(movingPiece);
        if (kanjiMovePiece.isEmpty()) {
            // 駒が無いマスからの指し手は、USI形式の読み筋を残さない
            m_pvUsiStr.clear();
            return -1;
        }

        const qsizetype moveStart = m_pvKanjiStr.size();
        appendShogiMove(m_pvKanjiStr, kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote,
                        moveIndex, isPondering);

        setPreviousFileTo(fileTo);
        setPreviousRankTo(rankTo);

        if (moveIndex == 0) {
            m_searchedHand = m_pvKanjiStr.mid(moveStart);
            m_searchedUsi = token.toString();
        }

        movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
    }

    return 0;
}

void ShogiEngineInfoParser::parseAndApplyMoveToClonedBoard(const QString& str, QList<QChar>& clonedBoardData)
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

    const QChar movingPiece = pieceCharacter(clonedBoardData, fileFrom, rankFrom);
    movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
}
