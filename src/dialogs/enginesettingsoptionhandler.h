#ifndef ENGINESETTINGSOPTIONHANDLER_H
#define ENGINESETTINGSOPTIONHANDLER_H

/// @file enginesettingsoptionhandler.h
/// @brief エンジン設定オプションのウィジェット生成・データ管理ハンドラ

#include <QObject>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QList>

#include "longlongspinbox.h"
#include "engineoptions.h"
#include "engineoptiondescriptions.h"
#include "collapsiblegroupbox.h"

namespace EngineSettings {
// ファイルまたはディレクトリ選択のためのEnum型の定義
enum class FileType {
    File = 0,
    Directory = 1
};
}

/// エンジン設定オプションのウィジェット生成・データ管理を担当するハンドラ
class EngineSettingsOptionHandler : public QObject
{
    Q_OBJECT

public:
    explicit EngineSettingsOptionHandler(QWidget* parentWidget, QObject* parent = nullptr);

    void setEngineName(const QString& engineName) { m_engineName = engineName; }
    void setEngineAuthor(const QString& engineAuthor) { m_engineAuthor = engineAuthor; }
    const QString& engineName() const { return m_engineName; }
    const QString& engineAuthor() const { return m_engineAuthor; }

    // 設定ファイルからエンジンオプションを読み込む
    void readEngineOptions();

    // オプションウィジェットを生成してレイアウトに追加する
    void buildOptionWidgets(QVBoxLayout* layout);

    // エンジンオプションを既定値に戻す
    void restoreDefaultOptions();

    // ウィジェットの現在値を読み取り設定ファイルに保存する
    void writeEngineOptions();

private:
    // オプションUIのためのウィジェットを格納する構造体
    struct OptionWidgets
    {
        QLineEdit* lineEdit = nullptr;
        QLabel* optionNameLabel = nullptr;
        QLabel* optionDescriptionLabel = nullptr;
        QLabel* helpDescriptionLabel = nullptr;
        LongLongSpinBox* integerSpinBox = nullptr;
        QCheckBox* optionCheckBox = nullptr;
        QPushButton* selectionButton = nullptr;
        QComboBox* comboBox = nullptr;
    };

    QWidget* m_parentWidget;
    QString m_engineName;
    QString m_engineAuthor;
    QList<EngineOption> m_optionList;
    OptionWidgets m_optionWidgets;
    QList<OptionWidgets> m_engineOptionWidgetsList;

    // 設定ファイルにエンジンのオプション設定を保存する
    void saveOptionsToSettings();

    // オプションのタイプに応じたUIコンポーネントを作成する
    void createWidgetForOption(const EngineOption& option, int optionIndex, QVBoxLayout* targetLayout);

    // カテゴリ別の折りたたみ可能なグループボックスを作成する
    CollapsibleGroupBox* createCategoryGroupBox(EngineOptionCategory category);

    // エンジンオプションのためのテキストボックスを作成する
    void createTextBox(const EngineOption& option, QVBoxLayout* layout, int optionIndex);

    // エンジンオプションのためのスピンボックスを作成する
    void createSpinBox(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのチェックボックスを作成する
    void createCheckBox(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのボタンを作成する
    void createButton(const EngineOption& option, QVBoxLayout* layout);

    // エンジンオプションのためのコンボボックスを作成する
    void createComboBox(const EngineOption& option, QVBoxLayout* layout);

    // オプションの説明ラベルを作成してレイアウトに追加する
    void createHelpDescriptionLabel(const QString& optionName, QVBoxLayout* layout);

    // 各ボタンが押されているかどうかを確認し、ボタンの色を設定する
    void changeStatusColorTypeButton();

    // イベントフィルター（スピンボックス・コンボボックスのホイール操作抑止）
    bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    // ファイル・ディレクトリ選択ダイアログを表示する
    void openFile();

    // ボタンが押された場合、ボタンの色を変える
    void changeColorTypeButton();
};

#endif // ENGINESETTINGSOPTIONHANDLER_H
