/// @file shogiengineinfoparser.cpp
/// @brief USIエンジンのinfo行パーサの実装

#include "shogiengineinfoparser.h"
#include "kifmovetext.h"
#include "logcategories.h"

namespace {

/// 読み筋の末尾に付く "(57.54%)" のような、指し手でない語か（指し手の形の崩れたものは含めない）
bool isPvAnnotation(QStringView token)
{
    if (token.size() < 4) return false;
    const QChar head = token.front();
    return !head.isDigit() && !QStringView(u"PLNSGBR").contains(head);
}

bool belongsTo(Piece piece, Turn side)
{
    return side == Turn::Black ? isBlackPiece(piece) : isWhitePiece(piece);
}

} // namespace

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

ShogiEngineInfoParser::EvaluationBound ShogiEngineInfoParser::evaluationBound() const
{
    return m_evaluationBound;
}

// ============================================================
// 局面
// ============================================================

void ShogiEngineInfoParser::setPosition(const QString& sfen)
{
    m_hasPosition = !sfen.isEmpty() && m_position.trySetSfen(sfen);
    if (!m_hasPosition && !sfen.isEmpty()) {
        qCWarning(lcEngine) << "読み筋の局面を読めません:" << sfen.left(80);
    }
}

void ShogiEngineInfoParser::setPreviousMove(BoardSquare to)
{
    m_previousTo = to;
}

Piece ShogiEngineInfoParser::movingPiece(const ShogiBoard& board, const UsiMove& move, Turn mover)
{
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(mover);
        if (board.pieceAt(move.to) != Piece::None || board.pieceStandCount(piece) <= 0) return Piece::None;
        return piece;
    }
    const Piece piece = board.pieceAt(move.from);
    if (!belongsTo(piece, mover) || belongsTo(board.pieceAt(move.to), mover)) return Piece::None;
    return piece;
}

// ============================================================
// info行解析
// ============================================================

QString ShogiEngineInfoParser::convertCurrMoveToKanjiNotation(QStringView str) const
{
    const std::optional<UsiMove> move = UsiMove::parse(str);
    if (!m_hasPosition || !move) return QString();

    const Turn mover = m_position.currentPlayer();
    const Piece moving = movingPiece(m_position, *move, mover);
    if (moving == Piece::None) return QString();

    QString text;
    KifMoveText::append(text, mover, moving, *move, m_previousTo, KifMoveText::SameSquare::Compact);
    return text;
}

qsizetype ShogiEngineInfoParser::parseEngineInfoTokens(const QList<QStringView>& tokens)
{
    for (qsizetype i = 0; i < tokens.size() - 1; i++) {
        const QStringView token = tokens.at(i);
        const QStringView nextToken = tokens.at(i + 1);

        if (token == u"pv") {
            // pv より後ろはすべて読み筋の指し手
            return i;
        } else if (token == u"depth") {
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
            m_searchedHand = convertCurrMoveToKanjiNotation(nextToken);
        } else if (token == u"score") {
            parseScore(tokens, i);
        }
    }
    return -1;
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
                m_evaluationBound = EvaluationBound::LowerBound;
            } else if (boundType == u"upperbound") {
                m_evaluationBound = EvaluationBound::UpperBound;
            }
        }
    } else if (type == u"mate") {
        m_scoreMate = value.toString();
    }
}

void ShogiEngineInfoParser::parsePvAndSimulateMoves(const QList<QStringView>& tokens, qsizetype first)
{
    if (!m_hasPosition) return;

    // 読み筋は info 行ごとに全手を変換する。1手ずつ文字列を作らず、結果の文字列に直接足す
    const qsizetype moveCount = tokens.size() - first;
    m_pvKanjiStr.reserve(moveCount * 10);
    m_pvUsiStr.reserve(moveCount * 6);

    m_pvBoard.copyPositionFrom(m_position);
    Turn mover = m_position.currentPlayer();
    BoardSquare previousTo = m_previousTo;

    for (qsizetype i = first; i < tokens.size(); ++i) {
        const QStringView token = tokens.at(i);

        // 詰んだ局面などでエンジンが返す resign / win は指し手ではなく終局の宣言。
        // 棋譜と同じ日本語表記にし、表示時に KifuPresentation が表記・言語に合わせる
        if (token == u"resign" || token == u"win") {
            m_pvKanjiStr += QChar(mover == Turn::Black ? u'▲' : u'△');
            m_pvKanjiStr += (token == u"resign" ? QStringLiteral("投了") : QStringLiteral("入玉勝ち"));
            return;
        }

        const std::optional<UsiMove> move = UsiMove::parse(token);
        if (!move) {
            if (i == tokens.size() - 1 && isPvAnnotation(token)) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了（m_pvUsiStr には含めない）
                m_pvKanjiStr += QLatin1Char(' ');
                m_pvKanjiStr += token;
            } else if (!isPvAnnotation(token)) {
                qCWarning(lcEngine) << "読み筋の指し手を読めません:" << token;
            }
            return;
        }

        const Piece moving = movingPiece(m_pvBoard, *move, mover);
        if (moving == Piece::None) {
            // 局面と合わない指し手は、USI形式の読み筋を残さない
            qCWarning(lcEngine) << "読み筋の指し手が局面と合いません:" << token;
            m_pvUsiStr.clear();
            return;
        }

        // USI形式の読み筋は、変換できた手までを空白区切りで並べる
        if (!m_pvUsiStr.isEmpty()) m_pvUsiStr += QLatin1Char(' ');
        m_pvUsiStr += token;

        const qsizetype moveStart = m_pvKanjiStr.size();
        KifMoveText::append(m_pvKanjiStr, mover, moving, *move, previousTo, KifMoveText::SameSquare::Compact);
        if (i == first) {
            m_searchedHand = m_pvKanjiStr.mid(moveStart);
            m_searchedUsi = token.toString();
        }

        m_pvBoard.applyUsiMove(*move, mover);
        previousTo = move->to;
        mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
    }
}

// ============================================================
// メインエントリ: info行全体の解析
// ============================================================

void ShogiEngineInfoParser::parseEngineOutputAndUpdateState(const QString& line)
{
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

    // pvが含まれていない場合（info stringなど）は読み筋として表示しない
    const qsizetype pvIndex = parseEngineInfoTokens(tokens);
    if (pvIndex < 0) {
        return;
    }

    parsePvAndSimulateMoves(tokens, pvIndex + 1);
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
