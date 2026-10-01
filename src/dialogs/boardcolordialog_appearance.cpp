#include "boardcolordialog.h"
#include "boardappearance.h"
#include "pieceimageprovider.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

void BoardColorDialog::createAppearancePage()
{
    auto* page = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(page);
    auto* description = new QLabel(tr("盤・駒台・駒・対局者情報をまとめて整える外観です。選択するとすべての将棋盤に反映・保存されます。"), page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* form = new QFormLayout;
    m_themeCombo = new QComboBox(page);
    m_themeCombo->setObjectName(QStringLiteral("boardThemeCombo"));
    m_themeCombo->setPlaceholderText(tr("カスタム（個別に指定）"));
    m_themeCombo->setIconSize(QSize(120, 72));
    m_themes = BoardColorPresets::themes();
    for (const auto& theme : std::as_const(m_themes))
        m_themeCombo->addItem(presetIcon(theme.colors, QStringLiteral("standard")), theme.name, theme.id);
    form->addRow(tr("外観"), m_themeCombo);
    m_pieceScale = new QSpinBox(page);
    m_pieceScale->setObjectName(QStringLiteral("boardPieceScale"));
    m_pieceScale->setRange(90, 112);
    m_pieceScale->setSuffix(QStringLiteral(" %"));
    form->addRow(tr("駒の大きさ"), m_pieceScale);
    layout->addLayout(form);
    m_woodGrain = new QCheckBox(tr("控えめな木目を表示する"), page);
    m_woodGrain->setObjectName(QStringLiteral("boardWoodGrain"));
    m_pieceShadow = new QCheckBox(tr("駒の影を表示する"), page);
    m_pieceShadow->setObjectName(QStringLiteral("boardPieceShadow"));
    layout->addWidget(m_woodGrain);
    layout->addWidget(m_pieceShadow);
    auto* note = new QLabel(tr("外観を選ぶと「標準の駒」と標準の大きさ・影に切り替わります。色は各タブ、駒の種類は「表示」メニューで個別に変更できます。"), page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    // 既存タブの保存済み番号を維持するため末尾に追加する。
    m_tabs->addTab(page, tr("外観"));
    refreshVisuals();
    connect(m_themeCombo, &QComboBox::activated, this, &BoardColorDialog::applyTheme);
    connect(m_pieceScale, &QSpinBox::valueChanged, this, &BoardColorDialog::applyVisuals);
    connect(m_woodGrain, &QCheckBox::toggled, this, &BoardColorDialog::applyVisuals);
    connect(m_pieceShadow, &QCheckBox::toggled, this, &BoardColorDialog::applyVisuals);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &BoardColorDialog::refreshVisuals);
}

void BoardColorDialog::applyTheme(int index)
{
    if (index < 0 || index >= m_themes.size()) return;
    const auto colors = m_themes.at(index).colors;
    PieceImageProvider::instance().setStyle(QStringLiteral("standard"));
    BoardAppearance::instance().setColors(colors);
    BoardAppearance::instance().setVisuals(BoardVisuals{});
    syncThemeSelection();
}

void BoardColorDialog::applyVisuals()
{
    BoardAppearance::instance().setVisuals({m_woodGrain->isChecked(), m_pieceShadow->isChecked(),
                                          m_pieceScale->value()});
}

void BoardColorDialog::refreshVisuals()
{
    const auto visuals = BoardAppearance::instance().visuals();
    const QSignalBlocker blockGrain(m_woodGrain), blockShadow(m_pieceShadow), blockScale(m_pieceScale);
    m_woodGrain->setChecked(visuals.woodGrain);
    m_pieceShadow->setChecked(visuals.pieceShadow);
    m_pieceScale->setValue(visuals.pieceScale);
    syncThemeSelection();
}

void BoardColorDialog::syncThemeSelection()
{
    if (!m_themeCombo) return;
    int selected = -1;
    if (PieceImageProvider::instance().style() == QLatin1String("standard")
        && BoardAppearance::instance().visuals() == BoardVisuals{}) {
        const auto colors = BoardAppearance::instance().colors();
        for (qsizetype i = 0; i < m_themes.size(); ++i)
            if (m_themes.at(i).colors == colors) selected = static_cast<int>(i);
    }
    const QSignalBlocker blocker(m_themeCombo);
    m_themeCombo->setCurrentIndex(selected);
}
