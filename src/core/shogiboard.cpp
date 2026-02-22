/// @file shogiboard.cpp
/// @brief 将棋盤の状態管理・SFEN変換・駒台操作の実装

#include "shogiboard.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCore, "shogi.core")

// ============================================================
// 初期化
// ============================================================

ShogiBoard::ShogiBoard(int ranks, int files, QObject *parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    initBoard();
    initStand();
}

void ShogiBoard::initBoard()
{
    m_boardData.fill(' ', ranks() * files());
}

void ShogiBoard::initStand()
{
    m_pieceStand.clear();

    static const QList<QChar> pieces = {'p', 'l', 'n', 's', 'g', 'b', 'r', 'k',
                                        'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K'};

    for (const QChar& piece : pieces) {
        m_pieceStand.insert(piece, 0);
    }
}

// ============================================================
// 盤面アクセス
// ============================================================

const QVector<QChar>& ShogiBoard::boardData() const
{
    return m_boardData;
}

const QMap<QChar, int>& ShogiBoard::getPieceStand() const
{
    return m_pieceStand;
}

QString ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

// 指定した位置の駒を表す文字を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
QChar ShogiBoard::getPieceCharacter(const int file, const int rank)
{
    static const QMap<int, QChar> pieceMapBlack = {{1,'P'},{2,'L'},{3,'N'},{4,'S'},{5,'G'},{6,'B'},{7,'R'},{8,'K'}};
    static const QMap<int, QChar> pieceMapWhite = {{2,'k'},{3,'r'},{4,'b'},{5,'g'},{6,'s'},{7,'n'},{8,'l'},{9,'p'}};

    if (file >= 1 && file <= 9) {
        if (rank < 1 || rank > ranks()) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return QChar(' ');
        }
        return m_boardData.at((rank - 1) * files() + (file - 1));
    } else if (file == 10) {
        const auto it = pieceMapBlack.find(rank);
        if (it != pieceMapBlack.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for black stand: %d", rank);
        return QChar();
    } else if (file == 11) {
        const auto it = pieceMapWhite.find(rank);
        if (it != pieceMapWhite.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for white stand: %d", rank);
        return QChar();
    } else {
        qCWarning(lcCore, "Invalid file value: %d", file);
        return QChar();
    }
}

bool ShogiBoard::setDataInternal(const int file, const int rank, const QChar piece)
{
    int index = (rank - 1) * files() + (file - 1);

    // 変更がなければ再描画不要
    if (m_boardData.at(index) == piece) return false;

    m_boardData[index] = piece;
    return true;
}

// ============================================================
// 盤面操作
// ============================================================

void ShogiBoard::setData(const int file, const int rank, const QChar piece)
{
    if (setDataInternal(file, rank, piece)) {
        // ShogiView::setBoardのconnectで再描画がトリガされる
        emit dataChanged(file, rank);
    }
}

// 編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
void ShogiBoard::movePieceToSquare(QChar selectedPiece, const int fileFrom, const int rankFrom,
                                   const int fileTo, const int rankTo, const bool promote)
{
    auto promotedOf = [](QChar p)->QChar {
        static const QMap<QChar, QChar> promoteMap = {
            {'P','Q'},{'L','M'},{'N','O'},{'S','T'},{'B','C'},{'R','U'},
            {'p','q'},{'l','m'},{'n','o'},{'s','t'},{'b','c'},{'r','u'}
        };
        return promoteMap.contains(p) ? promoteMap[p] : p;
    };

    // 1) 「成る」指定が来ていれば、先に成駒へ
    if (promote) {
        selectedPiece = promotedOf(selectedPiece);
    }

    // 2) 元位置を空白に
    if ((fileFrom >= 1) && (fileFrom <= 9)) {
        setData(fileFrom, rankFrom, ' ');
    }

    // 3) まず素の駒を置く（盤上のみ）
    if ((fileTo >= 1) && (fileTo <= 9)) {
        setData(fileTo, rankTo, selectedPiece);

        // 4) 置いた結果に対して「必成」補正（歩/桂/香）
        QChar placed = getPieceCharacter(fileTo, rankTo);

        // 先手の必成
        if (placed == 'P' && rankTo == 1) {
            setData(fileTo, rankTo, 'Q'); // 先手歩 → と金
        } else if (placed == 'L' && rankTo == 1) {
            setData(fileTo, rankTo, 'M'); // 先手香 → 成香
        } else if (placed == 'N' && (rankTo == 1 || rankTo == 2)) {
            setData(fileTo, rankTo, 'O'); // 先手桂 → 成桂
        }
        // 後手の必成
        else if (placed == 'p' && rankTo == 9) {
            setData(fileTo, rankTo, 'q'); // 後手歩 → と金
        } else if (placed == 'l' && rankTo == 9) {
            setData(fileTo, rankTo, 'm'); // 後手香 → 成香
        } else if (placed == 'n' && (rankTo == 8 || rankTo == 9)) {
            setData(fileTo, rankTo, 'o'); // 後手桂 → 成桂
        }
    }
}

// ============================================================
// SFEN入力・検証
// ============================================================

// 将棋盤内のみのSFEN文字列を入力し、エラーチェックを行い、成り駒を1文字に変換したSFEN文字列を返す。
QString ShogiBoard::validateAndConvertSfenBoardStr(QString initialSfenStr)
{
    // 処理フロー:
    // 1. 成り駒文字列（"+P"等）を1文字に変換
    // 2. "/"で分割して9段あるか検証
    // 3. 各段の駒数が9であるか検証

    const QStringList promotions = {"+P", "+L", "+N", "+S", "+B", "+R",
                                    "+p", "+l", "+n", "+s", "+b", "+r"};

    const QString replacements = "QMOTCUqmotcu";

    for (qsizetype i = 0; i < promotions.size(); ++i) {
        initialSfenStr.replace(promotions[i], replacements[i]);
    }

    QStringList sfenParts = initialSfenStr.split("/");

    if (sfenParts.size() != 9) {
        qCWarning(lcCore, "SFEN parts != 9: %s", qUtf8Printable(initialSfenStr));
        return QString();
    }

    // 各段に配置可能な駒のリスト
   static  const QStringList pieces[9] = {
        {"S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 1
        {"P", "L", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 2
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 3
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 4
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 5
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 6
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "n", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 7
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "p", "l", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"},  // 8
        {"P", "L", "N", "S", "G", "B", "R", "K", "Q", "M", "O", "T", "C", "U", "s", "g", "b", "r", "k", "q", "m", "o", "t", "c", "u"}   // 9
    };

    for (int i = 0; i < 9; ++i) {
        QString rankStr = sfenParts.at(i);
        int pieceCount = 0;
        for (QChar ch : rankStr) {
            if (ch.isDigit()) {
                pieceCount += ch.digitValue();
            } else if (pieces[i].contains(ch)) {
                ++pieceCount;
            } else {
                qCWarning(lcCore, "Unexpected SFEN char: %s at rank %d in %s",
                         qUtf8Printable(QString(ch)), i, qUtf8Printable(rankStr));
                return QString();
            }
        }

        if (pieceCount != 9) {
            qCWarning(lcCore, "SFEN rank piece count: %d in %s", pieceCount, qUtf8Printable(initialSfenStr));
            return QString();
        }
    }

    return initialSfenStr;
}

// SFEN文字列の持ち駒部分を解析し、駒台に各駒の枚数をセットする。
void ShogiBoard::setPieceStandFromSfen(const QString& str)
{
    // 処理フロー:
    // 1. 初期化（全持ち駒を0に）
    // 2. "-"なら持ち駒なしで即リターン
    // 3. 文字列を1文字ずつ走査し、数字+駒文字の組み合わせを解析

    initStand();

    if (str == "-") return;

    if (str.contains(' ')) {
        qCWarning(lcCore, "Piece stand string contains space: %s", qUtf8Printable(str));
        return;
    }

    bool isTwoDigits = false;

    for (int i = 0; i < str.length(); ++i) {
        if (str[i].isDigit()) {
            if (isTwoDigits) {
                if (i + 1 < str.length() && m_pieceStand.contains(str[i + 1])) {
                    // 10の位と1の位を合算
                    int twoDigits = 10 * str[i - 1].digitValue() + str[i].digitValue();

                    QChar pieceType = str[i + 1];
                    m_pieceStand[pieceType] += twoDigits;
                    ++i;

                    isTwoDigits = false;
                }
                else {
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return;
                }
            }
            else {
                if (i + 1 < str.length() && m_pieceStand.contains(str[i + 1])) {
                    int count = str[i].digitValue();

                    QChar pieceType = str[i + 1];
                    m_pieceStand[pieceType] += count;
                    ++i;
                }
                else if (str[i+1].isDigit()) {
                    isTwoDigits = true;
                }
                else {
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return;
                }
            }
        }
        // 数字が前にない場合は1枚と数える
        else if (m_pieceStand.contains(str[i])) {
            m_pieceStand[str[i]] += 1;
        }
        else {
            qCWarning(lcCore, "Invalid piece type in stand string: %s", qUtf8Printable(str));
            return;
        }
    }
}

void ShogiBoard::setPiecePlacementFromSfen(QString& initialSfenStr)
{
    QString sfenStr = validateAndConvertSfenBoardStr(initialSfenStr);
    if (sfenStr.isEmpty()) return;

    int strIndex = 0;
    int emptySquares = 0;
    const int fileCount = files();
    QChar pieceChar;

    for (int rank = 1; rank <= ranks(); ++rank) {
        for (int file = fileCount; file > 0; --file) {
            if (emptySquares > 0) {
                pieceChar = ' ';
                emptySquares--;
            }
            else {
                pieceChar = sfenStr.at(strIndex++);

                if (pieceChar.isDigit()) {
                    emptySquares = pieceChar.toLatin1() - '0';
                    pieceChar = ' ';
                    emptySquares--;
                }
            }
            setDataInternal(file, rank, pieceChar.toLatin1());
        }

        strIndex++;
    }
}

// SFEN文字列を検証し、盤面・手番・持ち駒・手数の各要素に分解する。
bool ShogiBoard::validateSfenString(const QString& sfenStr, QString& sfenBoardStr, QString& sfenStandStr)
{
    QStringList sfenComponents = sfenStr.split(" ");

    if (sfenComponents.size() != 4) {
        qCWarning(lcCore, "SFEN components: %lld in %s",
                 static_cast<long long>(sfenComponents.size()), qUtf8Printable(sfenStr));

        return false;
    }

    sfenBoardStr = sfenComponents.at(0);

    QString playerTurnStr = sfenComponents.at(1);

    if (playerTurnStr == "b" || playerTurnStr == "w") {
        m_currentPlayer = playerTurnStr;
    }
    else {
        qCWarning(lcCore, "Invalid turn: %s in %s",
                 qUtf8Printable(playerTurnStr), qUtf8Printable(sfenStr));

        return false;
    }

    sfenStandStr = sfenComponents.at(2);

    bool conversionSuccessful;
    m_currentMoveNumber = sfenComponents.at(3).toInt(&conversionSuccessful);

    if (!conversionSuccessful || m_currentMoveNumber <= 0) {
        qCWarning(lcCore, "Invalid move number in SFEN: %s", qUtf8Printable(sfenStr));

        return false;
    }

    return true;
}

// SFEN文字列で将棋盤全体（盤面＋駒台）を更新し再描画する。
void ShogiBoard::setSfen(const QString& sfenStr)
{
    // 処理フロー:
    // 1. SFEN文字列の構文検証・分解
    // 2. 盤面に駒を配置
    // 3. 持ち駒を設定
    // 4. 再描画シグナルを発行

    {
        const QString preview = (sfenStr.size() > 200) ? sfenStr.left(200) + " ..." : sfenStr;
        qCDebug(lcCore, "setSfen: %s", qUtf8Printable(preview));
        if (sfenStr.startsWith(QLatin1String("position "))) {
            qCWarning(lcCore, "NON-SFEN passed to setSfen (caller bug)");
        }
    }

    QString sfenBoardStr;
    QString sfenStandStr;

    if (!validateSfenString(sfenStr, sfenBoardStr, sfenStandStr))
        return;

    setPiecePlacementFromSfen(sfenBoardStr);

    setPieceStandFromSfen(sfenStandStr);

    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
}

// ============================================================
// SFEN出力
// ============================================================

QString ShogiBoard::convertPieceToSfen(const QChar piece) const
{
    static const QMap<QChar, QString> sfenMap = {{'Q', "+P"}, {'M', "+L"}, {'O', "+N"}, {'T', "+S"},
                                                 {'C', "+B"}, {'U', "+R"}, {'q', "+p"}, {'m', "+l"},
                                                 {'o', "+n"}, {'t', "+s"}, {'c', "+b"}, {'u', "+r"}};

    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(piece);
}

QString ShogiBoard::convertBoardToSfen()
{
    QString str;
    str.reserve(200);

    for (int rank = 1; rank <= ranks(); ++rank) {
        int spacecount = 0;
        for (int file = files(); file > 0; --file) {
            QChar piece = getPieceCharacter(file, rank);

            if (piece == ' ') {
                spacecount++;

                if (spacecount == 9 || file == 1) {
                    str += QString::number(spacecount);
                }
            }
            else {
                if (spacecount > 0) {
                    str += QString::number(spacecount);
                    spacecount = 0;
                }

                str += convertPieceToSfen(piece);
            }
        }

        if (rank != 9) str += "/";
    }

    return str;
}

QString ShogiBoard::convertStandToSfen() const
{
    QString handPiece;
    handPiece.reserve(40);

    static const QList<QChar> keys = {'R', 'B', 'G', 'S', 'N', 'L', 'P'};

    for(const auto& key : keys) {
        // 先手
        int value = m_pieceStand.value(key, 0);
        if (value > 0) {
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key);
        }

        // 後手
        value = m_pieceStand.value(key.toLower(), 0);
        if (value > 0) {
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key.toLower());
        }
    }

    return handPiece.isEmpty() ? "-" : handPiece;
}

// ============================================================
// SFEN記録
// ============================================================

void ShogiBoard::addSfenRecord(const QString& nextTurn, int moveIndex, QStringList* sfenRecord)
{
    if (!sfenRecord) {
        qCWarning(lcCore, "addSfenRecord: sfenRecord is null");
        return;
    }

    // moveIndex: 0始まりの着手番号を想定
    //   通常記録     → +1 で手数フィールドへ
    //   特例(-1等)   → 1 を明示（開始直後など）
    const int moveCountField = (moveIndex < 0) ? 1 : (moveIndex + 1);

    const qsizetype before = sfenRecord->size();
    const QString boardSfen = convertBoardToSfen();
    QString stand = convertStandToSfen();
    if (stand.isEmpty()) stand = QStringLiteral("-");

    qCDebug(lcCore, "addSfenRecord BEFORE size= %lld nextTurn= %s moveIndex= %d => field= %d",
            static_cast<long long>(before), qUtf8Printable(nextTurn), moveIndex, moveCountField);
    qCDebug(lcCore, "addSfenRecord board= %s stand= %s",
            qUtf8Printable(boardSfen), qUtf8Printable(stand));

    const QString sfen = QStringLiteral("%1 %2 %3 %4")
                             .arg(boardSfen, nextTurn, stand, QString::number(moveCountField));
    sfenRecord->append(sfen);

    qCDebug(lcCore, "addSfenRecord AFTER size= %lld appended= %s",
            static_cast<long long>(sfenRecord->size()), qUtf8Printable(sfen));

    if (!sfenRecord->isEmpty()) {
        qCDebug(lcCore, "addSfenRecord head[0]= %s", qUtf8Printable(sfenRecord->first()));
        qCDebug(lcCore, "addSfenRecord tail[last]= %s", qUtf8Printable(sfenRecord->last()));
    }

    // 先頭が破壊されてないか簡易チェック
    if (!sfenRecord->isEmpty()) {
        const QStringList parts = sfenRecord->first().split(QLatin1Char(' '), Qt::KeepEmptyParts);
        if (parts.size() == 4) {
            const QString turn0 = parts[1];
            const QString move0 = parts[3];
            if (move0 != QLatin1String("1")) {
                qCWarning(lcCore, "addSfenRecord head[0] moveCount != 1  head= %s", qUtf8Printable(sfenRecord->first()));
            }
            if (turn0 != QLatin1String("b") && turn0 != QLatin1String("w")) {
                qCWarning(lcCore, "addSfenRecord head[0] turn invalid  head= %s", qUtf8Printable(sfenRecord->first()));
            }
        } else {
            qCWarning(lcCore, "addSfenRecord head[0] malformed  head= %s", qUtf8Printable(sfenRecord->first()));
        }
    }
}

// ============================================================
// 駒台操作
// ============================================================

// 成駒は相手の生駒に変換し、それ以外は大文字/小文字を反転する。
QChar ShogiBoard::convertPieceChar(const QChar c) const
{
    static const QMap<QChar, QChar> conversionMap = {
        {'Q', 'p'}, {'M', 'l'}, {'O', 'n'}, {'T', 's'}, {'C', 'b'}, {'U', 'r'},
        {'q', 'P'}, {'m', 'L'}, {'o', 'N'}, {'t', 'S'}, {'c', 'B'}, {'u', 'R'}
    };

    if (conversionMap.contains(c)) {
        return conversionMap.value(c);
    }

    return c.isUpper() ? c.toLower() : c.toUpper();
}

// 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(QChar dest)
{
    QChar pieceChar = convertPieceChar(dest);

    if (m_pieceStand.contains(pieceChar)) {
            m_pieceStand[pieceChar]++;
    }
}

void ShogiBoard::decrementPieceOnStand(QChar source)
{
    if (m_pieceStand.contains(source)) {
        m_pieceStand[source]--;
    }
}

// 駒台から指そうとした場合、駒台の駒数が0以下なら指せない。
bool ShogiBoard::isPieceAvailableOnStand(const QChar source, const int fileFrom) const
{
    if (fileFrom == 10 || fileFrom == 11) {
        if (m_pieceStand.contains(source) && m_pieceStand[source] > 0) {
            return true;
        }
        else {
            return false;
        }
    }
    // 盤内から指そうとした場合、チェック範囲外のため常にtrue
    else {
        return true;
    }
}

QChar ShogiBoard::convertPromotedPieceToOriginal(const QChar dest) const
{
    static const QMap<QChar, QChar> promotedToOriginalMap = {
        {'Q', 'P'}, {'M', 'L'}, {'O', 'N'}, {'T', 'S'}, {'C', 'B'}, {'U', 'R'},
        {'q', 'p'}, {'m', 'l'}, {'o', 'n'}, {'t', 's'}, {'c', 'b'}, {'u', 'r'}
    };

    return promotedToOriginalMap.value(dest, dest);
}

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const QChar dest)
{
    QChar originalPiece = convertPromotedPieceToOriginal(dest);

    if (m_pieceStand.contains(originalPiece)) {
        m_pieceStand[originalPiece]++;
    }
}

// ============================================================
// 盤面更新
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const QChar source, const QChar dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    if (fileTo < 10) {
        // 指したマスに相手の駒があった場合、自分の駒台に加える
        addPieceToStand(dest);
    } else {
        // 駒台に駒を移した場合
        incrementPieceOnStand(dest);
    }

    if (fileFrom == 10 || fileFrom == 11) {
        decrementPieceOnStand(source);
    }

    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

void ShogiBoard::setInitialPieceStandValues()
{
    static const QList<QPair<QChar, int>> initialValues = {
        {'P', 9}, {'L', 2}, {'N', 2}, {'S', 2}, {'G', 2}, {'B', 1}, {'R', 1}, {'K', 1},
        {'k', 1}, {'r', 1}, {'b', 1}, {'g', 2}, {'s', 2}, {'n', 2}, {'l', 2}, {'p', 9},
    };

    for (const auto& pair : initialValues) {
        m_pieceStand[pair.first] = pair.second;
    }
}

// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void ShogiBoard::resetGameBoard()
{
    m_boardData.fill(' ', ranks() * files());
    setInitialPieceStandValues();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    QVector<QChar> originalBoardData = m_boardData;
    QVector<QChar> newBoardData;

    for (int i = 0; i < 81; i++) {
        QChar pieceChar = originalBoardData.at(80 - i);
        pieceChar = pieceChar.isLower() ? pieceChar.toUpper() : pieceChar.toLower();
        newBoardData.append(pieceChar);
    }

    m_boardData = newBoardData;

    const QMap<QChar, int> originalPieceStand = m_pieceStand;

    for (auto it = originalPieceStand.cbegin(); it != originalPieceStand.cend(); ++it) {
        QChar flippedChar = it.key().isLower() ? it.key().toUpper() : it.key().toLower();
        m_pieceStand[flippedChar] = it.value();
    }
}

// ============================================================
// 局面編集: 成駒/不成駒/先後の巡回変換
// ============================================================

// 局面編集中に右クリックで成駒/不成駒/先後を巡回変換する（禁置き段＋二歩をスキップ）。
void ShogiBoard::promoteOrDemotePiece(const int fileFrom, const int rankFrom)
{
    // 処理フロー:
    // 1. 駒種ごとの巡回リストを特定
    // 2. 禁置き段・二歩の候補をフィルタ
    // 3. フィルタ済みリストで次の候補に切り替え

    auto nextInCycle = [](const QVector<QChar>& cyc, QChar cur)->QChar {
        qsizetype idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QVector<QChar>{'L','M','l','m'};
    const auto knightCycle = QVector<QChar>{'N','O','n','o'};
    const auto silverCycle = QVector<QChar>{'S','T','s','t'};
    const auto bishopCycle = QVector<QChar>{'B','C','b','c'};
    const auto rookCycle   = QVector<QChar>{'R','U','r','u'};
    const auto pawnCycle   = QVector<QChar>{'P','Q','p','q'};

    const QChar cur = getPieceCharacter(fileFrom, rankFrom);
    QVector<QChar> base;
    switch (cur.unicode()) {
    case 'L': case 'M': case 'l': case 'm': base = lanceCycle;  break;
    case 'N': case 'O': case 'n': case 'o': base = knightCycle; break;
    case 'S': case 'T': case 's': case 't': base = silverCycle; break;
    case 'B': case 'C': case 'b': case 'c': base = bishopCycle; break;
    case 'R': case 'U': case 'r': case 'u': base = rookCycle;   break;
    case 'P': case 'Q': case 'p': case 'q': base = pawnCycle;   break;
    default:
        return; // 金・玉などは変換対象外
    }

    const bool onBoard = (fileFrom >= 1 && fileFrom <= 9);

    auto isRankDisallowed = [&](QChar piece)->bool {
        if (!onBoard) return false;
        if (piece == 'L' && rankFrom == 1) return true;
        if (piece == 'N' && (rankFrom == 1 || rankFrom == 2)) return true;
        if (piece == 'P' && rankFrom == 1) return true;
        if (piece == 'l' && rankFrom == 9) return true;
        if (piece == 'n' && (rankFrom == 8 || rankFrom == 9)) return true;
        if (piece == 'p' && rankFrom == 9) return true;
        return false;
    };

    auto isNiFuDisallowed = [&](QChar candidate)->bool {
        if (!onBoard) return false;
        if (candidate != 'P' && candidate != 'p') return false;
        for (int r = 1; r <= 9; ++r) {
            if (r == rankFrom) continue;
            const QChar pc = getPieceCharacter(fileFrom, r);
            if (candidate == 'P' && pc == 'P') return true;
            if (candidate == 'p' && pc == 'p') return true;
        }
        return false;
    };

    auto isDisallowed = [&](QChar piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    QVector<QChar> filtered;
    filtered.reserve(base.size());
    for (QChar p : std::as_const(base)) {
        if (!isDisallowed(p)) filtered.push_back(p);
    }
    if (filtered.isEmpty()) return;

    // 現在形がfiltered外（既に禁止形）なら、baseを回して最初の許可形へジャンプ
    QChar next = cur;
    if (filtered.indexOf(cur) < 0) {
        QChar probe = cur;
        for (qsizetype i = 0; i < base.size(); ++i) {
            probe = nextInCycle(base, probe);
            if (filtered.indexOf(probe) >= 0) { next = probe; break; }
        }
    } else {
        qsizetype idx = filtered.indexOf(cur);
        next = filtered[(idx + 1) % filtered.size()];
    }

    setData(fileFrom, rankFrom, next);
}

// ============================================================
// デバッグ出力
// ============================================================

void ShogiBoard::printPlayerPieces(const QString& player, const QString& pieceSet) const
{
    static const QMap<QChar, QString> pieceNames = {
        {'K', "玉"}, {'R', "飛"}, {'B', "角"}, {'G', "金"}, {'S', "銀"}, {'N', "桂"}, {'L', "香"}, {'P', "歩"},
        {'k', "玉"}, {'r', "飛"}, {'b', "角"}, {'g', "金"}, {'s', "銀"}, {'n', "桂"}, {'l', "香"}, {'p', "歩"}
    };

    qCDebug(lcCore, "%s の持ち駒", qUtf8Printable(player));

    for (const QChar& piece : pieceSet) {
        qCDebug(lcCore, "%s  %d", qUtf8Printable(pieceNames[piece]), m_pieceStand[piece]);
    }
}

void ShogiBoard::printPieceStand()
{
    printPlayerPieces("先手", "KRGBSNLP");
    printPlayerPieces("後手", "krgbsnlp");
}

void ShogiBoard::printPieceCount() const
{
    qCDebug(lcCore, "先手の持ち駒:");
    qCDebug(lcCore, "歩:  %d", m_pieceStand['P']);
    qCDebug(lcCore, "香車:  %d", m_pieceStand['L']);
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand['N']);
    qCDebug(lcCore, "銀:  %d", m_pieceStand['S']);
    qCDebug(lcCore, "金:  %d", m_pieceStand['G']);
    qCDebug(lcCore, "角:  %d", m_pieceStand['B']);
    qCDebug(lcCore, "飛車:  %d", m_pieceStand['R']);

    qCDebug(lcCore, "後手の持ち駒:");
    qCDebug(lcCore, "歩:  %d", m_pieceStand['p']);
    qCDebug(lcCore, "香車:  %d", m_pieceStand['l']);
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand['n']);
    qCDebug(lcCore, "銀:  %d", m_pieceStand['s']);
    qCDebug(lcCore, "金:  %d", m_pieceStand['g']);
    qCDebug(lcCore, "角:  %d", m_pieceStand['b']);
    qCDebug(lcCore, "飛車:  %d", m_pieceStand['r']);
}
