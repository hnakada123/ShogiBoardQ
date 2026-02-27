/// @file promotedialog.cpp
/// @brief 成り確認ダイアログクラスの実装

#include "promotedialog.h"

#include "ui_promotedialog.h"

// 成る・不成の選択ダイアログを表示するクラス
PromoteDialog::PromoteDialog(QWidget *parent) : QDialog(parent), ui(std::make_unique<Ui::PromoteDialog>())
{
    ui->setupUi(this);
}

PromoteDialog::~PromoteDialog() = default;
