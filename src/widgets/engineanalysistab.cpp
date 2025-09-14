#include "engineanalysistab.h"
#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

#include <QTabWidget>
#include <QTableView>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <QPainter>
#include <QTextCursor>
#include <QSplitter>
#include <QLabel>

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void EngineAnalysisTab::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    m_tab = new QTabWidget(this);

    // --- Tab 1: エンジン（1 と 2 を上下に） ---
    {
        auto* page = new QWidget(m_tab);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(6, 6, 6, 6);
        v->setSpacing(6);

        m_engineSplit = new QSplitter(Qt::Vertical, page);
        m_engineSplit->setChildrenCollapsible(false);

        // 上段：エンジン1
        m_engine1Pane = new QWidget(m_engineSplit);
        auto* v1 = new QVBoxLayout(m_engine1Pane);
        v1->setContentsMargins(0, 0, 0, 0);
        v1->setSpacing(6);

        auto* label1 = new QLabel(tr("エンジン1"), m_engine1Pane);
        m_info1 = new EngineInfoWidget(m_engine1Pane);
        m_view1 = new QTableView(m_engine1Pane);
        m_view1->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_view1->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view1->setAlternatingRowColors(true);
        if (auto* hh = m_view1->horizontalHeader())
            hh->setStretchLastSection(true);

        v1->addWidget(label1);
        v1->addWidget(m_info1);
        v1->addWidget(m_view1, 1);
        m_engine1Pane->setLayout(v1);

        // 下段：エンジン2
        m_engine2Pane = new QWidget(m_engineSplit);
        auto* v2 = new QVBoxLayout(m_engine2Pane);
        v2->setContentsMargins(0, 0, 0, 0);
        v2->setSpacing(6);

        auto* label2 = new QLabel(tr("エンジン2"), m_engine2Pane);
        m_info2 = new EngineInfoWidget(m_engine2Pane);
        m_view2 = new QTableView(m_engine2Pane);
        m_view2->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_view2->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view2->setAlternatingRowColors(true);
        if (auto* hh2 = m_view2->horizontalHeader())
            hh2->setStretchLastSection(true);

        v2->addWidget(label2);
        v2->addWidget(m_info2);
        v2->addWidget(m_view2, 1);
        m_engine2Pane->setLayout(v2);

        m_engineSplit->addWidget(m_engine1Pane);
        m_engineSplit->addWidget(m_engine2Pane);
        m_engineSplit->setStretchFactor(0, 1);
        m_engineSplit->setStretchFactor(1, 1);

        v->addWidget(m_engineSplit, 1);
        page->setLayout(v);

        m_tab->addTab(page, tr("エンジン"));
    }

    // --- USI ログ（以降は従来どおり） ---
    {
        auto* page = new QWidget(m_tab);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(6, 6, 6, 6);
        v->setSpacing(6);

        m_usiLog = new QPlainTextEdit(page);
        m_usiLog->setReadOnly(true);
        m_usiLog->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_usiLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

        v->addWidget(m_usiLog, 1);
        page->setLayout(v);

        m_tab->addTab(page, tr("USIログ"));
    }

    {
        auto* page = new QWidget(m_tab);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(6, 6, 6, 6);
        v->setSpacing(6);
        m_comment = new QTextBrowser(page);
        v->addWidget(m_comment, 1);
        page->setLayout(v);
        m_tab->addTab(page, tr("コメント"));
    }

    {
        auto* page = new QWidget(m_tab);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(6, 6, 6, 6);
        v->setSpacing(6);
        m_branchTree = new QGraphicsView(page);
        m_branchTree->setRenderHint(QPainter::Antialiasing, true);
        v->addWidget(m_branchTree, 1);
        page->setLayout(v);
        m_tab->addTab(page, tr("分岐"));
    }

    root->addWidget(m_tab);
    setLayout(root);

    // ★ デフォルト: 起動時は「エンジン1のみ表示」
    setDualEngineVisible(false);
}

void EngineAnalysisTab::setEngine1ThinkingModel(QAbstractItemModel* m)
{
    if (m_view1) m_view1->setModel(m);
}

void EngineAnalysisTab::setEngine2ThinkingModel(QAbstractItemModel* m)
{
    if (m_view2) m_view2->setModel(m);
}

// 例（前にお渡しした実装と同等）
void EngineAnalysisTab::setDualEngineVisible(bool on)
{
    if (!m_info2 || !m_view2) return;
    m_info2->setVisible(on);
    m_view2->setVisible(on);
}

void EngineAnalysisTab::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                                  UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    // 思考テーブル
    if (m_view1) {
        m_view1->setModel(m1);
        if (auto* hh = m_view1->horizontalHeader())
            hh->setStretchLastSection(true);
    }
    if (m_view2) {
        m_view2->setModel(m2);
        if (auto* hh = m_view2->horizontalHeader())
            hh->setStretchLastSection(true);
    }

    // EngineInfoWidget は UsiCommLogModel* を受け取る
    if (m_info1) m_info1->setModel(log1);
    if (m_info2) m_info2->setModel(log2);

    // USIログのライブ追記（UniqueConnectionは使わない：ラムダは重複判定できない）
    if (m_usiLog) m_usiLog->clear();

    auto hook = [this](UsiCommLogModel* log, const QString& prefix) {
        if (!log || !m_usiLog) return;

        // 最初に現在値があれば追記
        const QString cur = log->usiCommLog();
        if (!cur.isEmpty()) {
            m_usiLog->appendPlainText(prefix + cur);
            m_usiLog->moveCursor(QTextCursor::End);
        }

        // 以後の更新を受けて追記
        QObject::disconnect(log, nullptr, this, nullptr); // 二重接続の保険
        connect(log, &UsiCommLogModel::usiCommLogChanged, this,
                [this, log, prefix]() {
                    if (!m_usiLog) return;
                    const QString s = log->usiCommLog();
                    if (!s.isEmpty()) {
                        m_usiLog->appendPlainText(prefix + s);
                        m_usiLog->moveCursor(QTextCursor::End);
                    }
                });
    };

    hook(log1, QStringLiteral("[E1] "));
    hook(log2, QStringLiteral("[E2] "));
}

QTabWidget* EngineAnalysisTab::tab() const
{
    return m_tab;
}

void EngineAnalysisTab::setAnalysisVisible(bool on)
{
    if (m_tab) m_tab->setVisible(on);
    setVisible(on);
}

void EngineAnalysisTab::setSecondEngineVisible(bool on)
{
    if (m_info2)  m_info2->setVisible(on);
    if (m_view2)  m_view2->setVisible(on);
    // 必要ならレイアウト再計算
    if (m_tab) m_tab->update();
}

void EngineAnalysisTab::setCommentText(const QString& text)
{
    if (m_comment) m_comment->setText(text);
}
