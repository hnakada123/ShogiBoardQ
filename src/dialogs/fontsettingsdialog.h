#ifndef FONTSETTINGSDIALOG_H
#define FONTSETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QFontComboBox;
class QLabel;

/// GUI共通の書体をプレビューし、確定時に保存・反映する。
class FontSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FontSettingsDialog(QWidget* parent = nullptr);

public slots:
    void accept() override;
    void done(int result) override;

private slots:
    void updatePreview();
    void restoreDefaults();

private:
    QCheckBox* m_useDefault;
    QFontComboBox* m_family;
    QLabel* m_preview;
};

#endif // FONTSETTINGSDIALOG_H
