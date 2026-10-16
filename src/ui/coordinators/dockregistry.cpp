/// @file dockregistry.cpp
/// @brief メインウィンドウのドックの表と、作成・配置・保存の実装

#include "dockregistry.h"

#include "docksettings.h"
#include "layoutboundarywidget.h"
#include "opaquebackground.h"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>

using DockId = DockRegistry::DockId;

namespace {
constexpr Qt::DockWidgetArea kRight = Qt::RightDockWidgetArea;
constexpr Qt::DockWidgetArea kBottom = Qt::BottomDockWidgetArea;

std::size_t indexOf(DockId id)
{
    return static_cast<std::size_t>(id);
}
} // namespace

const std::array<DockRegistry::DockSpec, DockRegistry::kDockCount>& DockRegistry::specs()
{
    // 右: 棋譜（メニューは棋譜のタブに重ねて非表示）。下: 対局情報から棋譜解析までを1つのタブにまとめる。
    // 思考・ログなど頻繁に更新する下段のパネルは、描き直しをドックのタイトルとウィンドウまで広げない
    static const std::array<DockSpec, kDockCount> table = {{
        {DockId::Record, "RecordPaneDock", QT_TRANSLATE_NOOP("DockRegistry", "棋譜"), kRight,
         true, true, false, true, 0, 350, "RecordPaneDock"},
        {DockId::GameInfo, "GameInfoDock", QT_TRANSLATE_NOOP("DockRegistry", "対局情報"), kBottom,
         true, false, false, true, 100, 0, "GameInfoDock"},
        {DockId::Thinking, "ThinkingDock", QT_TRANSLATE_NOOP("DockRegistry", "思考"), kBottom,
         true, true, true, true, 100, 0, "ThinkingDock"},
        {DockId::Consideration, "ConsiderationDock", QT_TRANSLATE_NOOP("DockRegistry", "検討"), kBottom,
         true, false, true, true, 100, 0, "ConsiderationDock"},
        {DockId::UsiLog, "UsiLogDock", QT_TRANSLATE_NOOP("DockRegistry", "USI通信ログ"), kBottom,
         true, false, false, true, 100, 0, "UsiLogDock"},
        {DockId::CsaLog, "CsaLogDock", QT_TRANSLATE_NOOP("DockRegistry", "CSA通信ログ"), kBottom,
         true, false, false, true, 100, 0, "CsaLogDock"},
        {DockId::Comment, "CommentDock", QT_TRANSLATE_NOOP("DockRegistry", "棋譜コメント"), kBottom,
         true, false, false, true, 100, 0, "CommentDock"},
        {DockId::BranchTree, "BranchTreeDock", QT_TRANSLATE_NOOP("DockRegistry", "分岐ツリー"), kBottom,
         true, false, false, true, 100, 0, "BranchTreeDock"},
        {DockId::EvalChart, "EvalChartDock", QT_TRANSLATE_NOOP("DockRegistry", "評価値グラフ"), kBottom,
         true, false, false, false, 0, 0, "EvalChartDock"},
        {DockId::Menu, "MenuWindowDock", QT_TRANSLATE_NOOP("DockRegistry", "メニュー"), kRight,
         false, false, false, false, 0, 0, "MenuWindowDock"},
        // 定跡ウィンドウは1手ごとに表と局面のラベルが変わる
        {DockId::Joseki, "JosekiWindowDock", QT_TRANSLATE_NOOP("DockRegistry", "定跡"), kBottom,
         false, false, true, true, 0, 0, "JosekiWindowDock"},
        {DockId::AnalysisResults, "AnalysisResultsDock", QT_TRANSLATE_NOOP("DockRegistry", "棋譜解析"), kBottom,
         false, false, false, false, 0, 0, "KifuAnalysisResultsDock"},
    }};
    return table;
}

const DockRegistry::DockSpec& DockRegistry::spec(DockId id)
{
    // 表は DockId の順に並べる
    const DockSpec& entry = specs().at(indexOf(id));
    Q_ASSERT(entry.id == id);
    return entry;
}

QString DockRegistry::title(DockId id)
{
    return QCoreApplication::translate("DockRegistry", spec(id).title);
}

DockRegistry::DockRegistry(QMainWindow* window, QMenu* displayMenu, QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_displayMenu(displayMenu)
{
    Q_ASSERT(m_window);
    m_window->setStyleSheet(m_window->styleSheet() + QStringLiteral(
        "QMainWindow > QTabBar::tab { background: #edf0f3; color: #596571;"
        " border: none; border-bottom: 2px solid transparent; padding: 6px 12px; }"
        "QMainWindow > QTabBar::tab:hover { background: #e7edf3; color: #243b53; }"
        "QMainWindow > QTabBar::tab:selected { background: #ffffff; color: #243b53;"
        " border-bottom-color: #52799c; }"));
}

void DockRegistry::createAll(const Contents& contents)
{
    for (const DockSpec& entry : specs()) {
        QWidget* content = contents.at(indexOf(entry.id));
        if (!content) continue;
        const QString dockTitle = title(entry.id);
        auto* dock = new QDockWidget(dockTitle, m_window);
        dock->setObjectName(QString::fromLatin1(entry.objectName));
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        // タイトルバーの高さと色を設定（ドラッグ領域を認識しやすくする）
        dock->setStyleSheet(QStringLiteral("QDockWidget::title { padding: 6px 8px; background-color: #edf0f3; }"));
        // 中身の大きさが頻繁に変わるドックは、ドックの大きさの条件が変わらない限り、
        // メインウィンドウの配置（すべてのドックのタイトルの大きさの計算を含む）をやり直さない
        dock->setWidget(entry.layoutBoundary ? new LayoutBoundaryWidget(content, dock) : content);
        // 頻繁に更新する中身の描き直しを、ドックのタイトル（Fusion のアイコンの読み込みを含む）と
        // メインウィンドウまで広げない
        if (entry.opaqueContent) OpaqueBackground::keep(content);
        if (entry.minimumHeight > 0) {
            dock->setMinimumWidth(0);
            dock->setMinimumHeight(entry.minimumHeight);
        }
        if (m_displayMenu) {
            QAction* toggle = dock->toggleViewAction();
            toggle->setText(dockTitle);
            m_displayMenu->addAction(toggle);
        }
        m_docks.at(indexOf(entry.id)) = dock;
    }
    placeAll();
    for (const DockSpec& entry : specs()) {
        QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (!dock) continue;
        const DockSettings::DockState state =
            DockSettings::dockState(QString::fromLatin1(entry.settingsGroup), entry.defaultVisible);
        if (!state.geometry.isEmpty()) dock->restoreGeometry(state.geometry);
        dock->setFloating(state.floating);
        dock->setVisible(state.visible);
    }
    raiseDefaults();
}

QDockWidget* DockRegistry::dock(DockId id) const
{
    return id == DockId::Count ? nullptr : m_docks.at(indexOf(id));
}

QList<QDockWidget*> DockRegistry::docks() const
{
    QList<QDockWidget*> result;
    for (const DockSpec& entry : specs())
        if (QDockWidget* created = m_docks.at(indexOf(entry.id))) result.append(created);
    return result;
}

void DockRegistry::placeAll()
{
    QDockWidget* lastRight = nullptr;
    QDockWidget* lastBottom = nullptr;
    for (const DockSpec& entry : specs()) {
        QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (!dock) continue;
        QDockWidget*& last = entry.area == kRight ? lastRight : lastBottom;
        if (last) {
            m_window->tabifyDockWidget(last, dock);
        } else {
            m_window->addDockWidget(entry.area, dock);
        }
        last = dock;
    }
}

void DockRegistry::raiseDefaults()
{
    for (const DockSpec& entry : specs()) {
        QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (dock && entry.raised) dock->raise();
    }
}

void DockRegistry::resetToDefault()
{
    const QList<QDockWidget*> created = docks();
    // すべてのドックを浮動から戻して外し、表の順に置き直す
    for (QDockWidget* dock : created) {
        dock->setFloating(false);
        m_window->removeDockWidget(dock);
    }
    placeAll();
    for (const DockSpec& entry : specs()) {
        QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (dock) dock->setVisible(entry.defaultVisible);
    }
    raiseDefaults();
    for (const DockSpec& entry : specs()) {
        QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (dock && entry.defaultWidth > 0) m_window->resizeDocks({dock}, {entry.defaultWidth}, Qt::Horizontal);
    }
}

void DockRegistry::saveStates() const
{
    for (const DockSpec& entry : specs()) {
        const QDockWidget* dock = m_docks.at(indexOf(entry.id));
        if (!dock) continue;
        DockSettings::DockState state;
        state.floating = dock->isFloating();
        state.visible = dock->isVisible();
        state.geometry = dock->saveGeometry();
        DockSettings::setDockState(QString::fromLatin1(entry.settingsGroup), state);
    }
}
