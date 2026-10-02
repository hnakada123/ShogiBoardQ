#ifndef TSUMESHOGISEARCHDIALOG_H
#define TSUMESHOGISEARCHDIALOG_H

/// @file tsumeshogisearchdialog.h
/// @brief 詰み探索専用の条件設定ダイアログ

#include <QDialog>
#include <memory>

#include "enginelistsettings.h"
#include "fontsizehelper.h"

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
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void showEngineSettingsDialog();
    void updateEngineSelection();
    void onFontIncrease();
    void onFontDecrease();
    void updateMinimumSize();

private:
    std::unique_ptr<Ui::TsumeShogiSearchDialog> ui;
    QList<EngineListSettings::EngineEntry> m_engines;
    FontSizeHelper m_fontHelper;

    bool hasValidEngine() const;
    void applyFontSize();
};

#endif // TSUMESHOGISEARCHDIALOG_H
