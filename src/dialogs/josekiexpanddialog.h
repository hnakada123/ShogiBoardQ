#ifndef JOSEKIEXPANDDIALOG_H
#define JOSEKIEXPANDDIALOG_H

/// @file josekiexpanddialog.h
/// @brief 定跡を棋譜の変化に展開する条件のダイアログ（定跡ウィンドウの「棋譜に展開...」）

#include <QDialog>

#include "josekilines.h"

class QDoubleSpinBox;
class QSpinBox;

/**
 * @brief 展開する手数・1局面の手の数・比率の下限を選ぶ（選んだ条件は保存して次に開いたときに戻す）
 */
class JosekiExpandDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JosekiExpandDialog(QWidget *parent = nullptr);
    ~JosekiExpandDialog() override;

    JosekiLines::ExpandOptions options() const;

    void done(int result) override;

private:
    QSpinBox *m_maxPlies = nullptr;
    QSpinBox *m_movesPerPosition = nullptr;
    QDoubleSpinBox *m_minRatio = nullptr;
};

#endif // JOSEKIEXPANDDIALOG_H
