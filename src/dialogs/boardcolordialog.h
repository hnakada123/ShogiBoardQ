#ifndef BOARDCOLORDIALOG_H
#define BOARDCOLORDIALOG_H

#include "boardcolors.h"
#include "boardcolorpresets.h"
#include <QDialog>
#include <array>

class QColorDialog;
class QPushButton;
class QComboBox;
class QLabel;
class QIcon;

/// 4か所の色を個別に選択し、確定した色を即時反映する。
class BoardColorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BoardColorDialog(QWidget* parent = nullptr);
    ~BoardColorDialog() override;

private slots:
    void chooseColor();
    void applyColor(const QColor& color);
    void refreshButtons();
    void restoreDefaults();
    void savePickerSize();
    void rebuildPresets();
    void applyPreset(int index);

private:
    struct ColorField {
        QPushButton* button;
        QColor BoardColors::* member;
        QString label;
    };
    std::array<ColorField, 4> m_fields;
    QColorDialog* m_picker;
    QColor BoardColors::* m_selectedMember = nullptr;
    QLabel* m_presetLabel;
    QComboBox* m_presetCombo;
    QList<BoardColorPreset> m_presets;
    void syncPresetSelection();
    QIcon presetIcon(const BoardColors& colors) const;
};

#endif // BOARDCOLORDIALOG_H
