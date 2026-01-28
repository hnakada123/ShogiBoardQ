#include "analysisresultspresenter.h"
#include <QDockWidget>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QScrollBar>
#include <QPushButton>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include "settingsservice.h"
#include "numeric_right_align_comma_delegate.h"
#include "kifuanalysislistmodel.h"

// 「表示」ボタン列用のデリゲート（選択時も色を維持）
class BoardButtonDelegate : public QStyledItemDelegate
{
public:
    explicit BoardButtonDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& /*index*/) const override
    {
        painter->save();

        // 背景色（常に青色）
        QColor bgColor(0x20, 0x9c, 0xee);
        painter->fillRect(option.rect, bgColor);

        // テキスト「表示」を白色で中央に描画
        painter->setPen(Qt::white);
        painter->drawText(option.rect, Qt::AlignCenter, QObject::tr("表示"));

        painter->restore();
    }
};

AnalysisResultsPresenter::AnalysisResultsPresenter(QObject* parent)
    : QObject(parent)
    , m_reflowTimer(new QTimer(this))
{
    m_reflowTimer->setSingleShot(true);
    m_reflowTimer->setInterval(0);
    connect(m_reflowTimer, &QTimer::timeout, this, &AnalysisResultsPresenter::reflowNow);
}

void AnalysisResultsPresenter::setDockWidget(QDockWidget* dock)
{
    m_dock = dock;
}

QWidget* AnalysisResultsPresenter::containerWidget()
{
    // コンテナウィジェットが未作成なら作成
    if (!m_container) {
        m_container = new QWidget;
    }

    // UIが未構築なら構築（モデルなしで）
    if (!m_uiBuilt) {
        buildUi(nullptr);
        m_uiBuilt = true;
    }

    return m_container;
}

void AnalysisResultsPresenter::showWithModel(KifuAnalysisListModel* model)
{
    // UIが未構築なら構築
    if (!m_uiBuilt) {
        buildUi(nullptr);
        m_uiBuilt = true;
    }

    // モデルを設定（既存の接続を切断してから新しいモデルを設定）
    if (m_view) {
        // 古いモデルの接続を切断
        QAbstractItemModel* oldModel = m_view->model();
        if (oldModel) {
            disconnect(oldModel, nullptr, this, nullptr);
        }
        // 古いselectionModelの接続も切断
        if (m_view->selectionModel()) {
            disconnect(m_view->selectionModel(), nullptr, this, nullptr);
        }

        // 新しいモデルを設定
        if (model) {
            m_view->setModel(model);
            connectModelSignals(model);
            setupHeaderConfiguration();

            // selectionModelの接続（モデル設定後に行う必要がある）
            connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged,
                    this, &AnalysisResultsPresenter::onTableSelectionChanged);
        }
    }

    // ドックを表示
    if (m_dock) {
        m_dock->setVisible(true);
        m_dock->raise();
    }

    // レイアウト調整
    m_reflowTimer->start();
}

void AnalysisResultsPresenter::buildUi(KifuAnalysisListModel* /*model*/)
{
    // コンテナウィジェットが未作成なら作成
    if (!m_container) {
        m_container = new QWidget;
    }

    m_view = new QTableView(m_container);

    // ヘッダー表示用に空のモデルを作成（起動時からヘッダーを表示するため）
    auto* emptyModel = new KifuAnalysisListModel(m_view);
    m_view->setModel(emptyModel);

    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setTextElideMode(Qt::ElideRight);
    
    // 選択行の背景色を黄色に設定、ヘッダーを青背景・白文字に設定（棋譜欄と同じスタイル）
    m_view->setStyleSheet(QStringLiteral(
        "QTableView::item:selected { background-color: rgb(255, 255, 0); color: black; }"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #4a9eff, stop:1 #2d7dd2);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #2d7dd2;"
        "}"
    ));
    
    // テーブル全体の水平スクロールは無効化（ヘッダーを常に表示するため）
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 盤面列（列6）のクリックで読み筋表示ウィンドウを開く
    connect(m_view, &QTableView::clicked, this, &AnalysisResultsPresenter::onTableClicked);

    // 注意: selectionModelへの接続はモデル設定後に行う（showWithModel()内）

    // 数値デリゲートを評価値と差の列に設定
    auto* numDelegate = new NumericRightAlignCommaDelegate(m_view);
    m_view->setItemDelegateForColumn(3, numDelegate); // 評価値
    m_view->setItemDelegateForColumn(5, numDelegate); // 差

    // 盤面列用のデリゲート（選択時も青色を維持）
    auto* boardBtnDelegate = new BoardButtonDelegate(m_view);
    m_view->setItemDelegateForColumn(6, boardBtnDelegate); // 盤面

    m_header = m_view->horizontalHeader();
    m_header->setMinimumSectionSize(30);

    // 空のモデルが設定されているのでヘッダー設定を適用
    setupHeaderConfiguration();

    // フォントサイズのA-/A+ボタン
    QPushButton* fontDecBtn = new QPushButton(tr("A-"), m_container);
    fontDecBtn->setFixedWidth(40);
    connect(fontDecBtn, &QPushButton::clicked, this, &AnalysisResultsPresenter::decreaseFontSize);

    QPushButton* fontIncBtn = new QPushButton(tr("A+"), m_container);
    fontIncBtn->setFixedWidth(40);
    connect(fontIncBtn, &QPushButton::clicked, this, &AnalysisResultsPresenter::increaseFontSize);

    // 棋譜解析中止ボタン
    m_stopButton = new QPushButton(tr("棋譜解析中止"), m_container);
    connect(m_stopButton, &QPushButton::clicked, this, &AnalysisResultsPresenter::stopRequested);

    // 閉じるボタン（ドックを非表示にする）
    QPushButton* closeButton = new QPushButton(tr("閉じる"), m_container);
    connect(closeButton, &QPushButton::clicked, this, &AnalysisResultsPresenter::saveWindowSize);

    // ボタン用の水平レイアウト
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(fontDecBtn);
    buttonLayout->addWidget(fontIncBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(closeButton);

    QVBoxLayout* lay = new QVBoxLayout(m_container);
    lay->setContentsMargins(8,8,8,8);
    lay->addWidget(m_view);
    lay->addLayout(buttonLayout);

    // ウィンドウリサイズ時に再レイアウト
    connect(m_view->verticalScrollBar(), &QAbstractSlider::rangeChanged,
            this, &AnalysisResultsPresenter::onScrollRangeChanged);
    
    // 保存されたフォントサイズを復元
    restoreFontSize();
}

void AnalysisResultsPresenter::connectModelSignals(KifuAnalysisListModel* model)
{
    if (!model) return;
    connect(model, &QAbstractItemModel::modelReset,   this, &AnalysisResultsPresenter::onModelReset);
    connect(model, &QAbstractItemModel::rowsInserted, this, &AnalysisResultsPresenter::onRowsInserted);
    connect(model, &QAbstractItemModel::dataChanged,  this, &AnalysisResultsPresenter::onDataChanged);
    connect(model, &QAbstractItemModel::layoutChanged,this, &AnalysisResultsPresenter::onLayoutChanged);
}

void AnalysisResultsPresenter::setupHeaderConfiguration()
{
    if (!m_header) return;

    // 列の構成: 指し手(0), 候補手(1), 一致(2), 評価値(3), 形勢(4), 差(5), 盤面(6), 読み筋(7)
    // Interactiveモードを使用し、reflowNowで適切な幅を設定
    m_header->setSectionResizeMode(0, QHeaderView::Interactive); // 指し手
    m_header->setSectionResizeMode(1, QHeaderView::Interactive); // 候補手
    m_header->setSectionResizeMode(2, QHeaderView::Interactive); // 一致
    m_header->setSectionResizeMode(3, QHeaderView::Interactive); // 評価値
    m_header->setSectionResizeMode(4, QHeaderView::Interactive); // 形勢
    m_header->setSectionResizeMode(5, QHeaderView::Interactive); // 差
    m_header->setSectionResizeMode(6, QHeaderView::Interactive); // 盤面
    m_header->setSectionResizeMode(7, QHeaderView::Stretch);     // 読み筋（残り幅を使用）
    m_header->setStretchLastSection(true);  // 最後の列を引き伸ばす
}

void AnalysisResultsPresenter::reflowNow()
{
    if (!m_view || !m_header) return;

    // 列の構成: 指し手(0), 候補手(1), 一致(2), 評価値(3), 形勢(4), 差(5), 盤面(6), 読み筋(7)
    // 各列の最小幅（ヘッダーテキストが見える幅）
    static const int minWidths[] = {100, 100, 40, 70, 90, 50, 50};

    // 各列のコンテンツに合わせてリサイズし、最小幅を保証
    for (int col = 0; col < 7; ++col) {
        m_view->resizeColumnToContents(col);
        int currentWidth = m_header->sectionSize(col);
        if (currentWidth < minWidths[col]) {
            m_header->resizeSection(col, minWidths[col]);
        }
    }

    // 読み筋列は残りの幅を使用（Stretchモード）
    m_header->setSectionResizeMode(7, QHeaderView::Stretch);
    m_header->setStretchLastSection(true);
}

void AnalysisResultsPresenter::onModelReset() { m_reflowTimer->start(); }

void AnalysisResultsPresenter::onRowsInserted(const QModelIndex&, int, int)
{
    m_reflowTimer->start();
    
    // 解析中は最後の行を自動選択（黄色ハイライト）
    if (m_isAnalyzing && m_view) {
        // 自動選択中はシグナル発行をスキップするためフラグを立てる
        m_autoSelecting = true;
        
        // 最後に追加された行を選択
        QAbstractItemModel* model = m_view->model();
        if (model) {
            int lastRow = model->rowCount() - 1;
            if (lastRow >= 0) {
                // 一時的に選択モードをSingleSelectionに変更（setCurrentIndexを機能させるため）
                m_view->setSelectionMode(QAbstractItemView::SingleSelection);
                
                QModelIndex idx = model->index(lastRow, 0);
                m_view->setCurrentIndex(idx);
                m_view->scrollTo(idx);  // 最後の行が見えるようにスクロール
                
                // 選択モードをNoSelectionに戻す（ユーザークリックを無効化）
                m_view->setSelectionMode(QAbstractItemView::NoSelection);
            }
        }
        
        m_autoSelecting = false;
    }
}

void AnalysisResultsPresenter::onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onLayoutChanged() { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onScrollRangeChanged(int, int) { m_reflowTimer->start(); }

void AnalysisResultsPresenter::setStopButtonEnabled(bool enabled)
{
    if (m_stopButton) {
        m_stopButton->setEnabled(enabled);
    }
    
    // 解析中フラグを更新（enabled=true: 解析中, enabled=false: 解析終了）
    m_isAnalyzing = enabled;
    
    // 解析中はテーブルのクリック操作を無効化
    if (m_view) {
        if (enabled) {
            // 解析中: 選択不可（ただし自動選択は可能）
            m_view->setSelectionMode(QAbstractItemView::NoSelection);
        } else {
            // 解析終了: 選択可能に戻す
            m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
    }
}

void AnalysisResultsPresenter::onTableClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // 解析中はクリックを無視
    if (m_isAnalyzing) {
        return;
    }

    // 盤面列（列6）のみ反応
    if (index.column() != 6) {
        return;
    }

    qDebug().noquote() << "[AnalysisResultsPresenter::onTableClicked] row=" << index.row();
    Q_EMIT rowDoubleClicked(index.row());
}

void AnalysisResultsPresenter::onTableSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid()) return;
    
    // 自動選択中（解析中の行追加時）はシグナル発行をスキップ
    // （解析中は既にanalysisProgressReportedで盤面更新済みのため）
    if (m_autoSelecting) {
        return;
    }
    
    // 解析中は選択変更を無視（NoSelectionモードなので通常来ないが念のため）
    if (m_isAnalyzing) {
        return;
    }
    
    int row = current.row();
    qDebug().noquote() << "[AnalysisResultsPresenter::onTableSelectionChanged] row=" << row;
    Q_EMIT rowSelected(row);
}

void AnalysisResultsPresenter::showAnalysisComplete(int totalMoves)
{
    // 中止ボタンを無効化
    setStopButtonEnabled(false);

    // 完了メッセージを表示（イベント処理完了後に遅延実行）
    // シグナル処理中にモーダルダイアログを表示するとクラッシュするため
    QTimer::singleShot(0, this, [this, totalMoves]() {
        QWidget* parentWidget = m_dock ? qobject_cast<QWidget*>(m_dock.data()) : m_container.data();
        QMessageBox::information(
            parentWidget,
            tr("棋譜解析完了"),
            tr("棋譜解析が完了しました。\n\n解析手数: %1 手").arg(totalMoves),
            QMessageBox::Ok
        );
    });
}

void AnalysisResultsPresenter::increaseFontSize()
{
    if (!m_view) return;

    QFont font = m_view->font();
    int newSize = font.pointSize() + 1;
    if (newSize > 24) newSize = 24;  // 最大サイズ制限
    font.setPointSize(newSize);
    m_view->setFont(font);

    // ヘッダーのフォントサイズも変更（棋譜欄と同じスタイルを維持）
    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #4a9eff, stop:1 #2d7dd2);"
            "  color: white;"
            "  font-weight: normal;"
            "  font-size: %1pt;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #2d7dd2;"
            "}").arg(newSize));
    }

    // SettingsServiceで設定を保存
    SettingsService::setKifuAnalysisFontSize(newSize);

    m_reflowTimer->start();
}

void AnalysisResultsPresenter::decreaseFontSize()
{
    if (!m_view) return;

    QFont font = m_view->font();
    int newSize = font.pointSize() - 1;
    if (newSize < 8) newSize = 8;  // 最小サイズ制限
    font.setPointSize(newSize);
    m_view->setFont(font);

    // ヘッダーのフォントサイズも変更（棋譜欄と同じスタイルを維持）
    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #4a9eff, stop:1 #2d7dd2);"
            "  color: white;"
            "  font-weight: normal;"
            "  font-size: %1pt;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #2d7dd2;"
            "}").arg(newSize));
    }

    // SettingsServiceで設定を保存
    SettingsService::setKifuAnalysisFontSize(newSize);

    m_reflowTimer->start();
}

void AnalysisResultsPresenter::restoreFontSize()
{
    if (!m_view) return;

    // SettingsServiceからフォントサイズを復元
    int savedSize = SettingsService::kifuAnalysisFontSize();

    // 有効なサイズでない場合はデフォルトのフォントサイズを使用
    if (savedSize < 8 || savedSize > 24) {
        savedSize = m_view->font().pointSize();
        if (savedSize < 8) savedSize = 10;  // デフォルト
    }

    QFont font = m_view->font();
    font.setPointSize(savedSize);
    m_view->setFont(font);

    // ヘッダーのフォントサイズも同じサイズに設定（棋譜欄と同じスタイルを維持）
    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #4a9eff, stop:1 #2d7dd2);"
            "  color: white;"
            "  font-weight: normal;"
            "  font-size: %1pt;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #2d7dd2;"
            "}").arg(savedSize));
    }
}

void AnalysisResultsPresenter::saveWindowSize()
{
    // ドックのサイズを保存
    if (m_dock) {
        SettingsService::setKifuAnalysisResultsWindowSize(m_dock->size());
        // 閉じるボタンが押されたらドックを非表示にする
        m_dock->setVisible(false);
    }
}
