#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"

#include <QTextBrowser>
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

    // ★ 追加: アダプタに実体を紐付け
    m_commentAdapter.reset(m_branchText);

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
    m_eval->setMinimumHeight(220);
    m_eval->setFixedWidth(10000); // 横方向に長くしてスクロール

    auto* evalWrapLay = new QHBoxLayout;
    evalWrapLay->setContentsMargins(0,0,0,0);
    evalWrapLay->addWidget(m_eval);

    m_evalWrap = new QWidget(this);
    m_evalWrap->setLayout(evalWrapLay);
    m_evalWrap->setMinimumHeight(220);

    m_scroll = new QScrollArea(this);
    m_scroll->setFixedHeight(250);  // コントロールパネル(28) + グラフ(200) + 余裕
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_scroll->setWidgetResizable(false);  // 明示的にfalseに設定
    m_scroll->setWidget(m_evalWrap);

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
    // 既存：
    connect(m_branch, &QTableView::activated, this, &RecordPane::branchActivated, Qt::UniqueConnection);
    connect(m_branch, &QTableView::clicked, this, &RecordPane::branchActivated, Qt::UniqueConnection);

    // ★ 追加：棋譜表の選択ハイライトを黄色に
    setupKifuSelectionAppearance();

    // ★ 追加：分岐候補欄の選択ハイライトを黄色に
    setupBranchViewSelectionAppearance();
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

void RecordPane::setBranchCommentText(const QString& text)
{
    if (m_branchText) m_branchText->setPlainText(text);
}

void RecordPane::setBranchCommentHtml(const QString& html)
{
    if (m_branchText) m_branchText->setHtml(html);
}

// ★ 追加: MainWindow から呼ばれる getter 実装
CommentTextAdapter* RecordPane::commentLabel()
{
    return &m_commentAdapter;
}

// RecordPane に「本譜に戻る」ボタンを遅延生成して差し込む。
// 右側の縦スプリッタ m_right の [分岐テーブル, ←このボタン, コメント欄] の順に挿入する。
// 既に存在する場合はそれを返す。
QPushButton* RecordPane::backToMainButton()
{
    if (!m_right) return nullptr;

    // 既に作ってあればそれを返す
    if (auto* existed = this->findChild<QPushButton*>("backToMainButton"))
        return existed;

    auto* btn = new QPushButton(tr("本譜に戻る"), this);
    btn->setObjectName(QStringLiteral("backToMainButton"));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setVisible(false); // 初期は非表示
    btn->setToolTip(tr("現在の手数で本譜（メインライン）に戻る"));

    // m_right の 1 番目（分岐テーブルとコメントの間）に挿入
    m_right->insertWidget(1, btn);
    return btn;
}

void RecordPane::setupKifuSelectionAppearance()
{
    if (!m_kifu) return;

    // 行選択（行全体を黄色でハイライトするために必須）
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);

    // 選択ハイライト色を黄色に（フォーカスあり/なし両方に適用）
    QPalette pal = m_kifu->palette();
    const QColor kSelYellow(255, 255, 0); // 黄色
    pal.setColor(QPalette::Active,   QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Active,   QPalette::HighlightedText, Qt::black);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::black);
    m_kifu->setPalette(pal);
}

void RecordPane::setupBranchViewSelectionAppearance()
{
    if (!m_branch) return;

    // 行単位選択（行全体を黄色でハイライト）
    m_branch->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branch->setSelectionMode(QAbstractItemView::SingleSelection);

    // フォーカスの有無に関係なく黄色でハイライト
    QPalette pal = m_branch->palette();
    const QColor kSelYellow(255, 255, 0); // 黄色
    pal.setColor(QPalette::Active,   QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Active,   QPalette::HighlightedText, Qt::black);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::black);
    m_branch->setPalette(pal);
}

void RecordPane::setEvalChartHeight(int height)
{
    qDebug() << "[EVAL_HEIGHT] setEvalChartHeight called, height=" << height;

    // スクロールバー分（約20px）を引いた高さをグラフに設定
    const int evalHeight = qMax(height - 20, 200);

    if (m_eval) {
        qDebug() << "[EVAL_HEIGHT] m_eval: evalHeight=" << evalHeight
                 << "current minimumHeight=" << m_eval->minimumHeight()
                 << "current height=" << m_eval->height();
        m_eval->setMinimumHeight(evalHeight);
        m_eval->setMaximumHeight(evalHeight);
        m_eval->setFixedHeight(evalHeight);  // 明示的に高さを固定
        qDebug() << "[EVAL_HEIGHT] m_eval: after set, height=" << m_eval->height();
    } else {
        qDebug() << "[EVAL_HEIGHT] m_eval is NULL!";
    }

    if (m_evalWrap) {
        qDebug() << "[EVAL_HEIGHT] m_evalWrap: setting height=" << evalHeight;
        m_evalWrap->setMinimumHeight(evalHeight);
        m_evalWrap->setFixedHeight(evalHeight);
        m_evalWrap->resize(m_evalWrap->width(), evalHeight);
        qDebug() << "[EVAL_HEIGHT] m_evalWrap: after set, height=" << m_evalWrap->height();
    }

    if (m_scroll) {
        // 最小高さを確保（コントロールパネル28px + グラフ最小170px + スクロールバー20px）
        const int minHeight = 220;
        const int actualHeight = qMax(height, minHeight);
        qDebug() << "[EVAL_HEIGHT] m_scroll: minHeight=" << minHeight
                 << "actualHeight=" << actualHeight
                 << "current height=" << m_scroll->height();
        m_scroll->setFixedHeight(actualHeight);
        qDebug() << "[EVAL_HEIGHT] m_scroll: after setFixedHeight, height=" << m_scroll->height();
    } else {
        qDebug() << "[EVAL_HEIGHT] m_scroll is NULL!";
    }
}

int RecordPane::evalChartHeight() const
{
    return m_scroll ? m_scroll->height() : 200;
}
