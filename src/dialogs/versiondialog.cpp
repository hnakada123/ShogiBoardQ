/// @file versiondialog.cpp
/// @brief バージョンダイアログクラスの実装

#include <QPixmap>
#include "versiondialog.h"
#include "ui_versiondialog.h"

// バージョンを表示するダイアログ
VersionDialog::VersionDialog(QWidget *parent) : QDialog(parent), ui(new Ui::VersionDialog)
{
    ui->setupUi(this);

    QPixmap icon(":/icons/shogiboardq.png");
    ui->iconLabel->setPixmap(icon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    ui->versionLabel->setText(QStringLiteral("Version ") + QStringLiteral(APP_VERSION));
    ui->buildDateLabel->setText(tr("ビルド日時: %1 %2").arg(__DATE__, __TIME__));
}

VersionDialog::~VersionDialog()
{
    delete ui;
}
