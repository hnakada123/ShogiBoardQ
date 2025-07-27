#ifndef ENGINEOPTIONS_H
#define ENGINEOPTIONS_H

#include <QString>
#include <QStringList>

// エンジンオプションの構造体
struct EngineOption
{
    QString name;           // オプション名
    QString type;           // オプションの型
    QString defaultValue;   // オプションのデフォルト値
    QString min;            // オプションの最小値
    QString max;            // オプションの最大値
    QString currentValue;   // オプションの現在の値
    QStringList valueList;  // combo型の場合の値のリスト
};

#endif // ENGINEOPTIONS_H
