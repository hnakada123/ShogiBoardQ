/// @file csamoveconverter.cpp
/// @brief CSA/USI/SFEN形式変換ユーティリティクラスの実装

#include "csamoveconverter.h"
#include "boardconstants.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

// ============================================================
// CSA <-> USI 形式変換
// ============================================================

QString CsaMoveConverter::csaToUsi(const QString& csaMove)
{
    if (csaMove.length() < 7) {
        return QString();
    }

    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    QString usiMove;

    if (fromFile == 0 && fromRank == 0) {
        // 駒打ち
        QString usiPiece = csaPieceToUsi(piece);
        usiMove = QString("%1*%2%3")
                      .arg(usiPiece)
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));
    } else {
        // 通常の移動
        usiMove = QString("%1%2%3%4")
                      .arg(fromFile)
                      .arg(QChar('a' + fromRank - 1))
                      .arg(toFile)
                      .arg(QChar('a' + toRank - 1));

        // 成り判定
        static const QStringList promotedPieces = {
            QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
            QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
        };
        if (promotedPieces.contains(piece)) {
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
        // 駒打ち: P*7f
        QChar usiPiece = usiMove[0];
        int toFile = usiMove[2].digitValue();
        QChar toRankChar = usiMove[3];
        int toRank = toRankChar.toLatin1() - 'a' + 1;

        QString csaPiece = usiPieceToCsa(QString(usiPiece), false);
        csaMove = QString("%1%2%3%4%5%6")
                      .arg(turnSign)
                      .arg(0)
                      .arg(0)
                      .arg(toFile)
                      .arg(toRank)
                      .arg(csaPiece);
    } else {
        // 通常の移動
        int fromFile = usiMove[0].digitValue();
        QChar fromRankChar = usiMove[1];
        int fromRank = fromRankChar.toLatin1() - 'a' + 1;
        int toFile = usiMove[2].digitValue();
        QChar toRankChar = usiMove[3];
        int toRank = toRankChar.toLatin1() - 'a' + 1;
        bool promote = usiMove.endsWith(QLatin1Char('+'));

        QString csaPiece;
        if (board) {
            Piece piece = board->getPieceCharacter(fromFile, fromRank);
            csaPiece = pieceCharToCsa(piece, promote);
        }

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
            piece = board->getPieceCharacter(from.x(), from.y());
            qCDebug(lcNetwork) << "Got piece from stand:" << pieceToChar(piece)
                               << "at file=" << from.x() << "rank=" << from.y();
        } else {
            // 通常の移動: validateAndMoveで既に盤面が更新されているので、
            // 移動先(to)から駒を取得する
            piece = board->getPieceCharacter(toFile, toRank);
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
        return QStringLiteral("FU");
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
    return map.value(csaPiece, QStringLiteral("P"));
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
        return promotedMap.value(usiPiece, QStringLiteral("TO"));
    }
    return map.value(usiPiece, QStringLiteral("FU"));
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
    else blackPiece = Piece::BlackPawn;

    return isBlack ? blackPiece : toWhite(blackPiece);
}

// ============================================================
// 表示用変換
// ============================================================

QString CsaMoveConverter::csaToPretty(const QString& csaMove, bool isPromotion,
                                      int prevToFile, int prevToRank, int moveCount)
{
    if (csaMove.length() < 7) {
        return csaMove;
    }

    QChar turnSign = csaMove[0];
    QString turnMark = (turnSign == QLatin1Char('+')) ? QStringLiteral("▲") : QStringLiteral("△");

    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    static const QString zenFile[] = {
        QString(), QStringLiteral("１"), QStringLiteral("２"), QStringLiteral("３"),
        QStringLiteral("４"), QStringLiteral("５"), QStringLiteral("６"),
        QStringLiteral("７"), QStringLiteral("８"), QStringLiteral("９")
    };
    static const QString kanjiRank[] = {
        QString(), QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };

    // 成りの場合は成る前の駒名に変換
    QString displayPiece = piece;
    if (isPromotion) {
        static const QHash<QString, QString> unpromoteMap = {
            {QStringLiteral("TO"), QStringLiteral("FU")},
            {QStringLiteral("NY"), QStringLiteral("KY")},
            {QStringLiteral("NK"), QStringLiteral("KE")},
            {QStringLiteral("NG"), QStringLiteral("GI")},
            {QStringLiteral("UM"), QStringLiteral("KA")},
            {QStringLiteral("RY"), QStringLiteral("HI")}
        };
        displayPiece = unpromoteMap.value(piece, piece);
    }

    QString pieceKanji = csaPieceToKanji(displayPiece);

    QString moveStr = turnMark;

    // 「同」の判定（前の指し手の移動先と同じ場合）
    if (toFile == prevToFile && toRank == prevToRank && moveCount > 0) {
        moveStr += QStringLiteral("同　") + pieceKanji;
    } else {
        moveStr += zenFile[toFile] + kanjiRank[toRank] + pieceKanji;
    }

    // 成る手の場合は「成」を追加
    if (isPromotion) {
        moveStr += QStringLiteral("成");
    }

    // 駒打ちの場合
    if (fromFile == 0 && fromRank == 0) {
        moveStr += QStringLiteral("打");
    } else {
        moveStr += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");
    }

    return moveStr;
}

// ============================================================
// 対局結果文字列変換
// ============================================================

QString CsaMoveConverter::gameResultToString(CsaClient::GameResult result)
{
    switch (result) {
    case CsaClient::GameResult::Win:      return tr("勝ち");
    case CsaClient::GameResult::Lose:     return tr("負け");
    case CsaClient::GameResult::Draw:     return tr("引き分け");
    case CsaClient::GameResult::Censored: return tr("打ち切り");
    case CsaClient::GameResult::Chudan:   return tr("中断");
    default:                               return tr("不明");
    }
}

QString CsaMoveConverter::gameEndCauseToString(CsaClient::GameEndCause cause)
{
    switch (cause) {
    case CsaClient::GameEndCause::Resign:          return tr("投了");
    case CsaClient::GameEndCause::TimeUp:          return tr("時間切れ");
    case CsaClient::GameEndCause::IllegalMove:     return tr("反則");
    case CsaClient::GameEndCause::Sennichite:      return tr("千日手");
    case CsaClient::GameEndCause::OuteSennichite:  return tr("連続王手の千日手");
    case CsaClient::GameEndCause::Jishogi:         return tr("入玉宣言");
    case CsaClient::GameEndCause::MaxMoves:        return tr("手数制限");
    case CsaClient::GameEndCause::Chudan:          return tr("中断");
    case CsaClient::GameEndCause::IllegalAction:   return tr("不正行為");
    default:                                        return tr("不明");
    }
}

// ============================================================
// 盤面操作
// ============================================================

bool CsaMoveConverter::applyMoveToBoard(const QString& csaMove, ShogiGameController* gc,
                                        QStringList& usiMoves, QStringList* sfenHistory,
                                        int moveCount)
{
    if (!gc || !gc->board()) {
        return false;
    }

    if (csaMove.length() < 7) {
        return false;
    }

    QChar turnSign = csaMove[0];
    int fromFile = csaMove[1].digitValue();
    int fromRank = csaMove[2].digitValue();
    int toFile = csaMove[3].digitValue();
    int toRank = csaMove[4].digitValue();
    QString piece = csaMove.mid(5, 2);

    bool isPromoted = (piece == QStringLiteral("TO") || piece == QStringLiteral("NY") ||
                       piece == QStringLiteral("NK") || piece == QStringLiteral("NG") ||
                       piece == QStringLiteral("UM") || piece == QStringLiteral("RY"));

    ShogiBoard* board = gc->board();

    if (fromFile != 0 || fromRank != 0) {
        // 通常移動
        Piece selectedPiece = board->getPieceCharacter(fromFile, fromRank);
        Piece capturedPiece = board->getPieceCharacter(toFile, toRank);

        if (capturedPiece != Piece::None) {
            board->addPieceToStand(capturedPiece);
        }

        board->movePieceToSquare(selectedPiece, fromFile, fromRank, toFile, toRank, isPromoted);
    } else {
        // 駒打ち
        Piece dropPiece = csaPieceToSfenPiece(piece, turnSign == QLatin1Char('+'));
        board->decrementPieceOnStand(dropPiece);
        board->movePieceToSquare(dropPiece, 0, 0, toFile, toRank, false);
    }

    gc->changeCurrentPlayer();

    // USI形式の指し手をリストに追加
    QString usiMove = csaToUsi(csaMove);
    if (!usiMove.isEmpty()) {
        usiMoves.append(usiMove);
    }

    // SFEN記録を更新
    QString boardSfen = board->convertBoardToSfen();
    QString standSfen = board->convertStandToSfen();
    QString currentPlayerStr = (gc->currentPlayer() == ShogiGameController::Player1)
                               ? QStringLiteral("b") : QStringLiteral("w");
    QString fullSfen = QString("%1 %2 %3 %4")
                           .arg(boardSfen, currentPlayerStr, standSfen)
                           .arg(moveCount + 1);
    if (sfenHistory) {
        sfenHistory->append(fullSfen);
    }

    return true;
}
