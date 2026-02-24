/// @file jkfexporter.cpp
/// @brief JKF形式棋譜エクスポータクラスの実装

#include "jkfexporter.h"
#include "gamerecordmodel.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"
#include "logcategories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

static const QRegularExpression& newlineRe()
{
    static const QRegularExpression re(QStringLiteral("\r?\n"));
    return re;
}

// ========================================
// ヘルパ関数（JKFエクスポート専用）
// ========================================

// 日本語駒名からCSA形式駒種に変換
static QString kanjiToCsaPiece(const QString& kanji)
{
    if (kanji.contains(QStringLiteral("歩"))) return QStringLiteral("FU");
    if (kanji.contains(QStringLiteral("香"))) return QStringLiteral("KY");
    if (kanji.contains(QStringLiteral("桂"))) return QStringLiteral("KE");
    if (kanji.contains(QStringLiteral("銀"))) return QStringLiteral("GI");
    if (kanji.contains(QStringLiteral("金"))) return QStringLiteral("KI");
    if (kanji.contains(QStringLiteral("角"))) return QStringLiteral("KA");
    if (kanji.contains(QStringLiteral("飛"))) return QStringLiteral("HI");
    if (kanji.contains(QStringLiteral("玉")) || kanji.contains(QStringLiteral("王"))) return QStringLiteral("OU");
    if (kanji.contains(QStringLiteral("と"))) return QStringLiteral("TO");
    if (kanji.contains(QStringLiteral("成香"))) return QStringLiteral("NY");
    if (kanji.contains(QStringLiteral("成桂"))) return QStringLiteral("NK");
    if (kanji.contains(QStringLiteral("成銀"))) return QStringLiteral("NG");
    if (kanji.contains(QStringLiteral("馬"))) return QStringLiteral("UM");
    if (kanji.contains(QStringLiteral("龍")) || kanji.contains(QStringLiteral("竜"))) return QStringLiteral("RY");
    return QString();
}

// 全角数字を半角数字に変換
static int zenkakuToNumber(QChar c)
{
    static const QString zenkaku = QStringLiteral("０１２３４５６７８９");
    qsizetype idx = zenkaku.indexOf(c);
    if (idx >= 0) return static_cast<int>(idx);
    if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

// 漢数字を半角数字に変換
static int kanjiToNumber(QChar c)
{
    static const QString kanji = QStringLiteral("〇一二三四五六七八九");
    qsizetype idx = kanji.indexOf(c);
    if (idx >= 0) return static_cast<int>(idx);
    if (c >= QLatin1Char('1') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

// 終局語から JKF special 文字列に変換
static QString japaneseToJkfSpecial(const QString& japanese)
{
    if (japanese.contains(QStringLiteral("投了"))) return QStringLiteral("TORYO");
    if (japanese.contains(QStringLiteral("中断"))) return QStringLiteral("CHUDAN");
    if (japanese.contains(QStringLiteral("王手千日手"))) return QStringLiteral("OUTE_SENNICHITE");
    if (japanese.contains(QStringLiteral("千日手"))) return QStringLiteral("SENNICHITE");
    if (japanese.contains(QStringLiteral("持将棋"))) return QStringLiteral("JISHOGI");
    if (japanese.contains(QStringLiteral("切れ負け"))) return QStringLiteral("TIME_UP");
    if (japanese.contains(QStringLiteral("反則負け")) || japanese.contains(QStringLiteral("反則勝ち"))) return QStringLiteral("ILLEGAL_ACTION");
    if (japanese.contains(QStringLiteral("入玉勝ち"))) return QStringLiteral("KACHI");
    if (japanese.contains(QStringLiteral("引き分け"))) return QStringLiteral("HIKIWAKE");
    if (japanese.contains(QStringLiteral("詰み"))) return QStringLiteral("TSUMI");
    if (japanese.contains(QStringLiteral("不詰"))) return QStringLiteral("FUZUMI");
    return QString();
}

// time文字列（mm:ss/HH:MM:SS）をJKFの time オブジェクトに変換
static QJsonObject parseTimeToJkf(const QString& timeText)
{
    QJsonObject result;

    if (timeText.isEmpty()) return result;

    // mm:ss/HH:MM:SS 形式をパース
    QString text = timeText;
    // 括弧を除去
    text.remove(QLatin1Char('('));
    text.remove(QLatin1Char(')'));
    text = text.trimmed();

    const QStringList parts = text.split(QLatin1Char('/'));
    if (parts.size() >= 1) {
        // 1手の消費時間
        const QStringList nowParts = parts[0].split(QLatin1Char(':'));
        if (nowParts.size() >= 2) {
            QJsonObject now;
            now[QStringLiteral("m")] = nowParts[0].trimmed().toInt();
            now[QStringLiteral("s")] = nowParts[1].trimmed().toInt();
            result[QStringLiteral("now")] = now;
        }
    }
    if (parts.size() >= 2) {
        // 累計消費時間
        const QStringList totalParts = parts[1].split(QLatin1Char(':'));
        if (totalParts.size() >= 3) {
            QJsonObject total;
            total[QStringLiteral("h")] = totalParts[0].trimmed().toInt();
            total[QStringLiteral("m")] = totalParts[1].trimmed().toInt();
            total[QStringLiteral("s")] = totalParts[2].trimmed().toInt();
            result[QStringLiteral("total")] = total;
        }
    }

    return result;
}

// preset名からSFEN文字列を判定
static QString sfenToJkfPreset(const QString& sfen)
{
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString hiratePos = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    if (sfen.isEmpty() || sfen == defaultSfen || sfen.startsWith(hiratePos)) {
        return QStringLiteral("HIRATE");
    }

    // 各種駒落ちの判定（後手番から開始）
    struct PresetDef { const char* preset; const char* pos; };
    static const PresetDef presets[] = {
        {"KY",    "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},   // 香落ち
        {"KY_R",  "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},   // 右香落ち
        {"KA",    "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 角落ち
        {"HI",    "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 飛車落ち
        {"HIKY",  "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 飛香落ち
        {"2",     "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 二枚落ち
        {"3",     "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 三枚落ち
        {"4",     "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 四枚落ち
        {"5",     "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},        // 五枚落ち
        {"5_L",   "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},        // 左五枚落ち
        {"6",     "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},         // 六枚落ち
        {"7_L",   "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},          // 左七枚落ち
        {"7_R",   "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},          // 右七枚落ち
        {"8",     "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},           // 八枚落ち
        {"10",    "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},             // 十枚落ち
    };

    const QString boardPart = sfen.section(QLatin1Char(' '), 0, 0);
    for (const auto& p : presets) {
        if (boardPart == QString::fromUtf8(p.pos)) {
            return QString::fromUtf8(p.preset);
        }
    }

    return QString(); // カスタム局面
}

// SFEN駒文字からCSA駒種に変換
static QString sfenPieceToJkfKind(QChar c, bool promoted)
{
    QChar upper = c.toUpper();
    QString base;
    if (upper == QLatin1Char('P')) base = QStringLiteral("FU");
    else if (upper == QLatin1Char('L')) base = QStringLiteral("KY");
    else if (upper == QLatin1Char('N')) base = QStringLiteral("KE");
    else if (upper == QLatin1Char('S')) base = QStringLiteral("GI");
    else if (upper == QLatin1Char('G')) base = QStringLiteral("KI");
    else if (upper == QLatin1Char('B')) base = QStringLiteral("KA");
    else if (upper == QLatin1Char('R')) base = QStringLiteral("HI");
    else if (upper == QLatin1Char('K')) base = QStringLiteral("OU");
    else return QString();

    if (promoted) {
        if (base == QStringLiteral("FU")) return QStringLiteral("TO");
        if (base == QStringLiteral("KY")) return QStringLiteral("NY");
        if (base == QStringLiteral("KE")) return QStringLiteral("NK");
        if (base == QStringLiteral("GI")) return QStringLiteral("NG");
        if (base == QStringLiteral("KA")) return QStringLiteral("UM");
        if (base == QStringLiteral("HI")) return QStringLiteral("RY");
    }
    return base;
}

// SFENからJKFのdataオブジェクトを構築
static QJsonObject sfenToJkfData(const QString& sfen)
{
    QJsonObject data;

    if (sfen.isEmpty()) return data;

    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() < 3) return data;

    const QString boardStr = parts[0];
    const QString turnStr = parts[1];
    const QString handsStr = parts[2];

    // 手番 (0=先手, 1=後手)
    data[QStringLiteral("color")] = (turnStr == QStringLiteral("b")) ? 0 : 1;

    // 盤面を解析
    // SFEN: 9筋から1筋へ、1段から9段へ
    // JKF board: board[x-1][y-1] が (x,y) のマス
    // board[0] は1筋, board[0][0] は (1,1)
    QJsonArray board;
    for (int x = 0; x < 9; ++x) {
        QJsonArray col;
        for (int y = 0; y < 9; ++y) {
            col.append(QJsonObject()); // 空マス
        }
        board.append(col);
    }

    const QStringList rows = boardStr.split(QLatin1Char('/'));
    for (qsizetype y = 0; y < qMin(qsizetype(9), rows.size()); ++y) {
        const QString& rowStr = rows[y];
        int x = 8; // SFEN は9筋から開始
        bool isPromoted = false;

        for (qsizetype i = 0; i < rowStr.size() && x >= 0; ++i) {
            QChar c = rowStr.at(i);

            if (c == QLatin1Char('+')) {
                isPromoted = true;
                continue;
            }

            if (c.isDigit()) {
                x -= c.digitValue();
                isPromoted = false;
            } else {
                // 駒
                QJsonObject piece;
                piece[QStringLiteral("color")] = c.isUpper() ? 0 : 1;
                piece[QStringLiteral("kind")] = sfenPieceToJkfKind(c, isPromoted);

                // board[x] の y 位置に設定
                QJsonArray col = board[x].toArray();
                col[y] = piece;
                board[x] = col;

                --x;
                isPromoted = false;
            }
        }
    }
    data[QStringLiteral("board")] = board;

    // 持駒を解析
    // SFEN hands: 先手の駒（大文字）と後手の駒（小文字）が混在
    // JKF hands: hands[0]=先手, hands[1]=後手
    QJsonObject blackHands;
    QJsonObject whiteHands;

    if (handsStr != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handsStr.size(); ++i) {
            QChar c = handsStr.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                QString kind = sfenPieceToJkfKind(c, false);
                if (!kind.isEmpty()) {
                    if (c.isUpper()) {
                        blackHands[kind] = blackHands[kind].toInt() + count;
                    } else {
                        whiteHands[kind] = whiteHands[kind].toInt() + count;
                    }
                }
                count = 0;
            }
        }
    }

    QJsonArray hands;
    hands.append(blackHands);
    hands.append(whiteHands);
    data[QStringLiteral("hands")] = hands;

    return data;
}

// ========================================
// JKF構築用の内部関数
// ========================================

// JKF形式のヘッダ部分を構築
static QJsonObject buildJkfHeader(const GameRecordModel::ExportContext& ctx)
{
    QJsonObject header;

    const QList<KifGameInfoItem> items = GameRecordModel::collectGameInfo(ctx);
    for (const auto& item : items) {
        const QString key = item.key.trimmed();
        const QString val = item.value.trimmed();
        if (!key.isEmpty()) {
            header[key] = val;
        }
    }

    return header;
}

// JKF形式の初期局面部分を構築
static QJsonObject buildJkfInitial(const GameRecordModel::ExportContext& ctx)
{
    QJsonObject initial;

    const QString preset = sfenToJkfPreset(ctx.startSfen);
    if (!preset.isEmpty()) {
        initial[QStringLiteral("preset")] = preset;
    } else {
        // カスタム初期局面の場合
        initial[QStringLiteral("preset")] = QStringLiteral("OTHER");

        // SFEN から data オブジェクトを構築
        const QJsonObject data = sfenToJkfData(ctx.startSfen);
        if (!data.isEmpty()) {
            initial[QStringLiteral("data")] = data;
        }
    }

    return initial;
}

// 指し手をJKF形式の move オブジェクトに変換
static QJsonObject convertMoveToJkf(const KifDisplayItem& disp, int& prevToX, int& prevToY, int /*ply*/)
{
    QJsonObject result;

    QString moveText = disp.prettyMove.trimmed();
    if (moveText.isEmpty()) return result;

    // 手番記号を除去して判定
    const bool isSente = moveText.startsWith(QStringLiteral("▲"));
    if (moveText.startsWith(QStringLiteral("▲")) || moveText.startsWith(QStringLiteral("△"))) {
        moveText = moveText.mid(1);
    }

    // 終局語の判定
    const QString special = japaneseToJkfSpecial(moveText);
    if (!special.isEmpty()) {
        result[QStringLiteral("special")] = special;
        return result;
    }

    // move オブジェクトを構築
    QJsonObject move;
    move[QStringLiteral("color")] = isSente ? 0 : 1;

    // 移動先の解析
    int toX = 0, toY = 0;
    bool isSame = false;
    int parsePos = 0;

    if (moveText.startsWith(QStringLiteral("同"))) {
        isSame = true;
        toX = prevToX;
        toY = prevToY;
        parsePos = 1;
        // 「同　」のような全角スペースをスキップ
        while (parsePos < moveText.size() && (moveText.at(parsePos).isSpace() || moveText.at(parsePos) == QChar(0x3000))) {
            ++parsePos;
        }
    } else {
        // 全角数字＋漢数字
        if (moveText.size() >= 2) {
            toX = zenkakuToNumber(moveText.at(0));
            toY = kanjiToNumber(moveText.at(1));
            parsePos = 2;
        }
    }

    if (toX > 0 && toY > 0) {
        QJsonObject to;
        to[QStringLiteral("x")] = toX;
        to[QStringLiteral("y")] = toY;
        move[QStringLiteral("to")] = to;

        if (isSame) {
            move[QStringLiteral("same")] = true;
        }

        prevToX = toX;
        prevToY = toY;
    }

    // 駒種の解析
    QString remaining = moveText.mid(parsePos);
    QString pieceStr;

    // 駒名を抽出（成香、成桂、成銀は2文字）
    if (remaining.startsWith(QStringLiteral("成香")) ||
        remaining.startsWith(QStringLiteral("成桂")) ||
        remaining.startsWith(QStringLiteral("成銀"))) {
        pieceStr = remaining.left(2);
        remaining = remaining.mid(2);
    } else if (!remaining.isEmpty()) {
        pieceStr = remaining.left(1);
        remaining = remaining.mid(1);
    }

    const QString csaPiece = kanjiToCsaPiece(pieceStr);
    if (!csaPiece.isEmpty()) {
        move[QStringLiteral("piece")] = csaPiece;
    }

    // 修飾語と移動元の解析
    QString relative;
    bool isDrop = false;
    bool isPromote = false;
    bool isNonPromote = false;
    int fromX = 0, fromY = 0;

    // 修飾語（左右上引寄直打）
    while (!remaining.isEmpty()) {
        QChar c = remaining.at(0);
        if (c == QChar(0x5DE6)) { // 左
            relative += QLatin1Char('L');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x53F3)) { // 右
            relative += QLatin1Char('R');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x4E0A)) { // 上
            relative += QLatin1Char('U');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5F15)) { // 引
            relative += QLatin1Char('D');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5BC4)) { // 寄
            relative += QLatin1Char('M');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x76F4)) { // 直
            relative += QLatin1Char('C');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x6253)) { // 打
            isDrop = true;
            relative += QLatin1Char('H');
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("成"))) {
            isPromote = true;
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("不成"))) {
            isNonPromote = true;
            remaining = remaining.mid(2);
        } else if (c == QLatin1Char('(')) {
            // 移動元 (xy) を解析
            const qsizetype closePos = remaining.indexOf(QLatin1Char(')'));
            if (closePos > 1) {
                const QString fromStr = remaining.mid(1, closePos - 1);
                if (fromStr.size() >= 2) {
                    fromX = fromStr.at(0).digitValue();
                    if (fromX < 0) fromX = zenkakuToNumber(fromStr.at(0));
                    fromY = fromStr.at(1).digitValue();
                    if (fromY < 0) fromY = zenkakuToNumber(fromStr.at(1));
                }
            }
            break;
        } else {
            break;
        }
    }

    if (!relative.isEmpty()) {
        move[QStringLiteral("relative")] = relative;
    }

    if (fromX > 0 && fromY > 0 && !isDrop) {
        QJsonObject from;
        from[QStringLiteral("x")] = fromX;
        from[QStringLiteral("y")] = fromY;
        move[QStringLiteral("from")] = from;
    }

    if (isPromote) {
        move[QStringLiteral("promote")] = true;
    } else if (isNonPromote) {
        move[QStringLiteral("promote")] = false;
    }

    result[QStringLiteral("move")] = move;

    // 時間情報
    const QJsonObject timeObj = parseTimeToJkf(disp.timeText);
    if (!timeObj.isEmpty()) {
        result[QStringLiteral("time")] = timeObj;
    }

    // コメント
    if (!disp.comment.isEmpty()) {
        QJsonArray comments;
        const QStringList lines = disp.comment.split(newlineRe());
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            // KIF形式の*プレフィックスを除去
            if (trimmed.startsWith(QLatin1Char('*'))) {
                comments.append(trimmed.mid(1));
            } else if (!trimmed.isEmpty()) {
                comments.append(trimmed);
            }
        }
        if (!comments.isEmpty()) {
            result[QStringLiteral("comments")] = comments;
        }
    }

    return result;
}

// JKF形式の moves 配列を構築（本譜）
static QJsonArray buildJkfMoves(const QList<KifDisplayItem>& disp)
{
    QJsonArray moves;

    int prevToX = 0, prevToY = 0;

    for (qsizetype i = 0; i < disp.size(); ++i) {
        const auto& item = disp[i];

        if (i == 0 && (item.prettyMove.trimmed().isEmpty()
                       || item.prettyMove.contains(QStringLiteral("開始局面")))) {
            // 開始局面のコメント
            QJsonObject openingMove;
            if (!item.comment.isEmpty()) {
                QJsonArray comments;
                const QStringList lines = item.comment.split(newlineRe());
                for (const QString& line : lines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QLatin1Char('*'))) {
                        comments.append(trimmed.mid(1));
                    } else if (!trimmed.isEmpty()) {
                        comments.append(trimmed);
                    }
                }
                if (!comments.isEmpty()) {
                    openingMove[QStringLiteral("comments")] = comments;
                }
            }
            moves.append(openingMove);
        } else {
            const QJsonObject moveObj = convertMoveToJkf(item, prevToX, prevToY, static_cast<int>(i));
            if (!moveObj.isEmpty()) {
                moves.append(moveObj);
            }
        }
    }

    return moves;
}

// KifuBranchNode から JKF形式の分岐指し手配列を構築
static QJsonArray buildJkfForkMovesFromNode(KifuBranchNode* node, QSet<int>& visitedNodes)
{
    QJsonArray forkMoves;

    if (node == nullptr) {
        return forkMoves;
    }

    // 無限ループ防止
    if (visitedNodes.contains(node->nodeId())) {
        return forkMoves;
    }
    visitedNodes.insert(node->nodeId());

    int forkPrevToX = 0, forkPrevToY = 0;

    // このノードから終端まで辿る
    KifuBranchNode* current = node;
    while (current != nullptr) {
        if (visitedNodes.contains(current->nodeId()) && current != node) {
            break;
        }
        if (current != node) {
            visitedNodes.insert(current->nodeId());
        }

        const QString moveText = current->displayText().trimmed();
        if (moveText.isEmpty()) {
            // 子がある場合は続行
            if (current->childCount() > 0) {
                current = current->childAt(0);
                continue;
            }
            break;
        }

        // KifDisplayItem を構築して convertMoveToJkf に渡す
        KifDisplayItem item;
        item.prettyMove = current->displayText();
        item.comment = current->comment();
        item.timeText = current->timeText();

        QJsonObject forkMoveObj = convertMoveToJkf(item, forkPrevToX, forkPrevToY, current->ply());
        if (forkMoveObj.isEmpty()) {
            if (current->childCount() > 0) {
                current = current->childAt(0);
                continue;
            }
            break;
        }

        // このノードに分岐があれば、子分岐を追加
        if (current->hasBranch()) {
            const QVector<KifuBranchNode*>& children = current->children();
            if (children.size() > 1) {
                QJsonArray childForks;
                for (int i = 1; i < children.size(); ++i) {
                    QJsonArray childForkMoves = buildJkfForkMovesFromNode(children.at(i), visitedNodes);
                    if (!childForkMoves.isEmpty()) {
                        childForks.append(childForkMoves);
                    }
                }
                if (!childForks.isEmpty()) {
                    forkMoveObj[QStringLiteral("forks")] = childForks;
                }
            }
        }

        forkMoves.append(forkMoveObj);

        // 次のノードへ（本譜方向＝最初の子）
        if (current->childCount() > 0) {
            current = current->childAt(0);
        } else {
            break;
        }
    }

    return forkMoves;
}

// KifuBranchTree から JKF形式の分岐を追加
static void addJkfForksFromTree(QJsonArray& movesArray, const KifuBranchTree* tree)
{
    if (tree == nullptr || tree->isEmpty()) {
        return;
    }

    // 本譜ライン上のノードを取得
    QVector<KifuBranchNode*> mainLineNodes = tree->mainLine();

    // 各ノードについて、分岐があればforks配列を追加
    for (KifuBranchNode* node : std::as_const(mainLineNodes)) {
        if (!node->hasBranch()) {
            continue;
        }

        // このノードの子（分岐）を収集
        const QVector<KifuBranchNode*>& children = node->children();

        // 最初の子は本譜なのでスキップ、2番目以降が分岐
        if (children.size() <= 1) {
            continue;
        }

        // movesArray内の対応する位置を探す
        // ノードのplyがmovesArray内のインデックスに対応
        int ply = node->ply();
        if (ply >= movesArray.size()) {
            continue;
        }

        QJsonObject moveObj = movesArray[ply].toObject();

        QJsonArray forks;
        if (moveObj.contains(QStringLiteral("forks"))) {
            forks = moveObj[QStringLiteral("forks")].toArray();
        }

        // 2番目以降の子（分岐）を追加
        for (int i = 1; i < children.size(); ++i) {
            KifuBranchNode* branchChild = children.at(i);
            QSet<int> visitedNodes;
            visitedNodes.insert(node->nodeId());  // 親は訪問済み

            QJsonArray forkMovesArr = buildJkfForkMovesFromNode(branchChild, visitedNodes);
            if (!forkMovesArr.isEmpty()) {
                forks.append(forkMovesArr);
            }
        }

        if (!forks.isEmpty()) {
            moveObj[QStringLiteral("forks")] = forks;
            movesArray[ply] = moveObj;
        }
    }
}

// ========================================
// JkfExporter 本体
// ========================================

QStringList JkfExporter::exportLines(const GameRecordModel& model,
                                     const GameRecordModel::ExportContext& ctx)
{
    QStringList out;

    // JKF のルートオブジェクトを構築
    QJsonObject root;

    // 1) header
    root[QStringLiteral("header")] = buildJkfHeader(ctx);

    // 2) initial
    root[QStringLiteral("initial")] = buildJkfInitial(ctx);

    // 3) moves（本譜）
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();
    QJsonArray movesArray = buildJkfMoves(disp);

    // 4) 分岐を追加（KifuBranchTree から）
    const KifuBranchTree* tree = model.branchTree();
    if (tree != nullptr && !tree->isEmpty()) {
        addJkfForksFromTree(movesArray, tree);
    }

    root[QStringLiteral("moves")] = movesArray;

    // JSON を文字列に変換
    const QJsonDocument doc(root);
    out << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qCDebug(lcKifu).noquote() << "toJkfLines: generated JKF with"
                              << movesArray.size() << "moves";

    return out;
}
