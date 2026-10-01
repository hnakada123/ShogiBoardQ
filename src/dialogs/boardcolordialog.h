#ifndef BOARDCOLORDIALOG_H
#define BOARDCOLORDIALOG_H

#include "boardcolors.h"
#include "boardcolorpresets.h"
#include "boardvisuals.h"
#include <QDialog>

class QColorDialog;
class QPushButton;
class QComboBox;
class QLabel;
class QIcon;
class QFormLayout;
class QTabWidget;
class QCheckBox;
class QSpinBox;

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
    void applyTheme(int index);
    void applyVisuals();
    void refreshVisuals();

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
    QComboBox* m_themeCombo = nullptr;
    QCheckBox* m_woodGrain = nullptr;
    QCheckBox* m_pieceShadow = nullptr;
    QSpinBox* m_pieceScale = nullptr;
    QList<BoardThemePreset> m_themes;
    void createColorPages();
    void createAppearancePage();
    void syncThemeSelection();
    void addColorField(QFormLayout* form, BoardColors::Member member,
                       const QString& label, const QString& objectName);
    void syncPresetSelection();
    QIcon presetIcon(const BoardColors& colors, const QString& style = QString(),
                     const BoardVisuals& visuals = BoardVisuals{}) const;
};

#endif // BOARDCOLORDIALOG_H
