#ifndef ENGINEOPTION_H
#define ENGINEOPTION_H

/// @file engineoption.h
/// @brief USIエンジンオプション構造体の定義


#include <QString>
#include <QStringList>

/**
 * @brief USIエンジンの個別オプション（エンジンが報告した定義と、保存した値）
 *
 * "option name <name> type <type> [default/min/max/var ...]" を UsiLineParser::parseOption が読んでこの型にし
 * （currentValue は既定値）、登録・設定の保存（EngineProfileStore）・設定ダイアログで同じ型を使う。
 */
struct EngineOption
{
    QString name;           ///< オプション名
    QString type;           ///< オプションの型（spin, check, combo, button, filename, string）
    QString defaultValue;   ///< デフォルト値
    QString min;            ///< 最小値（spin型の場合）
    QString max;            ///< 最大値（spin型の場合）
    QString currentValue;   ///< 現在の値
    QStringList valueList;  ///< 選択肢リスト（combo型の場合）
};

#endif // ENGINEOPTION_H
