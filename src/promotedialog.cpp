#include "promotedialog.h"

#include "ui_promotedialog.h"

// 成る・不成の選択ダイアログを表示するクラス
// コンストラクタ
PromoteDialog::PromoteDialog(QWidget *parent) : QDialog(parent), ui(new Ui::PromoteDialog)
{
    ui->setupUi(this);
}

// デストラクタ
PromoteDialog::~PromoteDialog()
{
    delete ui;
}
