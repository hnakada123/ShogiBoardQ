#include "usentosfenconverter.h"
#include "sfenpositiontracer.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

// ============================================================================
// USEN (Url Safe sfen-Extended Notation) デコーダ
// ============================================================================
//
// USEN形式の仕様 (Shogi Playground):
// - 先頭: ~(version). で始まる
//   - 平手初期局面 → "" (空文字列) = "~0." ではなく "~."
//   - その他 → SFENを以下のように置換: '/' → '_', ' ' → '.', '+' → 'z'
// - 指し手: 3文字のbase36でエンコード
// - 分岐: ~(startPly). で区切り
// - 終局: .(code) で終わる
//   - .i = 反則
//   - .r = 投了 (resign)
//   - .t = 時間切れ
//   - .p = 中断
//   - .j = 持将棋
//
// 指し手エンコーディング (3文字のbase36 = 0-46655の範囲):
// - code = (from_sq * 81 + to_sq) * 2 + (成る場合は1)
// - 盤上移動: from_sq = 0 (1一) ～ 80 (9九)
// - 駒打ち: from_sq = 81 + 駒種
//   - 歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
//   - と金=2, 成香=3, 成桂=4, 成銀=5, 馬=6, 竜=7, 玉=8
// - to_sq = 0 (1一) ～ 80 (9九)
// - マス番号: sq = (rank - 1) * 9 + (file - 1)
//   - 1一 = 0, 1九 = 8, 9一 = 72, 9九 = 80
// ============================================================================

namespace {

// 平手初期局面のSFEN
static const char* kHirateSfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

// base36文字セット
static const QString kBase36Chars = QStringLiteral("0123456789abcdefghijklmnopqrstuvwxyz");

// 終局理由コードのマッピング (スライド Image 5より)
struct TerminalInfo {
    const char* code;
    const char* japanese;
};

static const TerminalInfo kTerminalCodes[] = {
    {"i", "反則"},
    {"r", "投了"},
    {"t", "時間切れ"},
    {"p", "中断"},
    {"j", "持将棋"},
    {nullptr, nullptr}
};

// 全角数字と漢数字
static const QString kZenkakuDigits = QStringLiteral("０１２３４５６７８９");
static const QString kKanjiRanks = QStringLiteral("〇一二三四五六七八九");

// 筋番号から列文字へ (1-9 -> '1'-'9')
inline QChar fileToChar(int f) {
    return QChar('0' + f);
}

// 段番号から行文字へ (1-9 -> 'a'-'i')
inline QChar rankToChar(int r) {
    return QChar('a' + r - 1);
}

// 列文字から筋番号へ ('1'-'9' -> 1-9)
inline int charToFile(QChar c) {
    return c.toLatin1() - '0';
}

// 行文字から段番号へ ('a'-'i' -> 1-9)
inline int charToRank(QChar c) {
    return c.toLatin1() - 'a' + 1;
}

// マス番号 (0-80) から筋・段を取得
// USEN座標系: sq = (rank - 1) * 9 + (file - 1)
// 例: 1一 = 0, 9九 = 80
inline void squareToFileRank(int sq, int& file, int& rank) {
    rank = sq / 9 + 1;  // 1-9
    file = sq % 9 + 1;  // 1-9
}

// 筋・段からマス番号を取得
inline int fileRankToSquare(int file, int rank) {
    return (rank - 1) * 9 + (file - 1);
}

// 駒種コード (駒打ち用)
// スライド Image 4 より:
// 歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
// と金=2, 成香=3, 成桂=4, 成銀=5, 馬=6, 竜=7, 玉=8
// 駒打ちの from_sq = 81 + piece_type
struct PieceDropInfo {
    int typeCode;
    char usiChar;
};

static const PieceDropInfo kDropPieces[] = {
    {9,  'G'},  // 金
    {10, 'P'},  // 歩
    {11, 'L'},  // 香
    {12, 'N'},  // 桂
    {13, 'S'},  // 銀
    {14, 'B'},  // 角
    {15, 'R'},  // 飛
};

inline QChar dropTypeToUsiChar(int typeCode) {
    for (const auto& p : kDropPieces) {
        if (p.typeCode == typeCode) {
            return QChar(p.usiChar);
        }
    }
    return QChar('?');
}

} // namespace

// ============================================================================
// 公開メソッド
// ============================================================================

QString UsenToSfenConverter::detectInitialSfenFromFile(const QString& usenPath, QString* detectedLabel)
{
    QString content;
    QString warn;
    if (!readUsenFile(usenPath, content, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    // USENの先頭をチェック (スライド Image 2 より)
    // 平手初期局面 → "" (空文字列) → USEN では "~." で始まる
    // その他 → SFENを以下のように置換: '/' → '_', ' ' → '.', '+' → 'z'
    
    if (!content.startsWith(QStringLiteral("~"))) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    // ~. で始まる場合は平手
    int firstDot = content.indexOf(QChar('.'));
    if (firstDot < 0) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    QString positionPart = content.mid(1, firstDot - 1);  // ~ と . の間
    
    if (positionPart.isEmpty() || positionPart == QStringLiteral("0")) {
        // 平手
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return QString::fromLatin1(kHirateSfen);
    }

    // カスタム初期局面の場合: USEN形式からSFENに逆変換
    // '_' → '/', '.' → ' ', 'z' → '+'
    QString sfen = positionPart;
    sfen.replace(QChar('_'), QChar('/'));
    sfen.replace(QChar('.'), QChar(' '));
    sfen.replace(QChar('z'), QChar('+'));

    if (detectedLabel) *detectedLabel = QStringLiteral("局面指定");
    return sfen;
}

QStringList UsenToSfenConverter::convertFile(const QString& usenPath, QString* errorMessage)
{
    QString content;
    if (!readUsenFile(usenPath, content, errorMessage)) {
        return QStringList();
    }

    QString terminalCode;
    return decodeUsenMoves(content, &terminalCode);
}

QList<KifDisplayItem> UsenToSfenConverter::extractMovesWithTimes(const QString& usenPath, QString* errorMessage)
{
    QList<KifDisplayItem> out;

    QString content;
    if (!readUsenFile(usenPath, content, errorMessage)) {
        return out;
    }

    // 初期局面を取得
    QString initialSfen = detectInitialSfenFromFile(usenPath, nullptr);

    QString terminalCode;
    QStringList usiMoves = decodeUsenMoves(content, &terminalCode);

    // 開始局面エントリ
    KifDisplayItem openingItem;
    openingItem.prettyMove = QString();
    openingItem.timeText = QStringLiteral("00:00/00:00:00");
    openingItem.comment = QString();
    openingItem.ply = 0;
    out.push_back(openingItem);

    // SfenPositionTracerを使って盤面を追跡
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(initialSfen)) {
        tracer.resetToStartpos();
    }

    int prevToFile = 0, prevToRank = 0;
    int plyNumber = 0;

    for (const QString& usi : std::as_const(usiMoves)) {
        ++plyNumber;

        // 指し手を適用する前に、移動元の駒トークンを取得
        QString pieceToken;
        if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
            // 駒打ちの場合はUSI文字から取得
            pieceToken = QString(usi.at(0).toUpper());
        } else if (usi.size() >= 4) {
            // 盤上移動の場合は盤面から取得
            int fromFile = usi.at(0).toLatin1() - '0';
            QChar fromRankChar = usi.at(1);
            pieceToken = tracer.tokenAtFileRank(fromFile, fromRankChar);
        }

        KifDisplayItem item;
        item.prettyMove = usiToPrettyMove(usi, plyNumber, prevToFile, prevToRank, pieceToken);
        item.timeText = QStringLiteral("00:00/00:00:00");
        item.comment = QString();
        item.ply = plyNumber;

        out.push_back(item);

        // 盤面に指し手を適用
        tracer.applyUsiMove(usi);
    }

    // 終局理由があれば追加
    if (!terminalCode.isEmpty()) {
        ++plyNumber;
        QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
        QString termJp = terminalCodeToJapanese(terminalCode);

        KifDisplayItem termItem;
        termItem.prettyMove = teban + termJp;
        termItem.timeText = QStringLiteral("00:00/00:00:00");
        termItem.comment = QString();
        termItem.ply = plyNumber;

        out.push_back(termItem);
    }

    return out;
}

bool UsenToSfenConverter::parseWithVariations(const QString& usenPath, 
                                               KifParseResult& out, 
                                               QString* errorMessage)
{
    out = KifParseResult{};

    QString content;
    if (!readUsenFile(usenPath, content, errorMessage)) {
        return false;
    }

    // 初期局面を取得
    QString initialSfen = detectInitialSfenFromFile(usenPath, nullptr);

    // 本譜と分岐を分離
    QString mainlineUsen;
    QVector<QPair<int, QString>> variations;  // (startPly, usenMoves)
    QString terminalCode;
    QString warn;

    if (!parseUsenString(content, mainlineUsen, variations, &terminalCode, &warn)) {
        if (errorMessage) *errorMessage = warn;
        return false;
    }

    // 本譜のデコード
    QString mainTerminal;
    QStringList mainUsiMoves = decodeUsenMoves(mainlineUsen, &mainTerminal);

    // 本譜のKifLineを構築
    out.mainline.baseSfen = initialSfen;
    out.mainline.usiMoves = mainUsiMoves;

    // 本譜の表示用データを構築（SfenPositionTracer使用）
    {
        // 開始局面
        KifDisplayItem openingItem;
        openingItem.prettyMove = QString();
        openingItem.timeText = QStringLiteral("00:00/00:00:00");
        openingItem.comment = QString();
        openingItem.ply = 0;
        out.mainline.disp.push_back(openingItem);

        // SfenPositionTracerを使って盤面を追跡
        SfenPositionTracer tracer;
        if (!tracer.setFromSfen(initialSfen)) {
            tracer.resetToStartpos();
        }

        int prevToFile = 0, prevToRank = 0;
        int plyNumber = 0;

        for (const QString& usi : std::as_const(mainUsiMoves)) {
            ++plyNumber;

            // 指し手を適用する前に、移動元の駒トークンを取得
            QString pieceToken;
            if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
                pieceToken = QString(usi.at(0).toUpper());
            } else if (usi.size() >= 4) {
                int fromFile = usi.at(0).toLatin1() - '0';
                QChar fromRankChar = usi.at(1);
                pieceToken = tracer.tokenAtFileRank(fromFile, fromRankChar);
            }

            KifDisplayItem item;
            item.prettyMove = usiToPrettyMove(usi, plyNumber, prevToFile, prevToRank, pieceToken);
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = QString();
            item.ply = plyNumber;

            out.mainline.disp.push_back(item);

            // 盤面に指し手を適用
            tracer.applyUsiMove(usi);
        }

        // 終局理由
        if (!mainTerminal.isEmpty()) {
            ++plyNumber;
            QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
            QString termJp = terminalCodeToJapanese(mainTerminal);

            KifDisplayItem termItem;
            termItem.prettyMove = teban + termJp;
            termItem.timeText = QStringLiteral("00:00/00:00:00");
            termItem.comment = QString();
            termItem.ply = plyNumber;

            out.mainline.disp.push_back(termItem);
        }
    }

    // 分岐のデコード
    for (const auto& var : std::as_const(variations)) {
        int startPly = var.first;
        const QString& varUsen = var.second;

        QString varTerminal;
        QStringList varUsiMoves = decodeUsenMoves(varUsen, &varTerminal);

        KifVariation kifVar;
        kifVar.startPly = startPly;
        kifVar.line.baseSfen = QString();  // 分岐は相対的な開始点から
        kifVar.line.usiMoves = varUsiMoves;

        // 分岐用のSfenPositionTracerを初期化
        // 本譜のstartPly手目までの局面を再現する
        SfenPositionTracer varTracer;
        if (!varTracer.setFromSfen(initialSfen)) {
            varTracer.resetToStartpos();
        }
        // 本譜のstartPly-1手目まで適用
        for (int i = 0; i < startPly - 1 && i < mainUsiMoves.size(); ++i) {
            varTracer.applyUsiMove(mainUsiMoves[i]);
        }

        // 表示用データ
        int prevToFile = 0, prevToRank = 0;
        int plyNumber = startPly - 1;

        for (const QString& usi : std::as_const(varUsiMoves)) {
            ++plyNumber;

            // 移動元の駒トークンを取得
            QString pieceToken;
            if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
                pieceToken = QString(usi.at(0).toUpper());
            } else if (usi.size() >= 4) {
                int fromFile = usi.at(0).toLatin1() - '0';
                QChar fromRankChar = usi.at(1);
                pieceToken = varTracer.tokenAtFileRank(fromFile, fromRankChar);
            }

            KifDisplayItem item;
            item.prettyMove = usiToPrettyMove(usi, plyNumber, prevToFile, prevToRank, pieceToken);
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = QString();
            item.ply = plyNumber;

            kifVar.line.disp.push_back(item);

            // 盤面に指し手を適用
            varTracer.applyUsiMove(usi);
        }

        // 分岐の終局理由
        if (!varTerminal.isEmpty()) {
            ++plyNumber;
            QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
            QString termJp = terminalCodeToJapanese(varTerminal);

            KifDisplayItem termItem;
            termItem.prettyMove = teban + termJp;
            termItem.timeText = QStringLiteral("00:00/00:00:00");
            termItem.comment = QString();
            termItem.ply = plyNumber;

            kifVar.line.disp.push_back(termItem);
        }

        out.variations.push_back(kifVar);
    }

    return true;
}

QList<KifGameInfoItem> UsenToSfenConverter::extractGameInfo(const QString& filePath)
{
    // USENは基本的にメタ情報を含まない
    // ファイル名から情報を推測することは可能だが、現時点では空を返す
    Q_UNUSED(filePath)
    return QList<KifGameInfoItem>();
}

QMap<QString, QString> UsenToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    Q_UNUSED(filePath)
    return QMap<QString, QString>();
}

QString UsenToSfenConverter::terminalCodeToJapanese(const QString& code)
{
    for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
        if (code == QString::fromLatin1(kTerminalCodes[i].code)) {
            return QString::fromUtf8(kTerminalCodes[i].japanese);
        }
    }
    return QStringLiteral("終了");
}

// ============================================================================
// プライベートメソッド
// ============================================================================

bool UsenToSfenConverter::readUsenFile(const QString& filePath, QString& content, QString* warn)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warn) *warn = QStringLiteral("ファイルを開けません: %1").arg(filePath);
        return false;
    }

    QTextStream in(&file);
    content = in.readAll().trimmed();
    file.close();

    if (content.isEmpty()) {
        if (warn) *warn = QStringLiteral("ファイルが空です");
        return false;
    }

    return true;
}

bool UsenToSfenConverter::parseUsenString(const QString& usen,
                                           QString& mainlineUsen,
                                           QVector<QPair<int, QString>>& variations,
                                           QString* terminalCode,
                                           QString* warn)
{
    variations.clear();
    if (terminalCode) terminalCode->clear();

    // スライド Image 1, 3 より:
    // 構造: [初期局面]~[本譜]~[分岐1]~[分岐2]...
    // 各パート: [オフセット].[指し手1][指し手2]...[.終局コード]
    // 
    // 本譜の場合: オフセット = 「指し手1」の手数 - 1 (通常は0)
    // 分岐の場合: オフセット = 分岐開始位置

    // ~で分割
    QStringList parts = usen.split(QChar('~'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (warn) *warn = QStringLiteral("USENが空です");
        return false;
    }

    // 最初のパートは初期局面(空なら平手) + 本譜
    // "0.{moves}..." または "{sfen}.{moves}..." の形式

    bool foundMainline = false;

    for (int partIdx = 0; partIdx < parts.size(); ++partIdx) {
        const QString& part = parts[partIdx];
        
        int dotPos = part.indexOf(QChar('.'));
        if (dotPos < 0) continue;

        QString prefix = part.left(dotPos);
        QString movesAndTerminal = part.mid(dotPos + 1);

        // 終局コードのチェック (末尾が .x の形式)
        QString moves = movesAndTerminal;
        QString terminal;
        
        // 末尾から終局コードを探す
        int lastDot = movesAndTerminal.lastIndexOf(QChar('.'));
        if (lastDot >= 0 && lastDot < movesAndTerminal.size() - 1) {
            QString possibleTerminal = movesAndTerminal.mid(lastDot + 1);
            // 1文字の終局コードかチェック
            if (possibleTerminal.size() == 1) {
                bool isTerminal = false;
                for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
                    if (possibleTerminal == QString::fromLatin1(kTerminalCodes[i].code)) {
                        isTerminal = true;
                        break;
                    }
                }
                if (isTerminal) {
                    terminal = possibleTerminal;
                    moves = movesAndTerminal.left(lastDot);
                }
            }
        }

        // 最初のパートを本譜として扱う
        if (partIdx == 0) {
            // prefix が数字の場合はオフセット（通常0）
            // prefix が英字を含む場合はSFEN（カスタム局面）
            mainlineUsen = QStringLiteral("~") + part;
            if (terminalCode && !terminal.isEmpty()) {
                *terminalCode = terminal;
            }
            foundMainline = true;
        } else {
            // 分岐: prefix は分岐開始位置
            bool ok;
            int startPly = prefix.toInt(&ok);
            if (ok && startPly >= 0) {
                // 分岐の指し手部分を抽出
                // 注意: 分岐は本譜の指定手数から分岐するので、
                // startPlyはその分岐点の手数を示す
                QString varUsen = QStringLiteral("~0.") + moves;
                if (!terminal.isEmpty()) {
                    varUsen += QStringLiteral(".") + terminal;
                }
                variations.append(qMakePair(startPly, varUsen));
            }
        }
    }

    if (!foundMainline) {
        if (warn) *warn = QStringLiteral("本譜が見つかりません");
        return false;
    }

    return true;
}

QStringList UsenToSfenConverter::decodeUsenMoves(const QString& usenStr, QString* terminalOut)
{
    QStringList usiMoves;

    QString movesStr = usenStr;

    // ~X. プレフィックスを除去
    // 平手の場合: ~0. または ~.
    if (movesStr.startsWith(QStringLiteral("~"))) {
        int dotPos = movesStr.indexOf(QChar('.'));
        if (dotPos >= 0) {
            movesStr = movesStr.mid(dotPos + 1);
        }
    }

    // 終局コードを除去
    if (terminalOut) terminalOut->clear();
    int lastDot = movesStr.lastIndexOf(QChar('.'));
    if (lastDot >= 0 && lastDot < movesStr.size() - 1) {
        QString possibleTerminal = movesStr.mid(lastDot + 1);
        if (possibleTerminal.size() == 1) {
            for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
                if (possibleTerminal == QString::fromLatin1(kTerminalCodes[i].code)) {
                    if (terminalOut) *terminalOut = possibleTerminal;
                    movesStr = movesStr.left(lastDot);
                    break;
                }
            }
        }
    }

    // 3文字ずつデコード
    // USENでは各指し手が3文字のbase36でエンコードされている
    // code = (from_sq * 81 + to_sq) * 2 + promote

    int i = 0;
    int moveCount = 0;
    while (i + 2 < movesStr.size()) {
        QString threeChars = movesStr.mid(i, 3);

        QString usi = decodeBase36Move(threeChars);
        if (!usi.isEmpty()) {
            usiMoves.append(usi);
        } else {
            // デコードできなかった場合はプレースホルダーを追加
            QString placeholder = QStringLiteral("?%1").arg(moveCount + 1);
            qDebug() << "[USEN] Move" << (moveCount + 1) << "'" << threeChars 
                     << "' could not be decoded, using placeholder";
            usiMoves.append(placeholder);
        }

        ++moveCount;
        i += 3;
    }

    qDebug() << "[USEN] Decoded" << usiMoves.size() << "moves from USEN string";
    return usiMoves;
}

int UsenToSfenConverter::base36CharToInt(QChar c)
{
    int idx = kBase36Chars.indexOf(c.toLower());
    return idx;  // 見つからなければ -1
}

int UsenToSfenConverter::base36ToMoveIndex(const QString& threeChars)
{
    if (threeChars.size() != 3) return -1;

    int v0 = base36CharToInt(threeChars.at(0));
    int v1 = base36CharToInt(threeChars.at(1));
    int v2 = base36CharToInt(threeChars.at(2));

    if (v0 < 0 || v1 < 0 || v2 < 0) return -1;

    return v0 * 36 * 36 + v1 * 36 + v2;  // 0-46655
}

QString UsenToSfenConverter::decodeBase36Move(const QString& base36str)
{
    if (base36str.size() != 3) return QString();

    int code = base36ToMoveIndex(base36str);
    if (code < 0) return QString();

    // USENエンコーディング仕様 (スライド Image 4 より):
    //
    // code = (from_sq * 81 + to_sq) * 2 + (成る場合は1)
    //
    // 駒の移動元の位置:
    // - 盤上の駒を移動: 0 (1一) ～ 80 (9九)
    // - 持駒を使用: 81 + 駒の種類
    //   歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
    //   と金=2, 成香=3, 成桂=4, 成銀=5, 馬=6, 竜=7, 玉=8
    //
    // マス番号: sq = (rank - 1) * 9 + (file - 1)
    // 例: 1一 = 0, 9九 = 80

    bool isPromotion = (code % 2 == 1);
    int moveCode = code / 2;

    int from_sq = moveCode / 81;
    int to_sq = moveCode % 81;

    // 移動先のマス座標を取得
    int toFile, toRank;
    squareToFileRank(to_sq, toFile, toRank);

    // 駒打ちの場合 (from_sq >= 81)
    if (from_sq >= 81) {
        int pieceType = from_sq - 81;
        QChar pieceChar = dropTypeToUsiChar(pieceType);
        
        if (pieceChar == QChar('?')) {
            qWarning() << "[USEN] Unknown drop piece type:" << pieceType;
            return QString();
        }

        // 駒打ちはUSI形式: P*5e
        return QStringLiteral("%1*%2%3")
            .arg(pieceChar)
            .arg(toFile)
            .arg(rankToChar(toRank));
    }

    // 盤上移動の場合 (from_sq: 0-80)
    int fromFile, fromRank;
    squareToFileRank(from_sq, fromFile, fromRank);

    // USI形式: 7g7f または 7g7f+ (成り)
    QString usi = QStringLiteral("%1%2%3%4")
        .arg(fromFile)
        .arg(rankToChar(fromRank))
        .arg(toFile)
        .arg(rankToChar(toRank));

    if (isPromotion) {
        usi += QLatin1Char('+');
    }

    return usi;
}

void UsenToSfenConverter::buildKifLine(const QStringList& usiMoves,
                                        const QString& baseSfen,
                                        int startPly,
                                        const QString& terminalCode,
                                        KifLine& outLine,
                                        QString* warn)
{
    Q_UNUSED(warn)

    outLine.startPly = startPly;
    outLine.usiMoves = usiMoves;
    outLine.disp.clear();

    // SfenPositionTracerを使って盤面を追跡
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(baseSfen)) {
        // フォールバック: 平手初期局面
        tracer.resetToStartpos();
    }

    int prevToFile = 0, prevToRank = 0;
    int plyNumber = startPly - 1;

    for (const QString& usi : std::as_const(usiMoves)) {
        ++plyNumber;

        // 指し手を適用する前に、移動元の駒トークンを取得
        QString pieceToken;
        if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
            // 駒打ちの場合はUSI文字から取得
            pieceToken = QString(usi.at(0).toUpper());
        } else if (usi.size() >= 4) {
            // 盤上移動の場合は盤面から取得
            int fromFile = usi.at(0).toLatin1() - '0';
            QChar fromRankChar = usi.at(1);
            pieceToken = tracer.tokenAtFileRank(fromFile, fromRankChar);
        }

        KifDisplayItem item;
        item.prettyMove = usiToPrettyMove(usi, plyNumber, prevToFile, prevToRank, pieceToken);
        item.timeText = QStringLiteral("00:00/00:00:00");
        item.comment = QString();
        item.ply = plyNumber;

        outLine.disp.push_back(item);

        // 盤面に指し手を適用
        tracer.applyUsiMove(usi);
    }

    // 終局理由
    if (!terminalCode.isEmpty()) {
        ++plyNumber;
        QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
        QString termJp = terminalCodeToJapanese(terminalCode);

        KifDisplayItem termItem;
        termItem.prettyMove = teban + termJp;
        termItem.timeText = QStringLiteral("00:00/00:00:00");
        termItem.comment = QString();
        termItem.ply = plyNumber;

        outLine.disp.push_back(termItem);
        outLine.endsWithTerminal = true;
    }
}

QString UsenToSfenConverter::usiToPrettyMove(const QString& usi, int plyNumber, 
                                              int& prevToFile, int& prevToRank,
                                              const QString& pieceToken)
{
    if (usi.isEmpty()) return QString();

    QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

    // プレースホルダー指し手のチェック (デコードできなかった指し手)
    if (usi.startsWith(QChar('?'))) {
        return teban + QStringLiteral("?(") + usi.mid(1) + QStringLiteral("手目)");
    }

    // 駒打ちのパターン: P*7f
    if (usi.size() >= 4 && usi.at(1) == QChar('*')) {
        QChar pieceChar = usi.at(0);
        int toFile = usi.at(2).toLatin1() - '0';
        int toRank = usi.at(3).toLatin1() - 'a' + 1;

        QString kanji = pieceToKanji(pieceChar);

        // 全角数字と漢数字
        static const QString kZenkakuDigits = QStringLiteral("０１２３４５６７８９");
        static const QString kKanjiRanks = QStringLiteral("〇一二三四五六七八九");

        QString result = teban;
        if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);
        result += kanji + QStringLiteral("打");

        prevToFile = toFile;
        prevToRank = toRank;

        return result;
    }

    // 通常移動のパターン: 7g7f, 7g7f+
    if (usi.size() >= 4) {
        int fromFile = usi.at(0).toLatin1() - '0';
        int fromRank = usi.at(1).toLatin1() - 'a' + 1;
        int toFile = usi.at(2).toLatin1() - '0';
        int toRank = usi.at(3).toLatin1() - 'a' + 1;
        bool promotes = (usi.size() >= 5 && usi.at(4) == QChar('+'));

        // 範囲チェック
        if (fromFile < 1 || fromFile > 9 || fromRank < 1 || fromRank > 9 ||
            toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            return teban + QStringLiteral("?");
        }

        // 全角数字と漢数字
        static const QString kZenkakuDigits = QStringLiteral("０１２３４５６７８９");
        static const QString kKanjiRanks = QStringLiteral("〇一二三四五六七八九");

        QString result = teban;

        // 同じ場所への移動
        if (toFile == prevToFile && toRank == prevToRank) {
            result += QStringLiteral("同　");
        } else {
            if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
            if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);
        }

        // 駒種を取得（pieceTokenから）
        QString kanji = tokenToKanji(pieceToken);
        result += kanji;

        // 成り
        if (promotes) {
            result += QStringLiteral("成");
        }

        // 移動元
        result += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");

        prevToFile = toFile;
        prevToRank = toRank;

        return result;
    }

    return teban + QStringLiteral("?");
}

QString UsenToSfenConverter::pieceToKanji(QChar usiPiece)
{
    switch (usiPiece.toUpper().toLatin1()) {
    case 'P': return QStringLiteral("歩");
    case 'L': return QStringLiteral("香");
    case 'N': return QStringLiteral("桂");
    case 'S': return QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return QStringLiteral("角");
    case 'R': return QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QString UsenToSfenConverter::tokenToKanji(const QString& token)
{
    // token: "P", "p", "+P", "+p", "B", "b", "+B", etc.
    if (token.isEmpty()) return QStringLiteral("?");

    // 成駒かどうか
    bool promoted = token.startsWith(QChar('+'));
    QChar pieceChar = promoted ? token.at(1) : token.at(0);
    
    switch (pieceChar.toUpper().toLatin1()) {
    case 'P': return promoted ? QStringLiteral("と") : QStringLiteral("歩");
    case 'L': return promoted ? QStringLiteral("杏") : QStringLiteral("香");
    case 'N': return promoted ? QStringLiteral("圭") : QStringLiteral("桂");
    case 'S': return promoted ? QStringLiteral("全") : QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return promoted ? QStringLiteral("馬") : QStringLiteral("角");
    case 'R': return promoted ? QStringLiteral("龍") : QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QChar UsenToSfenConverter::rankNumToLetter(int r)
{
    if (r < 1 || r > 9) return QChar();
    return QChar('a' + r - 1);
}

int UsenToSfenConverter::rankLetterToNum(QChar c)
{
    char ch = c.toLatin1();
    if (ch >= 'a' && ch <= 'i') return ch - 'a' + 1;
    return 0;
}
