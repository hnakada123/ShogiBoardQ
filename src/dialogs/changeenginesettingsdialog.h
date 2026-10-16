#ifndef CHANGEENGINESETTINGSDIALOG_H
#define CHANGEENGINESETTINGSDIALOG_H

/// @file changeenginesettingsdialog.h
/// @brief エンジン設定変更ダイアログクラスの定義

#include <QDialog>
#include <memory>

class DialogFontScale;
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
    // コンストラクタ
    explicit ChangeEngineSettingsDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~ChangeEngineSettingsDialog() override;

    // 将棋エンジン名のsetter
    void setEngineName(const QString &engineName);

    // 将棋エンジン作者名のsetter
    void setEngineAuthor(const QString &engineAuthor);

    // "エンジン設定"ダイアログを作成する。
    void setupEngineOptionsDialog();

private:
    // UIコンポーネントのポインタを保持するためのポインタ変数
    std::unique_ptr<Ui::ChangeEngineSettingsDialog> ui;

    // オプション管理ハンドラ
    std::unique_ptr<EngineSettingsOptionHandler> m_optionHandler;

    // 文字の大きさ（A-/A+。オプションの部品を作ったときに付ける。QObject parent 所有）
    DialogFontScale* m_fontScale = nullptr;

    QCheckBox* m_ponderEnabled = nullptr;
    QCheckBox* m_sendUnreportedPonder = nullptr;
    bool m_defaultPonderEnabled = false;

    void createPonderWidgets(QVBoxLayout* layout);

    // エンジンオプションに基づいてUIコンポーネントを作成して配置する。
    void createOptionWidgets();

private slots:
    void saveEngineSettings();
    void restoreDefaultSettings();

    // フォントサイズを増加する。

    // フォントサイズを減少する。
};

#endif // CHANGEENGINESETTINGSDIALOG_H
