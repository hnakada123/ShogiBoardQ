#ifndef KI2TOSFENCONVERTER_H
#define KI2TOSFENCONVERTER_H

/// @file ki2tosfenconverter.h
/// @brief KI2形式棋譜コンバータクラスの定義

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief KI2形式の棋譜を解析し、USI形式やGUI用データに変換するクラス
 *
 * KI2形式はKIF形式と異なり、移動元座標が省略されているため、
 * 局面（ShogiBoard）を追いながら、合法手の中から指し手を決める。
 * 字句解析は Ki2Lexer、指し手の決定は Ki2MoveNotation に委譲する。
 */
class Ki2ToSfenConverter
{
public:
    // テキスト（貼り付けた棋譜など）
    static QString detectInitialSfenFromText(const QString& text, QString* detectedLabel = nullptr);
    [[nodiscard]] static bool parseText(const QString& text, KifParseResult& out, QString* errorMessage = nullptr);
    /// 行に分けた棋譜を読む。detectedLabel には手合割の名前（局面図なら "BOD"）を返す
    [[nodiscard]] static bool parseLines(const QStringList& lines, KifParseResult& out, QString* errorMessage = nullptr,
                                         QString* detectedLabel = nullptr);
    static QList<KifGameInfoItem> extractGameInfoFromText(const QString& text);
    static QList<KifGameInfoItem> extractGameInfoFromLines(const QStringList& lines);

    static QString mapHandicapToSfen(const QString& label);
    [[nodiscard]] static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);
};

#endif // KI2TOSFENCONVERTER_H
