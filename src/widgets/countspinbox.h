#ifndef COUNTSPINBOX_H
#define COUNTSPINBOX_H

/// @file countspinbox.h
/// @brief 値が ±1 のときに単数形の単位を付けるスピンボックスクラスの定義

#include <QSpinBox>

/// 値が ±1 のときは単数形の単位を付ける（英語の「1 piece」と「2 pieces」）。
/// %n の複数形は翻訳ファイルの検査で扱えないため、単数形を別の訳として持つ。
/// 単位は valueChanged で切り替えるので、シグナルを止めて setValue しないこと。
class CountSpinBox : public QSpinBox
{
    Q_OBJECT

public:
    CountSpinBox(QString one, QString many, QWidget* parent = nullptr);

private:
    void updateSuffix(int value);

    QString m_one;
    QString m_many;
};

#endif // COUNTSPINBOX_H
