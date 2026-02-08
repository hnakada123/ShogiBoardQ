/// @file versiondialog.cpp
/// @brief バージョンダイアログクラスの実装

#include "versiondialog.h"
#include "ui_versiondialog.h"

// バージョンを表示するダイアログ
VersionDialog::VersionDialog(QWidget *parent) : QDialog(parent), ui(new Ui::VersionDialog)
{
    ui->setupUi(this);
}

VersionDialog::~VersionDialog()
{
    delete ui;
}
