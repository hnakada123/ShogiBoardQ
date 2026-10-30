/// @file thinkingpanel.cpp
/// @brief 思考ドックの中身の実装

#include "thinkingpanel.h"
#include "fontsizecontrol.h"

#include "analysissettings.h"
#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "thinkingtableview.h"
#include "usicommlogmodel.h"

#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
/// actions に無い切り替えは、owner の子として作る（単独で使うとき）
QAction* ensureAction(QAction* action, QObject* owner)
{
    if (action) return action;
    auto* created = new QAction(owner);
    created->setCheckable(true);
    return created;
}

/// チェックと QAction を両方向に結ぶ
void bindCheckBox(QCheckBox* checkBox, QAction* action)
{
    checkBox->setChecked(action->isChecked());
    QObject::connect(checkBox, &QCheckBox::toggled, action, &QAction::setChecked);
    QObject::connect(action, &QAction::toggled, checkBox, &QCheckBox::setChecked);
}
} // namespace

ThinkingPanel::ThinkingPanel(QWidget* parent)
    : ThinkingPanel(ArrowActions{}, parent)
{
}

ThinkingPanel::ThinkingPanel(const ArrowActions& actions, QWidget* parent)
    : QWidget(parent)
    , m_fontSize(AnalysisSettings::thinkingFontSize, 8, 24)
    , m_arrowActions{ensureAction(actions.match, this), ensureAction(actions.ponder, this)}
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(4);

    auto* controls = new QHBoxLayout;
    const auto* fontButtons = FontSizeControl::createToolButtons(
        this, &m_fontSize, QStringLiteral("thinkingFontDecrease"), QStringLiteral("thinkingFontIncrease"));
    for (auto* button : {fontButtons->decreaseButton(), fontButtons->increaseButton()}) controls->addWidget(button);
    m_matchArrows = new QCheckBox(tr("対局中の矢印表示"), this);
    m_matchArrows->setObjectName(QStringLiteral("matchArrows"));
    m_ponderArrows = new QCheckBox(tr("先読み側も表示"), this);
    m_ponderArrows->setObjectName(QStringLiteral("ponderArrows"));
    m_ponderArrows->setToolTip(tr("Ponder中の応手を青の破線で表示します。エンジンの先読み設定は変更しません。"));
    controls->addWidget(m_matchArrows);
    controls->addWidget(m_ponderArrows);
    controls->addStretch();
    v->addLayout(controls);
    m_arrowDescription = new QLabel(this);
    m_arrowDescription->setObjectName(QStringLiteral("matchArrowDescription"));
    m_arrowDescription->setWordWrap(true);
    v->addWidget(m_arrowDescription);
    bindCheckBox(m_matchArrows, m_arrowActions.match);
    bindCheckBox(m_ponderArrows, m_arrowActions.ponder);
    connect(m_arrowActions.match, &QAction::toggled, this, &ThinkingPanel::updateArrowControls);
    connect(m_arrowActions.ponder, &QAction::toggled, this, &ThinkingPanel::updateArrowControls);
    updateArrowControls();

    m_info1 = new EngineInfoWidget(AnalysisSettings::ThinkingTable::Engine1, this);
    m_view1 = new ThinkingTableView(AnalysisSettings::ThinkingTable::Engine1, this);
    m_view1->setObjectName(QStringLiteral("thinkingView1"));
    m_info2 = new EngineInfoWidget(AnalysisSettings::ThinkingTable::Engine2, this);
    m_view2 = new ThinkingTableView(AnalysisSettings::ThinkingTable::Engine2, this);

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    connect(m_view1, &ThinkingTableView::pvRowClicked, this, &ThinkingPanel::pvRowClicked);
    connect(m_view2, &ThinkingTableView::pvRowClicked, this, &ThinkingPanel::pvRowClicked);
    connect(m_view1, &ThinkingTableView::thinkingRowContextMenuAboutToShow, this, &ThinkingPanel::rowContextMenuAboutToShow);
    connect(m_view2, &ThinkingTableView::thinkingRowContextMenuAboutToShow, this, &ThinkingPanel::rowContextMenuAboutToShow);
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &ThinkingPanel::applyFontSize);
    // 既定の大きさ（10）のときは表の既定の書体のままにする
    if (m_fontSize.fontSize() != 10) applyFontSize();

    // 2つ目のエンジンは使うときだけ出す
    setSecondEngineVisible(false);
}

void ThinkingPanel::setModels(ShogiEngineThinkingModel* thinking1, ShogiEngineThinkingModel* thinking2,
                              UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_view1->setThinkingModel(thinking1);
    m_view2->setThinkingModel(thinking2);
    m_info1->setModel(log1);
    m_info2->setModel(log2);
}

void ThinkingPanel::setSecondEngineVisible(bool on)
{
    m_info2->setVisible(on);
    m_view2->setVisible(on);
}

void ThinkingPanel::setArrowDisplayState(bool considerationActive, const QString& ponderDescription)
{
    m_arrowConsideration = considerationActive;
    m_ponderDescription = ponderDescription;
    updateArrowControls();
}

void ThinkingPanel::clearSelection(const ShogiEngineThinkingModel* model)
{
    for (ThinkingTableView* view : {m_view1, m_view2}) {
        if (view->thinkingModel() == model) view->clearRowSelection();
    }
}

void ThinkingPanel::applyFontSize()
{
    const int size = m_fontSize.fontSize();
    m_info1->setFontSize(size);
    m_info2->setFontSize(size);
    m_view1->setFontPointSize(size);
    m_view2->setFontPointSize(size);
}

void ThinkingPanel::updateArrowControls()
{
    const bool consideration = m_arrowConsideration;
    const bool match = m_arrowActions.match->isChecked();
    m_matchArrows->setEnabled(!consideration);
    m_ponderArrows->setEnabled(!consideration && match);
    QString description;
    if (consideration) {
        description = tr("検討中の矢印表示は検討タブで設定します。");
    } else if (match) {
        description = tr("赤の実線：手番側");
        if (m_arrowActions.ponder->isChecked()) description += tr(" ／ 青の破線：先読み側");
        if (!m_ponderDescription.isEmpty())
            description += QStringLiteral(" — ") + m_ponderDescription;
    }
    m_arrowDescription->setText(description);
    m_arrowDescription->setVisible(!description.isEmpty());
}
