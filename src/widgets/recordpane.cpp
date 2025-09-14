#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"

#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QTextBrowser>
#include <QSplitter>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPalette>
#include <QIcon>
#include <QSize>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QTimer>

#include "evaluationchartwidget.h"
// KifuRecordListModel / KifuBranchListModel は前方宣言で十分（ここでは include 不要）

RecordPane::RecordPane(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    wireSignals(); // モデルに依存しないシグナルだけ先に配線
}

void RecordPane::buildUi()
{
    // --- 棋譜テーブル（左上） ---
    m_kifu = new QTableView(this);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->verticalHeader()->setVisible(false);

    // --- 矢印ボタン群（左下） ---
    m_btn1 = new QPushButton(this);
    m_btn2 = new QPushButton(this);
    m_btn3 = new QPushButton(this);
    m_btn4 = new QPushButton(this);
    m_btn5 = new QPushButton(this);
    m_btn6 = new QPushButton(this);

    // 既存実装と同じ配色（緑）
    {
        QPalette pal;
        pal.setColor(QPalette::Button, QColor(79, 146, 114));
        for (QPushButton* b : {m_btn1,m_btn2,m_btn3,m_btn4,m_btn5,m_btn6})
            b->setPalette(pal);
    }

    // アイコンとサイズ（既存のリソース名を流用）
    m_btn1->setIcon(QIcon(":/icons/gtk-media-next-rtl.png"));
    m_btn2->setIcon(QIcon(":/icons/gtk-media-forward-rtl.png"));
    m_btn3->setIcon(QIcon(":/icons/gtk-media-play-rtr.png"));
    m_btn4->setIcon(QIcon(":/icons/gtk-media-play-ltr.png"));
    m_btn5->setIcon(QIcon(":/icons/gtk-media-forward-ltr.png"));
    m_btn6->setIcon(QIcon(":/icons/gtk-media-next-ltr.png"));

    for (QPushButton* b : {m_btn1,m_btn2,m_btn3,m_btn4,m_btn5,m_btn6})
        b->setIconSize(QSize(32,32));

    auto* arrowsLay = new QHBoxLayout;
    arrowsLay->setContentsMargins(0,0,0,0);
    arrowsLay->setSpacing(6);
    arrowsLay->addWidget(m_btn1);
    arrowsLay->addWidget(m_btn2);
    arrowsLay->addWidget(m_btn3);
    arrowsLay->addWidget(m_btn4);
    arrowsLay->addWidget(m_btn5);
    arrowsLay->addWidget(m_btn6);

    m_arrows = new QWidget(this);
    m_arrows->setLayout(arrowsLay);
    m_arrows->setFixedHeight(50);
    m_arrows->setMinimumWidth(600);

    // 左側：棋譜 + 矢印 を縦積み
    auto* leftLay = new QVBoxLayout;
    leftLay->setContentsMargins(0,0,0,0);
    leftLay->setSpacing(6);
    leftLay->addWidget(m_kifu);
    leftLay->addWidget(m_arrows);

    QWidget* left = new QWidget(this);
    left->setLayout(leftLay);
    left->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // --- 右側：分岐 + コメント（縦分割） ---
    m_branch = new QTableView(this);
    m_branch->setSelectionMode(QAbstractItemView::SingleSelection);
    m_branch->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branch->verticalHeader()->setVisible(false);

    m_branchText = new QTextBrowser(this);
    m_branchText->setReadOnly(true);
    m_branchText->setOpenExternalLinks(true);
    m_branchText->setOpenLinks(true);
    m_branchText->setPlaceholderText(tr("コメントを表示"));

    m_right = new QSplitter(Qt::Vertical, this);
    m_right->addWidget(m_branch);
    m_right->addWidget(m_branchText);
    m_right->setChildrenCollapsible(false);
    m_right->setSizes({300, 200});

    // --- 左右分割 ---
    m_lr = new QSplitter(Qt::Horizontal, this);
    m_lr->addWidget(left);
    m_lr->addWidget(m_right);
    m_lr->setChildrenCollapsible(false);
    m_lr->setSizes({600, 400});

    // --- 評価値グラフ（スクロール） ---
    m_eval = new EvaluationChartWidget(this);
    m_eval->setMinimumHeight(170);
    m_eval->setFixedWidth(10000); // 横方向に長くしてスクロール

    auto* evalWrapLay = new QHBoxLayout;
    evalWrapLay->setContentsMargins(0,0,0,0);
    evalWrapLay->addWidget(m_eval);

    QWidget* evalWrap = new QWidget(this);
    evalWrap->setLayout(evalWrapLay);

    m_scroll = new QScrollArea(this);
    m_scroll->setFixedHeight(200);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_scroll->setWidget(evalWrap);

    // --- ルート（上下積み：上=左右スプリッタ、下=評価スクロール） ---
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(8);
    root->addWidget(m_lr);
    root->addWidget(m_scroll);
    setLayout(root);
}

void RecordPane::wireSignals()
{
    // 分岐テーブルの「選択/アクティベート」を上位へ中継
    connect(m_branch, &QTableView::activated,
            this, &RecordPane::branchActivated, Qt::UniqueConnection);
    connect(m_branch, &QTableView::clicked,
            this, &RecordPane::branchActivated, Qt::UniqueConnection);
}

void RecordPane::setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel)
{
    Q_ASSERT(m_kifu);
    Q_ASSERT(m_branch);
    if (!m_kifu || !m_branch) buildUi();

    // --- 棋譜テーブル ---
    m_kifu->setModel(recModel);

    // 行追加→自動スクロール（多重接続防止）
    if (auto* model = m_kifu->model()) {
        if (m_connRowsInserted)
            disconnect(m_connRowsInserted);
        m_connRowsInserted = connect(model, &QAbstractItemModel::rowsInserted,
                                     m_kifu, [this](const QModelIndex&, int, int) {
                                         m_kifu->scrollToBottom();
                                     });
    }

    // 行選択の中継（多重接続防止 & メンバ関数スロット化）
    if (auto* sel = m_kifu->selectionModel()) {
        if (m_connRowChanged)
            disconnect(m_connRowChanged);
        m_connRowChanged = connect(sel,
                                   &QItemSelectionModel::currentRowChanged,
                                   this,
                                   &RecordPane::onKifuCurrentRowChanged,
                                   Qt::UniqueConnection);
    }

    if (auto* hh = m_kifu->horizontalHeader()) {
        hh->setSectionResizeMode(0, QHeaderView::Stretch);
        hh->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);

    // ① 先に既存接続を解除
    if (m_connKifuCurrentRow) {
        QObject::disconnect(m_connKifuCurrentRow);
        m_connKifuCurrentRow = {};
    }

    // ② selectionModel() が確実にできてから接続（即時にあれば即時、なければ次のイベントループで）
    auto connectCurrentRow = [this]() {
        if (!m_kifu) return;
        if (auto* sel = m_kifu->selectionModel()) {
            // UniqueConnection は使わず、自前で前の接続を外す方が安全
            m_connKifuCurrentRow =
                connect(sel, &QItemSelectionModel::currentRowChanged,
                        this,
                        [this](const QModelIndex& idx, const QModelIndex&) {
                            emit mainRowChanged(idx.isValid() ? idx.row() : 0);
                        });
        }
    };

    if (m_kifu->selectionModel()) {
        connectCurrentRow();
    } else {
        // setModel 後すぐは selectionModel がまだ未設定なことがあるため遅延
        QTimer::singleShot(0, this, connectCurrentRow);
    }

    // --- 分岐テーブル ---
    m_branch->setModel(brModel);
    if (auto* hh2 = m_branch->horizontalHeader()) {
        hh2->setSectionResizeMode(0, QHeaderView::Stretch);
    }
}

QTableView* RecordPane::kifuView() const { return m_kifu; }
QTableView* RecordPane::branchView() const { return m_branch; }
EvaluationChartWidget* RecordPane::evalChart() const { return m_eval; }

void RecordPane::setArrowButtonsEnabled(bool on)
{
    for (QPushButton* b : {m_btn1,m_btn2,m_btn3,m_btn4,m_btn5,m_btn6})
        if (b) b->setEnabled(on);
}

void RecordPane::onKifuRowsInserted(const QModelIndex&, int, int)
{
    if (m_kifu) m_kifu->scrollToBottom();
}

void RecordPane::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    emit mainRowChanged(cur.isValid() ? cur.row() : 0);
}
