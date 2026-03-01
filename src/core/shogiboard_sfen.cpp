/// @file shogiboard_sfen.cpp
/// @brief SFEN文字列の入力・検証・出力・記録処理

#include "shogiboard.h"
#include "errorbus.h"
#include "logcategories.h"

// ============================================================
// SFEN入力・検証
// ============================================================

// 将棋盤内のみのSFEN文字列を入力し、エラーチェックを行い、成り駒を1文字に変換したSFEN文字列を返す。
std::optional<QString> ShogiBoard::validateAndConvertSfenBoardStr(QString initialSfenStr)
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
        return std::nullopt;
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
                return std::nullopt;
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
bool ShogiBoard::setPieceStandFromSfen(const QString& str)
{
    // 処理フロー:
    // 1. 初期化（全持ち駒を0に）
    // 2. "-"なら持ち駒なしで即リターン
    // 3. 文字列を1文字ずつ走査し、数字+駒文字の組み合わせを解析

    initStand();

    if (str == "-") return true;

    if (str.contains(' ')) {
        qCWarning(lcCore, "Piece stand string contains space: %s", qUtf8Printable(str));
        return false;
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
                        return false;
                    }
                }
                else {
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return false;
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
                        return false;
                    }
                }
                else {
                    qCWarning(lcCore, "Invalid piece after number at %d: %s in %s",
                             i, qUtf8Printable(QString(str[i])), qUtf8Printable(str));
                    return false;
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
                return false;
            }
        }
    }
    return true;
}

bool ShogiBoard::setPiecePlacementFromSfen(QString& initialSfenStr)
{
    auto opt = validateAndConvertSfenBoardStr(initialSfenStr);
    if (!opt) return false;

    const QString& sfenStr = *opt;
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
                if (strIndex >= sfenStr.size()) {
                    qCWarning(lcCore, "SFEN string too short at rank=%d file=%d", rank, file);
                    return false;
                }
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
    return true;
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

    if (!setPiecePlacementFromSfen(parsed->board)) {
        const auto msg = tr("SFEN board parse error: %1").arg(sfenStr.left(80));
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, msg);
        return;
    }
    if (!setPieceStandFromSfen(parsed->stand)) {
        const auto msg = tr("SFEN piece stand parse error: %1").arg(sfenStr.left(80));
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, msg);
        return;
    }

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
            const QString& turn0 = parts[1];
            const QString& move0 = parts[3];
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
