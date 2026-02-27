/// @file engineanalysispresenter.cpp
/// @brief エンジン解析思考ビュープレゼンタの実装

#include "engineanalysispresenter.h"
#include "logviewfontmanager.h"

#include <QTableView>
#include <QHeaderView>
#include <QFont>
#include <QTimer>
#include <QItemSelectionModel>

#include "analysissettings.h"
#include "logcategories.h"
#include "numericrightaligncommadelegate.h"
#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"

EngineAnalysisPresenter::EngineAnalysisPresenter(QObject* parent)
    : QObject(parent)
{
}

EngineAnalysisPresenter::~EngineAnalysisPresenter() = default;

void EngineAnalysisPresenter::setViews(QTableView* v1, QTableView* v2,
                                        EngineInfoWidget* i1, EngineInfoWidget* i2)
{
    m_view1 = v1;
    m_view2 = v2;
    m_info1 = i1;
    m_info2 = i2;
}

// ===================== モデル接続 =====================

void EngineAnalysisPresenter::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2)
{
    m_model1 = m1;
    m_model2 = m2;

    if (m_view1) m_view1->setModel(m1);
    if (m_view2) m_view2->setModel(m2);

    applyThinkingViewColumnWidths(m_view1, 0);
    applyThinkingViewColumnWidths(m_view2, 1);

    reapplyViewTuning(m_view1, m_model1);
    reapplyViewTuning(m_view2, m_model2);

    if (m_model1) {
        connect(m_model1, &QAbstractItemModel::modelReset,
                this, &EngineAnalysisPresenter::onModel1Reset, Qt::UniqueConnection);
    }
    if (m_model2) {
        connect(m_model2, &QAbstractItemModel::modelReset,
                this, &EngineAnalysisPresenter::onModel2Reset, Qt::UniqueConnection);
    }
}

void EngineAnalysisPresenter::setEngine1ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model1 = m;
    if (m_view1) m_view1->setModel(m);
    if (m_view1 && m_view1->horizontalHeader())
        m_view1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisPresenter::setEngine2ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model2 = m;
    if (m_view2) m_view2->setModel(m);
    if (m_view2 && m_view2->horizontalHeader())
        m_view2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisPresenter::clearThinkingViewSelection(int engineIndex, QTableView* considerationView)
{
    QTableView* view = nullptr;
    if (engineIndex == 0) {
        view = m_view1;
    } else if (engineIndex == 1) {
        view = m_view2;
    } else if (engineIndex == 2) {
        view = considerationView;
    }
    if (!view) return;

    if (auto* sel = view->selectionModel()) {
        sel->clearSelection();
        sel->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
}

// ===================== ヘッダ設定 =====================

void EngineAnalysisPresenter::setupThinkingViewHeader(QTableView* v)
{
    if (!v) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    v->setStyleSheet(QStringLiteral(
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}"));

    h->setDefaultSectionSize(100);
    h->setMinimumSectionSize(24);
    h->setStretchLastSection(true);

    auto* vh = v->verticalHeader();
    if (vh) {
        vh->setVisible(false);
        const int rowHeight = v->fontMetrics().height() + 4;
        vh->setDefaultSectionSize(rowHeight);
        vh->setSectionResizeMode(QHeaderView::Fixed);
    }
    v->setWordWrap(false);
}

void EngineAnalysisPresenter::applyThinkingViewColumnWidths(QTableView* v, int viewIndex)
{
    if (!v || !v->model()) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    constexpr int kColCount = 6;

    for (int col = 0; col < kColCount; ++col) {
        h->setSectionResizeMode(col, QHeaderView::Interactive);
    }

    const int defaultWidths[] = {50, 40, 80, 60, 45, 100};
    constexpr int kMaxTotalWidth = 450;

    QList<int> savedWidths = AnalysisSettings::thinkingViewColumnWidths(viewIndex);

    bool useSavedWidths = false;
    if (savedWidths.size() == kColCount) {
        int totalWidth = 0;
        for (int col = 0; col < kColCount - 1; ++col) {
            totalWidth += savedWidths.at(col);
        }
        useSavedWidths = (totalWidth <= kMaxTotalWidth);
    }

    h->blockSignals(true);
    if (useSavedWidths) {
        for (int col = 0; col < kColCount; ++col) {
            if (savedWidths.at(col) > 0) {
                v->setColumnWidth(col, savedWidths.at(col));
            }
        }
    } else {
        for (int col = 0; col < kColCount; ++col) {
            v->setColumnWidth(col, defaultWidths[col]);
        }
    }
    h->blockSignals(false);

    QTimer::singleShot(500, this, [this, viewIndex]() {
        if (viewIndex == 0) {
            m_thinkingView1WidthsLoaded = true;
        } else {
            m_thinkingView2WidthsLoaded = true;
        }
    });
}

// ===================== EngineInfo列幅 =====================

void EngineAnalysisPresenter::loadEngineInfoColumnWidths()
{
    if (m_info1) {
        QList<int> widths0 = AnalysisSettings::engineInfoColumnWidths(0);
        if (!widths0.isEmpty() && widths0.size() == m_info1->columnCount()) {
            m_info1->setColumnWidths(widths0);
        }
    }
    if (m_info2) {
        QList<int> widths1 = AnalysisSettings::engineInfoColumnWidths(1);
        if (!widths1.isEmpty() && widths1.size() == m_info2->columnCount()) {
            m_info2->setColumnWidths(widths1);
        }
    }
}

void EngineAnalysisPresenter::wireEngineInfoColumnWidthSignals()
{
    if (m_info1) {
        connect(m_info1, &EngineInfoWidget::columnWidthChanged,
                this, &EngineAnalysisPresenter::onEngineInfoColumnWidthChanged);
    }
    if (m_info2) {
        connect(m_info2, &EngineInfoWidget::columnWidthChanged,
                this, &EngineAnalysisPresenter::onEngineInfoColumnWidthChanged);
    }
}

void EngineAnalysisPresenter::wireThinkingViewSignals()
{
    if (m_view1) {
        connect(m_view1->horizontalHeader(), &QHeaderView::sectionResized,
                this, &EngineAnalysisPresenter::onView1SectionResized);
        connect(m_view1, &QTableView::clicked,
                this, &EngineAnalysisPresenter::onView1Clicked);
    }
    if (m_view2) {
        connect(m_view2->horizontalHeader(), &QHeaderView::sectionResized,
                this, &EngineAnalysisPresenter::onView2SectionResized);
        connect(m_view2, &QTableView::clicked,
                this, &EngineAnalysisPresenter::onView2Clicked);
    }
}

// ===================== 数値フォーマット =====================

int EngineAnalysisPresenter::findColumnByHeader(QAbstractItemModel* model, const QString& title)
{
    if (!model) return -1;
    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c) {
        const QVariant hd = model->headerData(c, Qt::Horizontal, Qt::DisplayRole);
        const QString h = hd.toString().trimmed();
        if (QString::compare(h, title, Qt::CaseInsensitive) == 0) {
            return c;
        }
    }
    return -1;
}

void EngineAnalysisPresenter::applyNumericFormattingTo(QTableView* view, QAbstractItemModel* model)
{
    if (!view || !model) return;

    auto* delegate = new NumericRightAlignCommaDelegate(view);

    const QStringList targets = {
        "Time", "Depth", "Nodes", "Score",
        "時間", "深さ", "ノード数", "評価値"
    };
    for (const QString& t : targets) {
        const int col = findColumnByHeader(model, t);
        if (col >= 0) {
            view->setItemDelegateForColumn(col, delegate);
        }
    }
}

void EngineAnalysisPresenter::reapplyViewTuning(QTableView* v, QAbstractItemModel* m)
{
    if (!v) return;
    if (m) applyNumericFormattingTo(v, m);
}

// ===================== フォント管理 =====================

void EngineAnalysisPresenter::initThinkingFontManager()
{
    m_thinkingFontSize = AnalysisSettings::thinkingFontSize();
    m_thinkingFontManager = std::make_unique<LogViewFontManager>(m_thinkingFontSize, nullptr);
    m_thinkingFontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);

        if (m_info1) m_info1->setFontSize(size);
        if (m_info2) m_info2->setFontSize(size);

        QString headerStyle = QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #209cee;"
            "  font-size: %1pt;"
            "}")
            .arg(size);

        if (m_view1) {
            m_view1->setFont(font);
            m_view1->setStyleSheet(headerStyle);
            const int rowHeight = m_view1->fontMetrics().height() + 4;
            m_view1->verticalHeader()->setDefaultSectionSize(rowHeight);
        }
        if (m_view2) {
            m_view2->setFont(font);
            m_view2->setStyleSheet(headerStyle);
            const int rowHeight = m_view2->fontMetrics().height() + 4;
            m_view2->verticalHeader()->setDefaultSectionSize(rowHeight);
        }

        AnalysisSettings::setThinkingFontSize(size);
    });
    if (m_thinkingFontSize != 10) {
        m_thinkingFontManager->apply();
    }
}

void EngineAnalysisPresenter::onThinkingFontIncrease()
{
    m_thinkingFontManager->increase();
}

void EngineAnalysisPresenter::onThinkingFontDecrease()
{
    m_thinkingFontManager->decrease();
}

// ===================== スロット =====================

void EngineAnalysisPresenter::onModel1Reset()
{
    reapplyViewTuning(m_view1, m_model1);
}

void EngineAnalysisPresenter::onModel2Reset()
{
    reapplyViewTuning(m_view2, m_model2);
}

void EngineAnalysisPresenter::onView1Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    if (index.column() == 4) {
        qCDebug(lcUi) << "[EngineAnalysisPresenter] onView1Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(0, index.row());
    }
}

void EngineAnalysisPresenter::onView2Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    if (index.column() == 4) {
        qCDebug(lcUi) << "[EngineAnalysisPresenter] onView2Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(1, index.row());
    }
}

void EngineAnalysisPresenter::onEngineInfoColumnWidthChanged()
{
    EngineInfoWidget* senderWidget = qobject_cast<EngineInfoWidget*>(QObject::sender());
    if (!senderWidget) return;

    int widgetIndex = senderWidget->widgetIndex();
    QList<int> widths = senderWidget->columnWidths();
    AnalysisSettings::setEngineInfoColumnWidths(widgetIndex, widths);
}

void EngineAnalysisPresenter::onThinkingViewColumnWidthChanged(int viewIndex)
{
    QTableView* view = (viewIndex == 0) ? m_view1 : m_view2;
    if (!view) return;

    QList<int> widths;
    const int colCount = view->horizontalHeader()->count();
    for (int col = 0; col < colCount; ++col) {
        widths.append(view->columnWidth(col));
    }
    AnalysisSettings::setThinkingViewColumnWidths(viewIndex, widths);
}

void EngineAnalysisPresenter::onView1SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    if (m_thinkingView1WidthsLoaded) {
        onThinkingViewColumnWidthChanged(0);
    }
}

void EngineAnalysisPresenter::onView2SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    if (m_thinkingView2WidthsLoaded) {
        onThinkingViewColumnWidthChanged(1);
    }
}
