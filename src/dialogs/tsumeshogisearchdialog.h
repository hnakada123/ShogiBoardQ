#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

/// @file tsumeshogisearchdialog.h
/// @brief 詰み探索専用の条件設定ダイアログ

#include <QDialog>
#include <memory>

#include "enginelistsettings.h"

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

    const QList<EngineListSettings::EngineEntry>& engineList() const;
    int engineNumber() const;
    int byoyomiSec() const;
    bool unlimitedTimeFlag() const;

public slots:
    void accept() override;
    void done(int result) override;

protected:

private slots:
    void showEngineSettingsDialog();
    void updateEngineSelection();

private:
    std::unique_ptr<Ui::TsumeShogiSearchDialog> ui;
    QList<EngineListSettings::EngineEntry> m_engines;

    bool hasValidEngine() const;
};

#endif // TSUMESHOGISEARCHDIALOG_H
