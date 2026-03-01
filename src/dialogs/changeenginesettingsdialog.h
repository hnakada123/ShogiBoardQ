#ifndef CHANGEENGINESETTINGSDIALOG_H
#define CHANGEENGINESETTINGSDIALOG_H

/// @file changeenginesettingsdialog.h
/// @brief エンジン設定変更ダイアログクラスの定義

#include <QDialog>
#include <memory>
#include "fontsizehelper.h"

class EngineSettingsOptionHandler;

namespace Ui {
class ChangeEngineSettingsDialog;
}

/// 将棋エンジン設定を変更するダイアログ
class ChangeEngineSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit ChangeEngineSettingsDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~ChangeEngineSettingsDialog() override;

    // 将棋エンジン番号のsetter
    void setEngineNumber(const int &engineNumber);

    // 将棋エンジン名のsetter
    void setEngineName(const QString &engineName);

    // 将棋エンジン作者名のsetter
    void setEngineAuthor(const QString &engineAuthor);

    // "エンジン設定"ダイアログを作成する。
    void setupEngineOptionsDialog();

private:
    // UIコンポーネントのポインタを保持するためのポインタ変数
    std::unique_ptr<Ui::ChangeEngineSettingsDialog> ui;

    // エンジン番号
    int m_engineNumber = 0;

    // オプション管理ハンドラ
    std::unique_ptr<EngineSettingsOptionHandler> m_optionHandler;

    // フォントサイズヘルパー
    FontSizeHelper m_fontHelper;

    // エンジンオプションに基づいてUIコンポーネントを作成して配置する。
    void createOptionWidgets();

    // すべてのウィジェットにフォントサイズを適用する。
    void applyFontSize();

private slots:
    // フォントサイズを増加する。
    void increaseFontSize();

    // フォントサイズを減少する。
    void decreaseFontSize();
};

#endif // CHANGEENGINESETTINGSDIALOG_H
