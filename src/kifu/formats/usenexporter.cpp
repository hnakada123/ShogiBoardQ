/// @file usenexporter.cpp
/// @brief USEN形式棋譜エクスポータクラスの実装

#include "usenexporter.h"
#include "kifubranchtree.h"
#include "logcategories.h"

#include <QStringView>

// ========================================
// ヘルパ関数
// ========================================

// ヘルパ関数: 指し手から手番記号（▲△）を除去
static QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

// ヘルパ関数: 終局語を判定
static bool isTerminalMove(const QString& move)
{
    static const QStringList terminals = {
        QStringLiteral("投了"),
        QStringLiteral("中断"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("不詰")
    };
    const QString stripped = removeTurnMarker(move);
    for (const QString& t : terminals) {
        if (stripped.contains(t)) return true;
    }
    return false;
}

// ========================================
// USEN形式エンコーディング
// ========================================

// USEN形式の座標系:
// - マス番号: sq = (rank - 1) * 9 + (file - 1)
// - 例: 1一 = 0, 9九 = 80
// - rank: 段（一=1, 二=2, ..., 九=9）
// - file: 筋（1=1, 2=2, ..., 9=9）

// 駒打ち時の駒コード (from_sq = 81 + piece_code):
// 歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
static int usiPieceToUsenDropCode(QChar usiPiece)
{
    switch (usiPiece.toUpper().toLatin1()) {
    case 'P': return 10;  // 歩
    case 'L': return 11;  // 香
    case 'N': return 12;  // 桂
    case 'S': return 13;  // 銀
    case 'G': return 9;   // 金
    case 'B': return 14;  // 角
    case 'R': return 15;  // 飛
    default:  return -1;  // 無効
    }
}

// base36文字セット
static constexpr QStringView kUsenBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";

static QString intToBase36(int moveCode)
{
    // 3文字のbase36に変換（範囲: 0-46655）
    if (moveCode < 0 || moveCode > 46655) {
        qCWarning(lcKifu) << "moveCode out of range:" << moveCode;
        return QStringLiteral("000");
    }

    QString result;
    result.reserve(3);

    int v2 = moveCode % 36;
    moveCode /= 36;
    int v1 = moveCode % 36;
    moveCode /= 36;
    int v0 = moveCode;

    result.append(kUsenBase36Chars.at(v0));
    result.append(kUsenBase36Chars.at(v1));
    result.append(kUsenBase36Chars.at(v2));

    return result;
}

static QString encodeUsiMoveToUsen(const QString& usiMove)
{
    if (usiMove.isEmpty() || usiMove.size() < 4) {
        return QString();
    }

    // 成りフラグを確認
    bool isPromotion = usiMove.endsWith(QLatin1Char('+'));

    int from_sq, to_sq;

    // 駒打ちの判定 (P*5e 形式)
    if (usiMove.at(1) == QLatin1Char('*')) {
        // 駒打ち: from_sq = 81 + piece_code
        QChar pieceChar = usiMove.at(0);
        int pieceCode = usiPieceToUsenDropCode(pieceChar);
        if (pieceCode < 0) {
            qCWarning(lcKifu) << "Unknown drop piece:" << pieceChar;
            return QString();
        }
        from_sq = 81 + pieceCode;

        // 移動先を解析
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    } else {
        // 盤上移動: 7g7f 形式
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        from_sq = (fromRank - 1) * 9 + (fromFile - 1);
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    }

    // エンコード: code = (from_sq * 81 + to_sq) * 2 + (成る場合は1)
    int moveCode = (from_sq * 81 + to_sq) * 2 + (isPromotion ? 1 : 0);

    return intToBase36(moveCode);
}

static QString sfenToUsenPosition(const QString& sfen)
{
    // 平手初期局面のチェック
    static const QString kHirateSfenPrefix = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QString();  // 平手: 空文字列（~の前に何も置かない）
    }

    // 平手初期局面の場合は空文字列を返す
    if (trimmed.startsWith(kHirateSfenPrefix)) {
        return QString();
    }

    // カスタム局面: SFEN -> USEN変換
    // 手数（4番目のフィールド）を除去してからエンコード
    // SFEN形式: "board turn hands [movecount]"
    const QStringList parts = trimmed.split(QLatin1Char(' '));
    QString sfenWithoutMoveCount;
    if (parts.size() >= 3) {
        sfenWithoutMoveCount = parts[0] + QLatin1Char(' ') + parts[1] + QLatin1Char(' ') + parts[2];
    } else {
        sfenWithoutMoveCount = trimmed;
    }

    // '/' -> '_', ' ' -> '.', '+' -> 'z'
    QString usen = sfenWithoutMoveCount;
    usen.replace(QLatin1Char('/'), QLatin1Char('_'));
    usen.replace(QLatin1Char(' '), QLatin1Char('.'));
    usen.replace(QLatin1Char('+'), QLatin1Char('z'));

    return usen;
}

static QString getUsenTerminalCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);

    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("i");
    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("r");
    if (move.contains(QStringLiteral("時間切れ")) || move.contains(QStringLiteral("切れ負け")))
        return QStringLiteral("t");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("p");
    if (move.contains(QStringLiteral("持将棋")) || move.contains(QStringLiteral("千日手")))
        return QStringLiteral("j");

    return QString();
}

static QString inferUsiFromSfenDiff(const QString& sfenBefore, const QString& sfenAfter, bool isSente)
{
    // 2つのSFEN間の差分からUSI形式の指し手を推測
    // SFEN形式: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    // 盤面部分を比較して、移動した駒を特定する

    if (sfenBefore.isEmpty() || sfenAfter.isEmpty()) {
        return QString();
    }

    // SFEN盤面部分を抽出
    QString boardBefore = sfenBefore.section(QLatin1Char(' '), 0, 0);
    QString boardAfter = sfenAfter.section(QLatin1Char(' '), 0, 0);

    // 盤面を9x9配列に展開 (promoted pieces use 0x100 flag)
    // board[rank][file] の順序で格納 (rank: 0-8 = 一段〜九段, file: 0-8 = 1筋〜9筋)
    auto expandBoard = [](const QString& board) -> QVector<int> {
        QVector<int> result(81, 0);  // 0 = empty
        int sfenIdx = 0;  // SFEN上のインデックス（左上から右へ、上から下へ）
        bool promoted = false;

        for (QChar c : board) {
            if (c == QLatin1Char('/')) continue;
            if (c == QLatin1Char('+')) {
                promoted = true;
                continue;
            }
            if (c.isDigit()) {
                int n = c.toLatin1() - '0';
                sfenIdx += n;
            } else {
                if (sfenIdx < 81) {
                    int rank = sfenIdx / 9;       // 0-8 (一段目〜九段目)
                    int sfenFile = sfenIdx % 9;   // 0-8 (9筋〜1筋の順)
                    int file = 8 - sfenFile;      // 0-8 (1筋〜9筋の順に変換)
                    int boardIdx = rank * 9 + file;

                    int pieceCode = static_cast<unsigned char>(c.toLatin1());
                    if (promoted) pieceCode |= 0x100;  // 成駒フラグ
                    result[boardIdx] = pieceCode;
                }
                sfenIdx++;
                promoted = false;
            }
        }
        return result;
    };

    QVector<int> before = expandBoard(boardBefore);
    QVector<int> after = expandBoard(boardAfter);

    // 差分を探す
    int fromRank = -1, fromFile = -1;
    int toRank = -1, toFile = -1;
    int movedPiece = 0;
    bool isPromotion = false;

    for (int rank = 0; rank < 9; ++rank) {
        for (int file = 0; file < 9; ++file) {
            int idx = rank * 9 + file;
            int beforePiece = before[idx];
            int afterPiece = after[idx];

            if (beforePiece != afterPiece) {
                // beforeにあってafterにない → 移動元
                if (beforePiece != 0 && afterPiece == 0) {
                    fromRank = rank;
                    fromFile = file;
                    movedPiece = beforePiece;
                }
                // beforeになくてafterにある → 移動先
                else if (beforePiece == 0 && afterPiece != 0) {
                    toRank = rank;
                    toFile = file;
                    // 成り判定: 移動元が非成りで移動先が成り
                    if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                        isPromotion = true;
                    }
                }
                // 両方に駒がある → 駒取り
                else if (beforePiece != 0 && afterPiece != 0) {
                    // 先手の駒は大文字、後手の駒は小文字
                    char beforeChar = static_cast<char>(beforePiece & 0xFF);
                    char afterChar = static_cast<char>(afterPiece & 0xFF);
                    bool beforeIsSente = (beforeChar >= 'A' && beforeChar <= 'Z');
                    bool afterIsSente = (afterChar >= 'A' && afterChar <= 'Z');

                    if (beforeIsSente != afterIsSente) {
                        if ((isSente && afterIsSente) || (!isSente && !afterIsSente)) {
                            toRank = rank;
                            toFile = file;
                            // 成り判定
                            if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                                isPromotion = true;
                            }
                        } else {
                            fromRank = rank;
                            fromFile = file;
                            movedPiece = beforePiece;
                        }
                    }
                }
            }
        }
    }

    // 駒打ちの検出（fromRank == -1 で toRank != -1）
    if (fromRank < 0 && toRank >= 0) {
        // 持ち駒から打った
        int piece = after[toRank * 9 + toFile];
        char pieceChar = static_cast<char>(piece & 0xFF);
        QChar usiPiece = QChar(pieceChar).toUpper();

        // USI座標: ファイルは1-9、ランクはa-i
        int usiFile = toFile + 1;  // 1-9
        char usiRank = static_cast<char>('a' + toRank);  // a-i

        return QStringLiteral("%1*%2%3")
            .arg(usiPiece)
            .arg(usiFile)
            .arg(QChar(usiRank));
    }

    // 盤上移動
    if (fromRank >= 0 && toRank >= 0) {
        // USI座標: ファイルは1-9、ランクはa-i
        int fromUsiFile = fromFile + 1;
        char fromUsiRank = static_cast<char>('a' + fromRank);
        int toUsiFile = toFile + 1;
        char toUsiRank = static_cast<char>('a' + toRank);

        QString usi = QStringLiteral("%1%2%3%4")
            .arg(fromUsiFile)
            .arg(QChar(fromUsiRank))
            .arg(toUsiFile)
            .arg(QChar(toUsiRank));

        if (isPromotion) {
            usi += QLatin1Char('+');
        }

        return usi;
    }

    return QString();
}

// ========================================
// エクスポート
// ========================================

QStringList UsenExporter::exportLines(const GameRecordModel& model,
                                      const GameRecordModel::ExportContext& ctx,
                                      const QStringList& usiMoves)
{
    QStringList out;

    // 1) 初期局面をUSEN形式に変換
    QString position = sfenToUsenPosition(ctx.startSfen);

    // 2) 本譜の指し手をエンコード
    QString mainMoves;
    QString terminalCode;

    // 本譜の指し手リストを取得
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();

    // 各USI指し手をUSEN形式にエンコード
    for (const QString& usi : std::as_const(usiMoves)) {
        QString encoded = encodeUsiMoveToUsen(usi);
        if (!encoded.isEmpty()) {
            mainMoves += encoded;
        }
    }

    // 終局語のチェック
    if (!disp.isEmpty()) {
        const QString& lastMove = disp.last().prettyMove;
        if (isTerminalMove(lastMove)) {
            terminalCode = getUsenTerminalCode(lastMove);
        }
    }

    // 本譜のUSEN文字列を構築
    // USEN構造: [初期局面]~[オフセット].[指し手][.終局コード]
    // 初期局面は~の前に置く（平手の場合は空）、本譜のオフセットは0
    QString usen = position + QStringLiteral("~0.%1").arg(mainMoves);

    // 終局コードを追加（明示的な終局語がない場合は投了をデフォルトとする）
    if (terminalCode.isEmpty()) {
        terminalCode = QStringLiteral("r");
    }
    usen += QStringLiteral(".") + terminalCode;

    // 3) 分岐を追加
    // 新システム: KifuBranchTree から出力
    KifuBranchTree* branchTree = model.branchTree();
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QVector<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);

            // オフセット（分岐開始位置 - 1）
            int offset = line.branchPly - 1;
            if (offset < 0) offset = 0;

            QString branchMoves;
            QString branchTerminal;

            // 分岐のSFENリストとディスプレイアイテムを取得
            QStringList sfenList = branchTree->getSfenListForLine(lineIdx);
            QList<KifDisplayItem> branchDisp = branchTree->getDisplayItemsForLine(lineIdx);

            // SFENの差分からUSI指し手を推測
            for (qsizetype i = line.branchPly; i < branchDisp.size(); ++i) {
                const KifDisplayItem& item = branchDisp[i];
                if (item.prettyMove.isEmpty()) continue;

                // 終局語チェック
                if (isTerminalMove(item.prettyMove)) {
                    branchTerminal = getUsenTerminalCode(item.prettyMove);
                    break;
                }

                // SFENリストを使ってUSI指し手を推測
                // i-1番目のSFENからi番目のSFENへの差分を計算
                if (i < sfenList.size() && (i - 1) >= 0 && (i - 1) < sfenList.size()) {
                    QString usi = inferUsiFromSfenDiff(sfenList[i - 1], sfenList[i], (i % 2 != 0));
                    if (!usi.isEmpty()) {
                        QString encoded = encodeUsiMoveToUsen(usi);
                        if (!encoded.isEmpty()) {
                            branchMoves += encoded;
                        }
                    }
                }
            }

            // 分岐のUSEN文字列を構築
            // 終局コードがない場合は投了をデフォルトとする（USEN仕様）
            if (branchTerminal.isEmpty()) {
                branchTerminal = QStringLiteral("r");
            }
            QString branchUsen = QStringLiteral("~%1.%2.%3").arg(offset).arg(branchMoves, branchTerminal);

            usen += branchUsen;
        }
    }

    out << usen;

    const int branchCount = (branchTree != nullptr) ? (branchTree->lineCount() - 1) : 0;
    qCDebug(lcKifu).noquote() << "toUsenLines: generated USEN with"
                              << mainMoves.size() / 3 << "mainline moves,"
                              << branchCount << "variations";

    return out;
}
