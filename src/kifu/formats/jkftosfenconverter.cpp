/// @file jkftosfenconverter.cpp
/// @brief JKF形式棋譜コンバータクラスの実装
///
/// 指し手変換・初期局面構築は JkfMoveParser に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "jkftosfenconverter.h"
#include "jkfmoveparser.h"
#include "parsecommon.h"

#include <QFile>
#include <QJsonDocument>

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

        if (moveObj.contains(QStringLiteral("special"))) {
            break;
        }

        if (moveObj.contains(QStringLiteral("move"))) {
            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();
            const QString usi = JkfMoveParser::convertMoveToUsi(mv, prevToX, prevToY);
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
    qint64 cumSec[2] = {0, 0};
    QString pendingComment;

    KifDisplayItem openingItem;
    openingItem.prettyMove = QString();
    openingItem.timeText = QStringLiteral("00:00/00:00:00");
    openingItem.ply = 0;

    for (qsizetype i = 0; i < moves.size(); ++i) {
        const QJsonObject moveObj = moves[i].toObject();

        const QString comment = JkfMoveParser::extractCommentsFromMoveObj(moveObj);

        // 終局語
        if (moveObj.contains(QStringLiteral("special"))) {
            if (out.isEmpty()) {
                openingItem.comment = pendingComment;
                out.append(openingItem);
            } else if (!pendingComment.isEmpty() && out.size() > 1) {
                QString& dst = out.last().comment;
                if (!dst.isEmpty()) dst += QLatin1Char('\n');
                dst += pendingComment;
            }
            pendingComment.clear();

            const QString special = moveObj[QStringLiteral("special")].toString();
            const QString label = JkfMoveParser::specialToJapanese(special);
            const QString teban = ((plyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

            KifDisplayItem item;
            item.prettyMove = teban + label;
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = comment;
            item.ply = plyNumber + 1;
            out.append(item);
            break;
        }

        // move フィールド
        if (moveObj.contains(QStringLiteral("move"))) {
            if (out.isEmpty()) {
                openingItem.comment = pendingComment;
                out.append(openingItem);
                pendingComment.clear();
            } else if (!pendingComment.isEmpty()) {
                QString& dst = out.last().comment;
                if (!dst.isEmpty()) dst += QLatin1Char('\n');
                dst += pendingComment;
                pendingComment.clear();
            }

            ++plyNumber;

            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();
            const QString pretty = JkfMoveParser::convertMoveToPretty(mv, plyNumber, prevToX, prevToY);

            QString timeText = QStringLiteral("00:00/00:00:00");
            if (moveObj.contains(QStringLiteral("time"))) {
                const QJsonObject timeObj = moveObj[QStringLiteral("time")].toObject();
                const int color = mv[QStringLiteral("color")].toInt();
                timeText = JkfMoveParser::formatTimeText(timeObj, cumSec[color]);
            }

            KifDisplayItem item;
            item.prettyMove = pretty;
            item.timeText = timeText;
            item.comment = comment;
            item.ply = plyNumber;
            out.append(item);
        } else {
            if (!comment.isEmpty()) {
                if (out.isEmpty()) {
                    openingItem.comment = comment;
                } else {
                    if (!pendingComment.isEmpty()) pendingComment += QLatin1Char('\n');
                    pendingComment += comment;
                }
            }
        }
    }

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

    QString detectedLabel;
    out.mainline.baseSfen = buildInitialSfen(root, &detectedLabel);
    out.mainline.startPly = 1;

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return false;
    }

    const QJsonArray movesArray = root[QStringLiteral("moves")].toArray();
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

    return JkfMoveParser::presetToSfen(preset);
}

void JkfToSfenConverter::parseMovesArray(const QJsonArray& movesArray,
                                          const QString& /*baseSfen*/,
                                          KifLine& mainline,
                                          QVector<KifVariation>& variations,
                                          QString* /*warn*/)
{
    int prevToX = 0, prevToY = 0;
    int plyNumber = 0;
    qint64 cumSec[2] = {0, 0};
    bool openingEntryAdded = false;

    KifDisplayItem openingItem;
    openingItem.prettyMove = QString();
    openingItem.timeText = QStringLiteral("00:00/00:00:00");
    openingItem.ply = 0;

    for (qsizetype i = 0; i < movesArray.size(); ++i) {
        const QJsonObject moveObj = movesArray[i].toObject();

        // 終局語
        if (moveObj.contains(QStringLiteral("special"))) {
            if (!openingEntryAdded) {
                mainline.disp.append(openingItem);
                openingEntryAdded = true;
            }

            const QString special = moveObj[QStringLiteral("special")].toString();
            const QString label = JkfMoveParser::specialToJapanese(special);
            const QString teban = ((plyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
            const QString comment = JkfMoveParser::extractCommentsFromMoveObj(moveObj);

            KifDisplayItem item;
            item.prettyMove = teban + label;
            item.timeText = QStringLiteral("00:00/00:00:00");
            item.comment = comment;
            item.ply = plyNumber + 1;
            mainline.disp.append(item);
            mainline.endsWithTerminal = true;
            break;
        }

        // move フィールド
        if (moveObj.contains(QStringLiteral("move"))) {
            if (!openingEntryAdded) {
                mainline.disp.append(openingItem);
                openingEntryAdded = true;
            }

            ++plyNumber;

            const QJsonObject mv = moveObj[QStringLiteral("move")].toObject();

            const QString usi = JkfMoveParser::convertMoveToUsi(mv, prevToX, prevToY);
            if (!usi.isEmpty()) {
                mainline.usiMoves.append(usi);
            }

            const QString pretty = JkfMoveParser::convertMoveToPretty(mv, plyNumber, prevToX, prevToY);

            QString timeText = QStringLiteral("00:00/00:00:00");
            if (moveObj.contains(QStringLiteral("time"))) {
                const QJsonObject timeObj = moveObj[QStringLiteral("time")].toObject();
                const int color = mv[QStringLiteral("color")].toInt();
                timeText = JkfMoveParser::formatTimeText(timeObj, cumSec[color]);
            }

            const QString comment = JkfMoveParser::extractCommentsFromMoveObj(moveObj);

            KifDisplayItem item;
            item.prettyMove = pretty;
            item.timeText = timeText;
            item.comment = comment;
            item.ply = plyNumber;
            mainline.disp.append(item);
        } else if (i == 0) {
            openingItem.comment = JkfMoveParser::extractCommentsFromMoveObj(moveObj);
        }

        // 分岐 (forks)
        if (moveObj.contains(QStringLiteral("forks"))) {
            const QJsonArray forks = moveObj[QStringLiteral("forks")].toArray();

            for (qsizetype f = 0; f < forks.size(); ++f) {
                const QJsonArray forkMoves = forks[f].toArray();

                KifVariation var;
                var.startPly = plyNumber;

                int forkPrevToX = 0, forkPrevToY = 0;
                int forkPlyNumber = plyNumber - 1;
                qint64 forkCumSec[2] = {cumSec[0], cumSec[1]};

                for (qsizetype j = 0; j < forkMoves.size(); ++j) {
                    const QJsonObject forkMoveObj = forkMoves[j].toObject();

                    if (forkMoveObj.contains(QStringLiteral("special"))) {
                        const QString special = forkMoveObj[QStringLiteral("special")].toString();
                        const QString label = JkfMoveParser::specialToJapanese(special);
                        const QString teban = ((forkPlyNumber + 1) % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
                        const QString forkComment = JkfMoveParser::extractCommentsFromMoveObj(forkMoveObj);

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

                        const QString usi = JkfMoveParser::convertMoveToUsi(forkMv, forkPrevToX, forkPrevToY);
                        if (!usi.isEmpty()) {
                            var.line.usiMoves.append(usi);
                        }

                        const QString pretty = JkfMoveParser::convertMoveToPretty(forkMv, forkPlyNumber, forkPrevToX, forkPrevToY);

                        QString forkTimeText = QStringLiteral("00:00/00:00:00");
                        if (forkMoveObj.contains(QStringLiteral("time"))) {
                            const QJsonObject timeObj = forkMoveObj[QStringLiteral("time")].toObject();
                            const int color = forkMv[QStringLiteral("color")].toInt();
                            forkTimeText = JkfMoveParser::formatTimeText(timeObj, forkCumSec[color]);
                        }

                        const QString forkComment = JkfMoveParser::extractCommentsFromMoveObj(forkMoveObj);

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

    if (!openingEntryAdded) {
        mainline.disp.prepend(openingItem);
    }
}
