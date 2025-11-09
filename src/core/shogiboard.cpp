#include "shogiboard.h"
#include "qdebug.h"

// 将棋盤に関するクラス
// コンストラクタ
ShogiBoard::ShogiBoard(int ranks, int files, QObject *parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    // 将棋盤の81マスに空白を代入する。
    initBoard();

    // 駒台の各駒の数を全て0にする。
    initStand();
}

// 指定した位置の駒を表す文字を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
QChar ShogiBoard::getPieceCharacter(const int file, const int rank)
{
    static const QMap<int, QChar> pieceMapBlack = {{1,'P'},{2,'L'},{3,'N'},{4,'S'},{5,'G'},{6,'B'},{7,'R'},{8,'K'}};
    static const QMap<int, QChar> pieceMapWhite = {{2,'k'},{3,'r'},{4,'b'},{5,'g'},{6,'s'},{7,'n'},{8,'l'},{9,'p'}};

    if (file >= 1 && file <= 9) {
        return m_boardData.at((rank - 1) * files() + (file - 1));
    } else if (file == 10) {
        const auto it = pieceMapBlack.find(rank);
        if (it != pieceMapBlack.end()) return it.value();

        const QString errorMessage =
            tr("An error occurred in ShogiBoard::getPieceCharacter. Invalid rank for the black player's stand.");
        qDebug() << "rank:" << rank;
        emit errorOccurred(errorMessage);
        return QChar(); // ★ 打ち切り
    } else if (file == 11) {
        const auto it = pieceMapWhite.find(rank);
        if (it != pieceMapWhite.end()) return it.value();

        const QString errorMessage =
            tr("An error occurred in ShogiBoard::getPieceCharacter. Invalid rank for the white player's stand.");
        qDebug() << "rank:" << rank;
        emit errorOccurred(errorMessage);
        return QChar(); // ★ 打ち切り
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiBoard::getPieceCharacter. Invalid file value.");
        qDebug() << "file:" << file;
        emit errorOccurred(errorMessage);
        return QChar(); // ★ 打ち切り
    }
}

// 将棋盤のマスに駒を配置する。
// file 筋、rank 段で指定されたマスに駒文字pieceをセットする。
void ShogiBoard::setData(const int file, const int rank, const QChar piece)
{
    // file 筋、rank 段で指定されたマスに駒文字pieceをセットする。
    if (setDataInternal(file, rank, piece)) {
        // ShogiView::setBoardのconnectで使用されているシグナル
        // スロットのrepaint関数が実行される。（将棋盤、駒台の再描画）
        emit dataChanged(file, rank);
    }
}

// 駒を指定したマスへ移動する。配置データのみを更新する。
// 駒を指定したマスへ移動する。配置データのみを更新する。
// ★編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
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

// 将棋盤の81マスに空白を代入する。
void ShogiBoard::initBoard()
{
    m_boardData.fill(' ', ranks() * files());
}

// 駒台の各駒の数を全て0にする。
void ShogiBoard::initStand()
{
    // 持ち駒情報を全て初期化
    m_pieceStand.clear();

    // 駒のキーのリスト
    static const QList<QChar> pieces = {'p', 'l', 'n', 's', 'g', 'b', 'r', 'k',
                                        'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K'};

    // 各駒をQMapに挿入
    // 全ての持ち駒の枚数を0にする。
    for (const QChar& piece : pieces) {
        m_pieceStand.insert(piece, 0);
    }
}

// 将棋盤のマスに駒を配置する。
bool ShogiBoard::setDataInternal(const int file, const int rank, const QChar piece)
{
    // マス番号を計算する。
    int index = (rank - 1) * files() + (file - 1);

    // マスの駒文字が一致する場合は何もせずにfalseを返す。
    if (m_boardData.at(index) == piece) return false;

    // マスの駒文字を設定する。
    m_boardData[index] = piece;

    // trueを返す。
    return true;
}

// 将棋盤内のデータ（81マスの駒文字情報）を返す。
const QVector<QChar>& ShogiBoard::boardData() const
{
    return m_boardData;
}

// 駒台のデータ（駒台の各駒の枚数情報）を返す。
const QMap<QChar, int>& ShogiBoard::getPieceStand() const
{
    return m_pieceStand;
}

// 将棋盤内のみのSFEN文字列を入力し、エラーチェックを行い、成り駒を1文字に変換したSFEN文字列を返す。
QString ShogiBoard::validateAndConvertSfenBoardStr(QString initialSfenStr)
{
    // 成り駒文字列
    const QStringList promotions = {"+P", "+L", "+N", "+S", "+B", "+R",
                                    "+p", "+l", "+n", "+s", "+b", "+r"};

    // 成り駒変換文字
    const QString replacements = "QMOTCUqmotcu";

    // SFEN文字列中の成り駒を1文字に変換する。
    for (int i = 0; i < promotions.size(); ++i) {
        initialSfenStr.replace(promotions[i], replacements[i]);
    }

    QStringList sfenParts = initialSfenStr.split("/");

    if (sfenParts.size() != 9) {
        // SFEN文字列は正確に9つの部分を含んでいる必要がある。
        const QString errorMessage = tr("An error occurred in ShogiBoard::validateAndConvertSfenBoardStr. SFEN string must contain exactly 9 parts.");

        qDebug() << "initialSfenStr: " << initialSfenStr;
        emit errorOccurred(errorMessage);
        return QString(); // ★ 打ち切り
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
                // SFEN文字列内に予期しない文字がある。
                const QString errorMessage = tr("An error occurred in ShogiBoard::validateAndConvertSfenBoardStr. Unexpected character in SFEN string.");

                // エラーメッセージを表示する。
                qDebug() << "sfenStr: " << initialSfenStr;
                qDebug() << "i: " << i;
                qDebug() << "rankStr: " << rankStr;
                qDebug() << "ch: " << ch;

                emit errorOccurred(errorMessage);
                return QString(); // ★ 打ち切り
            }
        }

        if (pieceCount != 9) {
            // 各段は正確に9個の駒または空マスを含む必要がある。
            const QString errorMessage = tr("An error occurred in ShogiBoard::validateAndConvertSfenBoardStr. Each rank must contain exactly 9 pieces or empty squares.");

            // エラーメッセージを表示する。
            qDebug() << "sfenStr: " << initialSfenStr;
            qDebug() << "pieceCount: " << pieceCount;

            emit errorOccurred(errorMessage);
            return QString(); // ★ 打ち切り
        }
    }

    return initialSfenStr;
}

// SFEN文字列の持ち駒部分の文字列を入力して、持ち駒を表す配列に各駒の枚数をセットする。
void ShogiBoard::setPieceStandFromSfen(const QString& str)
{
    // 初期化時にすべての持ち駒を0にする。
    initStand();

    // 持ち駒なしの場合は何もせずに関数を終了する。
    if (str == "-") return;

    // スペースが含まれている場合はエラーをスローする。
    if (str.contains(' ')) {
        // 持ち駒の文字列にスペースが含まれている。
        const QString errorMessage = tr("An error occurred in ShogiBoard::setPieceStandFromSFEN. The piece stand string contains a space.");

        // エラーメッセージを表示する。
        qDebug() << "str: " << str;

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }

    // 2桁の数字が含まれているかどうかを示すフラグ
    bool isTwoDigits = false;

    // 持ち駒の文字列を解析し、駒台の駒の数を更新する。
    // 文字列を1文字ずつ調べるループ
    for (int i = 0; i < str.length(); ++i) {
        // 文字が数字の場合
        if (str[i].isDigit()) {
            // 2桁の数字の場合
            if (isTwoDigits) {
                // 次の文字のインデックスが文字列の長さ未満であることを確認し、範囲外アクセスを防ぐ。
                // また、次の文字が有効な駒の種類を表しているかどうかを確認する。
                if (i + 1 < str.length() && m_pieceStand.contains(str[i + 1])) {
                    // str[i - 1]は10の位の数でとstr[i]は1の位の数なのでそれらを1つの数字に変換する。
                    int twoDigits = 10 * str[i - 1].digitValue() + str[i].digitValue();

                    // 次の文字（駒の種類）を取得する。
                    QChar pieceType = str[i + 1];

                    // 取得した駒の種類に対して持ち駒の数を加算する。
                    m_pieceStand[pieceType] += twoDigits;

                    // 数字と駒のタイプを表す2文字を処理したので、次の文字へインデックスを進める。
                    ++i;

                    // 2桁の数字のフラグを無効にする。
                    isTwoDigits = false;
                }
                // 数字の次に有効な駒が来ていない場合、例外を投げる。
                else {
                    // 数字の後に無効な駒が指定されている。
                    const QString errorMessage = tr("An error occurred in ShogiBoard::setPieceStandFromSFEN. Invalid piece type after number.");

                    // エラーメッセージを表示する。
                    qDebug() << "str: " << str;
                    qDebug() << "i: " << i;
                    qDebug() << "str[i]: " << str[i];

                    emit errorOccurred(errorMessage);
                    return; // ★ 打ち切り
                }
            }
            else {
                // 次の文字のインデックスが文字列の長さ未満であることを確認し、範囲外アクセスを防ぐ。
                // また、次の文字が有効な駒の種類を表しているかどうかを確認する。
                if (i + 1 < str.length() && m_pieceStand.contains(str[i + 1])) {
                    // 現在の数字を整数値に変換する、
                    int count = str[i].digitValue();

                    // 次の文字（駒の種類）を取得する。
                    QChar pieceType = str[i + 1];

                    // 取得した駒の種類に対して持ち駒の数を加算する。
                    m_pieceStand[pieceType] += count;

                    // 数字と駒のタイプを表す2文字を処理したので、次の文字へインデックスを進める。
                    ++i;
                }
                // 数字の次も数字の場合
                else if (str[i+1].isDigit()) {
                    // 2桁の数字であることを示すフラグを有効にする。
                    isTwoDigits = true;
                }
                // 数字の次に有効な駒が来ていない場合、例外を投げる。
                else {
                    // 数字の後に無効な駒が指定されている。
                    const QString errorMessage = tr("An error occurred in ShogiBoard::setPieceStandFromSFEN. Invalid piece type after number.");

                    // エラーメッセージを表示する。
                    qDebug() << "str: " << str;
                    qDebug() << "i: " << i;
                    qDebug() << "str[i]: " << str[i];

                    emit errorOccurred(errorMessage);
                    return; // ★ 打ち切り
                }
            }
        }
         // 現在の文字が駒の種類を直接表している場合（数字が前にない場合は1枚と数える。）
        else if (m_pieceStand.contains(str[i])) {
            m_pieceStand[str[i]] += 1;
        }
         // 有効な駒でも数字でもない不正な文字が含まれている場合、例外を投げる。
        else {
            // 無効な駒が含まれている。
            const QString errorMessage = tr("An error occurred in ShogiBoard::setPieceStandFromSFEN. Invalid piece type in piece stand string.");

            // エラーメッセージを表示する。
            qDebug() << "str: " << str;

            emit errorOccurred(errorMessage);
            return; // ★ 打ち切り
        }
    }
}

// SFEN文字列から将棋盤内に駒を配置する。
void ShogiBoard::setPiecePlacementFromSfen(QString& initialSfenStr)
{
    // 将棋盤内のみのSFEN文字列を入力し、エラーチェックを行い、成り駒を1文字に変換したSFEN文字列を返す。
    QString sfenStr = validateAndConvertSfenBoardStr(initialSfenStr);

    // SFEN文字列のインデックス
    int strIndex = 0;

    // 空白マスの数
    int emptySquares = 0;

    // 筋の数、すなわち9
    const int fileCount = files();

    // 駒文字
    QChar pieceChar;

    // 段
    for (int rank = 1; rank <= ranks(); ++rank) {
        // 筋
        for (int file = fileCount; file > 0; --file) {
            // 空白マスの数が0より大きい場合
            if (emptySquares > 0) {
                // 現在のマスの駒文字を空白に設定する。
                pieceChar = ' ';

                // 空白マスの数を1減らす。
                emptySquares--;
            }
            // 空白マスの数が0以下のの場合
            else {
                // SFEN文字列から駒文字を取得する。
                pieceChar = sfenStr.at(strIndex++);

                // 駒文字が数字の場合
                if (pieceChar.isDigit()) {
                    // 数字を整数に変換し、空白マスの数として設定する。
                    emptySquares = pieceChar.toLatin1() - '0';

                    // 現在のマスの駒文字を空白に設定する。
                    pieceChar = ' ';

                    // 空白マスの数を1減らす。
                    emptySquares--;
                }
            }
            // 指定したマスに駒を配置する。
            setDataInternal(file, rank, pieceChar.toLatin1());
        }

        // SFEN文字列のインデックスを進める。
        strIndex++;
    }
}

// SFEN文字列を入力し、エラーチェックを行い、次の手番、次の手が何手目かを取得する。
void ShogiBoard::validateSfenString(const QString& sfenStr, QString& sfenBoardStr, QString& sfenStandStr)
{
    QStringList sfenComponents = sfenStr.split(" ");

    if (sfenComponents.size() != 4) {
        // SFEN文字列は正確に3つのスペースで区切る必要がある。
        const QString errorMessage = tr("An error occurred in ShogiBoard::validateSfenString. SFEN string must be separated by exactly 3 spaces.");

        // エラーメッセージを表示する。
        qDebug() << "sfenStr: " << sfenStr;
        qDebug() << "sfenComponents.size(): " << sfenComponents.size();

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }

    // 将棋盤内のみのSFEN文字列を取得する。
    sfenBoardStr = sfenComponents.at(0);

    // 先手あるいは下手"b"、後手あるいは上手"w"の指定を取得する。
    QString playerTurnStr = sfenComponents.at(1);

    // 手番の指定が"b"または"w"のいずれかである場合
    if (playerTurnStr == "b" || playerTurnStr == "w") {
        // 現在の手番を設定する。
        m_currentPlayer = playerTurnStr;
    }
    // それ以外の場合
    else {
        // SFEN文字列は先手あるいは下手"b"または後手あるいは上手"w"の指定が必要
        const QString errorMessage = tr("An error occurred in ShogiBoard::validateSfenString. SFEN string must specify either black 'b' or white 'w'.");

        // エラーメッセージを表示する。
        qDebug() << "sfenStr: " << sfenStr;
        qDebug() << "playerTurnStr: " << playerTurnStr;

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }

    // 持ち駒のSFEN文字列を取得する。
    sfenStandStr = sfenComponents.at(2);

    // 次の手が何手目かを取得できたかどうかを返すフラグ
    bool conversionSuccessful;

    // 次の手が何手目かを取得する。
    m_currentMoveNumber = sfenComponents.at(3).toInt(&conversionSuccessful);

    // 次の手が何手目かを取得できない場合、または次の手が何手目かが負の整数の場合
    if (!conversionSuccessful || m_currentMoveNumber <= 0) {
        // SFEN文字列の最後の部分は正の整数（次の手が何手目かを示す）である必要がある。
        const QString errorMessage = tr("An error occurred in ShogiBoard::validateSfenString. The last part of the SFEN string must be a positive integer (indicating the next move number).");

        // エラーメッセージを表示する。
        qDebug() << "sfenStr: " << sfenStr;

        emit errorOccurred(errorMessage);
        return; // ★ 打ち切り
    }
}

// 将棋盤と駒台を含むSFEN文字列で将棋盤全体を更新する場合、この関数を使う。
// 将棋盤に入力で渡されるsfen形式の文字列に文法的に誤りが無いかチェックする。
// 将棋盤と駒台のSFEN文字列を指定して将棋盤と駒台の駒の更新を行い、再描画する。
// 入力は、将棋盤と駒台を含むSFEN文字列
void ShogiBoard::setSfen(const QString& sfenStr)
{
    // 例．
    // "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"の
    // "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"の文字列
    QString sfenBoardStr;

    // sfen形式の文字列の持ち駒部分
    QString sfenStandStr;

    // SFEN文字列に以下のチェックを行い、将棋盤内の文字列sfenStr0と
    // 駒台の文字列sfenStr2を取得する。
    // SFEN文字列にスペース文字が3個含まれているか。
    // SFEN文字列に先手、後手の指定があるか。
    // SFEN文字列に次の手が何手目かの指定があるか。
    validateSfenString(sfenStr, sfenBoardStr, sfenStandStr);

    // sfen文字列から将棋盤内に駒を配置する。
    // m_boardDataに駒文字をセットする。
    // 入力は、将棋盤内のみのSFEN文字列
    // 将棋盤内のみのSFEN文字列のチェックも最初に行う。
    setPiecePlacementFromSfen(sfenBoardStr);

    // SFEN文字列の持ち駒部分の文字列を入力して、持ち駒を表す配列に各駒の枚数をセットする。
    // 駒台の各持ち駒の枚数をカウントし、m_pieceStandにセットする。
    // 入力は、持ち駒のSFEN文字列
    // 持ち駒部分のSFEN文字列のチェックも行う。
    setPieceStandFromSfen(sfenStandStr);

    // ShogiView::setBoardのconnectで使用されているシグナル
    // スロットのrepaint関数が実行される。（将棋盤、駒台の再描画）
    emit boardReset();
}

// 成り駒の文字をSFEN形式の駒文字列に変換する。
QString ShogiBoard::convertPieceToSfen(const QChar piece) const
{
    // 駒文字をSFEN形式の駒文字に変換するためのマップ
    static const QMap<QChar, QString> sfenMap = {{'Q', "+P"}, {'M', "+L"}, {'O', "+N"}, {'T', "+S"},
                                                 {'C', "+B"}, {'U', "+R"}, {'q', "+p"}, {'m', "+l"},
                                                 {'o', "+n"}, {'t', "+s"}, {'c', "+b"}, {'u', "+r"}};

    // 該当する成り駒がある場合、SFEN形式の駒文字を返す。
    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(piece);
}

// 盤面をSFEN形式へ変換する。
QString ShogiBoard::convertBoardToSfen()
{
    // SFEN形式文字列
    QString str = "";

    // 段
    for (int rank = 1; rank <= ranks(); ++rank) {
        // 空白文字のカウンタ
        int spacecount = 0;
        // 筋
        for (int file = files(); file > 0; --file) {
            // 駒文字
            QChar piece = getPieceCharacter(file, rank);

            // 駒文字が空白の場合
            if (piece == ' ') {
                // 空白文字のカウントに1を足す。
                spacecount++;

                // 空白文字のカウンタが9、あるいは1筋の場合
                if (spacecount == 9 || file == 1) {
                    // 空白文字のカウンタを文字列に変換し、SFEN形式文字列に連結する。
                    str += QString::number(spacecount);
                }
            }
            // 駒文字が空白文字以外の場合
            else {
                // 空白文字のカウンタが0より大きい場合
                if (spacecount > 0) {
                    // 空白文字のカウンタを文字列に変換し、SFEN形式文字列に連結する。
                    str += QString::number(spacecount);

                    // 空白文字のカウンタを0にリセットする。
                    spacecount = 0;
                }

                // 駒文字をSFEN形式文字列に連結する。
                str += convertPieceToSfen(piece);
            }
        }

        // 9筋以外の場合、"/"をSFEN形式文字列に連結する。
        if (rank != 9) str += "/";
    }

    // SFEN形式文字列を返す。
    return str;
}

// 駒台の駒数をSFEN形式へ変換する。
QString ShogiBoard::convertStandToSfen() const
{
    // 持ち駒のSFEN形式文字列
    QString handPiece = "";

    // 駒文字のリスト
    QList<QChar> keys = {'R', 'B', 'G', 'S', 'N', 'L', 'P'};

    // 駒文字のリストを走査し、持ち駒のSFEN形式文字列を作成する。
    for(const auto& key : keys) {
        // 先手または下手
        // 駒文字に該当する駒数を取得する。
        int value = m_pieceStand.value(key, 0);

        // 駒数が1以上の場合
        if (value > 0) {
            // 駒数が1より大きい場合、駒数を文字列に変換し、SFEN形式文字列に連結する。
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key);
        }

        // 後手または上手
        // 駒文字に該当する駒数を取得する。
        value = m_pieceStand.value(key.toLower(), 0);

        // 駒数が1以上の場合
        if (value > 0) {
            // 駒数が1より大きい場合、駒数を文字列に変換し、SFEN形式文字列に連結する。
            handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key.toLower());
        }
    }

    // 持ち駒のSFEN形式文字列を返す。
    // 持ち駒がない場合は"-"を返す。
    return handPiece.isEmpty() ? "-" : handPiece;
}

// src/core/shogiboard.cpp
#include <QDebug>

void ShogiBoard::addSfenRecord(const QString& nextTurn, int moveIndex, QStringList* sfenRecord)
{
    if (!sfenRecord) {
        qDebug() << "[SFEN][add] sfenRecord is null";
        return;
    }

    // moveIndex: 0 始まりの着手番号を想定
    //   通常記録     → +1 で手数フィールドへ
    //   特例(-1等)   → 1 を明示（開始直後など）
    const int moveCountField = (moveIndex < 0) ? 1 : (moveIndex + 1);

    const int before = sfenRecord->size();
    const QString boardSfen = convertBoardToSfen();
    QString stand = convertStandToSfen();
    if (stand.isEmpty()) stand = QStringLiteral("-");

    qDebug().noquote() << "[SFEN][add] BEFORE size=" << before
                       << " rec*=" << static_cast<const void*>(sfenRecord)
                       << " nextTurn=" << nextTurn
                       << " moveIndex=" << moveIndex
                       << " => field=" << moveCountField;
    qDebug().noquote() << "[SFEN][add] board=" << boardSfen << " stand=" << stand;

    const QString sfen = QStringLiteral("%1 %2 %3 %4")
                             .arg(boardSfen, nextTurn, stand, QString::number(moveCountField));
    sfenRecord->append(sfen);

    qDebug().noquote() << "[SFEN][add] AFTER  size=" << sfenRecord->size()
                       << " appended=" << sfen;

    if (!sfenRecord->isEmpty()) {
        // head / tail を直接出す（std::min は使わない）
        qDebug().noquote() << "[SFEN][add] head[0]=" << sfenRecord->first();
        qDebug().noquote() << "[SFEN][add] tail[last]=" << sfenRecord->last();
    }

    // 先頭が破壊されてないか簡易チェック
    if (!sfenRecord->isEmpty()) {
        const QStringList parts = sfenRecord->first().split(QLatin1Char(' '), Qt::KeepEmptyParts);
        if (parts.size() == 4) {
            const QString turn0 = parts[1];
            const QString move0 = parts[3];
            if (move0 != QLatin1String("1")) {
                qDebug().noquote() << "[WARN][SFEN][add] head[0] moveCount != 1  head=" << sfenRecord->first();
            }
            if (turn0 != QLatin1String("b") && turn0 != QLatin1String("w")) {
                qDebug().noquote() << "[WARN][SFEN][add] head[0] turn invalid  head=" << sfenRecord->first();
            }
        } else {
            qDebug().noquote() << "[WARN][SFEN][add] head[0] malformed  head=" << sfenRecord->first();
        }
    }
}

// 局面編集中に右クリックで成駒/不成駒/先後を巡回変換する（禁置き段＋二歩をスキップ）。
void ShogiBoard::promoteOrDemotePiece(const int fileFrom, const int rankFrom)
{
    auto nextInCycle = [](const QVector<QChar>& cyc, QChar cur)->QChar {
        int idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QVector<QChar>{'L','M','l','m'}; // 香 → 成香 → 相手香 → 相手成香
    const auto knightCycle = QVector<QChar>{'N','O','n','o'}; // 桂 → 成桂 → 相手桂 → 相手成桂
    const auto silverCycle = QVector<QChar>{'S','T','s','t'};
    const auto bishopCycle = QVector<QChar>{'B','C','b','c'};
    const auto rookCycle   = QVector<QChar>{'R','U','r','u'};
    const auto pawnCycle   = QVector<QChar>{'P','Q','p','q'}; // 歩 → と金 → 相手歩 → 相手と金

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

    // 段による不成禁止（必成）を判定
    auto isRankDisallowed = [&](QChar piece)->bool {
        if (!onBoard) return false;
        // 先手側
        if (piece == 'L' && rankFrom == 1) return true;
        if (piece == 'N' && (rankFrom == 1 || rankFrom == 2)) return true;
        if (piece == 'P' && rankFrom == 1) return true;
        // 後手側
        if (piece == 'l' && rankFrom == 9) return true;
        if (piece == 'n' && (rankFrom == 8 || rankFrom == 9)) return true;
        if (piece == 'p' && rankFrom == 9) return true;
        return false;
    };

    // 二歩禁止：その筋に別の同側の“歩（不成）”が既にあるなら P/p は不可
    auto isNiFuDisallowed = [&](QChar candidate)->bool {
        if (!onBoard) return false;
        if (candidate != 'P' && candidate != 'p') return false;
        for (int r = 1; r <= 9; ++r) {
            if (r == rankFrom) continue; // 自マスは除外
            const QChar pc = getPieceCharacter(fileFrom, r);
            if (candidate == 'P' && pc == 'P') return true; // 同筋に先手歩
            if (candidate == 'p' && pc == 'p') return true; // 同筋に後手歩
        }
        return false;
    };

    auto isDisallowed = [&](QChar piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    // 禁止形は巡回列から除外（= 自動スキップ）
    QVector<QChar> filtered;
    filtered.reserve(base.size());
    for (QChar p : base) {
        if (!isDisallowed(p)) filtered.push_back(p);
    }
    if (filtered.isEmpty()) return;

    // 現在形が filtered 外（既に禁止形）なら、base を回して最初の許可形へジャンプ
    QChar next = cur;
    if (filtered.indexOf(cur) < 0) {
        QChar probe = cur;
        for (int i = 0; i < base.size(); ++i) {
            probe = nextInCycle(base, probe);
            if (filtered.indexOf(probe) >= 0) { next = probe; break; }
        }
    } else {
        // 通常：filtered 内で次へ
        int idx = filtered.indexOf(cur);
        next = filtered[(idx + 1) % filtered.size()];
    }

    setData(fileFrom, rankFrom, next);
}

// 手番の持ち駒を出力する。
void ShogiBoard::printPlayerPieces(const QString& player, const QString& pieceSet) const
{
    // 駒文字と漢字表記のマップ
    static const QMap<QChar, QString> pieceNames = {
        {'K', "玉"}, {'R', "飛"}, {'B', "角"}, {'G', "金"}, {'S', "銀"}, {'N', "桂"}, {'L', "香"}, {'P', "歩"},
        {'k', "玉"}, {'r', "飛"}, {'b', "角"}, {'g', "金"}, {'s', "銀"}, {'n', "桂"}, {'l', "香"}, {'p', "歩"}
    };

    qDebug() << player << "の持ち駒";

    // 駒文字のリストを走査し、持ち駒の枚数を出力する。
    for (const QChar& piece : pieceSet) {
        qDebug() << pieceNames[piece] << " " << m_pieceStand[piece];
    }
}

// 持ち駒を出力する。
void ShogiBoard::printPieceStand()
{
    printPlayerPieces("先手", "KRGBSNLP");
    printPlayerPieces("後手", "krgbsnlp");
}

// 駒の文字を変換する。成駒は相手の駒になるように、大文字を小文字に、小文字を大文字に変換する。
QChar ShogiBoard::convertPieceChar(const QChar c) const
{
    // 駒文字の変換マップ
    static const QMap<QChar, QChar> conversionMap = {
        {'Q', 'p'}, {'M', 'l'}, {'O', 'n'}, {'T', 's'}, {'C', 'b'}, {'U', 'r'},
        {'q', 'P'}, {'m', 'L'}, {'o', 'N'}, {'t', 'S'}, {'c', 'B'}, {'u', 'R'}
    };

    // 変換マップに含まれている場合、変換した文字を返す。
    if (conversionMap.contains(c)) {
        return conversionMap.value(c);
    }

    // それ以外の場合、大文字を小文字に、小文字を大文字に変換して返す。
    return c.isUpper() ? c.toLower() : c.toUpper();
}

// 指したマスが将棋盤内で相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(QChar dest)
{
    // 駒の文字を変換する。成駒は相手の駒になるように、大文字を小文字に、小文字を大文字に変換する。
    QChar pieceChar = convertPieceChar(dest);

    // 駒台の該当する駒の数を1増やす。
    if (m_pieceStand.contains(pieceChar)) {
            m_pieceStand[pieceChar]++;
    }
}

// 駒台から駒を指した場合、駒台の駒の枚数を1減らす。
void ShogiBoard::decrementPieceOnStand(QChar source)
{
    // 駒が駒台に存在するか確認する。
    if (m_pieceStand.contains(source)) {
        // 駒の数を1減らす。
        m_pieceStand[source]--;
    }
}

// 駒台から指そうとした場合、駒台の駒数が0以下の時は、駒台の駒は無いので指せない。
bool ShogiBoard::isPieceAvailableOnStand(const QChar source, const int fileFrom) const
{
    // 駒台から指そうとした場合
    if (fileFrom == 10 || fileFrom == 11) {
        // 駒が駒台に存在し、その数が0より大きいかを確認する。
        if (m_pieceStand.contains(source) && m_pieceStand[source] > 0) {
            // 駒が存在し、その数が0より大きい場合、指せる。
            return true;
        }
        // それ以外は指せない。
        else {
            return false;
        }
    }
    // 盤内から指そうとした場合、チェック範囲外のため、指せる。
    else {
        // 指せる。
        return true;
    }
}

// 成駒を元の駒に変換する。
QChar ShogiBoard::convertPromotedPieceToOriginal(const QChar dest) const
{
    // 成駒を元の駒に変換するマップ
    static const QMap<QChar, QChar> promotedToOriginalMap = {
        {'Q', 'P'}, // 先手のと金 -> 先手の歩
        {'M', 'L'}, // 先手の成香 -> 先手の香車
        {'O', 'N'}, // 先手の成桂 -> 先手の桂馬
        {'T', 'S'}, // 先手の成銀 -> 先手の銀
        {'C', 'B'}, // 先手の馬 -> 先手の角
        {'U', 'R'}, // 先手の龍 -> 先手の飛車
        {'q', 'p'}, // 後手のと金 -> 後手の歩
        {'m', 'l'}, // 後手の成香 -> 後手の香車
        {'o', 'n'}, // 後手の成桂 -> 後手の桂馬
        {'t', 's'}, // 後手の成銀 -> 後手の銀
        {'c', 'b'}, // 後手の馬 -> 後手の角
        {'u', 'r'}  // 後手の龍 -> 後手の飛車
    };

    return promotedToOriginalMap.value(dest, dest);
}

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const QChar dest)
{
    // 成駒を元の駒に変換
    QChar originalPiece = convertPromotedPieceToOriginal(dest);

    // 駒台の該当する駒の数を増やす。
    if (m_pieceStand.contains(originalPiece)) {
        m_pieceStand[originalPiece]++;
    }
}

// 先手か後手かのどちらか（bあるいはw）を返す。
QString ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const QChar source, const QChar dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    // 指した先が将棋盤内の場合
    if (fileTo < 10) {
        // 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
        addPieceToStand(dest);
    // 駒台に駒を移した場合
    } else {
        // 駒台の駒の枚数に1加える。
        incrementPieceOnStand(dest);
    }

    // 駒台から駒を指すかどうかを確認する。
    if (fileFrom == 10 || fileFrom == 11) {
        // 駒台から指した場合、駒台の駒の枚数を1減らす。
        decrementPieceOnStand(source);
    }

    //begin
    // printPieceCount();
    //end

    // 指す駒を指したマスに移動させる。m_boardDataの入れ替えだけを行う。
    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

// 駒の初期値を設定する。
void ShogiBoard::setInitialPieceStandValues()
{
    // 駒の初期値のリスト
    static const QList<QPair<QChar, int>> initialValues = {
        {'P', 9}, // 先手の歩
        {'L', 2}, // 先手の香車
        {'N', 2}, // 先手の桂馬
        {'S', 2}, // 先手の銀
        {'G', 2}, // 先手の金
        {'B', 1}, // 先手の角
        {'R', 1}, // 先手の飛車
        {'K', 1}, // 先手の王
        {'k', 1}, // 後手の玉
        {'r', 1}, // 後手の飛車
        {'b', 1}, // 後手の角
        {'g', 2}, // 後手の金
        {'s', 2}, // 後手の銀
        {'n', 2}, // 後手の桂馬
        {'l', 2}, // 後手の香車
        {'p', 9}, // 後手の歩
    };

    // 駒台の駒の初期値を設定する。
    for (const auto& pair : initialValues) {
        m_pieceStand[pair.first] = pair.second;
    }
}

// 「全ての駒を駒台へ」をクリックした時に実行される。
// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void ShogiBoard::resetGameBoard()
{
    // 将棋盤のマスを全て空白文字にする。
    m_boardData.fill(' ', ranks() * files());

    // 駒の初期値を設定する。
    setInitialPieceStandValues();
}

// 「先後反転」をクリックした時に実行される。
// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    // 元の配置データをバックアップ
    QVector<QChar> originalBoardData = m_boardData;

    // 新たな盤面データのリストを作成する。
    QVector<QChar> newBoardData;

    // 先手と後手の駒を反転
    for (int i = 0; i < 81; i++) {
        // 反転させる駒文字を取得する。
        QChar pieceChar = originalBoardData.at(80 - i);

        // 駒文字が大文字の場合、小文字に反転させる。
        // 駒文字が小文字の場合、大文字に反転させる。
        pieceChar = pieceChar.isLower() ? pieceChar.toUpper() : pieceChar.toLower();

        // 新たな盤面データに駒文字を追加する。
        newBoardData.append(pieceChar);
    }

    // 配置データを新たなもので上書き
    m_boardData = newBoardData;

    // 駒台の駒数を複写する。
    static const QMap<QChar, int> originalPieceStand = m_pieceStand;

    // 先手と後手の駒台上の駒の数を反転する。
    for (auto it = originalPieceStand.cbegin(); it != originalPieceStand.cend(); ++it) {
        // 駒文字を反転させる。
        QChar flippedChar = it.key().isLower() ? it.key().toUpper() : it.key().toLower();

        // 駒台の駒数を反転させる。
        m_pieceStand[flippedChar] = it.value();
    }
}

// 持ち駒を出力する。
void ShogiBoard::printPieceCount() const
{
    qDebug() << "先手の持ち駒:";
    qDebug() << "歩: " << m_pieceStand['P'];
    qDebug() << "香車: " << m_pieceStand['L'];
    qDebug() << "桂馬: " << m_pieceStand['N'];
    qDebug() << "銀: " << m_pieceStand['S'];
    qDebug() << "金: " << m_pieceStand['G'];
    qDebug() << "角: " << m_pieceStand['B'];
    qDebug() << "飛車: " << m_pieceStand['R'];

    qDebug() << "後手の持ち駒:";
    qDebug() << "歩: " << m_pieceStand['p'];
    qDebug() << "香車: " << m_pieceStand['l'];
    qDebug() << "桂馬: " << m_pieceStand['n'];
    qDebug() << "銀: " << m_pieceStand['s'];
    qDebug() << "金: " << m_pieceStand['g'];
    qDebug() << "角: " << m_pieceStand['b'];
    qDebug() << "飛車: " << m_pieceStand['r'];
}
