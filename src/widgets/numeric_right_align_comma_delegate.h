#ifndef NUMERIC_RIGHT_ALIGN_COMMA_DELEGATE_H
#define NUMERIC_RIGHT_ALIGN_COMMA_DELEGATE_H

#include <QStyledItemDelegate>
#include <QLocale>

class NumericRightAlignCommaDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // 数値は 3桁区切り（,）に整形。非数値はそのまま。
    QString displayText(const QVariant& value, const QLocale& locale) const override
    {
        (void)locale;
        if (!value.isValid()) return {};

        // QLocaleを毎回生成することでexit-time-destructorを回避
        const QLocale enUS(QLocale::English, QLocale::UnitedStates);

        // まず整数を優先
        bool ok = false;
        qlonglong iv = value.toLongLong(&ok);
        if (ok) {
            return enUS.toString(iv); // 3桁区切りに , を使う
        }

        // 次に符号なし整数
        qulonglong uiv = value.toULongLong(&ok);
        if (ok) {
            return enUS.toString(uiv);
        }

        // 必要なら double も（整数に丸められてしまうのを避ける）
        double dv = value.toDouble(&ok);
        if (ok) {
            // 桁区切り＋既定精度。必要なら 'f' と桁数を指定してもOK
            return enUS.toString(dv);
        }

        // 数値以外（例: "mate 1"）はそのまま
        return QStyledItemDelegate::displayText(value, QLocale::c());
    }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter; // 常に右寄せ
    }
};

#endif // NUMERIC_RIGHT_ALIGN_COMMA_DELEGATE_H
