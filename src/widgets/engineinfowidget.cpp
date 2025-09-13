#include "engineinfowidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFont>
#include "usicommlogmodel.h"

EngineInfoWidget::EngineInfoWidget(QWidget* parent): QWidget(parent) {
    QFont f("Noto Sans CJK JP", 8);
    auto lab = [f](const QString& t){ auto* L=new QLabel(t); L->setFont(f); return L; };
    auto ro  = [f](int w=-1){ auto* E=new QLineEdit; E->setReadOnly(true); E->setFont(f); if(w>0)E->setFixedWidth(w); return E; };

    m_engineName = ro(300);
    m_pred = ro(); m_searched = ro();
    m_depth = ro(60); m_depth->setAlignment(Qt::AlignRight);
    m_nodes = ro();  m_nodes->setAlignment(Qt::AlignRight);
    m_nps   = ro();  m_nps  ->setAlignment(Qt::AlignRight);
    m_hash  = ro(60);m_hash ->setAlignment(Qt::AlignRight);

    auto* h = new QHBoxLayout(this);
    h->addWidget(m_engineName);
    h->addWidget(lab("予想手"));    h->addWidget(m_pred);
    h->addWidget(lab("探索手"));    h->addWidget(m_searched);
    h->addWidget(lab("深さ"));      h->addWidget(m_depth);
    h->addWidget(lab("ノード数"));  h->addWidget(m_nodes);
    h->addWidget(lab("探索局面数"));h->addWidget(m_nps);
    h->addWidget(lab("ハッシュ使用率")); h->addWidget(m_hash);
    setLayout(h);
}

void EngineInfoWidget::setModel(UsiCommLogModel* m) {
    if (m_model == m) return;
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
    m_model = m;
    if (!m_model) {
        // モデルが無いときは見た目を空に
        m_engineName->clear(); m_pred->clear(); m_searched->clear();
        m_depth->clear(); m_nodes->clear(); m_nps->clear(); m_hash->clear();
        return;
    }

    connect(m_model, &UsiCommLogModel::engineNameChanged,     this, &EngineInfoWidget::onNameChanged);
    connect(m_model, &UsiCommLogModel::predictiveMoveChanged, this, &EngineInfoWidget::onPredChanged);
    connect(m_model, &UsiCommLogModel::searchedMoveChanged,   this, &EngineInfoWidget::onSearchedChanged);
    connect(m_model, &UsiCommLogModel::searchDepthChanged,    this, &EngineInfoWidget::onDepthChanged);
    connect(m_model, &UsiCommLogModel::nodeCountChanged,      this, &EngineInfoWidget::onNodesChanged);
    connect(m_model, &UsiCommLogModel::nodesPerSecondChanged, this, &EngineInfoWidget::onNpsChanged);
    connect(m_model, &UsiCommLogModel::hashUsageChanged,      this, &EngineInfoWidget::onHashChanged);
    onNameChanged(); onPredChanged(); onSearchedChanged(); onDepthChanged(); onNodesChanged(); onNpsChanged(); onHashChanged();
}

void EngineInfoWidget::onPredChanged()    { m_pred->setText(m_model->predictiveMove()); }
void EngineInfoWidget::onSearchedChanged(){ m_searched->setText(m_model->searchedMove()); }
void EngineInfoWidget::onDepthChanged()   { m_depth->setText(m_model->searchDepth()); }
void EngineInfoWidget::onNodesChanged()   { m_nodes->setText(m_model->nodeCount()); }
void EngineInfoWidget::onNpsChanged()     { m_nps  ->setText(m_model->nodesPerSecond()); }
void EngineInfoWidget::onHashChanged()    { m_hash ->setText(m_model->hashUsage()); }

// engineinfowidget.cpp
void EngineInfoWidget::setDisplayNameFallback(const QString& name) {
    m_fallbackName = name;
    // モデル未設定 or まだ空ならフォールバックを表示
    if (!m_model || m_model->engineName().isEmpty()) {
        m_engineName->setText(m_fallbackName);
    }
}

void EngineInfoWidget::onNameChanged() {
    const QString n = m_model ? m_model->engineName() : QString();
    m_engineName->setText(n.isEmpty() ? m_fallbackName : n);
}

