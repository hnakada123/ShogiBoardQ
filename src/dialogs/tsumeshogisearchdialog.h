#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

/// @file tsumeshogisearchdialog.h
/// @brief 詰み探索専用の条件設定ダイアログ

#include <QDialog>
#include <memory>
#include <optional>

#include "enginelistsettings.h"
#include "settingsformbinding.h"

class EngineSelector;

namespace Ui {
class TsumeShogiSearchDialog;
}

/**
 * @brief 詰み探索の条件（エンジンと探索の制限時間）を選ぶダイアログ
 *
 * 探索の制限時間の欄とラジオの組（時間を指定・時間無制限）は SettingsFormBinding で設定と結び、
 * 「探索開始」（accept）でエンジンの選択と一緒に保存する。
 */
class TsumeShogiSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TsumeShogiSearchDialog(QWidget *parent = nullptr);
    ~TsumeShogiSearchDialog() override;

    /// 選んでいるエンジン（未登録なら std::nullopt）
    std::optional<EngineEntry> engine() const;
    /// 探索の制限時間（ms。0 は無制限。AnalysisOptions::byoyomiMs にそのまま渡す）
    int timeLimitMs() const;

public slots:
    /// エンジンを選んでいれば、エンジンの選択と探索の制限時間を保存して閉じる（選んでいなければ閉じない）
    void accept() override;

private slots:
    void showEngineSettingsDialog(const EngineEntry& engine);
    void updateEngineSelection();

private:
    std::unique_ptr<Ui::TsumeShogiSearchDialog> ui;
    /// エンジンの選択（コンボ・設定ボタン。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;
    /// 探索の制限時間の欄・ラジオの組と設定（「探索開始」で保存する）
    SettingsFormBinding m_form;

    bool hasValidEngine() const;
};

#endif // TSUMESHOGISEARCHDIALOG_H
