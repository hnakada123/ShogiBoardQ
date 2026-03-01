/// @file shogiengineinfoparser.cpp
/// @brief USIエンジンのinfo行パーサの実装

#include "shogiengineinfoparser.h"
#include "usi.h"
#include <QStringList>
#include <QString>
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

    clearParsedInfo();
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
    qCDebug(lcEngine) << "getFormattedMove: fileTo=" << fileTo
                      << "rankTo=" << rankTo
                      << "prevFile=" << previousFileTo()
                      << "prevRank=" << previousRankTo()
                      << "kanji=" << kanji;
    if ((fileTo == previousFileTo()) && (rankTo == previousRankTo())) {
        qCDebug(lcEngine) << "getFormattedMove: 同" << kanji;
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
    if (index + 2 >= tokens.count()) {
        return;
    }

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
// メインエントリ: info行全体の解析
// ============================================================

void ShogiEngineInfoParser::parseEngineOutputAndUpdateState(const QString& line, const ShogiGameController* algorithm,
                                                            QVector<QChar>& clonedBoardData, const bool isPondering)
{
    clearParsedInfo();

    QString normalizedLine = line;
    while (!normalizedLine.isEmpty()) {
        const QChar tail = normalizedLine.back();
        if (tail == QLatin1Char('\n') || tail == QLatin1Char('\r')) {
            normalizedLine.chop(1);
            continue;
        }
        break;
    }

    // info行を空白で分割（例: "info depth 4 seldepth 4 ... pv 8e8f 8g8f 8b8f P*8g"）
    const QStringList tokens = normalizedLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return;
    }

    parseEngineInfoTokens(tokens, algorithm, clonedBoardData, isPondering);

    const qsizetype pvIndex = tokens.indexOf(QStringLiteral("pv"));
    if (pvIndex < 0 || pvIndex + 1 >= tokens.size()) {
        // pvが含まれていない場合（info stringなど）は読み筋として表示しない
        return;
    }

    const QStringList pvTokens = tokens.mid(pvIndex + 1);
    QVector<QChar> pvBoardCopy = clonedBoardData;
    parsePvAndSimulateMoves(pvTokens, algorithm, pvBoardCopy, isPondering);
}

void ShogiEngineInfoParser::clearParsedInfo()
{
    m_depth.clear();
    m_seldepth.clear();
    m_multipv.clear();
    m_nodes.clear();
    m_nps.clear();
    m_time.clear();
    m_score.clear();
    m_string.clear();
    m_scoreCp.clear();
    m_scoreMate.clear();
    m_pvKanjiStr.clear();
    m_pvUsiStr.clear();
    m_hashfull.clear();
    m_searchedHand.clear();
    m_evaluationBound = EvaluationBound::None;
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
