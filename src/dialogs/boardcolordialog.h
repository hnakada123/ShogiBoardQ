#ifndef BOARDCOLORDIALOG_H
#define BOARDCOLORDIALOG_H

#include "boardcolors.h"
#include "boardcolorpresets.h"
#include <QDialog>

class QColorDialog;
class QPushButton;
class QComboBox;
class QLabel;
class QIcon;
class QFormLayout;
class QTabWidget;

/// 盤面と対局者情報の色を個別に選択し、確定した色を即時反映する。
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
    QList<ColorField> m_fields;
    QTabWidget* m_tabs;
    QColorDialog* m_picker;
    QColor BoardColors::* m_selectedMember = nullptr;
    QLabel* m_presetLabel;
    QComboBox* m_presetCombo;
    QList<BoardColorPreset> m_presets;
    void createColorPages();
    void addColorField(QFormLayout* form, BoardColors::Member member,
                       const QString& label, const QString& objectName);
    void syncPresetSelection();
    QIcon presetIcon(const BoardColors& colors) const;
};

#endif // BOARDCOLORDIALOG_H
