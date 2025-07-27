#include "kiftosfenconverter.h"
#include "shogimove.h"
#include <QString>
#include <QRegularExpression>
#include <QDebug>

// 指し手の行を表す正規表現
const QRegularExpression KifToSfenConverter::rKifMove(R"(^\s*([0-9]+)\s*(?:((?:[1-9][1-9]|同\s?)成?[歩香桂銀金角飛と杏圭全馬竜龍玉王][打不成左直右上寄引]*(?:\([1-9][1-9]\))?)|(\S+))\s*(\(\s*([0-9]+):([0-9]+(?:\.[0-9]+)?)\s*\/\s*([0-9]+):([0-9]+):([0-9]+(?:\.[0-9]+)?)\))?)");

// 「同」が含まれる指し手の行を表す正規表現
const QRegularExpression KifToSfenConverter::rSameSpotMove(R"(^同\S)");

// 消費時間に関する行を表す正規表現
const QRegularExpression KifToSfenConverter::rMoveTime(R"([0-9]+:[0-9][0-9]/[0-9]+:[0-9][0-9]:[0-9][0-9])");

// 指し手の終了を表す行を表す正規表現
const QRegularExpression KifToSfenConverter::rGameEnd(R"(中断|投了|持将棋|千日手|詰み|切れ負け|反則勝ち|反則負け|入玉勝ち|不戦勝|不戦敗)");

// KIF形式の棋譜をSFEN形式に変換するクラス
// コンストラクタ
KifToSfenConverter::KifToSfenConverter() {}

// 棋譜データのリストからSFEN形式の棋譜データを生成する。
void KifToSfenConverter::convertKifToSfen(const QStringList& originalKifuLines, QStringList& extractedMoves, QStringList& moveDurations,
                                          QStringList* sfenMovesList, QVector<ShogiMove>& gameMoves, QString& sentePlayerName, QString& gotePlayerName,
                                          QString& uwatePlayerName, QString& shitatePlayerName, QString& handicapType)
{
    // KIF形式の棋譜データから指し手部分を取り出して格納する。
    extractedMoves = extractMovesFromKifu(originalKifuLines);

    // 手合割文字を取得する。
    handicapType = findHandicapInKifu(originalKifuLines);

    //begin
    std::cout << "手合割：" << handicapType.toStdString() << std::endl;
    //end

    // 手合割のSFEN文字列を取得する。
    QString handicapSFEN = getHandicapSFENString(handicapType);

    //begin
    qDebug() << "SFEN: " << handicapSFEN;
    //end

    if (handicapType == "平手") {
        // 棋譜データのリストから先手の対局者の文字列を検出して返す。
        sentePlayerName = findSentePlayerInKifu(originalKifuLines);

        // 棋譜データのリストから後手の対局者の文字列を検出して返す。
        gotePlayerName = findGotePlayerInKifu(originalKifuLines);
    }
    // 駒落ちの場合
    else {
        // 棋譜データのリストから先手の対局者の文字列を検出して返す。
        uwatePlayerName = findUwatePlayerInKifu(originalKifuLines);

        // 棋譜データのリストから後手の対局者の文字列を検出して返す。
        shitatePlayerName = findShitatePlayerInKifu(originalKifuLines);

        // 上手と下手の対局者名が両方とも空の場合
        if ((uwatePlayerName.isEmpty() && shitatePlayerName.isEmpty())) {
            // 棋譜データのリストから先手の対局者の文字列を検出して返す。
            sentePlayerName = findSentePlayerInKifu(originalKifuLines);

            // 棋譜データのリストから後手の対局者の文字列を検出して返す。
            gotePlayerName = findGotePlayerInKifu(originalKifuLines);
        }
    }

    // 棋譜ファイル全体。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換。
    QStringList kifuLinesWithHalfWidthChars = convertMovesToHalfWidth(originalKifuLines);

    // 指し手部分。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換したもので同を数字に置き換えたもの。
    QStringList kifuMovesAsNumbers;

    // 指し手全てSFEN形式に変換し、sfenMovesListに格納する。
    moveList(kifuLinesWithHalfWidthChars, kifuMovesAsNumbers, moveDurations);

    // 指し手全てをSFEN形式に変換し、指定された引数に格納する。
    makeSfenStr(kifuMovesAsNumbers, sfenMovesList, handicapSFEN, gameMoves, handicapType);
}

// KIF形式の棋譜データから指し手部分を抽出し、指し手のリストに格納する。
QStringList KifToSfenConverter::extractMovesFromKifu(const QStringList& originalKifuLines) const
{
    // 指し手を格納するリスト
    QStringList moves;

    for (const QString& line : originalKifuLines) {
        QRegularExpressionMatch match = rKifMove.match(line);

        if (match.hasMatch()) {
            // 例．
            // 118 ９七桂成(85)       ( 0:00/00:00:20)
            // captured(1) 118
            // captured(2) ９七桂成(85)
            // captured(3)
            // ===============================
            // 119 同　玉(96)        ( 0:00/00:00:10)
            // captured(1) 119
            // captured(2)
            // captured(3) 同　玉(96)
            QString move = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);

            if (!move.isEmpty()) {
                moves.append(move);
            }
        }
    }

    return moves;
}

// 棋譜データのリストから指定された正規表現にマッチする文字列を検出して返す。
QString KifToSfenConverter::findPatternInKifu(const QStringList& originalKifuLines, const QRegularExpression& pattern) const
{
    for (const QString& line : originalKifuLines) {
        QRegularExpressionMatch match = pattern.match(line);

        if (match.hasMatch()) {
            return match.captured(1);
        }
    }

    return QString();
}

// 棋譜データのリストから手合割の文字列を検出して返す。
QString KifToSfenConverter::findHandicapInKifu(const QStringList &originalKifuLines) const
{
    static QRegularExpression handicapRegex("手合割：(.*)");
    return findPatternInKifu(originalKifuLines, handicapRegex);
}

// 棋譜データのリストから先手の対局者の文字列を検出して返す。
QString KifToSfenConverter::findSentePlayerInKifu(const QStringList &originalKifuLines) const
{
    static QRegularExpression senteRegex("先手：(.*)$");
    return findPatternInKifu(originalKifuLines, senteRegex);
}

// 棋譜データのリストから後手の対局者の文字列を検出して返す。
QString KifToSfenConverter::findGotePlayerInKifu(const QStringList &originalKifuLines) const
{
    static QRegularExpression goteRegex("後手：(.*)$");
    return findPatternInKifu(originalKifuLines, goteRegex);
}

// 棋譜データのリストから上手の対局者の文字列を検出して返す。
QString KifToSfenConverter::findUwatePlayerInKifu(const QStringList &originalKifuLines) const
{
    static QRegularExpression uwateRegex("上手：(.*)$");
    return findPatternInKifu(originalKifuLines, uwateRegex);
}

// 棋譜データのリストから下手の対局者の文字列を検出して返す。
QString KifToSfenConverter::findShitatePlayerInKifu(const QStringList &originalKifuLines) const
{
    static QRegularExpression shitateRegex("下手：(.*)$");
    return findPatternInKifu(originalKifuLines, shitateRegex);
}

// 指定された列の文字列からSFEN形式の文字列を生成する。連続する0の数をその数値で置き換える。
QString KifToSfenConverter::convertToSfenString(const QString& fileString) const
{
    // SFEN形式の文字列を構築するための変数
    QString sfenString;

    // 連続する0の数をカウントする変数
    int consecutiveZerosCount = 0;

    // 入力された列の文字列を一文字ずつ処理する。
    for (QChar ch : fileString) {
        if (ch == '0') {
            // 0を見つけたらカウントアップ
            consecutiveZerosCount++;
        } else {
            if (consecutiveZerosCount > 0) {
                // 0の連続が途切れたら、その数を文字列に追加する。
                sfenString += QString::number(consecutiveZerosCount);

                // 0のカウントをリセット
                consecutiveZerosCount = 0;
            }

            // 0以外の文字を結果の文字列に追加する。
            sfenString += ch;
        }
    }

    // 0の連続が最後まで続いていた場合、その数を文字列に追加する。
    if (consecutiveZerosCount > 0) {
        sfenString += QString::number(consecutiveZerosCount);
    }

    // 変換後のSFEN形式の文字列を返す。
    return sfenString;
}

// 指し手の文字列を全角から半角に変換し、漢数字も半角数字に変換する。
QStringList KifToSfenConverter::convertMovesToHalfWidth(const QStringList& originalKifuLines) const
{
    // 棋譜データのコピーを作成
    QStringList convertedMoves = originalKifuLines;

    // 漢字全角と漢数字を半角数字に変換するマッピング
    QMap<QString, QString> conversionMap = {
        {"１", "1"}, {"２", "2"}, {"３", "3"},
        {"４", "4"}, {"５", "5"}, {"６", "6"},
        {"７", "7"}, {"８", "8"}, {"９", "9"},
        {"一", "1"}, {"二", "2"}, {"三", "3"},
        {"四", "4"}, {"五", "5"}, {"六", "6"},
        {"七", "7"}, {"八", "8"}, {"九", "9"},
        {"\u3000", ""} // 全角スペースを削除
    };

    // 変換マップを使用して、各指し手の文字列を変換する。
    for (auto it = conversionMap.cbegin(); it != conversionMap.cend(); ++it) {
        convertedMoves.replaceInStrings(it.key(), it.value());
    }

    // 変換後のリストを返す。
    return convertedMoves;
}

// 指し手全てSFEN形式に変換し、sfenMovesListに格納する。
// kifuLinesWithHalfWidthChars 棋譜ファイル全体。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換。
// kifuMovesAsNumbers 指し手部分。ただし、指し手部分の漢字全角を半角、かつ漢数字を半角数字に変換したもので同を数字に置き換えたもの。
// moveDurations 指し手部分の消費時間
void KifToSfenConverter::moveList(const QStringList& kifuLinesWithHalfWidthChars, QStringList& kifuMovesAsNumbers, QStringList& moveDurations)
{
    QString beforeStr = "";

    for (int i = 0; i < kifuLinesWithHalfWidthChars.size(); ++i) {
        // 指し手の行の正規表現
        QRegularExpressionMatch match = rKifMove.match(kifuLinesWithHalfWidthChars.at(i));
        if (match.hasMatch()) {
            // 指し手の文字列を取得する。
            QString currentMove = match.captured(2);

            // 消費時間を取得する。
            moveDurations.append(extractTime(kifuLinesWithHalfWidthChars.at(i)));

            // 指し手に"同"の文字が含まれている場合、前の指し手と同じマスの位置を返す。
            currentMove = handleSamePosition(currentMove, beforeStr);

            // 指し手の行が終了を表す行か判定して、指し手をリストに追加する。
            appendMatchToEndList(kifuLinesWithHalfWidthChars.at(i), kifuMovesAsNumbers, currentMove);
        }
    }
}

// 指定された行から消費時間を抽出して返す。
QString KifToSfenConverter::extractTime(const QString& line) const
{
    // 消費時間を抽出するための正規表現
    QRegularExpressionMatch match = rMoveTime.match(line);

    if (match.hasMatch()) {
        return match.captured(0);
    }

    return QString();
}

// 指し手に"同"の文字が含まれている場合、前の指し手と同じマスの位置を返す。
QString KifToSfenConverter::handleSamePosition(QString& currentMove, QString& beforeStr) const
{
    QRegularExpressionMatch match = rSameSpotMove.match(currentMove);

    if (match.hasMatch()) {
        currentMove.replace("同", beforeStr.left(2));
    } else {
        beforeStr = currentMove;
    }

    return currentMove;
}

// 指し手の行が終了を表す行か判定して、指し手をリストに追加する。
void KifToSfenConverter::appendMatchToEndList(const QString& line, QStringList& kifuMovesAsNumbers, const QString& currentMove) const
{
    // 指し手の終了を表す行の正規表現
    QRegularExpressionMatch match = rGameEnd.match(line);

    // 指し手の行が終了を表す行である場合
    if (match.hasMatch()) {
        kifuMovesAsNumbers.append(match.captured(0));
    }
    // 指し手の行が終了を表す行でない場合
    else {
        kifuMovesAsNumbers.append(currentMove);
    }
}

// 先手駒台の駒文字に対応する段番号を返す。
int KifToSfenConverter::getRankForBlackPieceStand(const QString& pieceName) const
{
    // 駒の名前と対応する段番号のマップ
    static const QMap<QString, int> pieceToRank = {
        {"歩", 1},
        {"香", 2},
        {"桂", 3},
        {"銀", 4},
        {"金", 5},
        {"角", 6},
        {"飛", 7}
    };

    // マップから段番号を取得して返す。該当する駒がない場合は0を返す。
    return pieceToRank.value(pieceName, 0);
}

// 後手駒台の駒文字に対応する段番号を返す。
int KifToSfenConverter::getRankForWhitePieceStand(const QString& pieceName) const
{
    // 駒の名前と対応する段番号のマップ
    static const QMap<QString, int> pieceToRank = {
        {"歩", 9},
        {"香", 8},
        {"桂", 7},
        {"銀", 6},
        {"金", 5},
        {"角", 4},
        {"飛", 3}
    };

    // マップから段番号を取得して返す。該当する駒がない場合は0を返す。
    return pieceToRank.value(pieceName, 0);
}

// SFEN形式の文字列内の数字を連続した'0'の列に置き換える。
QString KifToSfenConverter::replaceNumbersWithZeros(const QString& sfen) const
{
    QString modifiedSfen;

    for (const QChar &ch : sfen) {
        if (ch.isDigit() && ch != '0') {
            // 数字を取得する。
            int count = ch.digitValue(); // 数字を取得

            // 連続する'0'の数を追加する。
            modifiedSfen += QString(count, '0');
        } else {
            modifiedSfen += ch;
        }
    }

    return modifiedSfen;
}

// SFEN形式の文字列内の成駒を1文字のアルファベットに置換する。
QString KifToSfenConverter::replacePromotedPieces(const QString& sfen) const
{
    QString modifiedSfen = sfen;

    // 両者の成駒をマップで定義
    static const QMap<QString, QChar> replacementMap = {
        // 先手の成駒
        {"+P", 'Q'}, // と金
        {"+L", 'M'}, // 成香
        {"+N", 'O'}, // 成桂
        {"+S", 'T'}, // 成銀
        {"+B", 'C'}, // 馬
        {"+R", 'U'}, // 龍
        // 後手の成駒   
        {"+p", 'q'}, // と金
        {"+l", 'm'}, // 成香
        {"+n", 'o'}, // 成桂
        {"+s", 't'}, // 成銀
        {"+b", 'c'}, // 馬
        {"+r", 'u'}  // 龍
    };

    // マップを使用して+が付いた文字を1文字のアルファベットに置換する。
    for (auto it = replacementMap.cbegin(); it != replacementMap.cend(); ++it) {
        modifiedSfen.replace(it.key(), it.value());
    }

    return modifiedSfen;
}

// 先手あるいは下手で移動元の駒台のマスの筋、段番号を取得し、移動先のマスに変換された駒文字をセットする。
void KifToSfenConverter::setBlackPlayerMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QString& pieceKanji, QChar board[9][9]) const
{
    // 駒台の筋番号をセットする。（10は、先手あるいは下手）
    standX = 10;

    // 駒台の段番号を取得する。
    standY = getRankForBlackPieceStand(pieceKanji);

    // 移動先のマスに変換された駒文字をセットする。
    board[destinationX - 1][destinationY - 1] = translateKanjiToBlackCharacter(pieceKanji);
}

// 後手あるいは上手で移動元の駒台のマスの筋、段番号を取得し、移動先のマスに変換された駒文字をセットする。
void KifToSfenConverter::setWhitePlayerMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QString& pieceKanji, QChar board[9][9]) const
{
    // 駒台の筋番号をセットする。（11は、後手あるいは上手）
    standX = 11;

    // 駒台の段番号を取得する。
    standY = getRankForWhitePieceStand(pieceKanji);

    // 移動先のマスに変換された駒文字をセットする。
    board[destinationX - 1][destinationY - 1] = translateKanjiToWhiteCharacter(pieceKanji);
}

// 移動元と移動先のマスの筋と段を取得し、移動先のマスに変換された駒文字をセットする。
void KifToSfenConverter::setMoveSourceAndDestination(int& standX, int& standY, int& destinationX, int& destinationY, const QString& pieceKanji, QChar board[9][9],
                                                     bool isPlayingAsBlack, const QString& handicapType) const
{
    // 手合割が平手の場合
    if (handicapType == "平手") {
        if (isPlayingAsBlack) {
            setBlackPlayerMoveFromStand(standX, standY, destinationX, destinationY, pieceKanji, board);
        }
        // 後手の場合
        else {
            setWhitePlayerMoveFromStand(standX, standY, destinationX, destinationY, pieceKanji, board);
        }
    }
    // 駒落ちの場合
    else {
        if (isPlayingAsBlack) {
            setWhitePlayerMoveFromStand(standX, standY, destinationX, destinationY, pieceKanji, board);
        }
        // 下手の場合
        else {
            setBlackPlayerMoveFromStand(standX, standY, destinationX, destinationY, pieceKanji, board);
        }
    }
}

void KifToSfenConverter::processMoveFromStand(int& standX, int& standY, int& destinationX, int& destinationY, const QList<QString> &kifuMovesAsNumbers, int i, QChar board[9][9], int piecesCountByPlayerAndType[2][7], const QString& handicapType)
{
     // 駒を打った場合の移動先の筋と段を取得する。
    destinationX = kifuMovesAsNumbers.at(i).left(1).toInt();
    destinationY = kifuMovesAsNumbers.at(i).mid(1, 1).toInt();

    // 先手あるいは下手かどうかを示すフラグ
    bool isPlayingAsBlack = (i % 2 == 0);

    // 打った駒の漢字を取得する。
    QString pieceKanji = kifuMovesAsNumbers.at(i).mid(2, 1);

    // 移動元と移動先のマスの筋と段を取得し、移動先のマスに変換された駒文字をセットする。
    setMoveSourceAndDestination(standX, standY, destinationX, destinationY, pieceKanji, board, isPlayingAsBlack, handicapType);

     // 打った持ち駒の枚数を減らす。
    decrementPieceCount(board[destinationX - 1][destinationY - 1], piecesCountByPlayerAndType);
}

// 盤内の駒を指した場合の移動先と移動元の筋と段を取得し、処理する。
void KifToSfenConverter::processMoveFromBoard(int& sourceX, int& sourceY, int& destinationX, int& destinationY, const QList<QString> &kifuMovesAsNumbers, int i, QChar board[9][9],
                                              int piecesCountByPlayerAndType[2][7], const QString& handicapType)
{
    static QRegularExpression re(R"(^(\d+)(\D+)\((\d+).*$)");
    QRegularExpressionMatch match = re.match(kifuMovesAsNumbers.at(i));

    if (match.hasMatch()) {
        extractMoveCoordinates(match, sourceX, sourceY, destinationX, destinationY);

        // 駒を移動させる前に元の位置を空にする
        board[sourceX - 1][sourceY - 1] = '0';

        // 移動先にある駒を取得し、持ち駒カウントを増やす
        QChar destPiece = board[destinationX - 1][destinationY - 1];
        incrementPieceCount(destPiece, piecesCountByPlayerAndType);

        // 駒の移動と翻訳を実行
        moveAndTranslatePiece(match.captured(2), destinationX, destinationY, board, i, handicapType);
    }
}

// 移動先と移動元のマスの筋と段の番号を抽出する。
void KifToSfenConverter::extractMoveCoordinates(const QRegularExpressionMatch& match, int& sourceX, int& sourceY, int& destinationX, int& destinationY) const
{
    destinationX = match.captured(1).left(1).toInt();
    destinationY = match.captured(1).mid(1, 1).toInt();
    sourceX = match.captured(3).left(1).toInt();
    sourceY = match.captured(3).mid(1, 1).toInt();
}

// 駒を移動させ、対応する文字に変換する。
void KifToSfenConverter::moveAndTranslatePiece(const QString& pieceKanji, int destX, int destY, QChar board[9][9], int moveIndex, const QString& handicapType) const
{
    bool isPlayingAsBlack = (moveIndex % 2 == 0);
    bool isHandicapInverted = (handicapType != "平手");

    // 先手または下手かどうかによって駒の翻訳を行う
    QChar translatedPiece = (isPlayingAsBlack ^ isHandicapInverted) ?
                                translateKanjiToBlackCharacter(pieceKanji) :
                                translateKanjiToWhiteCharacter(pieceKanji);

    board[destX - 1][destY - 1] = translatedPiece;
}

// 成・不成どちらで指したのかを追加する。
bool KifToSfenConverter::appendPromotionInfo(const QStringList& kifu, const int moveIndex) const
{
    // 成駒（成香、成桂、成銀）のマッチングに使用する正規表現の定義
    // 「^\S*成(香|桂|銀)」は、任意の非空白文字に続く成香、成桂、または成銀にマッチする。
    static QRegularExpression rPromotion(R"(^\S*成(香|桂|銀))");

    // 指定された手番に成駒が含まれるかチェックする。
    bool hasPromotion = rPromotion.match(kifu.at(moveIndex)).hasMatch();

    return hasPromotion;
}

// 指し手全てをSFEN形式に変換し、指定された引数に格納する。
void KifToSfenConverter::makeSfenStr(const QStringList& kifuMovesAsNumbers, QStringList* sfenMovesList, QString handicapSFEN,
                                     QVector<ShogiMove>& gameMoves, const QString& handicapType)
{
    // SFEN形式の棋譜データを格納するリストをクリアする。
    sfenMovesList->clear();

    // 手合割の盤面のSFEN形式の文字列をリストに追加する。
    sfenMovesList->append(handicapSFEN);

    // 持ち駒の数をカウントするための配列
    int piecesCountByPlayerAndType[2][7] = {};

    // SFEN形式の文字列内の数字を連続した'0'の列に置き換える。
    handicapSFEN = replaceNumbersWithZeros(handicapSFEN);

    // SFEN形式の文字列内の成駒を1文字のアルファベットに置換する。
    handicapSFEN = replacePromotedPieces(handicapSFEN);

    // 駒文字の2次元配列
    QChar board[9][9];

    // 手合割のSFEN形式の文字列を2次元配列に変換する。
    convertSfenToMatrix(handicapSFEN, board);

    // 「打」という文字が含まれている行（持ち駒から盤上に駒を打つ動作）を識別する正規表現
    static QRegularExpression rePieceDrop(R"(^\S*打$)");
    QRegularExpressionMatch matchDrop;

    // 「成」という文字が含まれている行を識別する正規表現
    static QRegularExpression rePiecePromotion(R"(^\S*成)");
    QRegularExpressionMatch matchPromotion;

    int sourceX = 0;
    int sourceY = 0;
    int destinationX = 0;
    int destinationY = 0;

    for (int i = 0; i < kifuMovesAsNumbers.size() - 1; ++i) {
        matchDrop = rePieceDrop.match(kifuMovesAsNumbers.at(i));

        QChar oldBoard[9][9];

        // 盤面をコピーしておく
        for (int x = 0; x < 9; x++) {
            for (int y = 0; y < 9; y++) {
                oldBoard[x][y] = board[x][y];
            }
        }

        // 駒台から持ち駒を打った場合
        if (matchDrop.hasMatch()) {
            // 駒台から持ち駒を打った場合の移動先と移動元の筋と段を取得し、持ち駒の枚数を減らす処理を行う。
            processMoveFromStand(sourceX, sourceY, destinationX, destinationY, kifuMovesAsNumbers, i, board, piecesCountByPlayerAndType, handicapType);
        }
        // 盤内の駒を指した場合
        else {
            // 盤内の駒を指した場合の移動先と移動元の筋と段を取得する。
            processMoveFromBoard(sourceX, sourceY, destinationX, destinationY, kifuMovesAsNumbers, i, board, piecesCountByPlayerAndType, handicapType);
        }

        matchPromotion = rePiecePromotion.match(kifuMovesAsNumbers.at(i));

        bool hasPromotion;

        // 成の文字があった場合
        if (matchPromotion.hasMatch()) {
            // 成・不成どちらで指したのかを返し、リストに追加する。
            hasPromotion = appendPromotionInfo(kifuMovesAsNumbers, i);
        } else {
            hasPromotion = false;
        }

        // 移動元のマスの駒文字
        QChar movingPiece = oldBoard[sourceX - 1][sourceY - 1];

        // 移動先のマスの駒文字
        QChar capturedPiece = oldBoard[destinationX - 1][destinationY - 1];

        // 指し手を追加する。
        gameMoves.append(ShogiMove(QPoint(sourceX - 1, sourceY - 1), QPoint(destinationX - 1, destinationY - 1), movingPiece, capturedPiece, hasPromotion));

        // 持ち駒の数をSFEN形式の文字列で表現する。
        QString strPieceInHand = createSfenForPiecesInHand(piecesCountByPlayerAndType);

        // 指し手のSFEN形式の文字列を生成し、リストに追加する。
        sfenMovesList->append(convertBoardToSfenString(board, i, strPieceInHand, handicapType));
    }

    // 再度、最後の指し手のSFEN形式の文字列をリストに追加しておく。
    sfenMovesList->append(sfenMovesList->last());

    // 再度、最後の指し手の情報をリストに追加しておく。
    gameMoves.append(gameMoves.last());
}

// 盤面の状態、手番、持ち駒をSFEN形式の文字列に変換する。
QString KifToSfenConverter::convertBoardToSfenString(const QChar board[9][9], const int turnCount, const QString& inHandPieces, const QString& handicapType) const
{
    QString sfen = "";

    // 連続する空きマスのカウント用
    int emptySquares = 0;

    // 盤面の状態をSFEN文字列に変換する。
    for (int rank = 0; rank < 9; rank++) {
        if (rank > 0) sfen.append("/");
        for (int file = 8; file >= 0; file--) {
            if (board[file][rank] == '0') {
                emptySquares++;
            } else {
                if (emptySquares > 0) {
                    sfen.append(QString::number(emptySquares));
                    emptySquares = 0;
                }
                sfen.append(board[file][rank]);
            }
        }
        if (emptySquares > 0) {
            sfen.append(QString::number(emptySquares));
            emptySquares = 0;
        }
    }

    // 駒の置換規則
    static const QMap<QChar, QString> pieceTranslations = {
        {'Q', "+P"}, {'M', "+L"}, {'O', "+N"}, {'T', "+S"}, {'C', "+B"}, {'U', "+R"},
        {'q', "+p"}, {'m', "+l"}, {'o', "+n"}, {'t', "+s"}, {'c', "+b"}, {'u', "+r"}
    };

    // 成駒を元に戻す。
    for (auto it = pieceTranslations.begin(); it != pieceTranslations.end(); ++it) {
        sfen.replace(it.key(), it.value());
    }

    // 手番（プレイヤー）を取得する。
    QString playerTurn;

    if (handicapType == "平手") {
        playerTurn = (turnCount % 2 == 0) ? "w" : "b";
    } else {
        playerTurn = (turnCount % 2 == 0) ? "b" : "w";
    }

    // 手番（プレイヤー）と手数を追加する。
    sfen.append(" ").append(playerTurn).append(" ").append(inHandPieces).append(" ").append(QString::number(turnCount + 2));

    qDebug() << "SFEN: " << sfen;

    return sfen;
}

// 先手の指し手の漢字をアルファベットの駒文字に変換する。
QChar KifToSfenConverter::translateKanjiToBlackCharacter(const QString& kanji) const
{
    static const QMap<QString, QChar> kanjiMap = {
        {"歩", 'P'},
        {"香", 'L'},
        {"桂", 'N'},
        {"銀", 'S'},
        {"金", 'G'},
        {"角", 'B'},
        {"飛", 'R'},
        {"玉", 'K'},
        {"と", 'Q'}, {"歩成", 'Q'},
        {"香成", 'M'}, {"成香", 'M'},
        {"桂成", 'O'}, {"成桂", 'O'},
        {"銀成", 'T'}, {"成銀", 'T'},
        {"角成", 'C'}, {"馬", 'C'},
        {"飛成", 'U'}, {"龍", 'U'}
    };

    // QMap::value() はキーに対応する値を返す。
    // キーが見つからない場合は、第二引数で指定されたデフォルト値（ここでは空の QString）を返す。
    return kanjiMap.value(kanji, QChar());
}

// 後手の指し手の漢字をアルファベットの駒文字に変換する。
QChar KifToSfenConverter::translateKanjiToWhiteCharacter(const QString& kanji) const
{
    static const QMap<QString, QChar> kanjiMap = {
        {"歩", 'p'},
        {"香", 'l'},
        {"桂", 'n'},
        {"銀", 's'},
        {"金", 'g'},
        {"角", 'b'},
        {"飛", 'r'},
        {"玉", 'k'},
        {"と", 'q'}, {"歩成", 'q'},
        {"香成", 'm'}, {"成香", 'm'},
        {"桂成", 'o'}, {"成桂", 'o'},
        {"銀成", 't'}, {"成銀", 't'},
        {"角成", 'c'}, {"馬", 'c'},
        {"飛成", 'u'}, {"龍", 'u'}
    };

    // QMap::value() はキーに対応する値を返す。
    // キーが見つからない場合は、第二引数で指定されたデフォルト値（ここでは空の QString）を返す。
    return kanjiMap.value(kanji, QChar());
}

// 駒文字に対応する持ち駒の枚数を1増やす。
void KifToSfenConverter::incrementPieceCount(const QChar piece, int piecesCountByPlayerAndType[2][7]) const
{
    // 駒の種類と配列インデックスのマッピング
    static const QMap<QChar, int> pieceMap = {
        {'p', 0}, {'l', 1}, {'n', 2}, {'s', 3}, {'g', 4}, {'b', 5}, {'r', 6},
        {'q', 0}, {'m', 1}, {'o', 2}, {'t', 3}, {'c', 5}, {'u', 6}
    };

    // 取った駒文字が大文字なら先手の駒を取ったことになるので、後手の持ち駒を1増やす。
    // 逆に、取った駒文字が小文字なら後手の駒を取ったことになるので、先手の持ち駒を1増やす。
    int playerIndex = piece.isUpper() ? 1 : 0;

    // 駒の種類を小文字に統一
    QChar pieceType = piece.toLower();

    // マップを使用して駒のインデックスを取得し、持ち駒を1増やす。
    if (pieceMap.contains(pieceType)) {
        int pieceTypeIndex = pieceMap[pieceType];
        piecesCountByPlayerAndType[playerIndex][pieceTypeIndex]++;
    }
}

// 駒文字に対応する持ち駒の枚数を1減らす。
void KifToSfenConverter::decrementPieceCount(const QChar piece, int piecesCountByPlayerAndType[2][7]) const
{
    // 駒の種類と配列インデックスのマッピング
    static const QMap<QChar, int> pieceMap = {
        {'p', 0}, {'l', 1}, {'n', 2}, {'s', 3}, {'g', 4}, {'b', 5}, {'r', 6},
        {'q', 0}, {'m', 1}, {'o', 2}, {'t', 3}, {'c', 5}, {'u', 6}
    };

    // 打った駒文字が大文字なら先手の駒を打ったことになるので、先手の持ち駒を1増やす。
    // 逆に、打った駒文字が小文字なら後手の駒を打ったことになるので、後手の持ち駒を1増やす。
    int playerIndex = piece.isUpper() ? 0 : 1;

    // 駒の種類を小文字に統一
    QChar pieceType = piece.toLower();

    // マップを使用して駒のインデックスを取得し、持ち駒を1減らす。
    if (pieceMap.contains(pieceType)) {
        int pieceTypeIndex = pieceMap[pieceType];
        piecesCountByPlayerAndType[playerIndex][pieceTypeIndex]--;
    }
}

// 持ち駒の数をSFEN形式の文字列で表現する。
QString KifToSfenConverter::createSfenForPiecesInHand(const int piecesCountByPlayerAndType[2][7]) const
{
    QString sfenPiecesInHand = "";

    // 持ち駒のSFEN表記に使用する文字の配列（先手用、後手用）
    const char* sfenSymbols[2][7] = {
        {"P", "L", "N", "S", "G", "B", "R"}, // 先手の持ち駒用シンボル
        {"p", "l", "n", "s", "g", "b", "r"}  // 後手の持ち駒用シンボル
    };

    // プレイヤーを示すインデックス（0: 先手、1: 後手）を用いてループ
    for (int playerIndex = 0; playerIndex <= 1; ++playerIndex) {
        // 各種持ち駒を示すインデックス（歩、香車、桂馬、銀、金、角、飛車）を用いてループ
        for (int pieceTypeIndex = 0; pieceTypeIndex <= 6; ++pieceTypeIndex) {
            int pieceCount = piecesCountByPlayerAndType[playerIndex][pieceTypeIndex];

            if (pieceCount > 0) {
                // 持ち駒が2枚以上ある場合は数を追記
                if (pieceCount > 1) {
                    sfenPiecesInHand += QString::number(pieceCount);
                }

                sfenPiecesInHand += sfenSymbols[playerIndex][pieceTypeIndex];
            }
        }
    }

    // 持ち駒が一つもない場合は"-"を返す
    if (sfenPiecesInHand.isEmpty()) {
        sfenPiecesInHand = "-";
    }

    return sfenPiecesInHand;
}

// 手合割のSFEN文字列を2次元配列に変換する。
void KifToSfenConverter::convertSfenToMatrix(const QString& handicapSFEN, QChar board[9][9]) const
{
    // SFEN文字列を'/'で分割し、盤面を行ごとに処理する。
    QStringList rows = handicapSFEN.split("/");
    for (int rank = 0; rank < rows.size(); ++rank) {
        const QString& row = rows.at(rank);

        // 筋を順番に処理するためのインデックス
        int fileIndex = 0;

        for (int j = 0; j < row.length(); ++j) {
            // 通常の駒を配列に格納
            board[8 - fileIndex][rank] = row[j];

            // ファイルインデックスをインクリメント
            ++fileIndex;
        }
    }
}

// 指定された駒落ちの種類に基づいて対応するSFEN文字列を返す。
QString KifToSfenConverter::getHandicapSFENString(const QString handicapType) const
{
    // 駒落ちの種類と対応するSFEN文字列のマップ
    static const QMap<QString, QString> handicapTypeToSFENMap = {
        {"平手", "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"},
        {"香落ち", "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"右香落ち", "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"角落ち", "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛車落ち", "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛香落ち", "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"二枚落ち", "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"三枚落ち", "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"四枚落ち", "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"五枚落ち", "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"左五枚落ち", "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"六枚落ち", "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"八枚落ち", "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"十枚落ち", "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"}
    };

    // 正規表現を用いて駒落ちの種類を検索し、対応するSFEN文字列を返す。
    for (const auto& handicapPair : handicapTypeToSFENMap.toStdMap()) {
        QRegularExpression regex(handicapPair.first);
        if (regex.match(handicapType).hasMatch()) {
            return handicapPair.second;
        }
    }

    // 該当する駒落ちの種類がない場合は空文字列を返す。
    return "";
}
