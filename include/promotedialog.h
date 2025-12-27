#ifndef PROMOTEDIALOG_H
#define PROMOTEDIALOG_H

#include <QDialog>

namespace Ui {
class PromoteDialog;
}

// 成る・不成の選択ダイアログを表示するクラス
class PromoteDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit PromoteDialog(QWidget* parent = nullptr);

    // デストラクタ
    ~PromoteDialog() override;

private:
    Ui::PromoteDialog* ui;
};

#endif // PROMOTEDIALOG_H
