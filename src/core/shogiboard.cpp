/// @file shogiboard.cpp
/// @brief 将棋盤の状態管理・SFEN変換・駒台操作の実装

#include "shogiboard.h"
#include "logcategories.h"

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
    m_boardData.fill(Piece::None, ranks() * files());
}

void ShogiBoard::initStand()
{
    m_pieceStand.clear();

    static const QList<Piece> pieces = {
        Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight, Piece::WhiteSilver,
        Piece::WhiteGold, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteKing,
        Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
        Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook, Piece::BlackKing
    };

    for (const Piece& piece : pieces) {
        m_pieceStand.insert(piece, 0);
    }
}

// ============================================================
// 盤面アクセス
// ============================================================

const QVector<Piece>& ShogiBoard::boardData() const
{
    return m_boardData;
}

const QMap<Piece, int>& ShogiBoard::getPieceStand() const
{
    return m_pieceStand;
}

Turn ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
Piece ShogiBoard::getPieceCharacter(const int file, const int rank)
{
    static const QMap<int, Piece> pieceMapBlack = {
        {1, Piece::BlackPawn}, {2, Piece::BlackLance}, {3, Piece::BlackKnight},
        {4, Piece::BlackSilver}, {5, Piece::BlackGold}, {6, Piece::BlackBishop},
        {7, Piece::BlackRook}, {8, Piece::BlackKing}
    };
    static const QMap<int, Piece> pieceMapWhite = {
        {2, Piece::WhiteKing}, {3, Piece::WhiteRook}, {4, Piece::WhiteBishop},
        {5, Piece::WhiteGold}, {6, Piece::WhiteSilver}, {7, Piece::WhiteKnight},
        {8, Piece::WhiteLance}, {9, Piece::WhitePawn}
    };

    if (file >= 1 && file <= 9) {
        if (rank < 1 || rank > ranks()) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_boardData.at((rank - 1) * files() + (file - 1));
    } else if (file == 10) {
        const auto it = pieceMapBlack.find(rank);
        if (it != pieceMapBlack.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for black stand: %d", rank);
        return Piece::None;
    } else if (file == 11) {
        const auto it = pieceMapWhite.find(rank);
        if (it != pieceMapWhite.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for white stand: %d", rank);
        return Piece::None;
    } else {
        qCWarning(lcCore, "Invalid file value: %d", file);
        return Piece::None;
    }
}

bool ShogiBoard::setDataInternal(const int file, const int rank, const Piece piece)
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

void ShogiBoard::setData(const int file, const int rank, const Piece piece)
{
    if (setDataInternal(file, rank, piece)) {
        // ShogiView::setBoardのconnectで再描画がトリガされる
        emit dataChanged(file, rank);
    }
}

// 編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
void ShogiBoard::movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                                   const int fileTo, const int rankTo, const bool promote)
{
    // 1) 「成る」指定が来ていれば、先に成駒へ
    if (promote) {
        selectedPiece = ::promote(selectedPiece);
    }

    // 2) 元位置を空白に
    if ((fileFrom >= 1) && (fileFrom <= 9)) {
        setData(fileFrom, rankFrom, Piece::None);
    }

    // 3) まず素の駒を置く（盤上のみ）
    if ((fileTo >= 1) && (fileTo <= 9)) {
        setData(fileTo, rankTo, selectedPiece);

        // 4) 置いた結果に対して「必成」補正（歩/桂/香）
        Piece placed = getPieceCharacter(fileTo, rankTo);

        // 先手の必成
        if (placed == Piece::BlackPawn && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedPawn);
        } else if (placed == Piece::BlackLance && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedLance);
        } else if (placed == Piece::BlackKnight && (rankTo == 1 || rankTo == 2)) {
            setData(fileTo, rankTo, Piece::BlackPromotedKnight);
        }
        // 後手の必成
        else if (placed == Piece::WhitePawn && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedPawn);
        } else if (placed == Piece::WhiteLance && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedLance);
        } else if (placed == Piece::WhiteKnight && (rankTo == 8 || rankTo == 9)) {
            setData(fileTo, rankTo, Piece::WhitePromotedKnight);
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
                if (i + 1 < str.length()) {
                    Piece p = charToPiece(str[i + 1]);
                    if (m_pieceStand.contains(p)) {
                        // 10の位と1の位を合算
                        int twoDigits = 10 * str[i - 1].digitValue() + str[i].digitValue();
                        m_pieceStand[p] += twoDigits;
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
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return;
                }
            }
            else {
                if (i + 1 < str.length()) {
                    Piece p = charToPiece(str[i + 1]);
                    if (m_pieceStand.contains(p)) {
                        int count = str[i].digitValue();
                        m_pieceStand[p] += count;
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
                else {
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return;
                }
            }
        }
        // 数字が前にない場合は1枚と数える
        else {
            Piece p = charToPiece(str[i]);
            if (m_pieceStand.contains(p)) {
                m_pieceStand[p] += 1;
            }
            else {
                qCWarning(lcCore, "Invalid piece type in stand string: %s", qUtf8Printable(str));
                return;
            }
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
    Piece pieceVal;

    for (int rank = 1; rank <= ranks(); ++rank) {
        for (int file = fileCount; file > 0; --file) {
            if (emptySquares > 0) {
                pieceVal = Piece::None;
                emptySquares--;
            }
            else {
                QChar pieceChar = sfenStr.at(strIndex++);

                if (pieceChar.isDigit()) {
                    emptySquares = pieceChar.toLatin1() - '0';
                    pieceVal = Piece::None;
                    emptySquares--;
                }
                else {
                    pieceVal = charToPiece(pieceChar);
                }
            }
            setDataInternal(file, rank, pieceVal);
        }

        strIndex++;
    }
}

// SFEN文字列をパースし、各要素に分解する（副作用なしの static メソッド）。
std::optional<SfenComponents> ShogiBoard::parseSfen(const QString& sfenStr)
{
    QStringList parts = sfenStr.split(QLatin1Char(' '));

    if (parts.size() != 4) {
        qCWarning(lcCore, "SFEN components: %lld in %s",
                 static_cast<long long>(parts.size()), qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    const QString& turnStr = parts.at(1);
    if (turnStr != QLatin1String("b") && turnStr != QLatin1String("w")) {
        qCWarning(lcCore, "Invalid turn: %s in %s",
                 qUtf8Printable(turnStr), qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    bool ok;
    int moveNumber = parts.at(3).toInt(&ok);
    if (!ok || moveNumber <= 0) {
        qCWarning(lcCore, "Invalid move number in SFEN: %s", qUtf8Printable(sfenStr));
        return std::nullopt;
    }

    return SfenComponents{parts.at(0), parts.at(2), sfenToTurn(turnStr), moveNumber};
}

// SFEN文字列を検証し、盤面・手番・持ち駒・手数の各要素に分解する。
bool ShogiBoard::validateSfenString(const QString& sfenStr, QString& sfenBoardStr, QString& sfenStandStr)
{
    auto result = parseSfen(sfenStr);
    if (!result)
        return false;

    sfenBoardStr = result->board;
    sfenStandStr = result->stand;
    m_currentPlayer = result->turn;
    m_currentMoveNumber = result->moveNumber;
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
        const QString preview = (sfenStr.size() > 200) ? sfenStr.left(200) + QStringLiteral(" ...") : sfenStr;
        qCDebug(lcCore, "setSfen: %s", qUtf8Printable(preview));
        if (sfenStr.startsWith(QLatin1String("position "))) {
            qCWarning(lcCore, "NON-SFEN passed to setSfen (caller bug)");
        }
    }

    auto parsed = parseSfen(sfenStr);
    if (!parsed)
        return;

    m_currentPlayer = parsed->turn;
    m_currentMoveNumber = parsed->moveNumber;

    setPiecePlacementFromSfen(parsed->board);
    setPieceStandFromSfen(parsed->stand);

    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
}

// ============================================================
// SFEN出力
// ============================================================

QString ShogiBoard::convertPieceToSfen(const Piece piece) const
{
    static const QMap<Piece, QString> sfenMap = {
        {Piece::BlackPromotedPawn, "+P"}, {Piece::BlackPromotedLance, "+L"},
        {Piece::BlackPromotedKnight, "+N"}, {Piece::BlackPromotedSilver, "+S"},
        {Piece::BlackHorse, "+B"}, {Piece::BlackDragon, "+R"},
        {Piece::WhitePromotedPawn, "+p"}, {Piece::WhitePromotedLance, "+l"},
        {Piece::WhitePromotedKnight, "+n"}, {Piece::WhitePromotedSilver, "+s"},
        {Piece::WhiteHorse, "+b"}, {Piece::WhiteDragon, "+r"}
    };

    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(pieceToChar(piece));
}

QString ShogiBoard::convertBoardToSfen()
{
    QString str;
    str.reserve(200);

    for (int rank = 1; rank <= ranks(); ++rank) {
        int spacecount = 0;
        for (int file = files(); file > 0; --file) {
            Piece piece = getPieceCharacter(file, rank);

            if (piece == Piece::None) {
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

    static const QList<Piece> keys = {
        Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
        Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn
    };

    for (const auto& key : keys) {
        // 先手
        int value = m_pieceStand.value(key, 0);
        if (value > 0) {
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key);
        }

        // 後手
        Piece whiteKey = toWhite(key);
        value = m_pieceStand.value(whiteKey, 0);
        if (value > 0) {
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(whiteKey);
        }
    }

    return handPiece.isEmpty() ? "-" : handPiece;
}

// ============================================================
// SFEN記録
// ============================================================

void ShogiBoard::addSfenRecord(Turn nextTurn, int moveIndex, QStringList* sfenRecord)
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

    const QString nextTurnStr = turnToSfen(nextTurn);
    qCDebug(lcCore, "addSfenRecord BEFORE size= %lld nextTurn= %s moveIndex= %d => field= %d",
            static_cast<long long>(before), qUtf8Printable(nextTurnStr), moveIndex, moveCountField);
    qCDebug(lcCore, "addSfenRecord board= %s stand= %s",
            qUtf8Printable(boardSfen), qUtf8Printable(stand));

    const QString sfen = QStringLiteral("%1 %2 %3 %4")
                             .arg(boardSfen, nextTurnStr, stand, QString::number(moveCountField));
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

// 成駒は相手の生駒に変換し、それ以外は先後を反転する。
Piece ShogiBoard::convertPieceChar(const Piece c) const
{
    // 成駒→相手の生駒
    if (isPromoted(c)) {
        Piece base = demote(c);
        return isBlackPiece(base) ? toWhite(base) : toBlack(base);
    }
    // 非成駒→先後反転
    return isBlackPiece(c) ? toWhite(c) : toBlack(c);
}

// 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(Piece dest)
{
    Piece pieceChar = convertPieceChar(dest);

    if (m_pieceStand.contains(pieceChar)) {
            m_pieceStand[pieceChar]++;
    }
}

void ShogiBoard::decrementPieceOnStand(Piece source)
{
    if (m_pieceStand.contains(source)) {
        m_pieceStand[source]--;
    }
}

// 駒台から指そうとした場合、駒台の駒数が0以下なら指せない。
bool ShogiBoard::isPieceAvailableOnStand(const Piece source, const int fileFrom) const
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

Piece ShogiBoard::convertPromotedPieceToOriginal(const Piece dest) const
{
    if (isPromoted(dest)) {
        return demote(dest);
    }
    return dest;
}

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const Piece dest)
{
    Piece originalPiece = convertPromotedPieceToOriginal(dest);

    if (m_pieceStand.contains(originalPiece)) {
        m_pieceStand[originalPiece]++;
    }
}

// ============================================================
// 盤面更新
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
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
    static const QList<QPair<Piece, int>> initialValues = {
        {Piece::BlackPawn, 9}, {Piece::BlackLance, 2}, {Piece::BlackKnight, 2},
        {Piece::BlackSilver, 2}, {Piece::BlackGold, 2}, {Piece::BlackBishop, 1},
        {Piece::BlackRook, 1}, {Piece::BlackKing, 1},
        {Piece::WhiteKing, 1}, {Piece::WhiteRook, 1}, {Piece::WhiteBishop, 1},
        {Piece::WhiteGold, 2}, {Piece::WhiteSilver, 2}, {Piece::WhiteKnight, 2},
        {Piece::WhiteLance, 2}, {Piece::WhitePawn, 9},
    };

    for (const auto& pair : initialValues) {
        m_pieceStand[pair.first] = pair.second;
    }
}

// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void ShogiBoard::resetGameBoard()
{
    m_boardData.fill(Piece::None, ranks() * files());
    setInitialPieceStandValues();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    QVector<Piece> originalBoardData = m_boardData;
    QVector<Piece> newBoardData;

    for (int i = 0; i < 81; i++) {
        Piece piece = originalBoardData.at(80 - i);
        // 先後反転
        if (isBlackPiece(piece)) {
            piece = toWhite(piece);
        } else if (isWhitePiece(piece)) {
            piece = toBlack(piece);
        }
        newBoardData.append(piece);
    }

    m_boardData = newBoardData;

    const QMap<Piece, int> originalPieceStand = m_pieceStand;

    for (auto it = originalPieceStand.cbegin(); it != originalPieceStand.cend(); ++it) {
        Piece flipped = isBlackPiece(it.key()) ? toWhite(it.key()) : toBlack(it.key());
        m_pieceStand[flipped] = it.value();
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

    auto nextInCycle = [](const QVector<Piece>& cyc, Piece cur)->Piece {
        qsizetype idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QVector<Piece>{Piece::BlackLance, Piece::BlackPromotedLance, Piece::WhiteLance, Piece::WhitePromotedLance};
    const auto knightCycle = QVector<Piece>{Piece::BlackKnight, Piece::BlackPromotedKnight, Piece::WhiteKnight, Piece::WhitePromotedKnight};
    const auto silverCycle = QVector<Piece>{Piece::BlackSilver, Piece::BlackPromotedSilver, Piece::WhiteSilver, Piece::WhitePromotedSilver};
    const auto bishopCycle = QVector<Piece>{Piece::BlackBishop, Piece::BlackHorse, Piece::WhiteBishop, Piece::WhiteHorse};
    const auto rookCycle   = QVector<Piece>{Piece::BlackRook, Piece::BlackDragon, Piece::WhiteRook, Piece::WhiteDragon};
    const auto pawnCycle   = QVector<Piece>{Piece::BlackPawn, Piece::BlackPromotedPawn, Piece::WhitePawn, Piece::WhitePromotedPawn};

    const Piece cur = getPieceCharacter(fileFrom, rankFrom);
    QVector<Piece> base;
    switch (static_cast<char>(cur)) {
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

    auto isRankDisallowed = [&](Piece piece)->bool {
        if (!onBoard) return false;
        if (piece == Piece::BlackLance && rankFrom == 1) return true;
        if (piece == Piece::BlackKnight && (rankFrom == 1 || rankFrom == 2)) return true;
        if (piece == Piece::BlackPawn && rankFrom == 1) return true;
        if (piece == Piece::WhiteLance && rankFrom == 9) return true;
        if (piece == Piece::WhiteKnight && (rankFrom == 8 || rankFrom == 9)) return true;
        if (piece == Piece::WhitePawn && rankFrom == 9) return true;
        return false;
    };

    auto isNiFuDisallowed = [&](Piece candidate)->bool {
        if (!onBoard) return false;
        if (candidate != Piece::BlackPawn && candidate != Piece::WhitePawn) return false;
        for (int r = 1; r <= 9; ++r) {
            if (r == rankFrom) continue;
            const Piece pc = getPieceCharacter(fileFrom, r);
            if (candidate == Piece::BlackPawn && pc == Piece::BlackPawn) return true;
            if (candidate == Piece::WhitePawn && pc == Piece::WhitePawn) return true;
        }
        return false;
    };

    auto isDisallowed = [&](Piece piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    QVector<Piece> filtered;
    filtered.reserve(base.size());
    for (Piece p : std::as_const(base)) {
        if (!isDisallowed(p)) filtered.push_back(p);
    }
    if (filtered.isEmpty()) return;

    // 現在形がfiltered外（既に禁止形）なら、baseを回して最初の許可形へジャンプ
    Piece next = cur;
    if (filtered.indexOf(cur) < 0) {
        Piece probe = cur;
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
    qCDebug(lcCore, "%s の持ち駒", qUtf8Printable(player));

    for (const QChar& ch : pieceSet) {
        Piece piece = charToPiece(ch);
        qCDebug(lcCore, "%s  %d", qUtf8Printable(QString(ch)), m_pieceStand.value(piece, 0));
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
    qCDebug(lcCore, "歩:  %d", m_pieceStand.value(Piece::BlackPawn, 0));
    qCDebug(lcCore, "香車:  %d", m_pieceStand.value(Piece::BlackLance, 0));
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand.value(Piece::BlackKnight, 0));
    qCDebug(lcCore, "銀:  %d", m_pieceStand.value(Piece::BlackSilver, 0));
    qCDebug(lcCore, "金:  %d", m_pieceStand.value(Piece::BlackGold, 0));
    qCDebug(lcCore, "角:  %d", m_pieceStand.value(Piece::BlackBishop, 0));
    qCDebug(lcCore, "飛車:  %d", m_pieceStand.value(Piece::BlackRook, 0));

    qCDebug(lcCore, "後手の持ち駒:");
    qCDebug(lcCore, "歩:  %d", m_pieceStand.value(Piece::WhitePawn, 0));
    qCDebug(lcCore, "香車:  %d", m_pieceStand.value(Piece::WhiteLance, 0));
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand.value(Piece::WhiteKnight, 0));
    qCDebug(lcCore, "銀:  %d", m_pieceStand.value(Piece::WhiteSilver, 0));
    qCDebug(lcCore, "金:  %d", m_pieceStand.value(Piece::WhiteGold, 0));
    qCDebug(lcCore, "角:  %d", m_pieceStand.value(Piece::WhiteBishop, 0));
    qCDebug(lcCore, "飛車:  %d", m_pieceStand.value(Piece::WhiteRook, 0));
}
