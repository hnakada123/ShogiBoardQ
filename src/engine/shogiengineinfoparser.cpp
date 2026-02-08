/// @file shogiengineinfoparser.cpp
/// @brief USIエンジンのinfo行パーサの実装

#include "shogiengineinfoparser.h"
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QObject>
#include <QMessageBox>
#include "shogigamecontroller.h"
#include "shogiutils.h"

// ============================================================
// 初期化
// ============================================================

ShogiEngineInfoParser::ShogiEngineInfoParser()
{
    m_pieceMap = {{1, 'P'}, {2, 'L'}, {3, 'N'}, {4, 'S'}, {5, 'G'}, {6, 'B'}, {7, 'R'}};

    m_pieceCharToIntMap = {{'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}};

    m_promoteMap = {{'P', 'Q'}, {'L', 'M'}, {'N', 'O'}, {'S', 'T'},
                    {'B', 'C'}, {'R', 'U'}, {'p', 'q'}, {'l', 'm'},
                    {'n', 'o'}, {'s', 't'}, {'b', 'c'}, {'r', 'u'}};

    m_pieceMapping = {{'P', "歩"}, {'L', "香"}, {'N', "桂"}, {'S', "銀"}, {'G', "金"}, {'B', "角"},
                      {'R', "飛"}, {'K', "玉"}, {'Q', "と"}, {'M', "成香"}, {'O', "成桂"},
                      {'T', "成銀"}, {'C', "馬"}, {'U', "龍"}};
}

// ============================================================
// アクセサ
// ============================================================

QString ShogiEngineInfoParser::searchedHand() const
{
    return m_searchedHand;
}

void ShogiEngineInfoParser::setScoreMate(const QString& newScoremate)
{
    m_scoreMate = newScoremate;
}

QString ShogiEngineInfoParser::hashfull() const
{
    return m_hashfull;
}

QString ShogiEngineInfoParser::pvKanjiStr() const
{
    return m_pvKanjiStr;
}

QString ShogiEngineInfoParser::pvUsiStr() const
{
    return m_pvUsiStr;
}

QString ShogiEngineInfoParser::scoreMate() const
{
    return m_scoreMate;
}

QString ShogiEngineInfoParser::scoreCp() const
{
    return m_scoreCp;
}

QString ShogiEngineInfoParser::time() const
{
    return m_time;
}

QString ShogiEngineInfoParser::nps() const
{
    return m_nps;
}

QString ShogiEngineInfoParser::nodes() const
{
    return m_nodes;
}

QString ShogiEngineInfoParser::multipv() const
{
    return m_multipv;
}

QString ShogiEngineInfoParser::seldepth() const
{
    return m_seldepth;
}

QString ShogiEngineInfoParser::depth() const
{
    return m_depth;
}

// ============================================================
// 指し手の漢字変換
// ============================================================

QString ShogiEngineInfoParser::getMoveSymbol(const int moveIndex, const ShogiGameController* algorithm, const bool isPondering) const
{
    Q_UNUSED(algorithm)  // 局面更新の影響を避けるため、currentPlayer()は使用しない

    QString symbol;

    // 思考開始時の手番を使用（bestmove後の局面更新の影響を受けない）
    bool thinkingPlayerIsPlayer1 = (m_thinkingStartPlayer == ShogiGameController::Player1);

    // moveIndex & 1 と isPondering の XOR で手番を判定:
    // isPondering=false: 偶数=思考側、奇数=相手側
    // isPondering=true:  偶数=相手側（ponderは相手の予想手から始まる）、奇数=思考側
    if ((moveIndex & 1) ^ isPondering) {
        symbol = thinkingPlayerIsPlayer1 ? "△" : "▲";
    } else {
        symbol = thinkingPlayerIsPlayer1 ? "▲" : "△";
    }

    return symbol;
}

QString ShogiEngineInfoParser::getFormattedMove(int fileTo, int rankTo, const QString& kanji) const
{
    qDebug().noquote() << "[ShogiEngineInfoParser::getFormattedMove] fileTo=" << fileTo
                       << "rankTo=" << rankTo
                       << "previousFileTo()=" << previousFileTo()
                       << "previousRankTo()=" << previousRankTo()
                       << "kanji=" << kanji;
    if ((fileTo == previousFileTo()) && (rankTo == previousRankTo())) {
        qDebug().noquote() << "[ShogiEngineInfoParser::getFormattedMove] -> returning 同" << kanji;
        return "同" + kanji;
    }

    return ShogiUtils::transFileTo(fileTo) + ShogiUtils::transRankTo(rankTo) + kanji;
}

QString ShogiEngineInfoParser::convertMoveToShogiString(const QString& kanji, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo,
                                                        const bool promote, const ShogiGameController* algorithm, const int moveIndex, const bool isPondering)
{
    QString moveStr;

    moveStr += getMoveSymbol(moveIndex, algorithm, isPondering);
    moveStr += getFormattedMove(fileTo, rankTo, kanji);

    if (promote) moveStr += "成";

    if (fileFrom == STAND_FILE) {
        moveStr += "打";
    }
    else {
        // 移動元の座標を "(77)" 形式で付加
        moveStr += "(" + QString::number(fileFrom) + QString::number(rankFrom) + ")";
    }

    return moveStr;
}

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
        emit errorOccurred(errorMessage);
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
        emit errorOccurred(errorMessage);
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
            emit errorOccurred(errorMessage);
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
            emit errorOccurred(errorMessage);
            return -1;
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the source square are invalid.");
        emit errorOccurred(errorMessage);
        return -1;
    }

    // fileTo を取得
    if (moveChars[2].isDigit()) {
        fileTo = moveChars[2].digitValue();
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination file are invalid.");
        emit errorOccurred(errorMessage);
        return -1;
    }

    // rankTo を取得
    if (isBoardRankChar(moveChars[3])) {
        rankTo = convertRankCharToInt(moveChars[3]);
        if (rankTo <= 0) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert destination rank.");
            emit errorOccurred(errorMessage);
            return -1;
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination square are invalid.");
        qDebug() << "moveChars[3] = " << moveChars[3];
        emit errorOccurred(errorMessage);
        return -1;
    }

    // 成/不成フラグ（5文字目が '+' のとき）
    promote = (moveStr.size() > 4 && moveChars[4] == QLatin1Char('+'));

    return 0;
}

// ============================================================
// 直前の指し手座標
// ============================================================

int ShogiEngineInfoParser::previousFileTo() const
{
    return m_previousFileTo;
}

void ShogiEngineInfoParser::setPreviousFileTo(int newPreviousFileTo)
{
    m_previousFileTo = newPreviousFileTo;
}

int ShogiEngineInfoParser::previousRankTo() const
{
    return m_previousRankTo;
}

void ShogiEngineInfoParser::setPreviousRankTo(int newPreviousRankTo)
{
    m_previousRankTo = newPreviousRankTo;
}

void ShogiEngineInfoParser::setThinkingStartPlayer(ShogiGameController::Player player)
{
    m_thinkingStartPlayer = player;
}

ShogiGameController::Player ShogiEngineInfoParser::thinkingStartPlayer() const
{
    return m_thinkingStartPlayer;
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
        emit errorOccurred(errorMessage);
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
            qDebug() << "rank = " << rank;
            emit errorOccurred(errorMessage);
            return QChar();
        }
    }
    else {
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceCharacter. The file value is invalid.");
        qDebug() << "file = " << file;
        emit errorOccurred(errorMessage);
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
// info行解析
// ============================================================

QString ShogiEngineInfoParser::convertCurrMoveToKanjiNotation(const QString& str, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                              const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(str, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0) {
        return QString();
    }
    if (rc == INFO_STRING_SPECIAL_CASE) {
        return QString();
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
    if (kanjiMovePiece.isEmpty()) {
        return QString();
    }

    return convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote, algorithm, 0, isPondering);
}

void ShogiEngineInfoParser::parseEngineInfoTokens(const QStringList& tokens, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                  const bool isPondering)
{
    for (int i = 0; i < tokens.count() - 1; i++) {
        const QString& token = tokens.at(i);
        const QString& nextToken = tokens.at(i + 1);

        if (token == "depth") {
            m_depth = nextToken;
        } else if (token == "seldepth") {
            m_seldepth = nextToken;
        } else if (token == "multipv") {
            m_multipv = nextToken;
        } else if (token == "nodes") {
            m_nodes = nextToken;
        } else if (token == "nps") {
            m_nps = nextToken;
        } else if (token == "time") {
            m_time = nextToken;
        } else if (token == "hashfull") {
            m_hashfull = nextToken;
        } else if (token == "currmove") {
            m_searchedHand =  convertCurrMoveToKanjiNotation(nextToken, algorithm, clonedBoardData, isPondering);
        } else if (token == "score") {
            parseScore(tokens, i);
        }
    }
}

void ShogiEngineInfoParser::parseScore(const QStringList &tokens, int index)
{
    const QString& type = tokens.at(index + 1);
    const QString& value = tokens.at(index + 2);

    if (type == "cp") {
        m_scoreCp = value;
        m_evaluationBound = EvaluationBound::None;
        if (index + 3 < tokens.count()) {
            const QString& boundType = tokens.at(index + 3);
            if (boundType == "lowerbound") {
                setEvaluationBound(EvaluationBound::LowerBound);
            } else if (boundType == "upperbound") {
                setEvaluationBound(EvaluationBound::UpperBound);
            }
        }
    } else if (type == "mate") {
        m_scoreMate = value;
    }
}

ShogiEngineInfoParser::EvaluationBound ShogiEngineInfoParser::evaluationBound() const
{
    return m_evaluationBound;
}

void ShogiEngineInfoParser::setEvaluationBound(EvaluationBound newEvaluationBound)
{
    m_evaluationBound = newEvaluationBound;
}

// ============================================================
// PV解析・盤面シミュレーション
// ============================================================

int ShogiEngineInfoParser::parsePvAndSimulateMoves(const QStringList& pvTokens, const ShogiGameController* algorithm, QVector<QChar> clonedBoardData,
                                                   const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    m_pvKanjiStr.clear();
    m_pvUsiStr = pvTokens.join(QStringLiteral(" "));

    for (qsizetype i = 0; i < pvTokens.size(); ++i) {
        const QString token = pvTokens.at(i).trimmed();

        const int rc = parseMoveString(token, fileFrom, rankFrom, fileTo, rankTo, promote);
        if (rc == INFO_STRING_SPECIAL_CASE) {
            if (i == pvTokens.size() - 1) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了
                m_pvKanjiStr += " " + token;
                return 0;
            }
            return -1;
        }
        if (rc < 0) {
            return -1;
        }

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
// メインエントリ: info行全体の解析
// ============================================================

void ShogiEngineInfoParser::parseEngineOutputAndUpdateState(QString& line, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                           const bool isPondering)
{
    line.remove('\n');

    // info行を空白で分割（例: "info depth 4 seldepth 4 ... pv 8e8f 8g8f 8b8f P*8g"）
    QStringList tokens = line.split(" ");

    parseEngineInfoTokens(tokens, algorithm, clonedBoardData, isPondering);

    // " pv " で分割してPV部分を取り出す
    QStringList pvLineTokens = line.split(" pv ");

    if (pvLineTokens.size() != 2) {
        // pvが含まれていない場合、stringサブコマンドの値を確認
        QStringList stringTokens = line.split(" string ", Qt::KeepEmptyParts);

        if (stringTokens.size() == 2) {
            // stringサブコマンドの値を読み筋として表示する
            m_pvKanjiStr = stringTokens.at(1).trimmed();
        }

        return;
    }

    QStringList pvTokens = pvLineTokens.at(1).split(" ");

    parsePvAndSimulateMoves(pvTokens, algorithm, clonedBoardData, isPondering);
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
        qDebug() << "無効な盤面データ";
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
        qDebug() << row;
    }
    qDebug() << "----------------------------------------";
}

// ============================================================
// 評価値
// ============================================================

void ShogiEngineInfoParser::setScore(const QString &newScore)
{
    m_score = newScore;
}

QString ShogiEngineInfoParser::score() const
{
    return m_score;
}
