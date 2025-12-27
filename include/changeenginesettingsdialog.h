#ifndef CHANGEENGINESETTINGSDIALOG_H
#define CHANGEENGINESETTINGSDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QDir>
#include "longlongspinbox.h"
#include "engineoptions.h"

namespace EngineSettings {
// ファイルまたはディレクトリ選択のためのEnum型の定義
enum class FileType {
    File = 0,
    Directory = 1
};
}

namespace Ui {
class ChangeEngineSettingsDialog;
}

// 将棋エンジン設定を変更するダイアログ
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

    // "エンジン設定"ダイアログを作成する。
    void setupEngineOptionsDialog();

private:
    // UIコンポーネントのポインタを保持するためのポインタ変数
    Ui::ChangeEngineSettingsDialog* ui;

    // エンジン番号
    int m_engineNumber;

    // エンジン名
    QString m_engineName;

    // エンジンオプションのリスト
    QList<EngineOption> m_optionList;

    // エンジンオプションウィジェットのレイアウト
    QVBoxLayout* optionWidgetsLayout;

    // オプションUIのためのウィジェットを格納する構造体
    // 各メンバー変数は、デフォルトではnullptrに設定しておく。
    struct OptionWidgets
    {
        // テキスト入力用のLineEdit
        QLineEdit* lineEdit = nullptr;

        // オプション名を表示するためのLabel
        QLabel* optionNameLabel = nullptr;

        // オプションの説明や制約を表示するラベル
        QLabel* optionDescriptionLabel = nullptr;

        // 数値入力用のスピンボックス
        LongLongSpinBox* integerSpinBox = nullptr;

        // オン/オフを切り替えるためのチェックボックス
        QCheckBox* optionCheckBox = nullptr;

        // ファイルやディレクトリの選択ダイアログを開くボタン
        QPushButton* selectionButton = nullptr;

        // 複数の選択肢から選ぶためのコンボボックス。
        QComboBox* comboBox = nullptr;
    };

    // オプションUIのウィジェットを管理するためのOptionWidgets構造体のインスタンスを作成する。
    OptionWidgets m_optionWidgets;

    // エンジンオプションのためのウィジェットを格納するリスト
    QList<OptionWidgets> m_engineOptionWidgetsList;

    // 開くファイル名
    QString m_openFileName;

    // 設定ファイルから選択したエンジンのオプションを読み込む。
    void readEngineOptions();

    // エンジンオプションに基づいてUIコンポーネントを作成して配置する。
    void createOptionWidgets();

    // イベントフィルター関数。QObject型のobjとQEvent型のeを引数に取る。
    bool eventFilter(QObject *obj, QEvent *e) override;

    // ボタンが押された場合、ボタンの色を変える。
    void changeStatusColorTypeButton();

    // 一つのオプションを読み込む。
    EngineOption readOption(const QSettings& settings) const;

    // ファイルまたはディレクトリの選択ダイアログを開き、選択されたパスを返す。
    QString openSelectionDialog(QWidget* parent, int fileType, const QString& initialDir = QDir::homePath());

    // エンジンオプションのためのテキストボックスを作成する。
    void createTextBox(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのスピンボックスを作成する。
    void createSpinBox(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのチェックボックスを作成する。
    void createCheckBox(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのボタンを作成する。
    void createButton(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのコンボボックスを作成する。
    void createComboBox(const EngineOption& option, QVBoxLayout* layout);

    // オプションのタイプに応じたUIコンポーネントを作成し、レイアウトに追加する。
    void createWidgetForOption(const EngineOption& option);

    // 設定ファイルにエンジンのオプション設定を保存する。
    void saveOptionsToSettings();

private slots:
    // 設定画面で"フォルダ・ディレクトリの選択"ボタンあるいは"ファイルの選択"ボタンをクリックした場合にディレクトリ、ファイルを選択する画面を表示する。
    void openFile();

    // エンジンオプションを既定値に戻す。
    void restoreDefaultOptions();

    // 設定ファイルに追加エンジンのオプションを書き込む。
    void writeEngineOptions();

    // ボタンが押された場合、ボタンの色を変える。
    void changeColorTypeButton();
};

#endif // CHANGEENGINESETTINGSDIALOG_H
