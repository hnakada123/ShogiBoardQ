#ifndef VERSIONDIALOG_H
#define VERSIONDIALOG_H

/// @file versiondialog.h
/// @brief バージョンダイアログクラスの定義


#include <QDialog>

#include <memory>

class QUrl;

namespace Ui {
class VersionDialog;
}

// バージョンを表示するダイアログ
class VersionDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit VersionDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~VersionDialog() override;

private:
    // UI の言語に合わせたソース入手案内のファイル名
    static QString sourceCodeDocument();
    void showLicenseDocument(int index);
    // 文書中の licenses 内へのリンクを開く
    void openLicenseLink(const QUrl& url);
    std::unique_ptr<Ui::VersionDialog> ui;
};

#endif // VERSIONDIALOG_H
