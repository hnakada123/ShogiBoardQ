#ifndef JKFTOSFENCONVERTER_H
#define JKFTOSFENCONVERTER_H

/// @file jkftosfenconverter.h
/// @brief JKF形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QList>
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
    static QString mapPresetToSfen(const QString& preset);

    // テキスト（貼り付けた棋譜など）
    static QString detectInitialSfenFromText(const QString& text, QString* detectedLabel = nullptr);
    [[nodiscard]] static bool parseText(const QString& text, KifParseResult& out, QString* errorMessage = nullptr);
    static QList<KifGameInfoItem> extractGameInfoFromText(const QString& text);

private:
    static bool parseJson(const QByteArray& data, QJsonObject& root, QString* warn);
    static bool parseRoot(const QJsonObject& root, KifParseResult& out, QString* errorMessage);
    static QList<KifGameInfoItem> gameInfoFromRoot(const QJsonObject& root);
    static QString buildInitialSfen(const QJsonObject& root, QString* detectedLabel = nullptr);
    static bool parseMovesArray(const QJsonArray& movesArray, const QString& baseSfen,
                                KifParseResult& out, QString* warn);
};

#endif // JKFTOSFENCONVERTER_H
