/// @file jkftosfenconverter.cpp
/// @brief JKF形式棋譜コンバータクラスの実装

#include "jkftosfenconverter.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>

// ========== 内部ヘルパ (anonymous namespace) ==========
namespace {

// JKF preset から SFEN への変換マップ
static QString presetToSfenImpl(const QString& preset)
{
    static const char* kHirate = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

    struct Pair { const char* key; const char* sfen; };
    static const Pair tbl[] = {
        {"HIRATE",  kHirate},
        {"KY",      "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},   // 香落ち
        {"KY_R",    "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},   // 右香落ち
        {"KA",      "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},     // 角落ち
        {"HI",      "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},     // 飛車落ち
        {"HIKY",    "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},     // 飛香落ち
        {"2",       "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},       // 二枚落ち
        {"3",       "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},       // 三枚落ち
        {"4",       "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},       // 四枚落ち
        {"5",       "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},        // 五枚落ち
        {"5_L",     "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},        // 左五枚落ち
        {"6",       "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},         // 六枚落ち
        {"7_L",     "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},          // 左七枚落ち
        {"7_R",     "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},          // 右七枚落ち
        {"8",       "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},           // 八枚落ち
        {"10",      "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},             // 十枚落ち
    };

    for (const auto& p : tbl) {
        if (preset == QString::fromUtf8(p.key)) {
            return QString::fromUtf8(p.sfen);
        }
    }
    // デフォルトは平手
    return QString::fromUtf8(kHirate);
}

// CSA形式駒種から内部コード
static int pieceKindFromCsaImpl(const QString& kind)
{
    if (kind == QStringLiteral("FU")) return 1;  // 歩
    if (kind == QStringLiteral("KY")) return 2;  // 香
    if (kind == QStringLiteral("KE")) return 3;  // 桂
    if (kind == QStringLiteral("GI")) return 4;  // 銀
    if (kind == QStringLiteral("KI")) return 5;  // 金
    if (kind == QStringLiteral("KA")) return 6;  // 角
    if (kind == QStringLiteral("HI")) return 7;  // 飛
    if (kind == QStringLiteral("OU")) return 8;  // 玉
    if (kind == QStringLiteral("TO")) return 9;  // と金
    if (kind == QStringLiteral("NY")) return 10; // 成香
    if (kind == QStringLiteral("NK")) return 11; // 成桂
    if (kind == QStringLiteral("NG")) return 12; // 成銀
    if (kind == QStringLiteral("UM")) return 13; // 馬
    if (kind == QStringLiteral("RY")) return 14; // 龍
    return 0;
}

// 駒種コードからSFEN文字
static QChar pieceToSfenChar(int kind, bool isBlack)
{
    QChar c;
    bool promoted = false;
    switch (kind) {
    case 1:  c = QLatin1Char('P'); break;  // FU
    case 2:  c = QLatin1Char('L'); break;  // KY
    case 3:  c = QLatin1Char('N'); break;  // KE
    case 4:  c = QLatin1Char('S'); break;  // GI
    case 5:  c = QLatin1Char('G'); break;  // KI
    case 6:  c = QLatin1Char('B'); break;  // KA
    case 7:  c = QLatin1Char('R'); break;  // HI
    case 8:  c = QLatin1Char('K'); break;  // OU
    case 9:  c = QLatin1Char('P'); promoted = true; break;  // TO
    case 10: c = QLatin1Char('L'); promoted = true; break;  // NY
    case 11: c = QLatin1Char('N'); promoted = true; break;  // NK
    case 12: c = QLatin1Char('S'); promoted = true; break;  // NG
    case 13: c = QLatin1Char('B'); promoted = true; break;  // UM
    case 14: c = QLatin1Char('R'); promoted = true; break;  // RY
    default: return QChar();
    }

    // 後手は小文字
    if (!isBlack) c = c.toLower();

    // 成駒は '+' を付けるが、ここでは1文字だけ返すので呼び出し側で処理
    (void)promoted;
    return c;
}

// 駒種の日本語名
static QString pieceKindToKanjiImpl(const QString& kind)
{
    if (kind == QStringLiteral("FU")) return QStringLiteral("歩");
    if (kind == QStringLiteral("KY")) return QStringLiteral("香");
    if (kind == QStringLiteral("KE")) return QStringLiteral("桂");
    if (kind == QStringLiteral("GI")) return QStringLiteral("銀");
    if (kind == QStringLiteral("KI")) return QStringLiteral("金");
    if (kind == QStringLiteral("KA")) return QStringLiteral("角");
    if (kind == QStringLiteral("HI")) return QStringLiteral("飛");
    if (kind == QStringLiteral("OU")) return QStringLiteral("玉");
    if (kind == QStringLiteral("TO")) return QStringLiteral("と");
    if (kind == QStringLiteral("NY")) return QStringLiteral("成香");
    if (kind == QStringLiteral("NK")) return QStringLiteral("成桂");
    if (kind == QStringLiteral("NG")) return QStringLiteral("成銀");
    if (kind == QStringLiteral("UM")) return QStringLiteral("馬");
    if (kind == QStringLiteral("RY")) return QStringLiteral("龍");
    return kind;
}

// 全角数字
static QString zenkakuDigit(int d)
{
    static const auto& digits = *[]() {
        static const QString s = QStringLiteral("０１２３４５６７８９");
        return &s;
    }();
    if (d >= 0 && d <= 9) return QString(digits.at(d));
    return QString::number(d);
}

// 漢数字
static QString kanjiRank(int y)
{
    static const auto& ranks = *[]() {
        static const QString s = QStringLiteral("〇一二三四五六七八九");
        return &s;
    }();
    if (y >= 1 && y <= 9) return QString(ranks.at(y));
    return QString::number(y);
}

// 秒をHH:MM:SS形式に変換
static QString formatHMS(qint64 totalSec)
{
    int h = static_cast<int>(totalSec / 3600);
    int m = static_cast<int>((totalSec % 3600) / 60);
    int s = static_cast<int>(totalSec % 60);
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

// 秒をmm:ss形式に変換
static QString formatMS(qint64 sec)
{
    int m = static_cast<int>(sec / 60);
    int s = static_cast<int>(sec % 60);
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

// --- JKFのcommentsフィールドからコメント文字列を抽出 ---
// QJsonObjectのcommentsフィールドを改行区切りの文字列に変換
static QString extractCommentsFromMoveObj(const QJsonObject& moveObj)
{
    if (!moveObj.contains(QStringLiteral("comments"))) {
        return QString();
    }
    const QJsonArray comments = moveObj[QStringLiteral("comments")].toArray();
    QStringList cmtList;
    for (const QJsonValueConstRef c : comments) {
        cmtList.append(c.toString());
    }
    return cmtList.join(QLatin1Char('\n'));
}

// 終局語の変換
static QString specialToJapaneseImpl(const QString& special)
{
    if (special == QStringLiteral("TORYO"))           return QStringLiteral("投了");
    if (special == QStringLiteral("CHUDAN"))          return QStringLiteral("中断");
    if (special == QStringLiteral("SENNICHITE"))      return QStringLiteral("千日手");
    if (special == QStringLiteral("OUTE_SENNICHITE")) return QStringLiteral("王手千日手");
    if (special == QStringLiteral("JISHOGI"))         return QStringLiteral("持将棋");
    if (special == QStringLiteral("TIME_UP"))         return QStringLiteral("切れ負け");
    if (special == QStringLiteral("ILLEGAL_MOVE"))    return QStringLiteral("反則負け");
    if (special == QStringLiteral("ILLEGAL_ACTION"))  return QStringLiteral("反則負け");
    if (special == QStringLiteral("+ILLEGAL_ACTION")) return QStringLiteral("反則負け");
    if (special == QStringLiteral("-ILLEGAL_ACTION")) return QStringLiteral("反則負け");
    if (special == QStringLiteral("KACHI"))           return QStringLiteral("入玉勝ち");
    if (special == QStringLiteral("HIKIWAKE"))        return QStringLiteral("引き分け");
    if (special == QStringLiteral("MAX_MOVES"))       return QStringLiteral("最大手数");
    if (special == QStringLiteral("TSUMI"))           return QStringLiteral("詰み");
    if (special == QStringLiteral("FUZUMI"))          return QStringLiteral("不詰");
    if (special == QStringLiteral("ERROR"))           return QStringLiteral("エラー");
    return special;
}

// relative 文字列から修飾語を取得
static QString relativeToModifierImpl(const QString& relative)
{
    QString mod;
    if (relative.contains(QLatin1Char('L'))) mod += QStringLiteral("左");
    if (relative.contains(QLatin1Char('C'))) mod += QStringLiteral("直");
    if (relative.contains(QLatin1Char('R'))) mod += QStringLiteral("右");
    if (relative.contains(QLatin1Char('U'))) mod += QStringLiteral("上");
    if (relative.contains(QLatin1Char('M'))) mod += QStringLiteral("寄");
    if (relative.contains(QLatin1Char('D'))) mod += QStringLiteral("引");
    if (relative.contains(QLatin1Char('H'))) mod += QStringLiteral("打");
    return mod;
}

} // anonymous namespace

// ========== public API ==========

QString JkfToSfenConverter::mapPresetToSfen(const QString& preset)
{
    return presetToSfenImpl(preset);
}

QString JkfToSfenConverter::detectInitialSfenFromFile(const QString& jkfPath, QString* detectedLabel)
{
    QJsonObject root;
    QString warn;
    if (!loadJsonFile(jkfPath, root, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return presetToSfenImpl(QStringLiteral("HIRATE"));
    }
    return buildInitialSfen(root, detectedLabel);
}

QStringList JkfToSfenConverter::convertFile(const QString& jkfPath, QString* errorMessage)
{
    QStringList out;

    QJsonObject root;
    if (!loadJsonFile(jkfPath, root, errorMessage)) {
        return out;
    }

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return out;
    }

    const QJsonArray moves = root[QStringLiteral("moves")].toArray();
    int prevToX = 0, prevToY = 0;

    for (qsizetype i = 0; i < moves.size(); ++i) {
        const QJsonObject moveObj = moves[i].toObject();

        // 終局判定
        if (moveObj.contains(QStringLiteral("special"))) {
            break;
        }

        // move フィールドがあれば処理
        if (moveObj.contains(QStringLiteral("move"))) {
            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();
            const QString usi = convertMoveToUsi(mv, prevToX, prevToY);
            if (!usi.isEmpty()) {
                out.append(usi);
            }
        }
    }

    return out;
}

QList<KifDisplayItem> JkfToSfenConverter::extractMovesWithTimes(const QString& jkfPath, QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QJsonObject root;
    if (!loadJsonFile(jkfPath, root, errorMessage)) {
        return out;
    }

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return out;
    }

    const QJsonArray moves = root[QStringLiteral("moves")].toArray();
    int prevToX = 0, prevToY = 0;
    int plyNumber = 0;
    qint64 cumSec[2] = {0, 0}; // [0]=先手, [1]=後手
    QString pendingComment;  // 次の指し手に付与するコメント

    // 開始局面エントリを追加
    KifDisplayItem openingItem;
    openingItem.prettyMove = QString();
    openingItem.timeText = QStringLiteral("00:00/00:00:00");
    openingItem.ply = 0;

    for (qsizetype i = 0; i < moves.size(); ++i) {
        const QJsonObject moveObj = moves[i].toObject();

        // コメントを取得
        const QString comment = extractCommentsFromMoveObj(moveObj);

        // 終局語
        if (moveObj.contains(QStringLiteral("special"))) {
            // 開始局面エントリがまだ追加されていない場合は追加
            if (out.isEmpty()) {
                openingItem.comment = pendingComment;
                out.append(openingItem);
            } else if (!pendingComment.isEmpty() && out.size() > 1) {
                // 直前の指し手にペンディングコメントを付与
                QString& dst = out.last().comment;
                if (!dst.isEmpty()) dst += QLatin1Char('\n');
                dst += pendingComment;
            }
            pendingComment.clear();

            const QString special = moveObj[QStringLiteral("special")].toString();
            const QString label = specialToJapaneseImpl(special);
            const QString teban = ((plyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

            KifDisplayItem item;
            item.prettyMove = teban + label;
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = comment;
            item.ply = plyNumber + 1;
            out.append(item);
            break;
        }

        // move フィールドがあれば処理
        if (moveObj.contains(QStringLiteral("move"))) {
            // 開始局面エントリがまだ追加されていない場合は追加
            if (out.isEmpty()) {
                openingItem.comment = pendingComment;
                out.append(openingItem);
                pendingComment.clear();
            } else if (!pendingComment.isEmpty()) {
                // 直前の指し手にペンディングコメントを付与
                QString& dst = out.last().comment;
                if (!dst.isEmpty()) dst += QLatin1Char('\n');
                dst += pendingComment;
                pendingComment.clear();
            }

            ++plyNumber;

            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();
            const QString pretty = convertMoveToPretty(mv, plyNumber, prevToX, prevToY);

            // 時間
            QString timeText = QStringLiteral("00:00/00:00:00");
            if (moveObj.contains(QStringLiteral("time"))) {
                const QJsonObject timeObj = moveObj[QStringLiteral("time")].toObject();
                const int color = mv[QStringLiteral("color")].toInt();
                timeText = formatTimeText(timeObj, cumSec[color]);
            }

            KifDisplayItem item;
            item.prettyMove = pretty;
            item.timeText = timeText;
            item.comment = comment;  // JKFでは各指し手の直後にコメントがあるので、そのまま付与
            item.ply = plyNumber;
            out.append(item);
        } else {
            // move がない（開始局面のコメントなど）場合
            // JKF仕様では moves[0] は初期局面のコメント用
            if (!comment.isEmpty()) {
                if (out.isEmpty()) {
                    // 開始局面のコメントとして保持
                    openingItem.comment = comment;
                } else {
                    // 次の指し手に付与するためペンディング
                    if (!pendingComment.isEmpty()) pendingComment += QLatin1Char('\n');
                    pendingComment += comment;
                }
            }
        }
    }

    // 開始局面エントリがまだ追加されていない場合は追加
    if (out.isEmpty()) {
        openingItem.comment = pendingComment;
        out.append(openingItem);
    }

    return out;
}

bool JkfToSfenConverter::parseWithVariations(const QString& jkfPath,
                                              KifParseResult& out,
                                              QString* errorMessage)
{
    out = KifParseResult{};

    QJsonObject root;
    if (!loadJsonFile(jkfPath, root, errorMessage)) {
        return false;
    }

    // 初期局面を設定
    QString detectedLabel;
    out.mainline.baseSfen = buildInitialSfen(root, &detectedLabel);
    out.mainline.startPly = 1;

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return false;
    }

    const QJsonArray movesArray = root[QStringLiteral("moves")].toArray();

    // moves配列を解析
    parseMovesArray(movesArray, out.mainline.baseSfen, out.mainline, out.variations, errorMessage);

    return true;
}

QList<KifGameInfoItem> JkfToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ordered;

    QJsonObject root;
    QString warn;
    if (!loadJsonFile(filePath, root, &warn)) {
        return ordered;
    }

    if (!root.contains(QStringLiteral("header"))) {
        return ordered;
    }

    // JKFのheaderはキー→値の連想配列
    const QJsonObject header = root[QStringLiteral("header")].toObject();
    const QStringList keys = header.keys();

    for (const QString& key : keys) {
        const QString value = header[key].toString();
        ordered.append({key, value});
    }

    return ordered;
}

QMap<QString, QString> JkfToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    QMap<QString, QString> m;
    const auto items = extractGameInfo(filePath);
    for (const auto& it : items) {
        m.insert(it.key, it.value);
    }
    return m;
}

// ========== private ヘルパ ==========

bool JkfToSfenConverter::loadJsonFile(const QString& filePath, QJsonObject& root, QString* warn)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warn) *warn = QStringLiteral("Failed to open file: %1").arg(filePath);
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (warn) *warn = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        if (warn) *warn = QStringLiteral("JKF root is not an object");
        return false;
    }

    root = doc.object();
    return true;
}

QString JkfToSfenConverter::buildInitialSfen(const QJsonObject& root, QString* detectedLabel)
{
    // initial フィールドがなければ平手
    if (!root.contains(QStringLiteral("initial"))) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return presetToSfenImpl(QStringLiteral("HIRATE"));
    }

    const QJsonObject initial = root[QStringLiteral("initial")].toObject();
    const QString preset = initial[QStringLiteral("preset")].toString();

    // preset が OTHER の場合は data から構築
    if (preset == QStringLiteral("OTHER")) {
        if (initial.contains(QStringLiteral("data"))) {
            if (detectedLabel) *detectedLabel = QStringLiteral("任意局面");
            const QJsonObject data = initial[QStringLiteral("data")].toObject();
            return buildSfenFromInitialData(data);
        }
    }

    // preset から変換
    if (detectedLabel) {
        // preset名を日本語に
        if (preset == QStringLiteral("HIRATE")) *detectedLabel = QStringLiteral("平手");
        else if (preset == QStringLiteral("KY")) *detectedLabel = QStringLiteral("香落ち");
        else if (preset == QStringLiteral("KY_R")) *detectedLabel = QStringLiteral("右香落ち");
        else if (preset == QStringLiteral("KA")) *detectedLabel = QStringLiteral("角落ち");
        else if (preset == QStringLiteral("HI")) *detectedLabel = QStringLiteral("飛車落ち");
        else if (preset == QStringLiteral("HIKY")) *detectedLabel = QStringLiteral("飛香落ち");
        else if (preset == QStringLiteral("2")) *detectedLabel = QStringLiteral("二枚落ち");
        else if (preset == QStringLiteral("3")) *detectedLabel = QStringLiteral("三枚落ち");
        else if (preset == QStringLiteral("4")) *detectedLabel = QStringLiteral("四枚落ち");
        else if (preset == QStringLiteral("5")) *detectedLabel = QStringLiteral("五枚落ち");
        else if (preset == QStringLiteral("5_L")) *detectedLabel = QStringLiteral("左五枚落ち");
        else if (preset == QStringLiteral("6")) *detectedLabel = QStringLiteral("六枚落ち");
        else if (preset == QStringLiteral("7_L")) *detectedLabel = QStringLiteral("左七枚落ち");
        else if (preset == QStringLiteral("7_R")) *detectedLabel = QStringLiteral("右七枚落ち");
        else if (preset == QStringLiteral("8")) *detectedLabel = QStringLiteral("八枚落ち");
        else if (preset == QStringLiteral("10")) *detectedLabel = QStringLiteral("十枚落ち");
        else *detectedLabel = preset;
    }

    return presetToSfenImpl(preset);
}

QString JkfToSfenConverter::buildSfenFromInitialData(const QJsonObject& data, int moveNumber)
{
    // color: 手番 (0=先手, 1=後手)
    const int color = data[QStringLiteral("color")].toInt(0);

    // board: 9x9の盤面 (board[x-1][y-1] が (x,y) のマス)
    // JKF仕様: board[0] は1筋, board[0][0] は (1,1)
    QString boardStr;
    if (data.contains(QStringLiteral("board"))) {
        const QJsonArray board = data[QStringLiteral("board")].toArray();

        for (int y = 0; y < 9; ++y) {
            int emptyCount = 0;
            QString rowStr;
            for (int x = 8; x >= 0; --x) {  // SFEN は 9筋から1筋の順
                const QJsonArray col = board[x].toArray();
                const QJsonObject cell = col[y].toObject();

                if (cell.isEmpty() || !cell.contains(QStringLiteral("kind"))) {
                    ++emptyCount;
                } else {
                    if (emptyCount > 0) {
                        rowStr += QString::number(emptyCount);
                        emptyCount = 0;
                    }
                    const QString kind = cell[QStringLiteral("kind")].toString();
                    const int cellColor = cell[QStringLiteral("color")].toInt();
                    const int pieceCode = pieceKindFromCsaImpl(kind);
                    const bool promoted = (pieceCode >= 9 && pieceCode <= 14);

                    if (promoted) rowStr += QLatin1Char('+');
                    rowStr += pieceToSfenChar(pieceCode, cellColor == 0);
                }
            }
            if (emptyCount > 0) {
                rowStr += QString::number(emptyCount);
            }
            if (!boardStr.isEmpty()) boardStr += QLatin1Char('/');
            boardStr += rowStr;
        }
    }

    // hands: 持駒
    // hands[0] = 先手の持駒, hands[1] = 後手の持駒
    QString handsStr;
    if (data.contains(QStringLiteral("hands"))) {
        const QJsonArray hands = data[QStringLiteral("hands")].toArray();
        if (hands.size() >= 2) {
            const QJsonObject blackHands = hands[0].toObject();
            const QJsonObject whiteHands = hands[1].toObject();

            auto emitHands = [](const QJsonObject& h, bool isBlack) -> QString {
                QString s;
                const char* order[] = {"HI", "KA", "KI", "GI", "KE", "KY", "FU"};
                const char* sfenChar[] = {"R", "B", "G", "S", "N", "L", "P"};
                for (int i = 0; i < 7; ++i) {
                    const QString key = QString::fromUtf8(order[i]);
                    if (h.contains(key)) {
                        int count = h[key].toInt();
                        if (count > 0) {
                            if (count > 1) s += QString::number(count);
                            QString ch = QString::fromUtf8(sfenChar[i]);
                            if (!isBlack) ch = ch.toLower();
                            s += ch;
                        }
                    }
                }
                return s;
            };

            handsStr = emitHands(blackHands, true) + emitHands(whiteHands, false);
        }
    }
    if (handsStr.isEmpty()) handsStr = QStringLiteral("-");

    // 手番
    const QString turnStr = (color == 0) ? QStringLiteral("b") : QStringLiteral("w");

    return QStringLiteral("%1 %2 %3 %4")
        .arg(boardStr, turnStr, handsStr, QString::number(moveNumber));
}

void JkfToSfenConverter::parseMovesArray(const QJsonArray& movesArray,
                                          const QString& /*baseSfen*/,
                                          KifLine& mainline,
                                          QVector<KifVariation>& variations,
                                          QString* /*warn*/)
{
    int prevToX = 0, prevToY = 0;
    int plyNumber = 0;
    qint64 cumSec[2] = {0, 0}; // [0]=先手, [1]=後手
    bool openingEntryAdded = false;

    // JKF仕様:
    // - moves[0] は初期局面用（moveフィールドなし、commentsのみ）
    // - moves[i] (i>=1) の comments は、moves[i] の指し手に対するコメント
    // つまり moves[i].comments は moves[i].move についてのコメント

    // 開始局面エントリを先に準備
    KifDisplayItem openingItem;
    openingItem.prettyMove = QString();
    openingItem.timeText = QStringLiteral("00:00/00:00:00");
    openingItem.ply = 0;

    for (qsizetype i = 0; i < movesArray.size(); ++i) {
        const QJsonObject moveObj = movesArray[i].toObject();

        // 終局語
        if (moveObj.contains(QStringLiteral("special"))) {
            // 開始局面エントリがまだ追加されていない場合は追加
            if (!openingEntryAdded) {
                mainline.disp.append(openingItem);
                openingEntryAdded = true;
            }

            const QString special = moveObj[QStringLiteral("special")].toString();
            const QString label = specialToJapaneseImpl(special);
            const QString teban = ((plyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

            // 終局語のコメントはその要素自身から取得
            const QString comment = extractCommentsFromMoveObj(moveObj);

            KifDisplayItem item;
            item.prettyMove = teban + label;
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = comment;
            item.ply = plyNumber + 1;
            mainline.disp.append(item);
            mainline.endsWithTerminal = true;
            break;
        }

        // move フィールドがあれば処理
        if (moveObj.contains(QStringLiteral("move"))) {
            // 開始局面エントリがまだ追加されていない場合は追加
            if (!openingEntryAdded) {
                mainline.disp.append(openingItem);
                openingEntryAdded = true;
            }

            ++plyNumber;

            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();

            // USI形式の指し手
            const QString usi = convertMoveToUsi(mv, prevToX, prevToY);
            if (!usi.isEmpty()) {
                mainline.usiMoves.append(usi);
            }

            // 日本語表記
            const QString pretty = convertMoveToPretty(mv, plyNumber, prevToX, prevToY);

            // 時間
            QString timeText = QStringLiteral("00:00/00:00:00");
            if (moveObj.contains(QStringLiteral("time"))) {
                const QJsonObject timeObj = moveObj[QStringLiteral("time")].toObject();
                const int color = mv[QStringLiteral("color")].toInt();
                timeText = formatTimeText(timeObj, cumSec[color]);
            }

            // コメントは同じ要素から取得 (moves[i].comments は moves[i].move のコメント)
            const QString comment = extractCommentsFromMoveObj(moveObj);

            KifDisplayItem item;
            item.prettyMove = pretty;
            item.timeText = timeText;
            item.comment = comment;
            item.ply = plyNumber;
            mainline.disp.append(item);
        } else if (i == 0) {
            // moves[0] は初期局面用 - コメントがあれば開始局面エントリに設定
            openingItem.comment = extractCommentsFromMoveObj(moveObj);
        }

        // 分岐 (forks) を処理
        if (moveObj.contains(QStringLiteral("forks"))) {
            const QJsonArray forks = moveObj[QStringLiteral("forks")].toArray();

            for (qsizetype f = 0; f < forks.size(); ++f) {
                const QJsonArray forkMoves = forks[f].toArray();

                KifVariation var;
                var.startPly = plyNumber; // この手の代替として分岐が始まる

                // 分岐の prevTo を現時点の値からコピー
                int forkPrevToX = 0, forkPrevToY = 0;
                // 分岐直前の局面での prevTo を復元するには、mainline の plyNumber-1 手目までの情報が必要
                // ここでは簡略化のため、分岐開始時点では prevTo をリセット
                // （実際の実装では分岐開始局面を正しく追跡する必要がある）

                int forkPlyNumber = plyNumber - 1;
                qint64 forkCumSec[2] = {cumSec[0], cumSec[1]}; // 分岐開始時の消費時間を継承

                for (qsizetype j = 0; j < forkMoves.size(); ++j) {
                    const QJsonObject forkMoveObj = forkMoves[j].toObject();

                    // 終局語
                    if (forkMoveObj.contains(QStringLiteral("special"))) {
                        const QString special = forkMoveObj[QStringLiteral("special")].toString();
                        const QString label = specialToJapaneseImpl(special);
                        const QString teban = ((forkPlyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

                        // 終局語のコメントはその要素自身から取得
                        const QString forkComment = extractCommentsFromMoveObj(forkMoveObj);

                        KifDisplayItem item;
                        item.prettyMove = teban + label;
                        item.timeText = QStringLiteral("00:00/00:00:00");
                        item.comment = forkComment;
                        item.ply = forkPlyNumber + 1;
                        var.line.disp.append(item);
                        var.line.endsWithTerminal = true;
                        break;
                    }

                    if (forkMoveObj.contains(QStringLiteral("move"))) {
                        ++forkPlyNumber;

                        const QJsonObject forkMv = forkMoveObj[QStringLiteral("move")].toObject();

                        const QString usi = convertMoveToUsi(forkMv, forkPrevToX, forkPrevToY);
                        if (!usi.isEmpty()) {
                            var.line.usiMoves.append(usi);
                        }

                        const QString pretty = convertMoveToPretty(forkMv, forkPlyNumber, forkPrevToX, forkPrevToY);

                        QString forkTimeText = QStringLiteral("00:00/00:00:00");
                        if (forkMoveObj.contains(QStringLiteral("time"))) {
                            const QJsonObject timeObj = forkMoveObj[QStringLiteral("time")].toObject();
                            const int color = forkMv[QStringLiteral("color")].toInt();
                            forkTimeText = formatTimeText(timeObj, forkCumSec[color]);
                        }

                        // コメントは同じ要素から取得
                        const QString forkComment = extractCommentsFromMoveObj(forkMoveObj);

                        KifDisplayItem item;
                        item.prettyMove = pretty;
                        item.timeText = forkTimeText;
                        item.comment = forkComment;
                        item.ply = forkPlyNumber;
                        var.line.disp.append(item);
                    }
                }

                variations.append(var);
            }
        }
    }

    // 開始局面エントリがまだ追加されていない場合は追加
    if (!openingEntryAdded) {
        mainline.disp.prepend(openingItem);
    }
}

QString JkfToSfenConverter::convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY)
{
    // to は必須
    if (!move.contains(QStringLiteral("to"))) {
        return QString();
    }

    const QJsonObject to = move[QStringLiteral("to")].toObject();
    const int toX = to[QStringLiteral("x")].toInt();
    const int toY = to[QStringLiteral("y")].toInt();

    QString usi;

    // from がなければ打ち
    if (!move.contains(QStringLiteral("from"))) {
        const QString piece = move[QStringLiteral("piece")].toString();
        QChar pieceChar;
        if (piece == QStringLiteral("FU")) pieceChar = QLatin1Char('P');
        else if (piece == QStringLiteral("KY")) pieceChar = QLatin1Char('L');
        else if (piece == QStringLiteral("KE")) pieceChar = QLatin1Char('N');
        else if (piece == QStringLiteral("GI")) pieceChar = QLatin1Char('S');
        else if (piece == QStringLiteral("KI")) pieceChar = QLatin1Char('G');
        else if (piece == QStringLiteral("KA")) pieceChar = QLatin1Char('B');
        else if (piece == QStringLiteral("HI")) pieceChar = QLatin1Char('R');
        else return QString();

        usi = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toX).arg(rankNumToLetter(toY));
    } else {
        // 移動
        const QJsonObject from = move[QStringLiteral("from")].toObject();
        const int fromX = from[QStringLiteral("x")].toInt();
        const int fromY = from[QStringLiteral("y")].toInt();

        usi = QStringLiteral("%1%2%3%4")
            .arg(fromX)
            .arg(rankNumToLetter(fromY))
            .arg(toX)
            .arg(rankNumToLetter(toY));

        // 成り
        if (move.contains(QStringLiteral("promote")) && move[QStringLiteral("promote")].toBool()) {
            usi += QLatin1Char('+');
        }
    }

    prevToX = toX;
    prevToY = toY;

    return usi;
}

QString JkfToSfenConverter::convertMoveToPretty(const QJsonObject& move, int plyNumber,
                                                 int& prevToX, int& prevToY)
{
    const QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

    // to は必須
    if (!move.contains(QStringLiteral("to"))) {
        return teban + QStringLiteral("???");
    }

    const QJsonObject to = move[QStringLiteral("to")].toObject();
    const int toX = to[QStringLiteral("x")].toInt();
    const int toY = to[QStringLiteral("y")].toInt();

    QString result = teban;

    // same (同〜) 判定
    const bool isSame = move.contains(QStringLiteral("same")) && move[QStringLiteral("same")].toBool();
    if (isSame) {
        result += QStringLiteral("同　");
    } else {
        result += zenkakuDigit(toX) + kanjiRank(toY);
    }

    // 駒種
    const QString piece = move[QStringLiteral("piece")].toString();
    result += pieceKindToKanjiImpl(piece);

    // relative (相対情報) - 「打」は relative に "H" として含まれる
    bool isDrop = false;
    if (move.contains(QStringLiteral("relative"))) {
        const QString relative = move[QStringLiteral("relative")].toString();
        if (relative.contains(QLatin1Char('H'))) {
            isDrop = true;
        }
        result += relativeToModifierImpl(relative);
    }

    // 打ち判定: from がなければ打ち（relative に H がない場合も対応）
    if (!move.contains(QStringLiteral("from")) && !isDrop) {
        result += QStringLiteral("打");
    }

    // 成り/不成
    if (move.contains(QStringLiteral("promote"))) {
        if (move[QStringLiteral("promote")].toBool()) {
            result += QStringLiteral("成");
        } else {
            // false の場合は不成を明示
            result += QStringLiteral("不成");
        }
    }

    // 移動元（打ちでなければ表示）
    if (move.contains(QStringLiteral("from"))) {
        const QJsonObject from = move[QStringLiteral("from")].toObject();
        const int fromX = from[QStringLiteral("x")].toInt();
        const int fromY = from[QStringLiteral("y")].toInt();
        result += QStringLiteral("(%1%2)").arg(fromX).arg(fromY);
    }

    prevToX = toX;
    prevToY = toY;

    return result;
}

QString JkfToSfenConverter::formatTimeText(const QJsonObject& timeObj, qint64& cumSec)
{
    // now: 1手の消費時間
    qint64 nowSec = 0;
    if (timeObj.contains(QStringLiteral("now"))) {
        const QJsonObject now = timeObj[QStringLiteral("now")].toObject();
        int h = now[QStringLiteral("h")].toInt(0);
        int m = now[QStringLiteral("m")].toInt(0);
        int s = now[QStringLiteral("s")].toInt(0);
        nowSec = h * 3600 + m * 60 + s;
    }

    // total: 累計消費時間（あれば使用、なければ計算）
    qint64 totalSec = 0;
    if (timeObj.contains(QStringLiteral("total"))) {
        const QJsonObject total = timeObj[QStringLiteral("total")].toObject();
        int h = total[QStringLiteral("h")].toInt(0);
        int m = total[QStringLiteral("m")].toInt(0);
        int s = total[QStringLiteral("s")].toInt(0);
        totalSec = h * 3600 + m * 60 + s;
        cumSec = totalSec;
    } else {
        cumSec += nowSec;
        totalSec = cumSec;
    }

    // mm:ss/HH:MM:SS 形式
    return formatMS(nowSec) + QLatin1Char('/') + formatHMS(totalSec);
}

QString JkfToSfenConverter::specialToJapanese(const QString& special)
{
    return specialToJapaneseImpl(special);
}

int JkfToSfenConverter::pieceKindFromCsa(const QString& kind)
{
    return pieceKindFromCsaImpl(kind);
}

QString JkfToSfenConverter::pieceKindToKanji(const QString& kind)
{
    return pieceKindToKanjiImpl(kind);
}

QChar JkfToSfenConverter::rankNumToLetter(int r)
{
    if (r < 1 || r > 9) return QChar();
    return QChar(QLatin1Char(static_cast<char>('a' + (r - 1))));
}

QString JkfToSfenConverter::relativeToModifier(const QString& relative)
{
    return relativeToModifierImpl(relative);
}
