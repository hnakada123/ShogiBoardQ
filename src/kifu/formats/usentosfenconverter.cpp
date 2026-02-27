/// @file usentosfenconverter.cpp
/// @brief USEN形式棋譜コンバータクラスの実装

#include "usentosfenconverter.h"
#include "parsecommon.h"
#include "sfenpositiontracer.h"

#include <QFile>
#include <QTextStream>
#include "logcategories.h"

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
static constexpr QStringView kBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";

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

// 終局コードかどうかを判定
static bool isTerminalCode(const QString& code)
{
    for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
        if (code == QString::fromLatin1(kTerminalCodes[i].code)) {
            return true;
        }
    }
    return false;
}

// 段番号から行文字へ (1-9 -> 'a'-'i')
inline QChar rankToChar(int r) {
    return QChar('a' + r - 1);
}

// マス番号 (0-80) から筋・段を取得
// USEN座標系: sq = (rank - 1) * 9 + (file - 1)
// 例: 1一 = 0, 9九 = 80
inline void squareToFileRank(int sq, int& file, int& rank) {
    rank = sq / 9 + 1;  // 1-9
    file = sq % 9 + 1;  // 1-9
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

    // ~ の位置を探す
    const qsizetype tildePos = content.indexOf(QChar('~'));
    if (tildePos < 0) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return QString::fromLatin1(kHirateSfen);
    }

    // ~ の前の部分が初期局面（空なら平手）
    const QString positionPart = content.left(tildePos);

    if (positionPart.isEmpty()) {
        // 平手（~ から始まる場合）
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return QString::fromLatin1(kHirateSfen);
    }

    // カスタム初期局面の場合: USEN形式からSFENに逆変換
    // '_' → '/', '.' → ' ', 'z' → '+'
    QString sfen = positionPart;
    sfen.replace(QChar('_'), QChar('/'));
    sfen.replace(QChar('.'), QChar(' '));
    sfen.replace(QChar('z'), QChar('+'));

    // 手数が省略されている場合は補完（SfenPositionTracerは4フィールド必須）
    const QStringList sfenParts = sfen.split(QChar(' '), Qt::SkipEmptyParts);
    if (sfenParts.size() == 3) {
        sfen += QStringLiteral(" 1");
    }

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
    const QString initialSfen = detectInitialSfenFromFile(usenPath, nullptr);

    QString terminalCode;
    const QStringList usiMoves = decodeUsenMoves(content, &terminalCode);

    // 開始局面エントリ
    out.push_back(KifuParseCommon::createOpeningDisplayItem(QString(), QString()));

    // 共通パイプラインで指し手アイテムを構築
    int plyNumber = KifuParseCommon::buildUsiMoveDisplayItems(usiMoves, initialSfen, 1, out);

    // 終局理由があれば追加
    if (!terminalCode.isEmpty()) {
        out.push_back(KifuParseCommon::createTerminalDisplayItem(
            plyNumber + 1, terminalCodeToJapanese(terminalCode)));
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
    const QString initialSfen = detectInitialSfenFromFile(usenPath, nullptr);

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
    const QStringList mainUsiMoves = decodeUsenMoves(mainlineUsen, &mainTerminal);

    // 本譜のKifLineを構築
    out.mainline.baseSfen = initialSfen;
    out.mainline.usiMoves = mainUsiMoves;

    // 開始局面
    out.mainline.disp.push_back(KifuParseCommon::createOpeningDisplayItem(QString(), QString()));

    // 共通パイプラインで本譜の指し手アイテムを構築
    int mainPlyNumber = KifuParseCommon::buildUsiMoveDisplayItems(
        mainUsiMoves, initialSfen, 1, out.mainline.disp);

    // 終局理由
    if (!mainTerminal.isEmpty()) {
        out.mainline.disp.push_back(KifuParseCommon::createTerminalDisplayItem(
            mainPlyNumber + 1, terminalCodeToJapanese(mainTerminal)));
    }

    // 分岐のデコード
    // USEN形式では各分岐は「直前に記述された手順」からの派生として扱う。
    // lastLineUsiMoves は直前に記述された手順の全USI指し手列を保持する。
    QStringList lastLineUsiMoves = mainUsiMoves;

    for (const auto& var : std::as_const(variations)) {
        const int startPly = var.first;
        const QString& varUsen = var.second;

        QString varTerminal;
        const QStringList varUsiMoves = decodeUsenMoves(varUsen, &varTerminal);

        // 直前の手順から分岐点までの指し手を取得
        const int offset = startPly - 1;  // 0-indexed
        QStringList prefixMoves;
        for (int i = 0; i < offset && i < lastLineUsiMoves.size(); ++i) {
            prefixMoves.append(lastLineUsiMoves[i]);
        }

        // 分岐点の局面を再現する
        SfenPositionTracer varTracer;
        if (!varTracer.setFromSfen(initialSfen)) {
            varTracer.resetToStartpos();
        }
        for (const QString& move : std::as_const(prefixMoves)) {
            varTracer.applyUsiMove(move);
        }

        KifVariation kifVar;
        kifVar.startPly = startPly;
        kifVar.line.baseSfen = varTracer.toSfenString();
        kifVar.line.usiMoves = varUsiMoves;

        // 共通パイプラインで分岐の表示アイテムを構築
        int varPlyNumber = KifuParseCommon::buildUsiMoveDisplayItems(
            varUsiMoves, kifVar.line.baseSfen, startPly, kifVar.line.disp);

        // 分岐の終局理由
        if (!varTerminal.isEmpty()) {
            kifVar.line.disp.push_back(KifuParseCommon::createTerminalDisplayItem(
                varPlyNumber + 1, terminalCodeToJapanese(varTerminal)));
        }

        // sfenList を構築（ツリービルダーが findBySfen で正しい分岐点を見つけるために必要）
        kifVar.line.sfenList = SfenPositionTracer::buildSfenRecord(
            kifVar.line.baseSfen, varUsiMoves, !varTerminal.isEmpty());

        out.variations.push_back(kifVar);

        // 直前の手順を更新: prefix + 今回の分岐の指し手
        lastLineUsiMoves = prefixMoves + varUsiMoves;
    }

    return true;
}

QList<KifGameInfoItem> UsenToSfenConverter::extractGameInfo(const QString& filePath)
{
    // USENは基本的にメタ情報を含まない
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

    // 構造: [初期局面]~[本譜]~[分岐1]~[分岐2]...
    // 各パート: [オフセット].[指し手1][指し手2]...[.終局コード]

    // ~ の前の初期局面部分を除去してから ~ で分割
    const qsizetype firstTilde = usen.indexOf(QChar('~'));
    if (firstTilde < 0) {
        if (warn) *warn = QStringLiteral("USENが空です");
        return false;
    }

    // ~ 以降を分割（初期局面部分はスキップ）
    const QString afterPosition = usen.mid(firstTilde + 1);
    const QStringList parts = afterPosition.split(QChar('~'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (warn) *warn = QStringLiteral("USENが空です");
        return false;
    }

    // parts[0] = 本譜, parts[1..] = 分岐

    bool foundMainline = false;

    for (qsizetype partIdx = 0; partIdx < parts.size(); ++partIdx) {
        const QString& part = parts[partIdx];

        const qsizetype dotPos = part.indexOf(QChar('.'));
        if (dotPos < 0) continue;

        const QString prefix = part.left(dotPos);
        const QString movesAndTerminal = part.mid(dotPos + 1);

        // 終局コードのチェック (末尾が .x の形式)
        QString moves = movesAndTerminal;
        QString terminal;

        // 末尾から終局コードを探す
        const qsizetype lastDot = movesAndTerminal.lastIndexOf(QChar('.'));
        if (lastDot >= 0 && lastDot < movesAndTerminal.size() - 1) {
            const QString possibleTerminal = movesAndTerminal.mid(lastDot + 1);
            // 1文字の終局コードかチェック
            if (possibleTerminal.size() == 1 && isTerminalCode(possibleTerminal)) {
                terminal = possibleTerminal;
                moves = movesAndTerminal.left(lastDot);
            }
        }

        // 最初のパートを本譜として扱う
        if (partIdx == 0) {
            mainlineUsen = QStringLiteral("~") + part;
            if (terminalCode && !terminal.isEmpty()) {
                *terminalCode = terminal;
            }
            foundMainline = true;
        } else {
            // 分岐: prefix は分岐開始位置（0-indexed）
            // USENのオフセットは「何手目の後から分岐するか」を示す
            // 例: オフセット2 = 2手目の後から分岐 = 3手目から分岐
            // したがって startPly = offset + 1
            bool ok;
            const int offset = prefix.toInt(&ok);
            if (ok && offset >= 0) {
                const int startPly = offset + 1;  // 1-indexed に変換
                // 分岐の指し手部分を抽出
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
        const qsizetype dotPos = movesStr.indexOf(QChar('.'));
        if (dotPos >= 0) {
            movesStr = movesStr.mid(dotPos + 1);
        }
    }

    // 終局コードを除去
    if (terminalOut) terminalOut->clear();
    const qsizetype lastDot = movesStr.lastIndexOf(QChar('.'));
    if (lastDot >= 0 && lastDot < movesStr.size() - 1) {
        const QString possibleTerminal = movesStr.mid(lastDot + 1);
        if (possibleTerminal.size() == 1 && isTerminalCode(possibleTerminal)) {
            if (terminalOut) *terminalOut = possibleTerminal;
            movesStr = movesStr.left(lastDot);
        }
    }

    // 3文字ずつデコード
    // USENでは各指し手が3文字のbase36でエンコードされている
    // code = (from_sq * 81 + to_sq) * 2 + promote

    int i = 0;
    int moveCount = 0;
    while (i + 2 < movesStr.size()) {
        const QString threeChars = movesStr.mid(i, 3);

        const QString usi = decodeBase36Move(threeChars);
        if (!usi.isEmpty()) {
            usiMoves.append(usi);
        } else {
            // デコードできなかった場合はプレースホルダーを追加
            const QString placeholder = QStringLiteral("?%1").arg(moveCount + 1);
            qCDebug(lcKifu) << "Move" << (moveCount + 1) << "'" << threeChars
                     << "' could not be decoded, using placeholder";
            usiMoves.append(placeholder);
        }

        ++moveCount;
        i += 3;
    }

    qCDebug(lcKifu) << "Decoded" << usiMoves.size() << "moves from USEN string";
    return usiMoves;
}

int UsenToSfenConverter::base36CharToInt(QChar c)
{
    const qsizetype idx = kBase36Chars.indexOf(c.toLower());
    return static_cast<int>(idx);  // 見つからなければ -1
}

int UsenToSfenConverter::base36ToMoveIndex(const QString& threeChars)
{
    if (threeChars.size() != 3) return -1;

    const int v0 = base36CharToInt(threeChars.at(0));
    const int v1 = base36CharToInt(threeChars.at(1));
    const int v2 = base36CharToInt(threeChars.at(2));

    if (v0 < 0 || v1 < 0 || v2 < 0) return -1;

    return v0 * 36 * 36 + v1 * 36 + v2;  // 0-46655
}

QString UsenToSfenConverter::decodeBase36Move(const QString& base36str)
{
    if (base36str.size() != 3) return QString();

    const int code = base36ToMoveIndex(base36str);
    if (code < 0) return QString();

    const bool isPromotion = (code % 2 == 1);
    const int moveCode = code / 2;

    const int from_sq = moveCode / 81;
    const int to_sq = moveCode % 81;

    // 移動先のマス座標を取得
    int toFile, toRank;
    squareToFileRank(to_sq, toFile, toRank);

    // 駒打ちの場合 (from_sq >= 81)
    if (from_sq >= 81) {
        const int pieceType = from_sq - 81;
        const QChar pieceChar = dropTypeToUsiChar(pieceType);

        if (pieceChar == QChar('?')) {
            qCWarning(lcKifu) << "Unknown drop piece type:" << pieceType;
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

    // 共通パイプラインで指し手アイテムを構築
    int plyNumber = KifuParseCommon::buildUsiMoveDisplayItems(
        usiMoves, baseSfen, startPly, outLine.disp);

    // 終局理由
    if (!terminalCode.isEmpty()) {
        outLine.disp.push_back(KifuParseCommon::createTerminalDisplayItem(
            plyNumber + 1, terminalCodeToJapanese(terminalCode)));
        outLine.endsWithTerminal = true;
    }
}
