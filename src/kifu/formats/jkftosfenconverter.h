#ifndef JKFTOSFENCONVERTER_H
#define JKFTOSFENCONVERTER_H

/// @file jkftosfenconverter.h
/// @brief JKF形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief JSON棋譜フォーマット(JKF)ファイルを解析し、GUIに必要なデータを取得するクラス
 *
 * JKF仕様: https://github.com/na2hiro/json-kifu-format
 *
 * 指し手変換・初期局面構築は JkfMoveParser 名前空間に委譲し、
 * 本クラスはファイルI/O・パーサ状態管理・オーケストレーションを担当する。
 */
class JkfToSfenConverter
{
public:
    static QString detectInitialSfenFromFile(const QString& jkfPath, QString* detectedLabel = nullptr);
    static QStringList convertFile(const QString& jkfPath, QString* errorMessage = nullptr);
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& jkfPath, QString* errorMessage = nullptr);
    [[nodiscard]] static bool parseWithVariations(const QString& jkfPath, KifParseResult& out, QString* errorMessage = nullptr);
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);
    static QString mapPresetToSfen(const QString& preset);

private:
    static bool loadJsonFile(const QString& filePath, QJsonObject& root, QString* warn);
    static QString buildInitialSfen(const QJsonObject& root, QString* detectedLabel = nullptr);
    static void parseMovesArray(const QJsonArray& movesArray,
                                const QString& baseSfen,
                                KifLine& mainline,
                                QVector<KifVariation>& variations,
                                QString* warn);
};

#endif // JKFTOSFENCONVERTER_H
