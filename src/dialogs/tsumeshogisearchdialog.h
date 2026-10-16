#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

/// @file tsumeshogisearchdialog.h
/// @brief 詰み探索専用の条件設定ダイアログ

#include <QDialog>
#include <memory>
#include <optional>

#include "enginelistsettings.h"

class EngineSelector;

namespace Ui {
class TsumeShogiSearchDialog;
}

// 詰み探索ダイアログを表示する。
class TsumeShogiSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TsumeShogiSearchDialog(QWidget *parent = nullptr);
    ~TsumeShogiSearchDialog() override;

    /// 選んでいるエンジン（未登録なら std::nullopt）
    std::optional<EngineListSettings::EngineEntry> engine() const;
    int byoyomiSec() const;
    bool unlimitedTimeFlag() const;

public slots:
    void accept() override;
    void done(int result) override;

private slots:
    void showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine);
    void updateEngineSelection();

private:
    std::unique_ptr<Ui::TsumeShogiSearchDialog> ui;
    /// エンジンの選択（コンボ・設定ボタン。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;

    bool hasValidEngine() const;
};

#endif // TSUMESHOGISEARCHDIALOG_H
