#ifndef ENGINEOPTIONS_H
#define ENGINEOPTIONS_H

/// @file engineoptions.h
/// @brief USIエンジンオプション構造体の定義


#include <QString>
#include <QStringList>

/**
 * @brief USIエンジンの個別オプションを保持する構造体
 *
 * USIプロトコルの "option" コマンドで報告されるオプション情報を格納する。
 *
 * @todo remove コメントスタイルガイド適用済み
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

#endif // ENGINEOPTIONS_H
