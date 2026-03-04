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
    QList<QPair<int, QString>> variations;  // (startPly, usenMoves)
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
            (void)varTracer.applyUsiMove(move);
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
