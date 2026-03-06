/// @file csamoveconverter.cpp
/// @brief CSA/USI/SFEN形式変換ユーティリティクラスの実装

#include "csamoveconverter.h"
#include "boardconstants.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

namespace {
constexpr QChar kRankMin = QLatin1Char('a');
constexpr QChar kRankMax = QLatin1Char('i');
bool isValidBoardCoordinate(int file, int rank)
{
    return file >= 1 && file <= 9 && rank >= 1 && rank <= 9;
}
bool isValidDropSource(int file, int rank)
{
    return file == 0 && rank == 0;
}
bool isPromotedCsaPiece(const QString& piece)
{
    return piece == QStringLiteral("TO") || piece == QStringLiteral("NY")
        || piece == QStringLiteral("NK") || piece == QStringLiteral("NG")
        || piece == QStringLiteral("UM") || piece == QStringLiteral("RY");
}
bool isKnownCsaPiece(const QString& piece)
{
    return CsaMoveConverter::pieceTypeFromCsa(piece) > 0;
}
bool isValidUsiRank(QChar rank)
{
    return rank >= kRankMin && rank <= kRankMax;
}
} // namespace

// ============================================================
// CSA <-> USI 形式変換
// ============================================================

QString CsaMoveConverter::csaToUsi(const QString& csaMove)
{
    if (csaMove.length() < 7) {
        return QString();
    }
    if (csaMove[0] != QLatin1Char('+') && csaMove[0] != QLatin1Char('-')) {
        qCWarning(lcNetwork) << "csaToUsi: invalid turn prefix:" << csaMove;
        return QString();
    }

    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    const int toFile = csaMove[3].digitValue();
    const int toRank = csaMove[4].digitValue();
    const QString piece = csaMove.mid(5, 2);

    if (!isValidBoardCoordinate(toFile, toRank)) {
        qCWarning(lcNetwork) << "csaToUsi: invalid destination coordinate:" << csaMove;
        return QString();
    }
    if (!isKnownCsaPiece(piece)) {
        qCWarning(lcNetwork) << "csaToUsi: unknown piece code:" << piece;
        return QString();
    }

    QString usiMove;

    if (isValidDropSource(fromFile, fromRank)) {
        // 駒打ち
        const QString usiPiece = csaPieceToUsi(piece);
        if (usiPiece.isEmpty() || usiPiece.startsWith(QLatin1Char('+'))) {
            qCWarning(lcNetwork) << "csaToUsi: invalid drop piece:" << piece;
            return QString();
        }
        usiMove = QString("%1*%2%3")
                      .arg(usiPiece)
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));
    } else {
        if (!isValidBoardCoordinate(fromFile, fromRank)) {
            qCWarning(lcNetwork) << "csaToUsi: invalid source coordinate:" << csaMove;
            return QString();
        }
        // 通常の移動
        usiMove = QString("%1%2%3%4")
                      .arg(fromFile)
                      .arg(QChar('a' + fromRank - 1))
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));

        // 成り判定
        if (isPromotedCsaPiece(piece)) {
            usiMove += QStringLiteral("+");
        }
    }

    return usiMove;
}

QString CsaMoveConverter::usiToCsa(const QString& usiMove, bool isBlack, ShogiBoard* board)
{
    if (usiMove.isEmpty()) {
        return QString();
    }

    QChar turnSign = isBlack ? QLatin1Char('+') : QLatin1Char('-');
    QString csaMove;

    if (usiMove.contains(QLatin1Char('*'))) {
        if (usiMove.size() != 4 || usiMove[1] != QLatin1Char('*')) {
            qCWarning(lcNetwork) << "usiToCsa: invalid drop format:" << usiMove;
            return QString();
        }
        // 駒打ち: P*7f
        const QChar usiPiece = usiMove[0];
        const int toFile = usiMove[2].digitValue();
        const QChar toRankChar = usiMove[3];
        const int toRank = toRankChar.toLatin1() - 'a' + 1;
        if (!isValidBoardCoordinate(toFile, toRank) || !isValidUsiRank(toRankChar)) {
            qCWarning(lcNetwork) << "usiToCsa: invalid drop destination:" << usiMove;
            return QString();
        }

        const QString csaPiece = usiPieceToCsa(QString(usiPiece), false);
        if (csaPiece.isEmpty()) {
            qCWarning(lcNetwork) << "usiToCsa: unknown drop piece:" << usiPiece;
            return QString();
        }
        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(0)
                      .arg(0)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    } else {
        if (usiMove.size() < 4) {
            qCWarning(lcNetwork) << "usiToCsa: invalid move format:" << usiMove;
            return QString();
        }
        // 通常の移動
        const int fromFile = usiMove[0].digitValue();
        const QChar fromRankChar = usiMove[1];
        const int fromRank = fromRankChar.toLatin1() - 'a' + 1;
        const int toFile = usiMove[2].digitValue();
        const QChar toRankChar = usiMove[3];
        const int toRank = toRankChar.toLatin1() - 'a' + 1;
        const bool promote = usiMove.endsWith(QLatin1Char('+'));
        if (!isValidBoardCoordinate(fromFile, fromRank)
            || !isValidBoardCoordinate(toFile, toRank)
            || !isValidUsiRank(fromRankChar) || !isValidUsiRank(toRankChar)) {
            qCWarning(lcNetwork) << "usiToCsa: invalid coordinates:" << usiMove;
            return QString();
        }

        QString csaPiece;
        if (!board) {
            qCWarning(lcNetwork) << "usiToCsa: board is null:" << usiMove;
            return QString();
        }
        const Piece piece = board->pieceCharacter(fromFile, fromRank);
        csaPiece = pieceCharToCsa(piece, promote);
        if (csaPiece.isEmpty()) {
            qCWarning(lcNetwork) << "usiToCsa: failed to resolve source piece, usiMove=" << usiMove;
            return QString();
        }

        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(fromFile)
                      .arg(fromRank)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    }

    return csaMove;
}

// ============================================================
// 盤面座標 <-> CSA 変換
// ============================================================

QString CsaMoveConverter::boardToCSA(const QPoint& from, const QPoint& to, bool promote,
                                     bool isBlackSide, ShogiBoard* board)
{
    qCDebug(lcNetwork) << "boardToCSA: from=" << from << "to=" << to << "promote=" << promote;
    qCDebug(lcNetwork) << "isBlackSide=" << isBlackSide;

    QChar turnSign = isBlackSide ? QLatin1Char('+') : QLatin1Char('-');

    int fromFile = from.x();
    int fromRank = from.y();
    int toFile = to.x();
    int toRank = to.y();

    qCDebug(lcNetwork) << "Calculated: fromFile=" << fromFile << "fromRank=" << fromRank
                       << "toFile=" << toFile << "toRank=" << toRank;

    bool isDrop = (from.x() >= BoardConstants::kBlackStandFile);
    if (isDrop) {
        fromFile = 0;
        fromRank = 0;
        qCDebug(lcNetwork) << "This is a drop move";
    }

    QString csaPiece;
    if (board) {
        Piece piece;
        if (isDrop) {
            piece = board->pieceCharacter(from.x(), from.y());
            qCDebug(lcNetwork) << "Got piece from stand:" << pieceToChar(piece)
                               << "at file=" << from.x() << "rank=" << from.y();
        } else {
            // 通常の移動: validateAndMoveで既に盤面が更新されているので、
            // 移動先(to)から駒を取得する
            piece = board->pieceCharacter(toFile, toRank);
            qCDebug(lcNetwork) << "Got piece from board (at destination):" << pieceToChar(piece)
                               << "at file=" << toFile << "rank=" << toRank;
        }
        csaPiece = pieceCharToCsa(piece, promote);
        qCDebug(lcNetwork) << "Converted to CSA piece:" << csaPiece;
    } else {
        qCWarning(lcNetwork) << "boardToCSA: board is null";
    }
    if (csaPiece.isEmpty()) {
        qCWarning(lcNetwork) << "boardToCSA: failed to resolve piece at move"
                             << "from=" << from << "to=" << to << "promote=" << promote;
        return QString();
    }

    QString result = QString("%1%2%3%4%5%6")
        .arg(turnSign)
        .arg(fromFile)
        .arg(fromRank)
        .arg(toFile)
        .arg(toRank)
        .arg(csaPiece);

    qCDebug(lcNetwork) << "Final CSA move string:" << result;
    return result;
}

// ============================================================
// 駒変換
// ============================================================

QString CsaMoveConverter::pieceCharToCsa(Piece piece, bool promote)
{
    char c = static_cast<char>(piece);

    switch (c) {
    // 先手の成り駒
    case 'Q': return QStringLiteral("TO");
    case 'M': return QStringLiteral("NY");
    case 'O': return QStringLiteral("NK");
    case 'T': return QStringLiteral("NG");
    case 'C': return QStringLiteral("UM");
    case 'U': return QStringLiteral("RY");

    // 後手の成り駒
    case 'q': return QStringLiteral("TO");
    case 'm': return QStringLiteral("NY");
    case 'o': return QStringLiteral("NK");
    case 't': return QStringLiteral("NG");
    case 'c': return QStringLiteral("UM");
    case 'u': return QStringLiteral("RY");

    // 先手の未成駒
    case 'P': return promote ? QStringLiteral("TO") : QStringLiteral("FU");
    case 'L': return promote ? QStringLiteral("NY") : QStringLiteral("KY");
    case 'N': return promote ? QStringLiteral("NK") : QStringLiteral("KE");
    case 'S': return promote ? QStringLiteral("NG") : QStringLiteral("GI");
    case 'G': return QStringLiteral("KI");
    case 'B': return promote ? QStringLiteral("UM") : QStringLiteral("KA");
    case 'R': return promote ? QStringLiteral("RY") : QStringLiteral("HI");
    case 'K': return QStringLiteral("OU");

    // 後手の未成駒
    case 'p': return promote ? QStringLiteral("TO") : QStringLiteral("FU");
    case 'l': return promote ? QStringLiteral("NY") : QStringLiteral("KY");
    case 'n': return promote ? QStringLiteral("NK") : QStringLiteral("KE");
    case 's': return promote ? QStringLiteral("NG") : QStringLiteral("GI");
    case 'g': return QStringLiteral("KI");
    case 'b': return promote ? QStringLiteral("UM") : QStringLiteral("KA");
    case 'r': return promote ? QStringLiteral("RY") : QStringLiteral("HI");
    case 'k': return QStringLiteral("OU");

    default:
        qCWarning(lcNetwork) << "Unknown piece character:" << pieceToChar(piece);
        return QString();
    }
}

QString CsaMoveConverter::csaPieceToUsi(const QString& csaPiece)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("FU"), QStringLiteral("P")},
        {QStringLiteral("KY"), QStringLiteral("L")},
        {QStringLiteral("KE"), QStringLiteral("N")},
        {QStringLiteral("GI"), QStringLiteral("S")},
        {QStringLiteral("KI"), QStringLiteral("G")},
        {QStringLiteral("KA"), QStringLiteral("B")},
        {QStringLiteral("HI"), QStringLiteral("R")},
        {QStringLiteral("OU"), QStringLiteral("K")},
        {QStringLiteral("TO"), QStringLiteral("+P")},
        {QStringLiteral("NY"), QStringLiteral("+L")},
        {QStringLiteral("NK"), QStringLiteral("+N")},
        {QStringLiteral("NG"), QStringLiteral("+S")},
        {QStringLiteral("UM"), QStringLiteral("+B")},
        {QStringLiteral("RY"), QStringLiteral("+R")}
    };
    const QString usi = map.value(csaPiece);
    if (usi.isEmpty()) {
        qCWarning(lcNetwork) << "csaPieceToUsi: unknown piece code:" << csaPiece;
    }
    return usi;
}

QString CsaMoveConverter::usiPieceToCsa(const QString& usiPiece, bool promoted)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("P"), QStringLiteral("FU")},
        {QStringLiteral("L"), QStringLiteral("KY")},
        {QStringLiteral("N"), QStringLiteral("KE")},
        {QStringLiteral("S"), QStringLiteral("GI")},
        {QStringLiteral("G"), QStringLiteral("KI")},
        {QStringLiteral("B"), QStringLiteral("KA")},
        {QStringLiteral("R"), QStringLiteral("HI")},
        {QStringLiteral("K"), QStringLiteral("OU")}
    };
    static const QHash<QString, QString> promotedMap = {
        {QStringLiteral("P"), QStringLiteral("TO")},
        {QStringLiteral("L"), QStringLiteral("NY")},
        {QStringLiteral("N"), QStringLiteral("NK")},
        {QStringLiteral("S"), QStringLiteral("NG")},
        {QStringLiteral("B"), QStringLiteral("UM")},
        {QStringLiteral("R"), QStringLiteral("RY")}
    };

    if (promoted) {
        const QString csa = promotedMap.value(usiPiece);
        if (csa.isEmpty()) {
            qCWarning(lcNetwork) << "usiPieceToCsa: unknown promoted piece:" << usiPiece;
        }
        return csa;
    }
    const QString csa = map.value(usiPiece);
    if (csa.isEmpty()) {
        qCWarning(lcNetwork) << "usiPieceToCsa: unknown piece:" << usiPiece;
    }
    return csa;
}

QString CsaMoveConverter::csaPieceToKanji(const QString& csaPiece)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("FU"), QStringLiteral("歩")},
        {QStringLiteral("KY"), QStringLiteral("香")},
        {QStringLiteral("KE"), QStringLiteral("桂")},
        {QStringLiteral("GI"), QStringLiteral("銀")},
        {QStringLiteral("KI"), QStringLiteral("金")},
        {QStringLiteral("KA"), QStringLiteral("角")},
        {QStringLiteral("HI"), QStringLiteral("飛")},
        {QStringLiteral("OU"), QStringLiteral("玉")},
        {QStringLiteral("TO"), QStringLiteral("と")},
        {QStringLiteral("NY"), QStringLiteral("成香")},
        {QStringLiteral("NK"), QStringLiteral("成桂")},
        {QStringLiteral("NG"), QStringLiteral("成銀")},
        {QStringLiteral("UM"), QStringLiteral("馬")},
        {QStringLiteral("RY"), QStringLiteral("龍")}
    };
    return map.value(csaPiece, csaPiece);
}

int CsaMoveConverter::pieceTypeFromCsa(const QString& csaPiece)
{
    if (csaPiece == QStringLiteral("FU") || csaPiece == QStringLiteral("TO")) return 1;
    if (csaPiece == QStringLiteral("KY") || csaPiece == QStringLiteral("NY")) return 2;
    if (csaPiece == QStringLiteral("KE") || csaPiece == QStringLiteral("NK")) return 3;
    if (csaPiece == QStringLiteral("GI") || csaPiece == QStringLiteral("NG")) return 4;
    if (csaPiece == QStringLiteral("KI")) return 5;
    if (csaPiece == QStringLiteral("KA") || csaPiece == QStringLiteral("UM")) return 6;
    if (csaPiece == QStringLiteral("HI") || csaPiece == QStringLiteral("RY")) return 7;
    if (csaPiece == QStringLiteral("OU")) return 8;
    return 0;
}

Piece CsaMoveConverter::csaPieceToSfenPiece(const QString& csaPiece, bool isBlack)
{
    Piece blackPiece;
    if (csaPiece == QStringLiteral("FU")) blackPiece = Piece::BlackPawn;
    else if (csaPiece == QStringLiteral("KY")) blackPiece = Piece::BlackLance;
    else if (csaPiece == QStringLiteral("KE")) blackPiece = Piece::BlackKnight;
    else if (csaPiece == QStringLiteral("GI")) blackPiece = Piece::BlackSilver;
    else if (csaPiece == QStringLiteral("KI")) blackPiece = Piece::BlackGold;
    else if (csaPiece == QStringLiteral("KA")) blackPiece = Piece::BlackBishop;
    else if (csaPiece == QStringLiteral("HI")) blackPiece = Piece::BlackRook;
    else if (csaPiece == QStringLiteral("OU")) blackPiece = Piece::BlackKing;
    else {
        qCWarning(lcNetwork) << "csaPieceToSfenPiece: unknown piece code:" << csaPiece;
        return Piece::None;
    }

    return isBlack ? blackPiece : toWhite(blackPiece);
}
