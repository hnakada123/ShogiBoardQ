/// @file promotedialog.cpp
/// @brief 成り確認ダイアログクラスの実装

#include "promotedialog.h"
#include "dialogfontscale.h"
#include <QPushButton>

#include "ui_promotedialog.h"

// 成る・不成の選択ダイアログを表示するクラス
PromoteDialog::PromoteDialog(QWidget *parent) : QDialog(parent), ui(std::make_unique<Ui::PromoteDialog>())
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("成る"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("成らない"));
    DialogFontScale::install(this, QStringLiteral("promotion"), true);
}

PromoteDialog::~PromoteDialog() = default;
