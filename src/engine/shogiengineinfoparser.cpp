#include "shogiengineinfoparser.h"
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QObject>
#include <QMessageBox>
#include "shogigamecontroller.h"
#include "shogiutils.h"

// エンジンは、infoコマンドによって思考中の情報を返す。
// info行を解析するクラス
// コンストラクタ
ShogiEngineInfoParser::ShogiEngineInfoParser()
{
    // 駒台の段と駒の対応マップ
    m_pieceMap = {{1, 'P'}, {2, 'L'}, {3, 'N'}, {4, 'S'}, {5, 'G'}, {6, 'B'}, {7, 'R'}};

     // 駒台の駒と段の対応マップ
    m_pieceCharToIntMap = {{'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}};

    // 駒とその成駒の対応マップ
    m_promoteMap = {{'P', 'Q'}, {'L', 'M'}, {'N', 'O'}, {'S', 'T'},
                    {'B', 'C'}, {'R', 'U'}, {'p', 'q'}, {'l', 'm'},
                    {'n', 'o'}, {'s', 't'}, {'b', 'c'}, {'r', 'u'}};

    // 駒文字と漢字の駒の対応マップ
    m_pieceMapping = {{'P', "歩"}, {'L', "香"}, {'N', "桂"}, {'S', "銀"}, {'G', "金"}, {'B', "角"},
                      {'R', "飛"}, {'K', "玉"}, {'Q', "と"}, {'M', "成香"}, {'O', "成桂"},
                      {'T', "成銀"}, {'C', "馬"}, {'U', "龍"}};
}

// GUIの「探索手」の欄に表示する読み筋の最初の文字列を取得する。
QString ShogiEngineInfoParser::searchedHand() const
{
    return m_searchedHand;
}

 // エンジンが詰みを発見した場合の詰み手数をセットする。
void ShogiEngineInfoParser::setScoreMate(const QString& newScoremate)
{
    m_scoreMate = newScoremate;
}

// エンジンが現在使用しているハッシュの使用率を取得する。
QString ShogiEngineInfoParser::hashfull() const
{
    return m_hashfull;
}

// 現在の読み筋を漢字表示で表した文字列を取得する。
QString ShogiEngineInfoParser::pvKanjiStr() const
{
    return m_pvKanjiStr;
}

// USI形式の読み筋を取得する。
QString ShogiEngineInfoParser::pvUsiStr() const
{
    return m_pvUsiStr;
}

// エンジンが詰みを発見した場合の詰み手数を取得する。
QString ShogiEngineInfoParser::scoreMate() const
{
    return m_scoreMate;
}

// エンジンによる現在の評価値を取得する。
QString ShogiEngineInfoParser::scoreCp() const
{
    return m_scoreCp;
}

// 思考を開始してから経過した時間（単位はミリ秒）を取得する。
QString ShogiEngineInfoParser::time() const
{
    return m_time;
}

// 1秒あたりの探索局面数を取得する。
QString ShogiEngineInfoParser::nps() const
{
    return m_nps;
}

// 思考開始から探索したノード数を取得する。
QString ShogiEngineInfoParser::nodes() const
{
    return m_nodes;
}

// pvで初手の異なる複数の読み筋を返す時、それがn通りあれば、最も良い（評価値の高い）ものを返す。
QString ShogiEngineInfoParser::multipv() const
{
    return m_multipv;
}

// 現在選択的に読んでいる手の探索深さを取得する。
QString ShogiEngineInfoParser::seldepth() const
{
    return m_seldepth;
}

// 現在思考中の手の探索深さを取得する。
QString ShogiEngineInfoParser::depth() const
{
    return m_depth;
}

// 指し手文字列の三角マークを返す。
// 例．「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
QString ShogiEngineInfoParser::getMoveSymbol(const int moveIndex, const ShogiGameController* algorithm, const bool isPondering) const
{
    Q_UNUSED(algorithm)  // 現在のcurrentPlayer()は使用しない（局面更新の影響を避けるため）
    
    QString symbol;

    // 思考開始時の手番を使用（bestmove後の局面更新の影響を受けない）
    bool thinkingPlayerIsPlayer1 = (m_thinkingStartPlayer == ShogiGameController::Player1);

    // moveIndex:      0 1 2 3 4 5 6 7 8 9
    // moveIndex & 1:  0 1 0 1 0 1 0 1 0 1
    // isPondering=false の場合:
    //   moveIndex=0（偶数）: (0 & 1) ^ 0 = 0 → 思考側の手
    //   moveIndex=1（奇数）: (1 & 1) ^ 0 = 1 → 相手側の手
    // isPondering=true の場合:
    //   moveIndex=0（偶数）: (0 & 1) ^ 1 = 1 → 相手側の手（ponderは相手の予想手から始まる）
    //   moveIndex=1（奇数）: (1 & 1) ^ 1 = 0 → 思考側の手
    if ((moveIndex & 1) ^ isPondering) {
        // 奇数（または ponder時の偶数）: 相手側の手
        symbol = thinkingPlayerIsPlayer1 ? "△" : "▲";
    } else {
        // 偶数（または ponder時の奇数）: 思考側の手
        symbol = thinkingPlayerIsPlayer1 ? "▲" : "△";
    }

    return symbol;
}

// 直前の指し手と同じマスに指す場合、「同」を付ける。（同歩、同銀など）
QString ShogiEngineInfoParser::getFormattedMove(int fileTo, int rankTo, const QString& kanji) const
{
    if ((fileTo == previousFileTo()) && (rankTo == previousRankTo())) {
        return "同" + kanji;
    }

    return ShogiUtils::transFileTo(fileTo) + ShogiUtils::transRankTo(rankTo) + kanji;
}

// 将棋エンジンから受信した読み筋を将棋の指し手の文字列に変換する。
// 例．
// 「7g7h 2f2e 8e8f 2e2d 2c2d 8i7g 8f8g+」
// 「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
QString ShogiEngineInfoParser::convertMoveToShogiString(const QString& kanji, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo,
                                                        const bool promote, const ShogiGameController* algorithm, const int moveIndex, const bool isPondering)
{
    // 指し手の文字列
    QString moveStr;

    // 指し手文字列の三角マークを返す。
    // 例．「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
    moveStr += getMoveSymbol(moveIndex, algorithm, isPondering);

    // 直前の指し手と同じマスに指す場合、「同」を付ける。（同歩、同銀など）
    moveStr += getFormattedMove(fileTo, rankTo, kanji);

    // 成る手の場合
    if (promote) moveStr += "成";

    // 駒台から駒を打った場合（5六歩打など）
    if (fileFrom == STAND_FILE) {
        moveStr += "打";
    }
    // 盤内の駒を動かして指した場合
    else {
        //例．「△７八馬(77)」の「(77)」を付加する。
        moveStr += "(" + QString::number(fileFrom) + QString::number(rankFrom) + ")";
    }

    return moveStr;
}

// 段を示す文字を整数に変換する。
// 文字'a'から'i'までを1から9に変換する。
int ShogiEngineInfoParser::convertRankCharToInt(const QChar rankChar)
{
    // アルファベットの文字（A-Zまたはa-z）であるかどうか、
    // かつ'i'またはそれより前（アルファベット順）の文字であることを確認する。
    if (rankChar.isLetter() && rankChar.toLatin1() <= 'i') {
        return rankChar.toLatin1() - 'a' + 1;
    }
    // それ以外の文字の場合
    else {
        // エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::convertRankCharToInt. Invalid character conversion %1.").arg(rankChar);

        emit errorOccurred(errorMessage);

        return -1;
    }
}

// 駒文字を駒台の段番号に変換する。
int ShogiEngineInfoParser::convertPieceToStandRank(const QChar pieceChar)
{
    if (m_pieceCharToIntMap.contains(pieceChar)) {
        return m_pieceCharToIntMap.value(pieceChar);
    } else {
        // 駒文字に対応する段番号が存在しない場合、-1を返す。
        return -1;
    }
}

// 引数で与えられた文字が将棋盤のランクを表す文字（'a'から'i'）であるかどうかをチェックする。
bool ShogiEngineInfoParser::isBoardRankChar(const QChar rankChar) const
{
    return rankChar.isLetter() && rankChar.toLatin1() <= 'i';
}

// 指し手を表す文字列から指し手のマスの座標と成るかどうかのフラグを取得する。
int ShogiEngineInfoParser::parseMoveString(const QString& moveStr, int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, bool& promote)
{
    // 初期化
    fileFrom = rankFrom = fileTo = rankTo = 0;
    promote = false;

    // 指し手を表す文字列が4文字未満ならばエラー処理を行い、打ち切り
    if (moveStr.length() < 4) {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The length of the move string %1 is insufficient.").arg(moveStr);
        emit errorOccurred(errorMessage);
        return -1; // ★ 打ち切り
    }

    // 指し手を表す文字列の最初の文字を指すQChar型のポインタ
    const QChar* moveChars = moveStr.data();

    // fileFrom を取得
    if (moveChars[0].isDigit()) {
        // 盤上の駒
        fileFrom = moveChars[0].digitValue();
    } else {
        // 駒台の駒（例: "G*5b"）
        const int standPieceNumber = convertPieceToStandRank(moveChars[0]); // 1..7 or -1
        if (standPieceNumber >= 1 && standPieceNumber <= 7) {
            fileFrom = STAND_FILE; // 盤外（駒台）を示す特別値
        } else {
            // 指し手以外（例: "(57.54%)"）→ 特別ケース
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
            return -1; // ★ 打ち切り
        }
    } else if (moveChars[1] == QLatin1Char('*')) {
        // 駒打ち："G*5b" のようなケース
        const int standPieceNumber = convertPieceToStandRank(moveChars[0]); // 1..7 or -1
        if (standPieceNumber >= 1 && standPieceNumber <= 7) {
            rankFrom = standPieceNumber;
        } else {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Invalid stand piece specification.");
            emit errorOccurred(errorMessage);
            return -1; // ★ 打ち切り
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the source square are invalid.");
        emit errorOccurred(errorMessage);
        return -1; // ★ 打ち切り
    }

    // fileTo を取得
    if (moveChars[2].isDigit()) {
        fileTo = moveChars[2].digitValue();
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination file are invalid.");
        emit errorOccurred(errorMessage);
        return -1; // ★ 打ち切り
    }

    // rankTo を取得
    if (isBoardRankChar(moveChars[3])) {
        rankTo = convertRankCharToInt(moveChars[3]);
        if (rankTo <= 0) {
            const QString errorMessage =
                tr("An error occurred in ShogiEngineInfoParser::parseMoveString. Failed to convert destination rank.");
            emit errorOccurred(errorMessage);
            return -1; // ★ 打ち切り
        }
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiEngineInfoParser::parseMoveString. The coordinates of the destination square are invalid.");
        qDebug() << "moveChars[3] = " << moveChars[3];
        emit errorOccurred(errorMessage);
        return -1; // ★ 打ち切り
    }

    // 成/不成フラグ（5文字目が '+' のとき）
    promote = (moveStr.size() > 4 && moveChars[4] == QLatin1Char('+'));

    return 0;
}

// 直前の指し手の筋を返す。
int ShogiEngineInfoParser::previousFileTo() const
{
    return m_previousFileTo;
}

// 直前の指し手の筋を設定する。
void ShogiEngineInfoParser::setPreviousFileTo(int newPreviousFileTo)
{
    m_previousFileTo = newPreviousFileTo;
}

// 直前の指し手の段を返す。
int ShogiEngineInfoParser::previousRankTo() const
{
    return m_previousRankTo;
}

// 直前の指し手の段を設定する。
void ShogiEngineInfoParser::setPreviousRankTo(int newPreviousRankTo)
{
    m_previousRankTo = newPreviousRankTo;
}

// 思考開始時の手番を設定する
void ShogiEngineInfoParser::setThinkingStartPlayer(ShogiGameController::Player player)
{
    m_thinkingStartPlayer = player;
}

// 思考開始時の手番を取得する
ShogiGameController::Player ShogiEngineInfoParser::thinkingStartPlayer() const
{
    return m_thinkingStartPlayer;
}

// 駒文字から漢字の駒を返す。
QString ShogiEngineInfoParser::getPieceKanjiName(QChar symbol)
{
    // 駒文字を大文字に変換する。
    symbol = symbol.toUpper();

    // 駒文字が対応する漢字の駒のマッピングに存在する場合
    if (m_pieceMapping.contains(symbol)) {
        // 駒文字に対応する漢字の駒を返す。
        return m_pieceMapping[symbol];
    }
    // 駒文字が対応する漢字の駒のマッピングに存在しない場合
    else {
        // 無効な文字が検出された場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceKanjiName. The piece character '%1' does not exist.").arg(symbol);

        emit errorOccurred(errorMessage);

        return QString();
    }
}

// 指定した位置の駒を表す文字を返す。
QChar ShogiEngineInfoParser::getPieceCharacter(const QVector<QChar>& boardData, const int file, const int rank)
{
    // 盤上の駒の場合
    if ((file >= 1) && (file <= 9)) {
        // 盤上の駒文字を返す。
        return boardData.at((rank - 1) * BOARD_SIZE + (file - 1));
    }
    // 駒台の駒の場合
    else if (file == STAND_FILE) {
        // 駒台の段に対応する駒文字を取得する。
        auto iter = m_pieceMap.find(rank);

        // 対応する駒文字が存在する場合
        if (iter != m_pieceMap.end()) {
            // 駒台の段に対応する駒文字を返す。
            return iter.value();
        } else {
            // 無効な文字が検出された場合、エラーメッセージを表示する。
            QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceCharacter. The rank value is invalid.");

            qDebug() << "rank = " << rank;

            emit errorOccurred(errorMessage);

            return QChar();
        }
    }
    // 筋番号が範囲外の場合
    else {
        // 不正な筋番号file値に対するエラーメッセージを表示する
        QString errorMessage = tr("An error occurred in ShogiEngineInfoParser::getPieceCharacter. The file value is invalid.");

        qDebug() << "file = " << file;

        emit errorOccurred(errorMessage);

        return QChar();
    }
}

// 将棋盤のマスに駒を配置する。
bool ShogiEngineInfoParser::setData(QVector<QChar>& boardData, const int file, const int rank, const QChar piece) const
{
    int index = (rank - 1) * BOARD_SIZE + (file - 1);

    if (boardData.at(index) == piece) return false;

    boardData[index] = piece;

    return true;
}

// 駒を指定したマスへ移動する。配置データのみを更新する。
void ShogiEngineInfoParser::movePieceToSquare(QVector<QChar>& boardData, QChar movingPiece, int fileFrom, int rankFrom,
                                              int fileTo, int rankTo, bool promote) const
{
    // 駒が成る場合
    if (promote) {
        QChar promotedPiece;

        // 成り駒の文字を取得する。
        if (m_promoteMap.contains(movingPiece)) {
            promotedPiece = m_promoteMap[movingPiece];
        }

        // 指した先に成った駒を設定し、元の位置を空白にする。
        setData(boardData, fileTo, rankTo, promotedPiece);
        setData(boardData, fileFrom, rankFrom, ' ');
    }
    // 不成の場合
    else {
        // 盤上の駒を動かす場合、元の位置を空白にする。
        if ((fileFrom >= 1) && (fileFrom <= 9)) {
            setData(boardData, fileFrom, rankFrom, ' ');
        }

        // 新しい位置に駒を配置する
        setData(boardData, fileTo, rankTo, movingPiece);
    }
}

// 将棋エンジンからinfo currmove <move>が返された場合、その漢字の指し手に変換する。
// 将棋エンジンからbestmove相当の現在指し手を漢字表記へ（currmove）
QString ShogiEngineInfoParser::convertCurrMoveToKanjiNotation(const QString& str, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                              const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(str, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0) {
        // 既に parseMoveString 内で emit しているのでここで打ち切る
        return QString(); // ★ 打ち切り
    }
    if (rc == INFO_STRING_SPECIAL_CASE) {
        return QString(); // currmoveでこのケースは通常来ないが保険
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
    if (kanjiMovePiece.isEmpty()) {
        return QString(); // ★ 打ち切り（エラー通知済み）
    }

    return convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote, algorithm, 0, isPondering);
}

// 将棋エンジンから受信したinfo行を解析し、depthなどのサブコマンドの値を取得する。
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

// scoreサブコマンドの解析を行う。
// score cp、score mate、およびそれらに関連する lowerbound、upperbound の処理を行う。
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

// lowerbound 評価値が下限値（実際にはその値を上回る可能性がある）
// upperbound 評価値が上限値（実際にはその値を下回る可能性がある）
// 評価値の境界を取得する。
ShogiEngineInfoParser::EvaluationBound ShogiEngineInfoParser::evaluationBound() const
{
    return m_evaluationBound;
}

// 評価値の境界を設定する。
void ShogiEngineInfoParser::setEvaluationBound(EvaluationBound newEvaluationBound)
{
    m_evaluationBound = newEvaluationBound;
}

// pv の情報を解析し、それに基づいて盤面を更新する。
int ShogiEngineInfoParser::parsePvAndSimulateMoves(const QStringList& pvTokens, const ShogiGameController* algorithm, QVector<QChar> clonedBoardData,
                                                   const bool isPondering)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    m_pvKanjiStr.clear();
    m_pvUsiStr = pvTokens.join(QStringLiteral(" "));  // ★ 追加: USI形式のPVを保存

    for (qsizetype i = 0; i < pvTokens.size(); ++i) {
        const QString token = pvTokens.at(i).trimmed();

        const int rc = parseMoveString(token, fileFrom, rankFrom, fileTo, rankTo, promote);
        if (rc == INFO_STRING_SPECIAL_CASE) {
            if (i == pvTokens.size() - 1) {
                // 末尾の "(57.54%)" 等はそのまま付加して終了
                m_pvKanjiStr += " " + token;
                return 0;
            }
            // 途中に来るのは想定外だが、打ち切り
            return -1; // ★ 打ち切り
        }
        if (rc < 0) {
            // parse 内で emit 済み
            return -1; // ★ 打ち切り
        }

        const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
        const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
        if (kanjiMovePiece.isEmpty()) {
            return -1; // ★ 打ち切り（通知済み）
        }

        const QString shogiStr = convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote,
                                                          algorithm, static_cast<int>(i), isPondering);

        setPreviousFileTo(fileTo);
        setPreviousRankTo(rankTo);

        m_pvKanjiStr += shogiStr;

        if (i == 0) m_searchedHand = shogiStr;

        // 盤面に 1 手適用
        movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
    }

    return 0;
}

// 指し手を解析し、その指し手に基づいてコピーした盤面データを更新する。
void ShogiEngineInfoParser::parseAndApplyMoveToClonedBoard(const QString& str, QVector<QChar>& clonedBoardData)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(str, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0) {
        // 既にエラー通知済み
        return; // ★ 打ち切り
    }
    if (rc == INFO_STRING_SPECIAL_CASE) {
        return; // 指し手ではないので無視
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    // getPieceKanjiName は不要（盤更新のみ）
    movePieceToSquare(clonedBoardData, movingPiece, fileFrom, rankFrom, fileTo, rankTo, promote);
}

// 将棋エンジンから受信したinfo行の読み筋を日本語に変換する。info行であることは確約されている。
// 例．
// 「7g7h 2f2e 8e8f 2e2d 2c2d 8i7g 8f8g+」
// 「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
void ShogiEngineInfoParser::parseEngineOutputAndUpdateState(QString& line, const ShogiGameController* algorithm, QVector<QChar>& clonedBoardData,
                                                           const bool isPondering)
{
    // 将棋エンジンから受信したinfo行から改行を除去する。
    line.remove('\n');

    // 以下のようなinfo行を空白文字を区切り文字として各文字列に分割する。
    // 例．info depth 4 seldepth 4 time 4 nodes 12510 nps 3127500 hashfull 0 score cp 218 multipv 1 pv 8e8f 8g8f 8b8f P*8g
    QStringList tokens = line.split(" ");

    // 将棋エンジンから受信したinfo行を解析し、depthなどのサブコマンドの値を取得する。
    parseEngineInfoTokens(tokens, algorithm, clonedBoardData, isPondering);

    // pvの値を取得するためinfo行を" pv "で区切る。
    QStringList pvLineTokens = line.split(" pv ");

    // info行を" pv "で区切った場合、pvLineTokens.size()は2になるはずだが、それ以外の値の場合は、info行にpvが含まれていない。
    if (pvLineTokens.size() != 2) {
        // info行にpvが含まれていない場合、stringサブコマンドの値を取得する。
        QStringList stringTokens = line.split(" string ", Qt::KeepEmptyParts);

        // stringサブコマンドの値がある場合
        if (stringTokens.size() == 2) {
            // stringサブコマンドの値を読み筋の文字列に設定する。
            // 漢字の文字列ではないが、読み筋の文字列として設定する。
            m_pvKanjiStr = stringTokens.at(1).trimmed();
        }

        // pvを含まないinfo行だった。
        return;
    }

    // info行のサブコマンドpv以降の文字列を空白文字で区切り各文字列を取得する。
    // 例．pv 8e8f 8g8f 8b8f P*8g
    QStringList pvTokens = pvLineTokens.at(1).split(" ");

    // pvの情報を解析し、それに基づいて盤面を更新する。
    parsePvAndSimulateMoves(pvTokens, algorithm, clonedBoardData, isPondering);

    // pvを含むinfo行だった。
    return;
}

// 将棋エンジンから受信した対局相手の予想手を漢字の指し手文字列に変換する。
QString ShogiEngineInfoParser::convertPredictedMoveToKanjiString(const ShogiGameController* algorithm, QString& predictedOpponentMove, QVector<QChar>& clonedBoardData)
{
    int fileFrom = 0, rankFrom = 0, fileTo = 0, rankTo = 0;
    bool promote = false;

    const int rc = parseMoveString(predictedOpponentMove, fileFrom, rankFrom, fileTo, rankTo, promote);
    if (rc < 0 || rc == INFO_STRING_SPECIAL_CASE) {
        // 既にエラー通知済み or 指し手以外
        return QString(); // ★ 打ち切り
    }

    const QChar movingPiece = getPieceCharacter(clonedBoardData, fileFrom, rankFrom);
    const QString kanjiMovePiece = getPieceKanjiName(movingPiece);
    if (kanjiMovePiece.isEmpty()) {
        return QString(); // ★ 打ち切り（通知済み）
    }

    const bool isPondering = true;
    const int i = 0;

    const QString shogiStr =
        convertMoveToShogiString(kanjiMovePiece, fileFrom, rankFrom, fileTo, rankTo, promote, algorithm, i, isPondering);

    setPreviousFileTo(fileTo);
    setPreviousRankTo(rankTo);

    return shogiStr;
}

// 盤面データを9x9のマスに表示する。
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

// 評価値の文字列を設定する。
void ShogiEngineInfoParser::setScore(const QString &newScore)
{
    m_score = newScore;
}

// 評価値の文字列を取得する。
QString ShogiEngineInfoParser::score() const
{
    return m_score;
}
