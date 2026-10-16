#ifndef ENGINESELECTOR_H
#define ENGINESELECTOR_H

/// @file engineselector.h
/// @brief 登録エンジンを選ぶ部品（コンボ・「設定」ボタン・未登録の案内・選択の保存）

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <optional>

#include "enginelistsettings.h"

class QAbstractButton;
class QComboBox;
class QLabel;

/**
 * @brief 登録エンジンを選ぶ部品
 *
 * 呼び出し側が置いたコンボ（.ui の部品でもよい）・「設定」ボタン・未登録の案内に、登録エンジンの一覧の読み込み、
 * 前回選んだエンジンの復元と保存、未登録・未選択のときの操作の無効化を受け持つ。
 * エンジンの設定ダイアログは dialogs にあるので（widgets からは include できない）、settingsRequested で頼む。
 */
class EngineSelector : public QObject
{
    Q_OBJECT

public:
    using Engine = EngineListSettings::EngineEntry;

    /// 結び付ける部品（コンボは必須。ほかは無ければ nullptr）
    struct Widgets {
        QComboBox* combo = nullptr;
        QAbstractButton* settingsButton = nullptr;  ///< 選んだエンジンの設定
        QLabel* hint = nullptr;                     ///< エンジンが無いときだけ出す案内
    };

    struct Options {
        /// 選択の保存先（name が nullptr なら保存しない）
        EngineListSettings::SelectionKeys keys;
        /// 空でなければ、先頭に内蔵エンジン（登録エンジン以外）の項目をこの表示名で置く
        QString builtinLabel;
        /// 一覧に出す登録エンジン（空なら全部）
        std::function<bool(const EngineListSettings::EngineEntry&)> filter;
    };

    /// 一覧を読み、保存した選択を戻す（保存が無ければ先頭の登録エンジン）
    EngineSelector(const Widgets& widgets, const Options& options, QObject* parent);

    /// 一覧を読み直す（選んでいたエンジンは保つ。エンジンの登録・削除の後）
    void reload();

    /// 一覧に出している登録エンジン（内蔵の項目は含まない）
    const QList<Engine>& engines() const { return m_engines; }
    bool hasEngines() const { return !m_engines.isEmpty(); }
    /// 選んだ登録エンジン（内蔵の項目を選んだとき・何も選んでいないときは std::nullopt）
    std::optional<Engine> selected() const;
    /// 内蔵エンジンの項目を選んでいる
    bool isBuiltinSelected() const;

    /// 名前で選ぶ（一覧に無ければ変えない）
    void select(const QString& name);
    /// 選んだエンジンを保存する（内蔵の項目なら空の名前）
    void saveSelection() const;
    /// コンボと「設定」ボタンを操作できるか（対局の実行中などに止める）
    void setEnabled(bool enabled);

signals:
    /// 選んだエンジンが変わった
    void selectionChanged();
    /// 「設定」が押された（選んだ登録エンジン。設定ダイアログは受け手が開く）
    void settingsRequested(const EngineListSettings::EngineEntry& engine);

private slots:
    void onSettingsClicked();
    void onCurrentIndexChanged();

private:
    /// 登録エンジンの一覧を読み、絞り込む
    void loadList();
    /// コンボを作り直し、selectName（builtin なら内蔵の項目）を選ぶ
    void populate(const QString& selectName, bool builtin);
    void restoreSaved();
    void updateState();
    /// コンボの位置から一覧の位置（内蔵の項目は -1）
    int engineIndexAt(int comboIndex) const;

    QPointer<QComboBox> m_combo;
    QPointer<QAbstractButton> m_settingsButton;
    QPointer<QLabel> m_hint;
    Options m_options;
    QList<Engine> m_engines;
    bool m_enabled = true;
};

#endif // ENGINESELECTOR_H
