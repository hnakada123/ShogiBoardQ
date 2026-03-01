/// @file parsemoveformat.cpp
/// @brief USI/USEN共通の指し手フォーマット変換関数の実装

#include "parsemoveformat.h"
#include "parsecommon.h"
#include "sfenpositiontracer.h"

namespace KifuParseCommon {

namespace {

static constexpr QStringView kUsiZenkakuDigits = u"０１２３４５６７８９";
static constexpr QStringView kUsiKanjiRanks    = u"〇一二三四五六七八九";

} // namespace

QString usiPieceToKanji(QChar usiPiece)
{
    switch (usiPiece.toUpper().toLatin1()) {
    case 'P': return QStringLiteral("歩");
    case 'L': return QStringLiteral("香");
    case 'N': return QStringLiteral("桂");
    case 'S': return QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return QStringLiteral("角");
    case 'R': return QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QString usiTokenToKanji(QStringView token)
{
    if (token.isEmpty()) return QStringLiteral("?");

    const bool promoted = token.startsWith(QChar('+'));
    if (promoted && token.size() < 2) return QStringLiteral("?");
    const QChar pieceChar = promoted ? token.at(1) : token.at(0);

    switch (pieceChar.toUpper().toLatin1()) {
    case 'P': return promoted ? QStringLiteral("と") : QStringLiteral("歩");
    case 'L': return promoted ? QStringLiteral("杏") : QStringLiteral("香");
    case 'N': return promoted ? QStringLiteral("圭") : QStringLiteral("桂");
    case 'S': return promoted ? QStringLiteral("全") : QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return promoted ? QStringLiteral("馬") : QStringLiteral("角");
    case 'R': return promoted ? QStringLiteral("龍") : QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QString extractUsiPieceToken(QStringView usi, SfenPositionTracer& tracer)
{
    if (usi.size() < 4) return QString();

    if (usi.at(1) == QChar('*')) {
        return QString(usi.at(0).toUpper());
    }

    auto fromFile = parseFileChar(usi.at(0));
    if (!fromFile) return QString();
    QChar fromRankChar = usi.at(1);
    return tracer.tokenAtFileRank(*fromFile, fromRankChar);
}

QString usiMoveToPretty(const QString& usi, int plyNumber,
                        int& prevToFile, int& prevToRank,
                        const QString& pieceToken)
{
    if (usi.isEmpty()) return QString();

    const QString teban = tebanMark(plyNumber);

    // プレースホルダー指し手のチェック (USEN でデコードできなかった指し手)
    if (usi.startsWith(QChar('?'))) {
        return teban + QStringLiteral("?(") + usi.mid(1) + QStringLiteral("手目)");
    }

    // 駒打ちのパターン: P*7f
    if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
        const QChar pieceChar = usi.at(0);
        auto toFile = parseFileChar(usi.at(2));
        auto toRank = parseRankChar(usi.at(3));
        if (!toFile || !toRank) return teban + QStringLiteral("?");

        QString result = teban;
        result += kUsiZenkakuDigits.at(*toFile);
        result += kUsiKanjiRanks.at(*toRank);
        result += usiPieceToKanji(pieceChar) + QStringLiteral("打");

        prevToFile = *toFile;
        prevToRank = *toRank;
        return result;
    }

    // 通常移動のパターン: 7g7f, 7g7f+
    if (usi.size() >= 4) {
        auto fromFile = parseFileChar(usi.at(0));
        auto fromRank = parseRankChar(usi.at(1));
        auto toFile   = parseFileChar(usi.at(2));
        auto toRank   = parseRankChar(usi.at(3));
        if (!fromFile || !fromRank || !toFile || !toRank) return teban + QStringLiteral("?");
        const bool promotes = (usi.size() >= 5 && usi.at(4) == QChar('+'));

        QString result = teban;
        if (*toFile == prevToFile && *toRank == prevToRank) {
            result += QStringLiteral("同　");
        } else {
            result += kUsiZenkakuDigits.at(*toFile);
            result += kUsiKanjiRanks.at(*toRank);
        }

        result += usiTokenToKanji(pieceToken);

        if (promotes) {
            result += QStringLiteral("成");
        }

        result += QStringLiteral("(") + QString::number(*fromFile) + QString::number(*fromRank) + QStringLiteral(")");

        prevToFile = *toFile;
        prevToRank = *toRank;
        return result;
    }

    return teban + QStringLiteral("?");
}

int buildUsiMoveDisplayItems(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             QList<KifDisplayItem>& outDisp)
{
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(baseSfen)) {
        tracer.resetToStartpos();
    }

    int prevToFile = 0, prevToRank = 0;
    int plyNumber = startPly - 1;

    for (const QString& usi : std::as_const(usiMoves)) {
        ++plyNumber;
        const QString pieceToken = extractUsiPieceToken(usi, tracer);
        outDisp.push_back(createMoveDisplayItem(
            plyNumber,
            usiMoveToPretty(usi, plyNumber, prevToFile, prevToRank, pieceToken)));
        (void)tracer.applyUsiMove(usi);
    }

    return plyNumber;
}

} // namespace KifuParseCommon
