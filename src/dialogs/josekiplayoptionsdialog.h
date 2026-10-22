#ifndef JOSEKIPLAYOPTIONSDIALOG_H
#define JOSEKIPLAYOPTIONSDIALOG_H

/// @file josekiplayoptionsdialog.h
/// @brief 対局でエンジンが定跡を使う条件のダイアログ（対局開始ダイアログの「条件…」）

#include <QDialog>

#include "josekiplayoptions.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

/**
 * @brief 定跡を使う条件を選ぶダイアログ
 *
 * 手数・選び方・評価値の差・評価値の下限・深さの下限・出現頻度の比率と回数の下限を選ぶ。
 * 「定跡を使う」かどうか（JosekiPlayOptions::enabled）は対局開始ダイアログのチェックで決めるので、ここでは変えない。
 */
class JosekiPlayOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    /// title はダイアログの題名（例「先手の定跡の条件」）
    JosekiPlayOptionsDialog(const JosekiPlayOptions &options, const QString &title, QWidget *parent = nullptr);
    ~JosekiPlayOptionsDialog() override;

    /// 選んだ条件（enabled は渡されたまま）
    JosekiPlayOptions options() const;

private slots:
    /// 選び方と評価値の下限のチェックに合わせて、使わない欄を押せなくする
    void updateEnabledFields();
    /// 既定値に戻す
    void resetToDefaults();

private:
    void setOptions(const JosekiPlayOptions &options);

    bool m_enabled = false;
    QSpinBox *m_maxPly = nullptr;
    QComboBox *m_method = nullptr;
    QSpinBox *m_valueDiff = nullptr;
    QCheckBox *m_useMinValue = nullptr;
    QSpinBox *m_minValue = nullptr;
    QSpinBox *m_minDepth = nullptr;
    QDoubleSpinBox *m_minRatio = nullptr;
    QSpinBox *m_minFrequency = nullptr;
};

#endif // JOSEKIPLAYOPTIONSDIALOG_H
