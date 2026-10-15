/// @file usentosfenconverter.cpp
/// @brief USEN形式棋譜コンバータクラスの実装 - 公開API

#include "usentosfenconverter.h"
#include "parsecommon.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "sfenutils.h"

#include <utility>
#include <QFile>
#include <QTextStream>
#include "logcategories.h"

// ============================================================================
// 公開メソッド
// ============================================================================

QString UsenToSfenConverter::detectInitialSfenFromFile(const QString& usenPath, QString* detectedLabel)
{
    QString content;
    QString warn;
    if (!readUsenFile(usenPath, content, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }
    return detectInitialSfenFromText(content, detectedLabel);
}

QString UsenToSfenConverter::detectInitialSfenFromText(const QString& text, QString* detectedLabel)
{
    const QString content = text.trimmed();

    // ~ の位置を探す
    const qsizetype tildePos = content.indexOf(QChar('~'));
    if (tildePos < 0) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }

    // ~ の前の部分が初期局面（空なら平手）
    const QString positionPart = content.left(tildePos);

    if (positionPart.isEmpty()) {
        // 平手（~ から始まる場合）
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return SfenUtils::hirateSfen();
    }

    // カスタム初期局面の場合: USEN形式からSFENに逆変換
    // '_' → '/', '.' → ' ', 'z' → '+'
    QString sfen = positionPart;
    sfen.replace(QChar('_'), QChar('/'));
    sfen.replace(QChar('.'), QChar(' '));
    sfen.replace(QChar('z'), QChar('+'));

    // 手数が省略されている場合は補完（盤の SFEN は4つの欄が必須）
    const QStringList sfenParts = sfen.split(QChar(' '), Qt::SkipEmptyParts);
    if (sfenParts.size() == 3) {
        sfen += QStringLiteral(" 1");
    }

    if (detectedLabel) *detectedLabel = QStringLiteral("局面指定");
    return sfen;
}

QStringList UsenToSfenConverter::convertFile(const QString& usenPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(usenPath, result, errorMessage) ? result.mainline.usiMoves() : QStringList();
}

QList<KifDisplayItem> UsenToSfenConverter::extractMovesWithTimes(const QString& usenPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(usenPath, result, errorMessage) ? result.mainline.displayItems() : QList<KifDisplayItem>();
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
    return parseText(content, out, errorMessage);
}

bool UsenToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};
    const QString content = text.trimmed();
    if (content.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("USEN: empty text");
        return false;
    }

    // 初期局面を取得
    const QString initialSfen = detectInitialSfenFromText(content, nullptr);

    // 本譜と分岐を分離
    QString mainlineUsen;
    QList<QPair<int, QString>> variations;  // (startPly, usenMoves)
    QString terminalCode;
    QString warn;

    if (!parseUsenString(content, mainlineUsen, variations, &terminalCode, &warn)) {
        if (errorMessage) *errorMessage = warn;
        return false;
    }

    // 本譜のデコード
    QString mainTerminal;
    const auto decodedMain = decodeUsenMovesStrict(mainlineUsen, &mainTerminal);
    if (decodedMain.invalidCount > 0) {
        if (errorMessage) *errorMessage = decodedMain.firstError;
        return false;
    }
    const QStringList mainUsiMoves = decodedMain.moves;

    KifuParseCommon::KifLineDraft main;
    main.baseSfen = initialSfen;
    main.usiMoves = mainUsiMoves;
    main.disp.append(KifuParseCommon::createOpeningDisplayItem({}, {}));
    for (qsizetype i = 0; i < mainUsiMoves.size(); ++i)
        main.disp.append(KifuParseCommon::createMoveDisplayItem(static_cast<int>(i) + 1, {}));
    if (!mainTerminal.isEmpty()) {
        main.disp.append(KifuParseCommon::createUsiTerminalDisplayItem(
            static_cast<int>(mainUsiMoves.size()) + 1, terminalFromCode(mainTerminal), initialSfen, mainUsiMoves.size()));
        main.endsWithTerminal = true;
    }
    if (!main.resolve(out.mainline, errorMessage)) return false;

    // 分岐のデコード
    // USEN形式では各分岐は「直前に記述された手順」からの派生として扱う。
    // lastLineUsiMoves は直前に記述された手順の全USI指し手列を保持する。
    QStringList lastLineUsiMoves = mainUsiMoves;
    QStringList lastLineSfens = out.mainline.sfenList();
    QList<int> lastLineOwners(mainUsiMoves.size(), -1);

    for (const auto& var : std::as_const(variations)) {
        const int startPly = var.first;
        const QString& varUsen = var.second;

        QString varTerminal;
        const auto decodedVariation = decodeUsenMovesStrict(varUsen, &varTerminal);
        if (decodedVariation.invalidCount > 0) {
            if (errorMessage) *errorMessage = decodedVariation.firstError;
            out = KifParseResult{};
            return false;
        }
        const QStringList varUsiMoves = decodedVariation.moves;

        // 直前の手順から分岐点までの指し手を取得
        const int offset = startPly - 1;  // 0-indexed
        if (offset < 0 || offset > lastLineUsiMoves.size()) {
            if (errorMessage) *errorMessage = QStringLiteral("USEN: invalid variation start: %1").arg(startPly);
            out = KifParseResult{};
            return false;
        }
        QStringList prefixMoves;
        for (int i = 0; i < offset && i < lastLineUsiMoves.size(); ++i) {
            prefixMoves.append(lastLineUsiMoves[i]);
        }

        KifVariation kifVar;
        kifVar.startPly = startPly;
        kifVar.parentVariation = offset > 0 ? lastLineOwners.at(offset - 1) : -1;
        KifuParseCommon::KifLineDraft branch;
        branch.startPly = startPly;
        branch.baseSfen = lastLineSfens.at(offset);
        branch.previousUsi = prefixMoves.isEmpty() ? QString() : prefixMoves.last();
        branch.usiMoves = varUsiMoves;
        for (qsizetype i = 0; i < varUsiMoves.size(); ++i)
            branch.disp.append(KifuParseCommon::createMoveDisplayItem(startPly + static_cast<int>(i), {}));
        if (!varTerminal.isEmpty()) {
            branch.disp.append(KifuParseCommon::createUsiTerminalDisplayItem(
                startPly + static_cast<int>(varUsiMoves.size()), terminalFromCode(varTerminal), branch.baseSfen, varUsiMoves.size()));
            branch.endsWithTerminal = true;
        }
        if (!branch.resolve(kifVar.line, errorMessage)) return false;
        lastLineSfens = lastLineSfens.mid(0, offset) + kifVar.line.sfenList();

        const int index = static_cast<int>(out.variations.size());
        out.variations.push_back(kifVar);

        // 直前の手順を更新: prefix + 今回の分岐の指し手
        lastLineUsiMoves = prefixMoves + varUsiMoves;
        lastLineOwners = lastLineOwners.mid(0, offset);
        lastLineOwners.append(QList<int>(varUsiMoves.size(), index));
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

// ============================================================
// USEN形式棋譜コンバータ - デコード・パース処理
// ============================================================

namespace {

// base36文字セット
static constexpr QStringView kBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";

// 終局理由コードのマッピング (スライド Image 5より)
struct TerminalInfo {
    const char* code;
    TerminalType type;
};

static const TerminalInfo kTerminalCodes[] = {
    {"i", TerminalType::IllegalLoss},
    {"r", TerminalType::Resign},
    {"t", TerminalType::Timeout},
    {"p", TerminalType::Interrupt},
    {"j", TerminalType::Impasse},
    {nullptr, TerminalType::None}
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
// デコード・パースメソッド
// ============================================================================

TerminalType UsenToSfenConverter::terminalFromCode(const QString& code)
{
    for (int i = 0; kTerminalCodes[i].code != nullptr; ++i) {
        if (code == QString::fromLatin1(kTerminalCodes[i].code)) {
            return kTerminalCodes[i].type;
        }
    }
    return TerminalType::Error;
}

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
                                           QList<QPair<int, QString>>& variations,
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
        if (lastDot >= 0) {
            const QString possibleTerminal = movesAndTerminal.mid(lastDot + 1);
            // 1文字の終局コードかチェック
            if (possibleTerminal.isEmpty() || isTerminalCode(possibleTerminal)) {
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

UsenDecodeResult UsenToSfenConverter::decodeUsenMovesStrict(const QString& usenStr, QString* terminalOut)
{
    UsenDecodeResult result;

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
    if (lastDot >= 0) {
        const QString possibleTerminal = movesStr.mid(lastDot + 1);
        if (possibleTerminal.isEmpty() || isTerminalCode(possibleTerminal)) {
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
            result.moves.append(usi);
        } else {
            // 無効な指し手を下流へ渡さず、棋譜全体を失敗として扱う。
            qCDebug(lcKifu) << "Move" << (moveCount + 1) << "'" << threeChars
                     << "' could not be decoded";
            ++result.invalidCount;
            if (result.firstError.isEmpty()) {
                result.firstError = QStringLiteral("Move %1 '%2' could not be decoded")
                    .arg(moveCount + 1).arg(threeChars);
            }
        }

        ++moveCount;
        i += 3;
    }

    if (i != movesStr.size()) {
        ++result.invalidCount;
        if (result.firstError.isEmpty()) result.firstError = QStringLiteral("Incomplete USEN move at %1").arg(moveCount + 1);
    }
    if (result.invalidCount > 0) result.moves.clear();

    qCDebug(lcKifu) << "Decoded" << result.moves.size() << "moves from USEN string"
             << "(invalid:" << result.invalidCount << ")";
    return result;
}

QStringList UsenToSfenConverter::decodeUsenMoves(const QString& usenStr, QString* terminalOut)
{
    UsenDecodeResult result = decodeUsenMovesStrict(usenStr, terminalOut);
    return result.moves;
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
