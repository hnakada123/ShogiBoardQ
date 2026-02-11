/// @file recordpane.cpp
/// @brief 棋譜欄ペインクラスの実装

#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "settingsservice.h"

#include "loggingcategory.h"
#include <QTextBrowser>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QTextBrowser>
#include <QSplitter>
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
#include <QFont>

// KifuRecordListModel / KifuBranchListModel は前方宣言で十分（ここでは include 不要）

RecordPane::RecordPane(QWidget* parent)
    : QWidget(parent)
    , m_fontSize(SettingsService::kifuPaneFontSize())
{
    buildUi();
    wireSignals(); // モデルに依存しないシグナルだけ先に配線
}

void RecordPane::buildUi()
{
    // --- 棋譜テーブル（左側） ---
    m_kifu = new QTableView(this);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->verticalHeader()->setVisible(false);
    // 行の高さを1行分のテキストに固定（コメント欄が複数行でも1行表示に制限）
    const int rowHeight = m_kifu->fontMetrics().height() + 4;  // フォント高さ + 最小限のパディング
    m_kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_kifu->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // テキストが溢れる場合は省略記号（...）で表示
    m_kifu->setWordWrap(false);
    m_kifu->setTextElideMode(Qt::ElideRight);

    // ヘッダーを青色にスタイル設定
    // QTableView::item の固定 background-color を削除し、モデルの BackgroundRole を使用
    // ただし選択行のスタイルは明示的に黄色に設定（Qtデフォルトの青/紫を防ぐ）
    m_kifu->setStyleSheet(QStringLiteral(
        "QTableView {"
        "  background-color: #ffffff;"
        "}"
        "QTableView::item:selected:active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QTableView::item:selected:!active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}"
    ));

    // --- 文字サイズ変更ボタン ---
    m_btnFontUp = new QPushButton(this);
    m_btnFontDown = new QPushButton(this);
    m_btnFontUp->setText(QStringLiteral("A+"));
    m_btnFontDown->setText(QStringLiteral("A-"));
    m_btnFontUp->setToolTip(tr("文字を大きくする"));
    m_btnFontDown->setToolTip(tr("文字を小さくする"));

    // 文字サイズボタンのスタイル設定（青系の背景色）
    const QString fontBtnStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #4A6FA5;"
        "  color: white;"
        "  border: 1px solid #3d5a80;"
        "  border-radius: 3px;"
        "  padding: 2px 4px;"
        "  font-weight: bold;"
        "  min-width: 28px;"
        "  max-width: 36px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5a82b8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3d5a80;"
        "}"
    );
    m_btnFontUp->setStyleSheet(fontBtnStyle);
    m_btnFontDown->setStyleSheet(fontBtnStyle);
    m_btnFontUp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_btnFontDown->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // --- しおり編集ボタン ---
    m_btnBookmarkEdit = new QPushButton(this);
    m_btnBookmarkEdit->setIcon(QIcon(QStringLiteral(":/images/actions/actionEditBookmark.svg")));
    m_btnBookmarkEdit->setIconSize(QSize(20, 20));
    m_btnBookmarkEdit->setToolTip(tr("しおりを編集"));
    m_btnBookmarkEdit->setStyleSheet(fontBtnStyle);
    m_btnBookmarkEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // --- 列表示トグルボタン ---
    const QString toggleBtnStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #8a9bb5;"
        "  color: white;"
        "  border: 1px solid #6b7d99;"
        "  border-radius: 3px;"
        "  padding: 2px 4px;"
        "  min-width: 28px;"
        "  max-width: 36px;"
        "}"
        "QPushButton:checked {"
        "  background-color: #4A6FA5;"
        "  border: 1px solid #3d5a80;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5a82b8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3d5a80;"
        "}"
    );

    m_btnToggleTime = new QPushButton(this);
    m_btnToggleTime->setCheckable(true);
    m_btnToggleTime->setChecked(SettingsService::kifuTimeColumnVisible());
    m_btnToggleTime->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleTimeColumn.svg")));
    m_btnToggleTime->setIconSize(QSize(20, 20));
    m_btnToggleTime->setToolTip(tr("消費時間列の表示/非表示"));
    m_btnToggleTime->setStyleSheet(toggleBtnStyle);
    m_btnToggleTime->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_btnToggleBookmark = new QPushButton(this);
    m_btnToggleBookmark->setCheckable(true);
    m_btnToggleBookmark->setChecked(SettingsService::kifuBookmarkColumnVisible());
    m_btnToggleBookmark->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleBookmarkColumn.svg")));
    m_btnToggleBookmark->setIconSize(QSize(20, 20));
    m_btnToggleBookmark->setToolTip(tr("しおり列の表示/非表示"));
    m_btnToggleBookmark->setStyleSheet(toggleBtnStyle);
    m_btnToggleBookmark->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_btnToggleComment = new QPushButton(this);
    m_btnToggleComment->setCheckable(true);
    m_btnToggleComment->setChecked(SettingsService::kifuCommentColumnVisible());
    m_btnToggleComment->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleCommentColumn.svg")));
    m_btnToggleComment->setIconSize(QSize(20, 20));
    m_btnToggleComment->setToolTip(tr("コメント列の表示/非表示"));
    m_btnToggleComment->setStyleSheet(toggleBtnStyle);
    m_btnToggleComment->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // --- ナビゲーションボタン群（中央に縦配置） ---
    m_btn1 = new QPushButton(this);
    m_btn2 = new QPushButton(this);
    m_btn3 = new QPushButton(this);
    m_btn4 = new QPushButton(this);
    m_btn5 = new QPushButton(this);
    m_btn6 = new QPushButton(this);

    // 簡易なテキストボタンに変更（画像アイコン不使用）
    m_btn1->setText(tr("▲|"));
    m_btn2->setText(tr("▲▲"));
    m_btn3->setText(tr("▲"));
    m_btn4->setText(tr("▼"));
    m_btn5->setText(tr("▼▼"));
    m_btn6->setText(tr("▼|"));

    // ツールチップを設定
    m_btn1->setToolTip(tr("最初に戻る"));
    m_btn2->setToolTip(tr("10手戻る"));
    m_btn3->setToolTip(tr("1手戻る"));
    m_btn4->setToolTip(tr("1手進む"));
    m_btn5->setToolTip(tr("10手進む"));
    m_btn6->setToolTip(tr("最後に進む"));

    // ボタンのスタイル設定（緑系の背景色、幅を狭く）
    const QString btnStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #4F9272;"
        "  color: white;"
        "  border: 1px solid #3d7259;"
        "  border-radius: 3px;"
        "  padding: 3px 4px;"
        "  font-weight: bold;"
        "  min-width: 28px;"
        "  max-width: 36px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5ba583;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3d7259;"
        "}"
    );

    const QList<QPushButton*> allBtns = {m_btn1, m_btn2, m_btn3, m_btn4, m_btn5, m_btn6};
    for (QPushButton* const b : allBtns) {
        b->setStyleSheet(btnStyle);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // ナビゲーションボタンを縦に配置（文字サイズボタンも縦並びで上に追加）
    auto* navLay = new QVBoxLayout;
    navLay->setContentsMargins(2, 4, 2, 4);
    navLay->setSpacing(3);
    navLay->addWidget(m_btnFontUp, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnFontDown, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnBookmarkEdit, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleTime, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleBookmark, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleComment, 0, Qt::AlignHCenter);
    navLay->addStretch();
    navLay->addWidget(m_btn1, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn2, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn3, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn4, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn5, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn6, 0, Qt::AlignHCenter);
    navLay->addStretch();

    m_navButtons = new QWidget(this);
    m_navButtons->setLayout(navLay);
    m_navButtons->setFixedWidth(50);
    m_navButtons->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // --- 右側：分岐候補欄 ---
    m_branch = new QTableView(this);
    m_branch->setSelectionMode(QAbstractItemView::SingleSelection);
    m_branch->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branch->verticalHeader()->setVisible(false);
    // 行の高さを1行分のテキストに固定
    const int branchRowHeight = m_branch->fontMetrics().height() + 4;
    m_branch->verticalHeader()->setDefaultSectionSize(branchRowHeight);
    m_branch->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_branch->setWordWrap(false);

    // ヘッダーを青色、データ欄を白色にスタイル設定（選択行は黄色を維持）
    m_branch->setStyleSheet(QStringLiteral(
        "QTableView {"
        "  background-color: #ffffff;"
        "  selection-background-color: #ffff00;"
        "  selection-color: black;"
        "}"
        "QTableView::item {"
        "  background-color: #ffffff;"
        "}"
        "QTableView::item:selected:active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QTableView::item:selected:!active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}"
    ));

    // 分岐候補欄を縦レイアウトでラップ（「本譜に戻る」ボタン用）
    m_branchContainer = new QWidget(this);
    auto* branchLay = new QVBoxLayout(m_branchContainer);
    branchLay->setContentsMargins(0, 0, 0, 0);
    branchLay->setSpacing(2);
    branchLay->addWidget(m_branch);
    m_branchContainer->setFixedWidth(120);  // 指し手が表示できる程度の幅

    // --- 左右分割（棋譜 | ナビボタン | 分岐候補） ---
    m_lr = new QSplitter(Qt::Horizontal, this);
    m_lr->addWidget(m_kifu);
    m_lr->addWidget(m_navButtons);
    m_lr->addWidget(m_branchContainer);
    m_lr->setChildrenCollapsible(false);
    m_lr->setSizes({800, 50, 120});

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addWidget(m_lr);
    setLayout(root);
}

void RecordPane::wireSignals()
{
    // 既存：
    connect(m_branch, &QTableView::activated, this, &RecordPane::branchActivated, Qt::UniqueConnection);
    connect(m_branch, &QTableView::clicked, this, &RecordPane::branchActivated, Qt::UniqueConnection);

    // 文字サイズ変更ボタンの接続
    connect(m_btnFontUp, &QPushButton::clicked, this, &RecordPane::onFontIncrease);
    connect(m_btnFontDown, &QPushButton::clicked, this, &RecordPane::onFontDecrease);

    // しおり編集ボタンの接続
    connect(m_btnBookmarkEdit, &QPushButton::clicked, this, &RecordPane::bookmarkEditRequested);

    // 列表示トグルボタンの接続
    connect(m_btnToggleTime, &QPushButton::toggled, this, &RecordPane::onToggleTimeColumn);
    connect(m_btnToggleBookmark, &QPushButton::toggled, this, &RecordPane::onToggleBookmarkColumn);
    connect(m_btnToggleComment, &QPushButton::toggled, this, &RecordPane::onToggleCommentColumn);

    // 棋譜表の選択ハイライトを黄色に
    setupKifuSelectionAppearance();

    // 分岐候補欄の選択ハイライトを黄色に
    setupBranchViewSelectionAppearance();

    // 初期フォントサイズを適用
    applyFontSize(m_fontSize);
}

void RecordPane::setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel)
{
    if (!m_kifu || !m_branch) {
        qCWarning(lcUi).noquote() << "[RecordPane] setModels: m_kifu or m_branch is null, rebuilding UI";
        buildUi();
    }

    qCDebug(lcUi) << "setModels called";
    qCDebug(lcUi) << "m_kifu styleSheet in setModels:" << m_kifu->styleSheet().left(100) << "...";

    // --- 棋譜テーブル ---
    m_kifu->setModel(recModel);

    // モデルに行がある場合、最初の行を選択する
    if (recModel && recModel->rowCount() > 0) {
        qCDebug(lcUi) << "[RecordPane] setModels: model has rows, selecting first row";
        QTimer::singleShot(0, this, [this]() {
            if (m_kifu && m_kifu->model() && m_kifu->model()->rowCount() > 0) {
                if (auto* sel = m_kifu->selectionModel()) {
                    const QModelIndex top = m_kifu->model()->index(0, 0);
                    sel->setCurrentIndex(top, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    qCDebug(lcUi) << "[RecordPane] initial row selected";
                }
            }
        });
    }

    // 行追加→自動スクロール（多重接続防止）
    if (auto* model = m_kifu->model()) {
        if (m_connRowsInserted)
            disconnect(m_connRowsInserted);
        m_connRowsInserted = connect(model, &QAbstractItemModel::rowsInserted,
                                     m_kifu, [this](const QModelIndex&, int, int) {
                                         m_kifu->scrollToBottom();
                                     });
    }

    // 注意: 行選択の中継は後続の connectCurrentRow で行うため、ここでは接続しない
    // （二重接続によるシグナル二重発火を防止）
    if (m_connRowChanged) {
        disconnect(m_connRowChanged);
        m_connRowChanged = {};
    }

    applyColumnVisibility();
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);

    // ① 先に既存接続を解除
    if (m_connKifuCurrentRow) {
        QObject::disconnect(m_connKifuCurrentRow);
        m_connKifuCurrentRow = {};
    }

    // ② selectionModel() が確実にできてから接続（即時にあれば即時、なければ次のイベントループで）
    // ラムダを使用せず、メンバ関数を使用（CLAUDE.md準拠）
    if (auto* sel = m_kifu->selectionModel()) {
        m_connKifuCurrentRow =
            connect(sel, &QItemSelectionModel::currentRowChanged,
                    this, &RecordPane::onKifuCurrentRowChanged);
    } else {
        // setModel 後すぐは selectionModel がまだ未設定なことがあるため遅延
        QTimer::singleShot(0, this, &RecordPane::connectKifuCurrentRowChanged);
    }

    // --- 分岐テーブル ---
    m_branch->setModel(brModel);
    if (auto* hh2 = m_branch->horizontalHeader()) {
        hh2->setSectionResizeMode(0, QHeaderView::Stretch);
    }

    // 分岐テーブルの選択変更時にモデルのハイライト行を更新
    if (auto* sel = m_branch->selectionModel()) {
        // 既存の接続があれば解除
        if (m_connBranchCurrentRow) {
            disconnect(m_connBranchCurrentRow);
        }
        m_connBranchCurrentRow = connect(sel, &QItemSelectionModel::currentRowChanged,
                this, [brModel](const QModelIndex& current, const QModelIndex&) {
                    if (brModel && current.isValid()) {
                        brModel->setCurrentHighlightRow(current.row());
                    }
                });
    }
}

QTableView* RecordPane::kifuView() const { return m_kifu; }
QTableView* RecordPane::branchView() const { return m_branch; }

void RecordPane::setArrowButtonsEnabled(bool on)
{
    const QList<QPushButton*> btns = {m_btn1, m_btn2, m_btn3, m_btn4, m_btn5, m_btn6};
    for (QPushButton* const b : btns)
        if (b) b->setEnabled(on);
}

void RecordPane::setKifuViewEnabled(bool on)
{
    // 共通のスタイルシート（ヘッダー青色、選択行黄色）
    // QTableView::item の固定 background-color を削除し、モデルの BackgroundRole を使用
    static const QString kBaseStyleSheet = QStringLiteral(
        "QTableView {"
        "  background-color: #ffffff;"
        "}"
        "QTableView::item:selected:active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QTableView::item:selected:!active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}"
    );

    if (m_kifu) {
        qCDebug(lcUi) << "setKifuViewEnabled called, on=" << on;

        m_navigationDisabled = !on;

        // setEnabled(false) を使うとテキストがグレーアウトされるため、
        // 代わりにビューポートのみマウスイベントを透過させてクリックを無効化する
        // ビュー自体には設定しないことで、スクロールバーは操作可能にする
        m_kifu->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents, !on);

        if (!on) {
            // フォーカスを無効にする
            m_kifu->setFocusPolicy(Qt::NoFocus);
            m_kifu->clearFocus();

            // スタイルシートでcurrentIndex（フォーカスセル）のハイライトを無効化
            // ベーススタイルに無効化用のスタイルを追加
            m_kifu->setStyleSheet(kBaseStyleSheet + QStringLiteral(
                "QTableView::item:focus { background-color: transparent; }"
                "QTableView::item:selected:focus { background-color: transparent; }"
            ));
            qCDebug(lcUi) << "setKifuViewEnabled: disabled stylesheet applied";

            // 選択をクリア（シグナルをブロックして盤面同期を防ぐ）
            if (QItemSelectionModel* sel = m_kifu->selectionModel()) {
                const bool wasBlocked = sel->blockSignals(true);
                sel->clearSelection();
                sel->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
                sel->blockSignals(wasBlocked);
            }

            m_kifu->viewport()->update();
        } else {
            // 有効化時はフォーカスを受け付けるように戻す
            m_kifu->setFocusPolicy(Qt::StrongFocus);
            // ベーススタイルシートを適用
            m_kifu->setStyleSheet(kBaseStyleSheet);
            qCDebug(lcUi) << "setKifuViewEnabled: base stylesheet restored (enabled)";
        }
    }
}

void RecordPane::setNavigationEnabled(bool on)
{
    setArrowButtonsEnabled(on);
    setKifuViewEnabled(on);
}

void RecordPane::onKifuRowsInserted(const QModelIndex&, int, int)
{
    if (m_kifu) m_kifu->scrollToBottom();
}

void RecordPane::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    const int row = cur.isValid() ? cur.row() : 0;
    qCDebug(lcUi).noquote() << "[RecordPane] onKifuCurrentRowChanged: emitting mainRowChanged(" << row << ")";
    emit mainRowChanged(row);
}

void RecordPane::connectKifuCurrentRowChanged()
{
    if (!m_kifu) return;
    if (auto* sel = m_kifu->selectionModel()) {
        // 既存接続を解除してから再接続
        if (m_connKifuCurrentRow) {
            QObject::disconnect(m_connKifuCurrentRow);
        }
        m_connKifuCurrentRow =
            connect(sel, &QItemSelectionModel::currentRowChanged,
                    this, &RecordPane::onKifuCurrentRowChanged);
    }
}

void RecordPane::setBranchCommentText(const QString& text)
{
    Q_UNUSED(text)
    // コメントは棋譜欄のコメント列に表示するため、ここでは何もしない
}

void RecordPane::setBranchCommentHtml(const QString& html)
{
    Q_UNUSED(html)
    // コメントは棋譜欄のコメント列に表示するため、ここでは何もしない
}

// MainWindow から呼ばれる getter 実装
CommentTextAdapter* RecordPane::commentLabel()
{
    return &m_commentAdapter;
}

// RecordPane に「本譜に戻る」ボタンを遅延生成して差し込む。
// 分岐候補欄コンテナの下部に挿入する。
// 既に存在する場合はそれを返す。
QPushButton* RecordPane::backToMainButton()
{
    if (!m_branchContainer) return nullptr;

    // 既に作ってあればそれを返す
    if (auto* existed = this->findChild<QPushButton*>("backToMainButton"))
        return existed;

    auto* btn = new QPushButton(tr("本譜に戻る"), this);
    btn->setObjectName(QStringLiteral("backToMainButton"));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setVisible(false); // 初期は非表示
    btn->setToolTip(tr("現在の手数で本譜（メインライン）に戻る"));

    // 分岐候補欄コンテナのレイアウトに追加
    if (auto* lay = qobject_cast<QVBoxLayout*>(m_branchContainer->layout())) {
        lay->addWidget(btn);
    }
    return btn;
}

void RecordPane::setupKifuSelectionAppearance()
{
    if (!m_kifu) return;

    qCDebug(lcUi) << "setupKifuSelectionAppearance called";
    qCDebug(lcUi) << "m_kifu styleSheet before:" << m_kifu->styleSheet();

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

    qCDebug(lcUi) << "setupKifuSelectionAppearance: palette Highlight (Active):" << pal.color(QPalette::Active, QPalette::Highlight);
    qCDebug(lcUi) << "setupKifuSelectionAppearance: palette Highlight (Inactive):" << pal.color(QPalette::Inactive, QPalette::Highlight);
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

void RecordPane::onFontIncrease(bool /*checked*/)
{
    if (m_fontSize < 24) {
        m_fontSize += 1;
        applyFontSize(m_fontSize);
        SettingsService::setKifuPaneFontSize(m_fontSize);
    }
}

void RecordPane::onFontDecrease(bool /*checked*/)
{
    if (m_fontSize > 8) {
        m_fontSize -= 1;
        applyFontSize(m_fontSize);
        SettingsService::setKifuPaneFontSize(m_fontSize);
    }
}

void RecordPane::onToggleTimeColumn(bool checked)
{
    if (m_kifu) {
        m_kifu->setColumnHidden(1, !checked);
        updateColumnResizeModes();
    }
    SettingsService::setKifuTimeColumnVisible(checked);
}

void RecordPane::onToggleBookmarkColumn(bool checked)
{
    if (m_kifu) {
        m_kifu->setColumnHidden(2, !checked);
        updateColumnResizeModes();
    }
    SettingsService::setKifuBookmarkColumnVisible(checked);
}

void RecordPane::onToggleCommentColumn(bool checked)
{
    if (m_kifu) {
        m_kifu->setColumnHidden(3, !checked);
        updateColumnResizeModes();
    }
    SettingsService::setKifuCommentColumnVisible(checked);
}

void RecordPane::updateColumnResizeModes()
{
    if (!m_kifu) return;
    auto* hh = m_kifu->horizontalHeader();
    if (!hh) return;

    // col 0: 常に ResizeToContents
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    const bool col1Visible = !m_kifu->isColumnHidden(1);
    const bool col2Visible = !m_kifu->isColumnHidden(2);
    const bool col3Visible = !m_kifu->isColumnHidden(3);

    // 最右の表示列が Stretch を受け持つ
    if (col3Visible) {
        // col 3 が表示されていれば col 3 が Stretch
        if (col1Visible) {
            hh->setSectionResizeMode(1, QHeaderView::Fixed);
            hh->resizeSection(1, 130);
        }
        if (col2Visible)
            hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::Stretch);
    } else if (col2Visible) {
        // col 3 非表示 → col 2 が Stretch
        if (col1Visible) {
            hh->setSectionResizeMode(1, QHeaderView::Fixed);
            hh->resizeSection(1, 130);
        }
        hh->setSectionResizeMode(2, QHeaderView::Stretch);
    } else if (col1Visible) {
        // col 2, 3 非表示 → col 1 が Stretch
        hh->setSectionResizeMode(1, QHeaderView::Stretch);
    } else {
        // col 1, 2, 3 すべて非表示 → col 0 が Stretch
        hh->setSectionResizeMode(0, QHeaderView::Stretch);
    }
}

void RecordPane::applyColumnVisibility()
{
    if (!m_kifu) return;

    const bool timeVisible = SettingsService::kifuTimeColumnVisible();
    const bool bookmarkVisible = SettingsService::kifuBookmarkColumnVisible();
    const bool commentVisible = SettingsService::kifuCommentColumnVisible();

    m_kifu->setColumnHidden(1, !timeVisible);
    m_kifu->setColumnHidden(2, !bookmarkVisible);
    m_kifu->setColumnHidden(3, !commentVisible);

    updateColumnResizeModes();
}

void RecordPane::applyFontSize(int size)
{
    QFont font;
    font.setPointSize(size);

    // 棋譜欄にフォントサイズを適用
    if (m_kifu) {
        m_kifu->setFont(font);
        // 行の高さもフォントサイズに合わせて更新
        const int rowHeight = m_kifu->fontMetrics().height() + 4;
        m_kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    }

    // 分岐候補欄にフォントサイズを適用
    if (m_branch) {
        m_branch->setFont(font);
        // 行の高さもフォントサイズに合わせて更新
        const int rowHeight = m_branch->fontMetrics().height() + 4;
        m_branch->verticalHeader()->setDefaultSectionSize(rowHeight);
    }
}
