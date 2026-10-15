/// @file usentosfenconverter.cpp
/// @brief USEN形式棋譜コンバータクラスの実装 - 公開API

#include "usentosfenconverter.h"
#include "parsecommon.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"

#include <utility>

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
            static_cast<int>(mainUsiMoves.size()) + 1, terminalCodeToJapanese(mainTerminal), initialSfen, mainUsiMoves.size()));
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
                startPly + static_cast<int>(varUsiMoves.size()), terminalCodeToJapanese(varTerminal), branch.baseSfen, varUsiMoves.size()));
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
