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
    clearParsedInfo();
}

// ============================================================
// アクセサ
// ============================================================

QString ShogiEngineInfoParser::searchedHand() const
{
    return m_searchedHand;
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

QChar ShogiEngineInfoParser::moveSymbol(const int moveIndex, const bool isPondering) const
{
    // 思考開始時の手番を使用（bestmove後の局面更新の影響を受けない）
    const bool thinkingPlayerIsPlayer1 = (m_thinkingStartPlayer == ShogiGameController::Player1);
    const QChar black(0x25B2);  // ▲
    const QChar white(0x25B3);  // △

    // moveIndex & 1 と isPondering の XOR で手番を判定:
    // isPondering=false: 偶数=思考側、奇数=相手側
    // isPondering=true:  偶数=相手側（ponderは相手の予想手から始まる）、奇数=思考側
    if ((moveIndex & 1) ^ isPondering) {
        return thinkingPlayerIsPlayer1 ? white : black;
    }
    return thinkingPlayerIsPlayer1 ? black : white;
}

void ShogiEngineInfoParser::appendFormattedMove(QString& out, int fileTo, int rankTo, const QString& kanji) const
{
    if ((fileTo == previousFileTo()) && (rankTo == previousRankTo())) {
        out += QChar(0x540C);  // 同
    } else {
        out += ShogiUtils::transFileTo(fileTo);
        out += ShogiUtils::transRankTo(rankTo);
    }
    out += kanji;
}

namespace {
// 移動元の座標の数字を足す（1桁は文字を作らずに足す）
void appendNumber(QString& out, int value)
{
    if (value >= 0 && value <= 9) out += QChar(u'0' + value);
    else out += QString::number(value);
}
} // namespace

// 読み筋は info 行ごとに全手を変換するので、1手ずつ文字列を作らず out に直接足す
void ShogiEngineInfoParser::appendShogiMove(QString& out, const QString& kanji, const int fileFrom, const int rankFrom,
                                            const int fileTo, const int rankTo, const bool promote, const int moveIndex,
                                            const bool isPondering) const
{
    out += moveSymbol(moveIndex, isPondering);
    appendFormattedMove(out, fileTo, rankTo, kanji);

    if (promote) out += QChar(0x6210);  // 成

    if (fileFrom == STAND_FILE) {
        out += QChar(0x6253);  // 打
    } else {
        // 移動元の座標を "(77)" 形式で付加
        out += QLatin1Char('(');
        appendNumber(out, fileFrom);
        appendNumber(out, rankFrom);
        out += QLatin1Char(')');
    }
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

// ============================================================
// info行解析
// ============================================================

QString ShogiEngineInfoParser::convertCurrMoveToKanjiNotation(QStringView str, QList<QChar>& clonedBoardData,
                                                              const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(str, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0) {
        return QString();
    }

    const QChar movingPiece = pieceCharacter(clonedBoardData, fileFrom, rankFrom);
    const QString& kanjiMovePiece = pieceKanjiName(movingPiece);
    if (kanjiMovePiece.isEmpty()) {
        return QString();
    }

    QString move;
    appendShogiMove(move, kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote, 0, isPondering);
    return move;
}

void ShogiEngineInfoParser::parseEngineInfoTokens(const QList<QStringView>& tokens, QList<QChar>& clonedBoardData,
                                                  const bool isPondering)
{
    for (qsizetype i = 0; i < tokens.size() - 1; i++) {
        const QStringView token = tokens.at(i);
        const QStringView nextToken = tokens.at(i + 1);

        if (token == u"depth") {
            m_depth = nextToken.toString();
        } else if (token == u"seldepth") {
            m_seldepth = nextToken.toString();
        } else if (token == u"multipv") {
            m_multipv = nextToken.toString();
        } else if (token == u"nodes") {
            m_nodes = nextToken.toString();
        } else if (token == u"nps") {
            m_nps = nextToken.toString();
        } else if (token == u"time") {
            m_time = nextToken.toString();
        } else if (token == u"hashfull") {
            m_hashfull = nextToken.toString();
        } else if (token == u"currmove") {
            m_searchedUsi = nextToken.toString();
            m_searchedHand = convertCurrMoveToKanjiNotation(nextToken, clonedBoardData, isPondering);
        } else if (token == u"score") {
            parseScore(tokens, i);
        }
    }
}

void ShogiEngineInfoParser::parseScore(const QList<QStringView>& tokens, qsizetype index)
{
    if (index + 2 >= tokens.size()) {
        return;
    }

    const QStringView type = tokens.at(index + 1);
    const QStringView value = tokens.at(index + 2);

    if (type == u"cp") {
        m_scoreCp = value.toString();
        m_evaluationBound = EvaluationBound::None;
        if (index + 3 < tokens.size()) {
            const QStringView boundType = tokens.at(index + 3);
            if (boundType == u"lowerbound") {
                setEvaluationBound(EvaluationBound::LowerBound);
            } else if (boundType == u"upperbound") {
                setEvaluationBound(EvaluationBound::UpperBound);
            }
        }
    } else if (type == u"mate") {
        m_scoreMate = value.toString();
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
                                                            QList<QChar>& clonedBoardData, const bool isPondering)
{
    Q_UNUSED(algorithm)  // 局面更新の影響を避けるため、手番は思考開始時のものを使う
    clearParsedInfo();

    QStringView normalizedLine(line);
    while (!normalizedLine.isEmpty()) {
        const QChar tail = normalizedLine.back();
        if (tail == QLatin1Char('\n') || tail == QLatin1Char('\r')) {
            normalizedLine.chop(1);
            continue;
        }
        break;
    }

    // info行を空白で分割（例: "info depth 4 seldepth 4 ... pv 8e8f 8g8f 8b8f P*8g"）。
    // 読み筋の手ごとに文字列を作らないよう、元の行を指すだけの区切りにする
    const QList<QStringView> tokens = normalizedLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return;
    }

    parseEngineInfoTokens(tokens, clonedBoardData, isPondering);

    const qsizetype pvIndex = tokens.indexOf(u"pv");
    if (pvIndex < 0 || pvIndex + 1 >= tokens.size()) {
        // pvが含まれていない場合（info stringなど）は読み筋として表示しない
        return;
    }

    QList<QChar> pvBoardCopy = clonedBoardData;
    parsePvAndSimulateMoves(tokens, pvIndex + 1, pvBoardCopy, isPondering);
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
    m_scoreCp.clear();
    m_scoreMate.clear();
    m_pvKanjiStr.clear();
    m_pvUsiStr.clear();
    m_hashfull.clear();
    m_searchedHand.clear();
    m_searchedUsi.clear();
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
