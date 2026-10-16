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
#include <functional>

#include "longlongspinbox.h"
#include "engineoption.h"
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
    /// オプション1つの入力欄。型ごとの値の読み書きを持つ（型で分けるのは作るときだけ）
    struct OptionEditor {
        std::function<void(const QString&)> setValue;  ///< 値を入力欄に出す
        std::function<QString()> value;                ///< 入力欄の値（設定ファイルに書く形）
    };

    QWidget* m_parentWidget;
    QString m_engineName;
    QString m_engineAuthor;
    QList<EngineOption> m_optionList;
    /// m_optionList と同じ並びの入力欄（GUI 共通の USI_Ponder は空）
    QList<OptionEditor> m_editors;


    /// オプションの型に合った入力欄を作ってレイアウトに足す
    OptionEditor createEditor(const EngineOption& option, QVBoxLayout* layout);
    OptionEditor createTextBox(const EngineOption& option, QVBoxLayout* layout);
    OptionEditor createSpinBox(const EngineOption& option, QVBoxLayout* layout);
    OptionEditor createCheckBox(const EngineOption& option, QVBoxLayout* layout);
    OptionEditor createButton(const EngineOption& option, QVBoxLayout* layout);
    OptionEditor createComboBox(const EngineOption& option, QVBoxLayout* layout);

    // カテゴリ別の折りたたみ可能なグループボックスを作成する
    CollapsibleGroupBox* createCategoryGroupBox(EngineOptionCategory category);

    /// 太字のオプション名のラベルを足す
    QLabel* addNameLabel(const QString& text, QVBoxLayout* layout);
    // オプションの説明ラベルを作成してレイアウトに追加する
    void addHelpDescription(const QString& optionName, QVBoxLayout* layout);

    // イベントフィルター（スピンボックス・コンボボックスのホイール操作抑止）
    bool eventFilter(QObject* obj, QEvent* e) override;
};

#endif // ENGINESETTINGSOPTIONHANDLER_H
