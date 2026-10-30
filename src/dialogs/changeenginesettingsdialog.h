#ifndef CHANGEENGINESETTINGSDIALOG_H
#define CHANGEENGINESETTINGSDIALOG_H

/// @file changeenginesettingsdialog.h
/// @brief エンジン設定変更ダイアログクラスの定義

#include <QDialog>
#include <memory>

#include "registeredengines.h"

class EngineSettingsOptionHandler;
class QCheckBox;
class QVBoxLayout;

namespace Ui {
class ChangeEngineSettingsDialog;
}

/// 将棋エンジン設定を変更するダイアログ
class ChangeEngineSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// engine のオプションと先読みの設定を読み、変更する画面を作る（作者名は見出しに出す）
    explicit ChangeEngineSettingsDialog(const EngineEntry& engine, QWidget* parent = nullptr);

    // デストラクタ
    ~ChangeEngineSettingsDialog() override;

    /// engine の設定ダイアログを parent の上にモーダルで開き、閉じるまで待つ（OK なら保存する）。
    /// エンジンを選ぶ部品（EngineSelector）の「設定」を受けるダイアログ・部品はこれで開く
    static void showFor(const EngineEntry& engine, QWidget* parent);

private:
    // UIコンポーネントのポインタを保持するためのポインタ変数
    std::unique_ptr<Ui::ChangeEngineSettingsDialog> ui;

    // オプション管理ハンドラ
    std::unique_ptr<EngineSettingsOptionHandler> m_optionHandler;

    QCheckBox* m_ponderEnabled = nullptr;
    QCheckBox* m_sendUnreportedPonder = nullptr;
    bool m_defaultPonderEnabled = false;

    void createPonderWidgets(QVBoxLayout* layout);

    // エンジンオプションに基づいてUIコンポーネントを作成して配置する。
    void createOptionWidgets();

private slots:
    void saveEngineSettings();
    void restoreDefaultSettings();
};

#endif // CHANGEENGINESETTINGSDIALOG_H
