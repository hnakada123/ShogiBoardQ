#ifndef NUMERICRIGHTALIGNCOMMADELEGATE_H
#define NUMERICRIGHTALIGNCOMMADELEGATE_H

/// @file numericrightaligncommadelegate.h
/// @brief 数値右寄せカンマ区切りデリゲートクラスの定義


#include "itemtextdelegate.h"

#include <QLocale>

/// 数値を3桁区切りにして右寄せで描く。文字は配置を覚えて描く（ItemTextDelegate。見た目は Qt と同じ）
class NumericRightAlignCommaDelegate : public ItemTextDelegate
{
    Q_OBJECT
public:
    using ItemTextDelegate::ItemTextDelegate;

    // 数値は 3桁区切り（,）に整形。非数値はそのまま。
    QString displayText(const QVariant& value, const QLocale& locale) const override
    {
        (void)locale;
        if (!value.isValid()) return {};

        // まず整数を優先
        bool ok = false;
        qlonglong iv = value.toLongLong(&ok);
        if (ok) {
            return m_enUS.toString(iv); // 3桁区切りに , を使う
        }

        // 次に符号なし整数
        qulonglong uiv = value.toULongLong(&ok);
        if (ok) {
            return m_enUS.toString(uiv);
        }

        // 必要なら double も（整数に丸められてしまうのを避ける）
        double dv = value.toDouble(&ok);
        if (ok) {
            // 桁区切り＋既定精度。必要なら 'f' と桁数を指定してもOK
            return m_enUS.toString(dv);
        }

        // 数値以外（例: "mate 1"）はそのまま
        return QStyledItemDelegate::displayText(value, QLocale::c());
    }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter; // 常に右寄せ
    }

private:
    /// 3桁区切りに , を使うロケール。セルを描くたび（info 行ごと）に作らず、デリゲートが持つ
    /// （static にすると終了時のデストラクタになるので、メンバにする）
    const QLocale m_enUS{QLocale::English, QLocale::UnitedStates};
};

#endif // NUMERICRIGHTALIGNCOMMADELEGATE_H
