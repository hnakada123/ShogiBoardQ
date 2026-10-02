/// @file jkftosfenconverter.cpp
/// @brief JKF形式棋譜コンバータクラスの実装
///
/// 指し手変換・初期局面構築は JkfMoveParser に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "jkftosfenconverter.h"
#include "jkfmoveparser.h"
#include "parsecommon.h"
#include "sfenpositiontracer.h"

#include <array>

#include <QFile>
#include <QJsonDocument>

namespace {
void appendWarning(QString* warn, const QString& message)
{
    if (!warn) {
        return;
    }
    if (!warn->isEmpty()) {
        *warn += QLatin1Char('\n');
    }
    *warn += message;
}

int normalizedColorIndex(const QJsonObject& moveObject, int plyNumber, QString* warn)
{
    const QJsonValue colorValue = moveObject.value(QStringLiteral("color"));
    if (!colorValue.isDouble()) {
        appendWarning(warn,
                      QStringLiteral("JKF: move %1 has invalid color type; fallback to 0")
                          .arg(plyNumber));
        return 0;
    }

    const int color = colorValue.toInt(-1);
    if (color != 0 && color != 1) {
        appendWarning(warn,
                      QStringLiteral("JKF: move %1 has out-of-range color %2; fallback to 0")
                          .arg(plyNumber)
                          .arg(color));
        return 0;
    }
    return color;
}
} // namespace

// ========== public API ==========

QString JkfToSfenConverter::mapPresetToSfen(const QString& preset)
{
    return JkfMoveParser::presetToSfen(preset);
}

QString JkfToSfenConverter::detectInitialSfenFromFile(const QString& jkfPath, QString* detectedLabel)
{
    QJsonObject root;
    QString warn;
    if (!loadJsonFile(jkfPath, root, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"));
    }
    return buildInitialSfen(root, detectedLabel);
}

QStringList JkfToSfenConverter::convertFile(const QString& jkfPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(jkfPath, result, errorMessage) ? result.mainline.usiMoves : QStringList();
}

QList<KifDisplayItem> JkfToSfenConverter::extractMovesWithTimes(const QString& jkfPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(jkfPath, result, errorMessage) ? result.mainline.disp : QList<KifDisplayItem>();
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

    QString detectedLabel;
    out.mainline.baseSfen = buildInitialSfen(root, &detectedLabel);
    out.mainline.startPly = 1;
    if (out.mainline.baseSfen.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: invalid initial position");
        return false;
    }

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return false;
    }

    const QJsonArray movesArray = root[QStringLiteral("moves")].toArray();
    if (parseMovesArray(movesArray, out.mainline.baseSfen, out.mainline, out.variations, errorMessage)) return true;
    out = KifParseResult{};
    return false;
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

    const QJsonObject header = root[QStringLiteral("header")].toObject();
    const QStringList keys = header.keys();

    for (const QString& key : std::as_const(keys)) {
        const QString value = header[key].toString();
        ordered.append({key, value});
    }

    return ordered;
}

QMap<QString, QString> JkfToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    return KifuParseCommon::toGameInfoMap(extractGameInfo(filePath));
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
    if (!root.value(QStringLiteral("moves")).isArray()) {
        if (warn) *warn = QStringLiteral("JKF: 'moves' must be an array");
        return false;
    }
    return true;
}

QString JkfToSfenConverter::buildInitialSfen(const QJsonObject& root, QString* detectedLabel)
{
    if (!root.contains(QStringLiteral("initial"))) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"));
    }

    const QJsonObject initial = root[QStringLiteral("initial")].toObject();
    const QString preset = initial[QStringLiteral("preset")].toString();

    if (preset == QStringLiteral("OTHER")) {
        if (initial.contains(QStringLiteral("data"))) {
            if (detectedLabel) *detectedLabel = QStringLiteral("任意局面");
            const QJsonObject data = initial[QStringLiteral("data")].toObject();
            return JkfMoveParser::buildSfenFromInitialData(data);
        }
        return {};
    }

    if (detectedLabel) {
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

    const QString sfen = JkfMoveParser::presetToSfen(preset);
    if (preset != QLatin1String("HIRATE") && sfen == JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"))) return {};
    return sfen;
}

namespace {
// 分岐にはその手を指す前の局面・移動先・累計時間を引き継ぐ。
bool parseJkfLine(const QJsonArray& moves, const QString& baseSfen, int firstPly,
                  int prevToX, int prevToY, std::array<qint64, 2> cumSec,
                  bool isMainline, KifLine& line, QList<KifVariation>& variations, QString* warn)
{
    line.baseSfen = baseSfen;
    line.startPly = firstPly;
    line.sfenList = {baseSfen};
    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(baseSfen);
    int ply = firstPly;
    if (isMainline) line.disp.append(KifuParseCommon::createOpeningDisplayItem({}, {}));

    for (const auto& value : moves) {
        if (!value.isObject()) {
            appendWarning(warn, QStringLiteral("JKF: move entry must be an object"));
            return false;
        }
        const QJsonObject obj = value.toObject();
        if (obj.contains(QStringLiteral("forks")) && !obj[QStringLiteral("forks")].isArray()) {
            appendWarning(warn, QStringLiteral("JKF: forks must be an array"));
            return false;
        }
        const QJsonArray forks = obj[QStringLiteral("forks")].toArray();
        for (const auto& fork : forks) {
            if (!fork.isArray()) {
                appendWarning(warn, QStringLiteral("JKF: fork must be an array of moves"));
                return false;
            }
            KifVariation variation;
            variation.startPly = ply;
            QList<KifVariation> nested;
            if (!parseJkfLine(fork.toArray(), tracer.toSfenString(), ply, prevToX, prevToY,
                              cumSec, false, variation.line, nested, warn)) return false;
            variations.append(variation);
            variations.append(nested);
        }

        const QString comment = JkfMoveParser::extractCommentsFromMoveObj(obj);
        if (obj.contains(QStringLiteral("special"))) {
            const QString special = obj[QStringLiteral("special")].toString();
            QString label = JkfMoveParser::specialToJapanese(special);
            const bool blackToMove = tracer.toSfenString().contains(QStringLiteral(" b "));
            if ((special == QLatin1String("+ILLEGAL_ACTION") && !blackToMove)
                || (special == QLatin1String("-ILLEGAL_ACTION") && blackToMove)) label = QStringLiteral("反則勝ち");
            auto item = KifuParseCommon::createTerminalDisplayItem(ply, label);
            item.prettyMove = (blackToMove ? QStringLiteral("▲") : QStringLiteral("△")) + label;
            item.comment = comment;
            if (obj.contains(QStringLiteral("time"))) {
                item.timeText = JkfMoveParser::formatTimeText(obj[QStringLiteral("time")].toObject(), cumSec[blackToMove ? 0 : 1]);
            }
            line.disp.append(item);
            line.endsWithTerminal = true;
            break;
        }
        if (!obj.contains(QStringLiteral("move"))) {
            if (isMainline && ply == firstPly) line.disp[0].comment = comment;
            continue;
        }

        const QJsonObject move = obj[QStringLiteral("move")].toObject();
        const QString usi = JkfMoveParser::convertMoveToUsi(move, prevToX, prevToY);
        if (usi.isEmpty()) {
            appendWarning(warn, QStringLiteral("JKF: invalid move at ply %1").arg(ply));
            return false;
        }
        const QString pretty = JkfMoveParser::convertMoveToPretty(move, ply, prevToX, prevToY);
        QString time;
        if (obj.contains(QStringLiteral("time"))) {
            const int color = normalizedColorIndex(move, ply, warn);
            time = JkfMoveParser::formatTimeText(obj[QStringLiteral("time")].toObject(), cumSec[static_cast<size_t>(color)]);
        }
        auto item = KifuParseCommon::createMoveDisplayItem(ply, pretty, time);
        item.comment = comment;
        line.disp.append(item);
        if (!usi.isEmpty()) {
            line.usiMoves.append(usi);
            (void)tracer.applyUsiMove(usi);
            line.sfenList.append(tracer.toSfenString());
        }
        ++ply;
    }
    return true;
}
} // namespace

bool JkfToSfenConverter::parseMovesArray(const QJsonArray& movesArray,
                                        const QString& baseSfen, KifLine& mainline,
                                        QList<KifVariation>& variations, QString* warn)
{
    return parseJkfLine(movesArray, baseSfen, 1, 0, 0, {0, 0}, true, mainline, variations, warn);
}
