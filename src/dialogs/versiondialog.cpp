#include "versiondialog.h"
#include "ui_versiondialog.h"

// バージョンを表示するダイアログ
// コンストラクタ
VersionDialog::VersionDialog(QWidget *parent) : QDialog(parent), ui(new Ui::VersionDialog)
{
    ui->setupUi(this);
}

// デストラクタ
VersionDialog::~VersionDialog()
{
    delete ui;
}
