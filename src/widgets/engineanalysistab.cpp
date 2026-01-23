#include "engineanalysistab.h"

#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTextEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QHeaderView>
#include <QFont>
#include <QMouseEvent>
#include <QtMath>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QQueue>
#include <QSet>
#include <QToolButton>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QLabel>       // ★ 追加
#include <QMessageBox>  // ★ 追加
#include <QTimer>       // ★ 追加: 列幅設定の遅延用
#include <QLineEdit>    // ★ 追加: CSAコマンド入力用
#include <QComboBox>    // ★ 追加: USIコマンド送信先選択用
#include <QTextCursor>      // ★ 追加: ログ色付け用
#include <QTextCharFormat>  // ★ 追加: ログ色付け用

#include "settingsservice.h"  // ★ 追加: フォントサイズ保存用
#include <QFontDatabase>      // ★ 追加: フォント検索用
#include <QFontInfo>          // ★ 追加: フォントデバッグ用
#include <QDebug>             // ★ 追加: デバッグ出力用
#include "numeric_right_align_comma_delegate.h"
#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

// ===================== ヘルパー関数 =====================

// クロスプラットフォーム対応の日本語フォントを取得
static QString getJapaneseFontFamily()
{
    // 優先順位付きフォントリスト（OS別に最適なフォントを優先）
#ifdef Q_OS_WIN
    // Windows: 標準フォントを優先（アンチエイリアスが安定）
    static const QStringList candidates = {
        QStringLiteral("Yu Gothic UI"),      // Windows 10/11
        QStringLiteral("Meiryo UI"),         // Windows Vista以降
        QStringLiteral("Meiryo"),            // Windows Vista以降
        QStringLiteral("MS UI Gothic"),      // Windows XP以降
        QStringLiteral("Noto Sans JP"),      // インストールされている場合
        QStringLiteral("MS Gothic"),         // フォールバック
    };
#elif defined(Q_OS_MAC)
    // macOS
    static const QStringList candidates = {
        QStringLiteral("Hiragino Sans"),
        QStringLiteral("Hiragino Kaku Gothic ProN"),
        QStringLiteral("Noto Sans JP"),
    };
#else
    // Linux
    static const QStringList candidates = {
        QStringLiteral("Noto Sans CJK JP"),
        QStringLiteral("Noto Sans JP"),
        QStringLiteral("IPAGothic"),
    };
#endif

    const QStringList availableFamilies = QFontDatabase::families();
    
    // デバッグ: 利用可能なフォント数を出力
    qDebug() << "[FontDebug] Available font families count:" << availableFamilies.size();
    
    for (const QString &candidate : candidates) {
        if (availableFamilies.contains(candidate)) {
            qDebug() << "[FontDebug] Selected font:" << candidate;
            return candidate;
        }
    }

    // 見つからない場合はシステムデフォルト
    qDebug() << "[FontDebug] No Japanese font found, using system default";
    return QString();
}

// フォント情報をデバッグ出力するヘルパー
static void debugFontInfo(const QFont &font, const QString &context)
{
    QFontInfo info(font);
    qDebug() << "[FontDebug]" << context;
    qDebug() << "  Requested family:" << font.family();
    qDebug() << "  Actual family:" << info.family();
    qDebug() << "  Point size:" << info.pointSize();
    qDebug() << "  Pixel size:" << info.pixelSize();
    qDebug() << "  Style hint:" << font.styleHint();
    qDebug() << "  Exact match:" << info.exactMatch();
}

// ===================== コンストラクタ/UI =====================

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent)
    : QWidget(parent)
{
}

void EngineAnalysisTab::buildUi()
{
    // --- QTabWidget 準備 ---
    if (!m_tab) {
        m_tab = new QTabWidget(this);  // 親をthisに指定（メモリリーク防止）
        m_tab->setObjectName(QStringLiteral("analysisTabs"));

        // タブバーを青色、タブ内をクリーム色にスタイル設定
        m_tab->setStyleSheet(QStringLiteral(
            "QTabWidget::pane {"
            "  background-color: #fefcf6;"
            "  border: 1px solid #d4c9a8;"
            "  border-top: none;"
            "}"
            "QTabBar::tab {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 4px 12px;"
            "  border: 1px solid #209cee;"
            "  border-bottom: none;"
            "  border-top-left-radius: 4px;"
            "  border-top-right-radius: 4px;"
            "  margin-right: 2px;"
            "}"
            "QTabBar::tab:selected {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #50bcff, stop:1 #30acfe);"
            "}"
            "QTabBar::tab:hover:!selected {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #50bcff, stop:1 #30acfe);"
            "}"
        ));
    } else {
        m_tab->clear();
    }

    // --- 思考タブ ---
    QWidget* page = new QWidget(m_tab);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(4,4,4,4);
    v->setSpacing(4);

    // ★ 変更: 最初のEngineInfoWidgetにフォントサイズボタンを表示
    m_info1 = new EngineInfoWidget(page, true);  // showFontButtons=true
    m_info1->setWidgetIndex(0);  // ★ 追加: インデックス設定
    m_view1 = new QTableView(page);
    m_info2 = new EngineInfoWidget(page, false); // showFontButtons=false
    m_info2->setWidgetIndex(1);  // ★ 追加: インデックス設定
    m_view2 = new QTableView(page);
    
    // ★ 追加: フォントサイズ変更シグナルを接続
    connect(m_info1, &EngineInfoWidget::fontSizeIncreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontIncrease);
    connect(m_info1, &EngineInfoWidget::fontSizeDecreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontDecrease);
    
    // ★ 追加: 列幅変更シグナルを接続
    connect(m_info1, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);
    connect(m_info2, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);
    
    // ★ 追加: 設定ファイルから列幅を読み込んで適用
    QList<int> widths0 = SettingsService::engineInfoColumnWidths(0);
    if (!widths0.isEmpty() && widths0.size() == m_info1->columnCount()) {
        m_info1->setColumnWidths(widths0);
    }
    QList<int> widths1 = SettingsService::engineInfoColumnWidths(1);
    if (!widths1.isEmpty() && widths1.size() == m_info2->columnCount()) {
        m_info2->setColumnWidths(widths1);
    }

    // ★ ヘッダの基本設定のみ（列幅はsetModels後に適用）
    setupThinkingViewHeader(m_view1);
    setupThinkingViewHeader(m_view2);
    
    // ★ 追加: 列幅変更シグナルを接続
    connect(m_view1->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView1SectionResized);
    connect(m_view2->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView2SectionResized);

    // ★ 追加: 読み筋テーブルのクリックシグナルを接続
    connect(m_view1, &QTableView::clicked,
            this, &EngineAnalysisTab::onView1Clicked);
    connect(m_view2, &QTableView::clicked,
            this, &EngineAnalysisTab::onView2Clicked);

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    m_tab->addTab(page, tr("思考"));

    // --- USI通信ログ ---
    // ★ 修正: ツールバー付きコンテナに変更
    m_usiLogContainer = new QWidget(m_tab);
    QVBoxLayout* usiLogLayout = new QVBoxLayout(m_usiLogContainer);
    usiLogLayout->setContentsMargins(4, 4, 4, 4);
    usiLogLayout->setSpacing(2);

    // ツールバーを構築
    buildUsiLogToolbar();
    usiLogLayout->addWidget(m_usiLogToolbar);

    // ★ 追加: コマンド入力バーを構築
    buildUsiCommandBar();
    usiLogLayout->addWidget(m_usiCommandBar);

    m_usiLog = new QPlainTextEdit(m_usiLogContainer);
    m_usiLog->setReadOnly(true);
    usiLogLayout->addWidget(m_usiLog);

    m_tab->addTab(m_usiLogContainer, tr("USI通信ログ"));

    // --- CSA通信ログ ---
    // SettingsServiceからフォントサイズを読み込み
    m_csaLogFontSize = SettingsService::csaLogFontSize();

    m_csaLogContainer = new QWidget(m_tab);
    QVBoxLayout* csaLogLayout = new QVBoxLayout(m_csaLogContainer);
    csaLogLayout->setContentsMargins(4, 4, 4, 4);
    csaLogLayout->setSpacing(2);

    // ツールバーを構築
    buildCsaLogToolbar();
    csaLogLayout->addWidget(m_csaLogToolbar);

    // コマンド入力バーを構築
    buildCsaCommandBar();
    csaLogLayout->addWidget(m_csaCommandBar);

    m_csaLog = new QPlainTextEdit(m_csaLogContainer);
    m_csaLog->setReadOnly(true);
    // 保存されたフォントサイズを適用
    {
        QFont font = m_csaLog->font();
        font.setPointSize(m_csaLogFontSize);
        m_csaLog->setFont(font);
    }
    csaLogLayout->addWidget(m_csaLog);

    m_tab->addTab(m_csaLogContainer, tr("CSA通信ログ"));

    // --- 棋譜コメント ---
    // コメント欄とツールバーを含むコンテナ
    QWidget* commentContainer = new QWidget(m_tab);
    QVBoxLayout* commentLayout = new QVBoxLayout(commentContainer);
    commentLayout->setContentsMargins(4, 4, 4, 4);
    commentLayout->setSpacing(2);

    // ツールバーを構築
    buildCommentToolbar();
    commentLayout->addWidget(m_commentToolbar);

    m_comment = new QTextEdit(commentContainer);
    m_comment->setReadOnly(false);  // 編集可能にする
    m_comment->setAcceptRichText(true);
    m_comment->setPlaceholderText(tr("コメントを表示・編集"));
    commentLayout->addWidget(m_comment);

    // ★ 追加: コメントのURLクリックを処理するためのイベントフィルター
    if (m_comment->viewport()) {
        m_comment->viewport()->installEventFilter(this);
    }

    // ★ 追加: コメント変更時の検知
    connect(m_comment, &QTextEdit::textChanged,
            this, &EngineAnalysisTab::onCommentTextChanged);

    m_tab->addTab(commentContainer, tr("棋譜コメント"));

    // --- 分岐ツリー ---
    m_branchTree = new QGraphicsView(m_tab);
    m_branchTree->setRenderHint(QPainter::Antialiasing, true);
    m_branchTree->setRenderHint(QPainter::TextAntialiasing, true);  // ★ テキストのアンチエイリアス
    m_branchTree->setRenderHint(QPainter::SmoothPixmapTransform, true);  // ★ 滑らかな変換
    m_branchTree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_branchTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_branchTree->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    // デバッグ: レンダリングヒントを出力
    qDebug() << "[FontDebug] QGraphicsView render hints:"
             << "Antialiasing=" << m_branchTree->renderHints().testFlag(QPainter::Antialiasing)
             << "TextAntialiasing=" << m_branchTree->renderHints().testFlag(QPainter::TextAntialiasing);

    m_scene = new QGraphicsScene(m_branchTree);
    m_branchTree->setScene(m_scene);

    m_tab->addTab(m_branchTree, tr("分岐ツリー"));

    // ★ 初回起動時（あるいは再構築時）にモデルが既にあるなら即時適用
    reapplyViewTuning(m_view1, m_model1);  // 右寄せ＋3桁カンマ＆列幅チューニング
    reapplyViewTuning(m_view2, m_model2);

    // --- 分岐ツリーのクリック検知（二重 install 防止のガード付き） ---
    if (m_branchTree && m_branchTree->viewport()) {
        QObject* vp = m_branchTree->viewport();
        if (!vp->property("branchFilterInstalled").toBool()) {
            vp->installEventFilter(this);
            vp->setProperty("branchFilterInstalled", true);
        }
    }

    // ★ 追加: 設定ファイルからフォントサイズを読み込んで適用
    m_usiLogFontSize = SettingsService::usiLogFontSize();
    if (m_usiLog) {
        QFont font = m_usiLog->font();
        font.setPointSize(m_usiLogFontSize);
        m_usiLog->setFont(font);
    }
    
    m_currentFontSize = SettingsService::commentFontSize();
    if (m_comment) {
        QFont font = m_comment->font();
        font.setPointSize(m_currentFontSize);
        m_comment->setFont(font);
    }
    
    // ★ 追加: 思考タブのフォントサイズを読み込んで適用
    m_thinkingFontSize = SettingsService::thinkingFontSize();
    if (m_thinkingFontSize != 10) {  // デフォルト以外の場合のみ適用
        QFont font;
        font.setPointSize(m_thinkingFontSize);
        // 上段（EngineInfoWidget）
        if (m_info1) m_info1->setFontSize(m_thinkingFontSize);
        if (m_info2) m_info2->setFontSize(m_thinkingFontSize);
        // 下段（TableView）
        if (m_view1) m_view1->setFont(font);
        if (m_view2) m_view2->setFont(font);
    }

    // ★ 追加：起動直後でも「開始局面」だけは描く
    rebuildBranchTree();
}

void EngineAnalysisTab::reapplyViewTuning(QTableView* v, QAbstractItemModel* m)
{
    if (!v) return;
    // 数値列（時間/深さ/ノード数/評価値）の右寄せ＆3桁カンマ
    if (m) applyNumericFormattingTo(v, m);
}

void EngineAnalysisTab::onModel1Reset()
{
    reapplyViewTuning(m_view1, m_model1);
}

void EngineAnalysisTab::onModel2Reset()
{
    reapplyViewTuning(m_view2, m_model2);
}

void EngineAnalysisTab::onLog1Changed()
{
    if (m_usiLog && m_log1) {
        appendColoredUsiLog(m_log1->usiCommLog(), QColor(0x20, 0x60, 0xa0));  // E1: 青色
    }
}

void EngineAnalysisTab::onLog2Changed()
{
    if (m_usiLog && m_log2) {
        appendColoredUsiLog(m_log2->usiCommLog(), QColor(0xa0, 0x20, 0x60));  // E2: 赤色
    }
}

void EngineAnalysisTab::appendColoredUsiLog(const QString& logLine, const QColor& lineColor)
{
    if (!m_usiLog || logLine.isEmpty()) return;

    // 行全体を指定された色で表示
    QTextCursor cursor = m_usiLog->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat coloredFormat;
    coloredFormat.setForeground(lineColor);
    cursor.insertText(logLine + QStringLiteral("\n"), coloredFormat);

    m_usiLog->setTextCursor(cursor);
    m_usiLog->ensureCursorVisible();
}

void EngineAnalysisTab::appendUsiLogStatus(const QString& message)
{
    if (!m_usiLog || message.isEmpty()) return;

    // ステータスメッセージをグレーで表示
    QTextCursor cursor = m_usiLog->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat grayFormat;
    grayFormat.setForeground(QColor(0x80, 0x80, 0x80));  // グレー
    cursor.insertText(QStringLiteral("⚙ ") + message + QStringLiteral("\n"), grayFormat);

    m_usiLog->setTextCursor(cursor);
    m_usiLog->ensureCursorVisible();
}

void EngineAnalysisTab::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                                  UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_model1 = m1;  m_model2 = m2;
    m_log1   = log1; m_log2  = log2;

    if (m_view1) m_view1->setModel(m1);
    if (m_view2) m_view2->setModel(m2);

    if (m_info1) m_info1->setModel(log1);
    if (m_info2) m_info2->setModel(log2);

    // ★ モデル設定後に列幅を適用（モデルがないと列幅が適用されない）
    applyThinkingViewColumnWidths(m_view1, 0);
    applyThinkingViewColumnWidths(m_view2, 1);

    // モデル設定直後に数値フォーマットを適用
    reapplyViewTuning(m_view1, m_model1);
    reapplyViewTuning(m_view2, m_model2);

    // modelReset時に再適用（ラムダ→関数スロット）
    if (m_model1) {
        QObject::connect(m_model1, &QAbstractItemModel::modelReset,
                         this, &EngineAnalysisTab::onModel1Reset, Qt::UniqueConnection);
    }
    if (m_model2) {
        QObject::connect(m_model2, &QAbstractItemModel::modelReset,
                         this, &EngineAnalysisTab::onModel2Reset, Qt::UniqueConnection);
    }

    // USIログ反映（ラムダ→関数スロット）
    if (m_log1) {
        QObject::connect(m_log1, &UsiCommLogModel::usiCommLogChanged,
                         this, &EngineAnalysisTab::onLog1Changed, Qt::UniqueConnection);
        // ★ 追加: エンジン名変更シグナルを接続
        QObject::connect(m_log1, &UsiCommLogModel::engineNameChanged,
                         this, &EngineAnalysisTab::onEngine1NameChanged, Qt::UniqueConnection);
        // 初期値を設定
        onEngine1NameChanged();
    }
    if (m_log2) {
        QObject::connect(m_log2, &UsiCommLogModel::usiCommLogChanged,
                         this, &EngineAnalysisTab::onLog2Changed, Qt::UniqueConnection);
        // ★ 追加: エンジン名変更シグナルを接続
        QObject::connect(m_log2, &UsiCommLogModel::engineNameChanged,
                         this, &EngineAnalysisTab::onEngine2NameChanged, Qt::UniqueConnection);
        // 初期値を設定
        onEngine2NameChanged();
    }
}

QTabWidget* EngineAnalysisTab::tab() const { return m_tab; }

void EngineAnalysisTab::setAnalysisVisible(bool on)
{
    if (this->isVisible() != on) this->setVisible(on);
}

void EngineAnalysisTab::setCommentHtml(const QString& html)
{
    if (m_comment) {
        qDebug().noquote()
            << "[EngineAnalysisTab] setCommentHtml ENTER:"
            << " html.len=" << html.size()
            << " m_isCommentDirty(before)=" << m_isCommentDirty;
        
        // ★ 元のコメントを保存（変更検知用）
        // HTMLからプレーンテキストを取得して保存
        QString processedHtml = convertUrlsToLinks(html);
        m_comment->setHtml(processedHtml);
        m_originalComment = m_comment->toPlainText();
        
        qDebug().noquote()
            << "[EngineAnalysisTab] setCommentHtml:"
            << " m_originalComment.len=" << m_originalComment.size();
        
        // ★ 編集状態をリセット
        m_isCommentDirty = false;
        updateEditingIndicator();
        
        qDebug().noquote()
            << "[EngineAnalysisTab] setCommentHtml LEAVE:"
            << " m_isCommentDirty=" << m_isCommentDirty;
    }
}

// ===================== 分岐ツリー・データ設定 =====================

void EngineAnalysisTab::setBranchTreeRows(const QVector<ResolvedRowLite>& rows)
{
    m_rows = rows;
    rebuildBranchTree();
}

// ===================== ノード/エッジ描画（＋登録） =====================

// ノード（指し手札）を描く：row=0(本譜)/1..(分岐), ply=手数(1始まり), rawText=指し手
QGraphicsPathItem* EngineAnalysisTab::addNode(int row, int ply, const QString& rawText)
{
    // レイアウト
    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;   // ← 現在使っているオフセットと揃える
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const    QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    // デバッグ: 初回のみフォント情報を出力
    static bool fontDebugDone = false;
    if (!fontDebugDone) {
        debugFontInfo(LABEL_FONT, "addNode LABEL_FONT");
        debugFontInfo(MOVE_NO_FONT, "addNode MOVE_NO_FONT");
        fontDebugDone = true;
    }

    const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
    const qreal y = BASE_Y + row * STEP_Y;

    // 先頭の手数数字（全角/半角）を除去
    static const QRegularExpression kDropHeadNumber(QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
    QString labelText = rawText;
    labelText.replace(kDropHeadNumber, QString());
    labelText = labelText.trimmed();

    // 棋譜欄では分岐ありを示すため末尾に '+' を付けているが、
    // 分岐ツリーでは装飾を表示しないのでここで取り除く
    if (labelText.endsWith(QLatin1Char('+'))) {
        labelText.chop(1);
        labelText = labelText.trimmed();
    }

    const bool odd = (ply % 2) == 1; // 奇数=先手、偶数=後手

    // ★ 分岐も本譜と同じ配色に統一
    const QColor mainOdd (196, 230, 255); // 先手=水色
    const QColor mainEven(255, 223, 196); // 後手=ピーチ
    const QColor fill = odd ? mainOdd : mainEven;

    // 札サイズ
    const QFontMetrics fm(LABEL_FONT);
    const int  wText = fm.horizontalAdvance(labelText);
    const int  hText = fm.height();
    const qreal padX = 12.0, padY = 6.0;
    const qreal rectW = qMax<qreal>(70.0, wText + padX * 2);
    const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);

    // 角丸札
    QPainterPath path;
    const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
    path.addRoundedRect(rect, RADIUS, RADIUS);

    auto* item = m_scene->addPath(path, QPen(Qt::black, 1.2));
    item->setBrush(fill);
    item->setZValue(10);
    item->setData(ROLE_ORIGINAL_BRUSH, item->brush().color().rgba());

    // メタ
    item->setData(ROLE_ROW, row);
    item->setData(ROLE_PLY, ply);
    item->setData(BR_ROLE_KIND, (row == 0) ? BNK_Main : BNK_Var);
    if (row == 0) item->setData(BR_ROLE_PLY, ply); // 既存クリック処理互換

    // 指し手ラベル（中央）
    auto* textItem = m_scene->addSimpleText(labelText, LABEL_FONT);
    const QRectF br = textItem->boundingRect();
    textItem->setParentItem(item);
    textItem->setPos(rect.center().x() - br.width() / 2.0,
                     rect.center().y() - br.height() / 2.0);

    // ★ 「n手目」ラベルは本譜の上だけに表示（分岐 row!=0 では表示しない）
    if (row == 0) {
        const QString moveNo = QString::number(ply) + QStringLiteral("手目");
        auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
        const QRectF nbr = noItem->boundingRect();
        noItem->setParentItem(item);
        const qreal gap = 4.0;
        noItem->setPos(rect.center().x() - nbr.width() / 2.0,
                       rect.top() - gap - nbr.height());
    }

    // クリック解決用（従来）
    m_nodeIndex.insert(qMakePair(row, ply), item);

    // ★ グラフ登録（vid はここでは row と同義で十分）
    const int nodeId = registerNode(/*vid*/row, row, ply, item);
    item->setData(ROLE_NODE_ID, nodeId);

    return item;
}

void EngineAnalysisTab::addEdge(QGraphicsPathItem* from, QGraphicsPathItem* to)
{
    if (!from || !to) return;

    // ノードの中心（シーン座標）
    const QPointF a = from->sceneBoundingRect().center();
    const QPointF b = to->sceneBoundingRect().center();

    // 緩やかなベジェ
    QPainterPath path(a);
    const QPointF c1(a.x() + 8, a.y());
    const QPointF c2(b.x() - 8, b.y());
    path.cubicTo(c1, c2, b);

    auto* edge = m_scene->addPath(path, QPen(QColor(90, 90, 90), 1.0));
    edge->setZValue(0); // ← 線は常に背面（長方形の中に罫線が見えなくなる）

    // ★ グラフ接続
    const int prevId = from->data(ROLE_NODE_ID).toInt();
    const int nextId = to  ->data(ROLE_NODE_ID).toInt();
    if (prevId > 0 && nextId > 0) linkEdge(prevId, nextId);
}

// --- 追加ヘルパ：row(>=1) の分岐元となる「親行」を決める ---
int EngineAnalysisTab::resolveParentRowForVariation(int row) const
{
    Q_ASSERT(row >= 1 && row < m_rows.size());

    // ★ 修正: 以前は startPly の前後関係から親を「推測」していたが、
    //    データとして渡された parent を正しく使うように変更しました。
    //    これにより、2局目の後に1局目の途中から分岐した3局目（row=2）が来た場合でも、
    //    parent=0（1局目）を正しく参照できるようになります。

    const int p = m_rows.at(row).parent;
    if (p >= 0 && p < m_rows.size()) {
        return p;
    }

    return 0; // フォールバック：本譜
}

// ===================== シーン再構築 =====================

void EngineAnalysisTab::rebuildBranchTree()
{
    if (!m_scene) return;
    m_scene->clear();
    m_nodeIndex.clear();

    // ★ グラフもクリア
    clearBranchGraph();
    m_prevSelected = nullptr;

    // レイアウト定数（既存と同値）
    static constexpr qreal STEP_X   = 110.0;
    static constexpr qreal BASE_X   = 40.0;
    static constexpr qreal SHIFT_X  = 40.0;   // addNode() と同じ
    static constexpr qreal BASE_Y   = 40.0;
    static constexpr qreal STEP_Y   = 56.0;
    static constexpr qreal RADIUS   = 8.0;
    static const    QFont LABEL_FONT(getJapaneseFontFamily(), 10);
    static const    QFont MOVE_NO_FONT(getJapaneseFontFamily(), 9);

    // デバッグ: 初回のみフォント情報を出力
    static bool fontDebugDone2 = false;
    if (!fontDebugDone2) {
        debugFontInfo(LABEL_FONT, "rebuildBranchGraph LABEL_FONT");
        debugFontInfo(MOVE_NO_FONT, "rebuildBranchGraph MOVE_NO_FONT");
        fontDebugDone2 = true;
    }

    // ===== まず「開始局面」を必ず描画（m_rows が空でも表示） =====
    QGraphicsPathItem* startNode = nullptr;
    {
        const qreal x = BASE_X + SHIFT_X;
        const qreal y = BASE_Y + 0 * STEP_Y;
        const QString label = tr("開始局面");

        const QFontMetrics fm(LABEL_FONT);
        const int  wText = fm.horizontalAdvance(label);
        const int  hText = fm.height();
        const qreal padX = 14.0, padY = 8.0;
        const qreal rectW = qMax<qreal>(84.0, wText + padX * 2);
        const qreal rectH = qMax<qreal>(26.0, hText + padY * 2);

        QPainterPath path;
        const QRectF rect(x - rectW / 2.0, y - rectH / 2.0, rectW, rectH);
        path.addRoundedRect(rect, RADIUS, RADIUS);

        startNode = m_scene->addPath(path, QPen(Qt::black, 1.4));
        startNode->setBrush(QColor(235, 235, 235));
        startNode->setZValue(10);

        startNode->setData(ROLE_ROW, 0);
        startNode->setData(ROLE_PLY, 0);
        startNode->setData(BR_ROLE_KIND, BNK_Start);
        startNode->setData(ROLE_ORIGINAL_BRUSH, startNode->brush().color().rgba());
        m_nodeIndex.insert(qMakePair(0, 0), startNode);

        auto* t = m_scene->addSimpleText(label, LABEL_FONT);
        const QRectF br = t->boundingRect();
        t->setParentItem(startNode);
        t->setPos(rect.center().x() - br.width() / 2.0,
                  rect.center().y() - br.height() / 2.0);

        const int nid = registerNode(/*vid*/0, /*row*/0, /*ply*/0, startNode);
        startNode->setData(ROLE_NODE_ID, nid);
    }

    // ===== 本譜 row=0（手札） =====
    if (!m_rows.isEmpty()) {
        const auto& main = m_rows.at(0);

        QGraphicsPathItem* prev = startNode;
        // disp[0]は開始局面エントリ（prettyMoveが空）なのでスキップ
        // disp[1]から処理し、ply=1から開始
        for (qsizetype i = 1; i < main.disp.size(); ++i) {
            const auto& it = main.disp.at(i);
            const int ply = static_cast<int>(i);  // disp[i]はi手目
            QGraphicsPathItem* node = addNode(0, ply, it.prettyMove);
            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== 分岐 row=1.. =====
    qDebug().noquote() << "[EAT] rebuildBranchTree: m_rows.size=" << m_rows.size();
    for (qsizetype row = 1; row < m_rows.size(); ++row) {
        const auto& rv = m_rows.at(row);
        const int startPly = qMax(1, rv.startPly);      // 1-origin
        qDebug().noquote() << "[EAT] rebuildBranchTree: row=" << row
                           << " startPly=" << startPly
                           << " rv.disp.size=" << rv.disp.size()
                           << " rv.parent=" << rv.parent;

        // 1) 親行を決定
        const int parentRow = resolveParentRowForVariation(static_cast<int>(row));

        // 2) 親と繋ぐ“合流手”は (startPly - 1) 手目のノード
        const int joinPly = startPly - 1;

        // 親の joinPly ノードを取得。無ければ本譜→開始局面へフォールバック。
        // ★修正: ターミナルノード（投了など）への接続は避ける
        static const QStringList kTerminalKeywords = {
            QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
            QStringLiteral("千日手"), QStringLiteral("切れ負け"),
            QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
            QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
            QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
        };
        auto isTerminalPly = [&](int targetRow, int ply) -> bool {
            if (targetRow < 0 || targetRow >= m_rows.size()) return false;
            const auto& rowData = m_rows.at(targetRow);
            if (ply < 0 || ply >= rowData.disp.size()) return false;
            const QString& text = rowData.disp.at(ply).prettyMove;
            for (const auto& kw : kTerminalKeywords) {
                if (text.contains(kw)) return true;
            }
            return false;
        };

        QGraphicsPathItem* prev = nullptr;

        // 1. 親行のjoinPly手目を試す（ターミナルでなければ）
        if (!isTerminalPly(parentRow, joinPly)) {
            prev = m_nodeIndex.value(qMakePair(parentRow, joinPly), nullptr);
        }

        // 2. なければ本譜のjoinPly手目を試す（ターミナルでなければ）
        if (!prev && !isTerminalPly(0, joinPly)) {
            prev = m_nodeIndex.value(qMakePair(0, joinPly), nullptr);
        }

        // 3. それでもなければ、親行のjoinPlyより前の最後の非ターミナルノードを探す
        if (!prev) {
            for (int p = joinPly - 1; p >= 0; --p) {
                if (!isTerminalPly(parentRow, p)) {
                    prev = m_nodeIndex.value(qMakePair(parentRow, p), nullptr);
                    if (prev) break;
                }
                if (!prev && !isTerminalPly(0, p)) {
                    prev = m_nodeIndex.value(qMakePair(0, p), nullptr);
                    if (prev) break;
                }
            }
        }

        // 4. 最終フォールバック: 開始局面
        if (!prev) {
            prev = m_nodeIndex.value(qMakePair(0, 0), nullptr);
        }

        // 3) 分岐の手リストを「開始手以降だけ」にスライス
        // 新データ構造: disp[0]=開始局面エントリ, disp[i]=i手目 (i>=1)
        // startPly手目から描画するので、disp[startPly]から開始
        const int cut   = startPly;                       // disp[startPly]がstartPly手目
        const qsizetype total = rv.disp.size();
        const int take  = (cut < total) ? static_cast<int>(total - cut) : 0;
        qDebug().noquote() << "[EAT] rebuildBranchTree: row=" << row
                           << " cut=" << cut << " total=" << total << " take=" << take;
        if (take <= 0) {
            qDebug().noquote() << "[EAT] rebuildBranchTree: SKIPPING row=" << row << " (no moves to draw)";
            continue;                              // 描くもの無し
        }

        // 4) 切り出した手だけを絶対手数で並べる（absPly = startPly + i）
        for (int i = 0; i < take; ++i) {
            const auto& it = rv.disp.at(cut + i);
            const int absPly = startPly + i;

            QGraphicsPathItem* node = addNode(static_cast<int>(row), absPly, it.prettyMove);

            // クリック用メタ
            node->setData(BR_ROLE_STARTPLY, startPly);
            node->setData(BR_ROLE_BUCKET,   row - 1);

            if (prev) addEdge(prev, node);
            prev = node;
        }
    }

    // ===== “一番上”の手数ラベルを最大手数まで補完表示 =====
    // 本譜ノード（row=0）が無い手（=本譜手数を超える手）についても、上段に「n手目」を表示する
    int maxAbsPly = 0;
    {
        // 既に描いた全ノードから最大 ply を求める
        const auto keys = m_nodeIndex.keys();
        for (const auto& k : keys) {
            const int ply = k.second; // QPair<int,int> の second が ply
            if (ply > maxAbsPly) maxAbsPly = ply;
        }
    }

    if (maxAbsPly > 0) {
        const QFontMetrics fmLabel(LABEL_FONT);
        const QFontMetrics fmMoveNo(MOVE_NO_FONT);
        const int hText = fmLabel.height();
        const qreal padY = 6.0; // addNode() と同じ
        const qreal rectH = qMax<qreal>(24.0, hText + padY * 2);
        const qreal gap   = 4.0;
        const qreal baselineY = BASE_Y + 0 * STEP_Y;
        const qreal topY = (baselineY - rectH / 2.0) - gap; // 「n手目」テキストの下辺基準

        for (int ply = 1; ply <= maxAbsPly; ++ply) {
            // すでに本譜ノードがある手（row=0, ply）は addNode() 側でラベルを付け済み
            if (m_nodeIndex.contains(qMakePair(0, ply))) continue;

            const QString moveNo = QString::number(ply) + QStringLiteral("手目");
            auto* noItem = m_scene->addSimpleText(moveNo, MOVE_NO_FONT);
            const QRectF nbr = noItem->boundingRect();

            const qreal x = BASE_X + SHIFT_X + ply * STEP_X;
            noItem->setZValue(15); // 札より少し前面
            noItem->setPos(x - nbr.width() / 2.0, topY - nbr.height());
        }
    }

    // ===== シーン境界 =====
    // disp[0]は開始局面エントリなので、指し手数は disp.size() - 1
    const int mainLen = m_rows.isEmpty() ? 0 : static_cast<int>(qMax(qsizetype(0), m_rows.at(0).disp.size() - 1));
    const int spanLen = qMax(mainLen, maxAbsPly);
    const qreal width  = (BASE_X + SHIFT_X) + STEP_X * qMax(40, spanLen + 6) + 40.0;
    const qreal height = 30 + STEP_Y * static_cast<qreal>(qMax(qsizetype(2), m_rows.size() + 1));
    m_scene->setSceneRect(QRectF(0, 0, width, height));

    // 【追加】初期状態で「開始局面」（row=0, ply=0）をハイライト（黄色）にする
    // これにより m_prevSelected が「開始局面」に設定され、
    // 次の手がハイライトされる際に「開始局面」は自動的に元の色に戻されます。
    highlightBranchTreeAt(0, 0, /*centerOn=*/false);
}

// ===================== ハイライト（フォールバック対応） =====================

void EngineAnalysisTab::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    // まず (row,ply) 直指定
    auto it = m_nodeIndex.find(qMakePair(row, ply));
    if (it != m_nodeIndex.end()) {
        highlightNodeId(it.value()->data(ROLE_NODE_ID).toInt(), centerOn);
        return;
    }

    // 無ければグラフでフォールバック（分岐開始前は親行へ、あるいは next/prev を辿る）
    const int nid = graphFallbackToPly(row, ply);
    if (nid > 0) {
        highlightNodeId(nid, centerOn);
    }
}

void EngineAnalysisTab::highlightNodeId(int nodeId, bool centerOn)
{
    if (nodeId <= 0) return;
    const auto node = m_nodesById.value(nodeId);
    QGraphicsPathItem* item = node.item;
    if (!item) return;

    // 直前ハイライト復元
    if (m_prevSelected) {
        const auto argb = m_prevSelected->data(ROLE_ORIGINAL_BRUSH).toUInt();
        m_prevSelected->setBrush(QColor::fromRgba(argb));
        m_prevSelected->setPen(QPen(Qt::black, 1.2));
        m_prevSelected->setZValue(10);
        m_prevSelected = nullptr;
    }

    // 黄色へ
    item->setBrush(QColor(255, 235, 80));
    item->setPen(QPen(Qt::black, 1.8));
    item->setZValue(20);
    m_prevSelected = item;

    if (centerOn && m_branchTree) m_branchTree->centerOn(item);
}

// ===================== クリック検出 =====================
bool EngineAnalysisTab::eventFilter(QObject* obj, QEvent* ev)
{
    // ★ コメント内のURLクリック処理
    if (m_comment && obj == m_comment->viewport()
        && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton) {
            // クリック位置にあるアンカー（URL）を取得
            const QString anchor = m_comment->anchorAt(me->pos());
            if (!anchor.isEmpty()) {
                // URLをブラウザで開く
                QDesktopServices::openUrl(QUrl(anchor));
                return true;  // イベントを消費
            }
        }
    }

    // 分岐ツリーのクリック処理
    if (m_branchTree && obj == m_branchTree->viewport()
        && ev->type() == QEvent::MouseButtonRelease)
    {
        // 対局中はクリックを無効化（スクロールバーは引き続き動作可能）
        if (!m_branchTreeClickEnabled) {
            return false;
        }

        auto* me = static_cast<QMouseEvent*>(ev);
        if (!(me->button() & Qt::LeftButton)) return QWidget::eventFilter(obj, ev);

        const QPointF scenePt = m_branchTree->mapToScene(me->pos());
        QGraphicsItem* hit =
            m_branchTree->scene() ? m_branchTree->scene()->itemAt(scenePt, QTransform()) : nullptr;

        // 子(Text)に当たることがあるので親まで遡る
        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (!hit) return false;

        // クリックされたノードの絶対(row, ply)
        const int row = hit->data(ROLE_ROW).toInt();  // 0=Main, 1..=VarN
        const int ply = hit->data(ROLE_PLY).toInt();  // 0=開始局面, 1..=手数

        // 即時の視覚フィードバック（黄色）※任意だが体感向上
        highlightBranchTreeAt(row, ply, /*centerOn=*/false);

        // 新API：MainWindow 側で (row, ply) をそのまま適用
        emit branchNodeActivated(row, ply);
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

// ===== 互換API 実装 =====
void EngineAnalysisTab::setSecondEngineVisible(bool on)
{
    if (m_info2)  m_info2->setVisible(on);
    if (m_view2)  m_view2->setVisible(on);
}

void EngineAnalysisTab::setEngine1ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model1 = m;
    if (m_view1) m_view1->setModel(m);
    if (m_view1 && m_view1->horizontalHeader())
        m_view1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setEngine2ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model2 = m;
    if (m_view2) m_view2->setModel(m);
    if (m_view2 && m_view2->horizontalHeader())
        m_view2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setCommentText(const QString& text)
{
    // 旧コード互換：プレーンテキストで設定（URLをリンクに変換）
    if (m_comment) {
        QString htmlText = convertUrlsToLinks(text);
        m_comment->setHtml(htmlText);
    }
}

// ===================== グラフAPI 実装 =====================

void EngineAnalysisTab::clearBranchGraph()
{
    m_nodeIdByRowPly.clear();
    m_nodesById.clear();
    m_prevIds.clear();
    m_nextIds.clear();
    m_rowEntryNode.clear();
    m_nextNodeId = 1;
}

int EngineAnalysisTab::registerNode(int vid, int row, int ply, QGraphicsPathItem* item)
{
    if (!item) return -1;
    const int id = m_nextNodeId++;

    BranchGraphNode n;
    n.id   = id;
    n.vid  = vid;
    n.row  = row;
    n.ply  = ply;
    n.item = item;

    m_nodesById.insert(id, n);
    m_nodeIdByRowPly.insert(qMakePair(row, ply), id);

    // 行のエントリノード（最初に登録されたもの）を覚えておくと探索が楽
    if (!m_rowEntryNode.contains(row))
        m_rowEntryNode.insert(row, id);

    return id;
}

void EngineAnalysisTab::linkEdge(int prevId, int nextId)
{
    if (prevId <= 0 || nextId <= 0) return;
    m_nextIds[prevId].push_back(nextId);
    m_prevIds[nextId].push_back(prevId);
}

// ===================== フォールバック探索 =====================

int EngineAnalysisTab::graphFallbackToPly(int row, int targetPly) const
{
    // 1) まず (row, ply) にノードがあるならそれ
    const int direct = nodeIdFor(row, targetPly);
    if (direct > 0) return direct;

    // 2) 分岐行の「開始手より前」なら親行へ委譲する
    if (row >= 1 && row < m_rows.size()) {
        const int startPly = qMax(1, m_rows.at(row).startPly);
        if (targetPly < startPly) {
            const int parentRow = resolveParentRowForVariation(row);
            return graphFallbackToPly(parentRow, targetPly);
        }
    }

    // 3) 同じ行内で近いノードから next を辿って同一 ply を探す
    //    まず「targetPly 以下で最も近い既存 ply」を見つける
    int seedId = -1;
    for (int p = targetPly; p >= 0; --p) {
        seedId = nodeIdFor(row, p);
        if (seedId > 0) break;
    }
    if (seedId <= 0) {
        // 行に何も無ければ、行の入口（例えば開始局面や最初の手）から辿る
        seedId = m_rowEntryNode.value(row, -1);
    }

    if (seedId > 0) {
        // BFSで next を辿り、targetPly と一致する ply を持つノードを探す
        QQueue<int> q;
        QSet<int> seen;
        q.enqueue(seedId);
        seen.insert(seedId);

        while (!q.isEmpty()) {
            const int cur = q.dequeue();
            const auto node = m_nodesById.value(cur);
            if (node.ply == targetPly) return cur;

            const auto nexts = m_nextIds.value(cur);
            for (int nx : nexts) {
                if (!seen.contains(nx)) {
                    seen.insert(nx);
                    q.enqueue(nx);
                }
            }
        }
    }

    // 4) それでも見つからない場合、親行へ委譲してみる（最終手段）
    if (row >= 1 && row < m_rows.size()) {
        const int parentRow = resolveParentRowForVariation(row);
        if (parentRow != row) {
            const int viaParent = graphFallbackToPly(parentRow, targetPly);
            if (viaParent > 0) return viaParent;
        }
    }

    // 5) 本譜 row=0 の seed からも探索してみる
    {
        int seed0 = nodeIdFor(0, targetPly);
        if (seed0 <= 0) seed0 = m_rowEntryNode.value(0, -1);
        if (seed0 > 0) {
            QQueue<int> q;
            QSet<int> seen;
            q.enqueue(seed0);
            seen.insert(seed0);
            while (!q.isEmpty()) {
                const int cur = q.dequeue();
                const auto node = m_nodesById.value(cur);
                if (node.ply == targetPly) return cur;
                const auto nexts = m_nextIds.value(cur);
                for (int nx : nexts) if (!seen.contains(nx)) { seen.insert(nx); q.enqueue(nx); }
            }
        }
    }

    return -1;
}

// ★ ヘッダの基本設定（モデル設定前でもOK）
void EngineAnalysisTab::setupThinkingViewHeader(QTableView* v)
{
    if (!v) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    // ★ 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    h->setDefaultSectionSize(100);
    h->setMinimumSectionSize(24);
    h->setStretchLastSection(true);

    // 行の高さを文字サイズに合わせて固定（余白を最小限に）
    auto* vh = v->verticalHeader();
    if (vh) {
        vh->setVisible(false);
        const int rowHeight = v->fontMetrics().height() + 4;
        vh->setDefaultSectionSize(rowHeight);
        vh->setSectionResizeMode(QHeaderView::Fixed);
    }
    v->setWordWrap(false);
}

// ★ 列幅の適用（モデル設定後に呼ぶ）
void EngineAnalysisTab::applyThinkingViewColumnWidths(QTableView* v, int viewIndex)
{
    if (!v || !v->model()) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    constexpr int kColCount = 6;  // 「時間」「深さ」「ノード数」「評価値」「盤面」「読み筋」

    // 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    for (int col = 0; col < kColCount; ++col) {
        h->setSectionResizeMode(col, QHeaderView::Interactive);
    }

    // ★ 設定ファイルから列幅を読み込む
    QList<int> savedWidths = SettingsService::thinkingViewColumnWidths(viewIndex);

    if (savedWidths.size() == kColCount) {
        // 保存された列幅を適用
        h->blockSignals(true);
        for (int col = 0; col < kColCount; ++col) {
            if (savedWidths.at(col) > 0) {
                v->setColumnWidth(col, savedWidths.at(col));
            }
        }
        h->blockSignals(false);

        // ★ 列幅読み込み済みフラグを遅延で設定
        QTimer::singleShot(500, this, [this, viewIndex]() {
            if (viewIndex == 0) {
                m_thinkingView1WidthsLoaded = true;
            } else {
                m_thinkingView2WidthsLoaded = true;
            }
        });
    } else {
        // デフォルトの列幅を設定
        // 「時間」「深さ」「ノード数」「評価値」「盤面」「読み筋」
        const int defaultWidths[] = {60, 50, 100, 80, 50, 330};
        h->blockSignals(true);
        for (int col = 0; col < kColCount; ++col) {
            v->setColumnWidth(col, defaultWidths[col]);
        }
        h->blockSignals(false);

        // デフォルト幅の場合も、初期化後に保存を有効にする
        QTimer::singleShot(500, this, [this, viewIndex]() {
            if (viewIndex == 0) {
                m_thinkingView1WidthsLoaded = true;
            } else {
                m_thinkingView2WidthsLoaded = true;
            }
        });
    }
}

// 追加：ヘッダー名で列を探す（大文字小文字は無視）
int EngineAnalysisTab::findColumnByHeader(QAbstractItemModel* model, const QString& title)
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

// 追加：Time/Depth/Nodes/Score を「右寄せ＋3桁カンマ」で表示するデリゲートを適用
void EngineAnalysisTab::applyNumericFormattingTo(QTableView* view, QAbstractItemModel* model)
{
    if (!view || !model) return;

    // 同じデリゲートを複数列に使い回す。親を view にしてメモリ管理を任せる
    auto* delegate = new NumericRightAlignCommaDelegate(view);

    // 日本語と英語の両方のヘッダー名に対応
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

// ★ 追加: USI通信ログツールバーを構築
void EngineAnalysisTab::buildUsiLogToolbar()
{
    m_usiLogToolbar = new QWidget(m_usiLogContainer);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_usiLogToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnUsiLogFontDecrease = new QToolButton(m_usiLogToolbar);
    m_btnUsiLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnUsiLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnUsiLogFontDecrease->setFixedSize(28, 24);
    connect(m_btnUsiLogFontDecrease, &QToolButton::clicked, this, &EngineAnalysisTab::onUsiLogFontDecrease);

    // フォントサイズ増加ボタン
    m_btnUsiLogFontIncrease = new QToolButton(m_usiLogToolbar);
    m_btnUsiLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnUsiLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnUsiLogFontIncrease->setFixedSize(28, 24);
    connect(m_btnUsiLogFontIncrease, &QToolButton::clicked, this, &EngineAnalysisTab::onUsiLogFontIncrease);

    // ★ 追加: エンジン名ラベル（E1: xxx, E2: xxx）
    m_usiLogEngine1Label = new QLabel(QStringLiteral("E1: ---"), m_usiLogToolbar);
    m_usiLogEngine1Label->setStyleSheet(QStringLiteral("QLabel { color: #2060a0; font-weight: bold; }"));
    m_usiLogEngine2Label = new QLabel(QStringLiteral("E2: ---"), m_usiLogToolbar);
    m_usiLogEngine2Label->setStyleSheet(QStringLiteral("QLabel { color: #a02060; font-weight: bold; }"));

    toolbarLayout->addWidget(m_btnUsiLogFontDecrease);
    toolbarLayout->addWidget(m_btnUsiLogFontIncrease);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_usiLogEngine1Label);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_usiLogEngine2Label);
    toolbarLayout->addStretch();

    m_usiLogToolbar->setLayout(toolbarLayout);
}

// ★ 追加: USI通信ログフォントサイズ変更
void EngineAnalysisTab::updateUsiLogFontSize(int delta)
{
    m_usiLogFontSize += delta;
    if (m_usiLogFontSize < 8) m_usiLogFontSize = 8;
    if (m_usiLogFontSize > 24) m_usiLogFontSize = 24;

    QFont font;
    font.setPointSize(m_usiLogFontSize);

    if (m_usiLog) {
        m_usiLog->setFont(font);
    }

    // ★ 追加: コマンドバーのフォントサイズも更新
    if (m_usiTargetCombo) m_usiTargetCombo->setFont(font);
    if (m_usiCommandInput) m_usiCommandInput->setFont(font);

    // ★ 追加: 設定ファイルに保存
    SettingsService::setUsiLogFontSize(m_usiLogFontSize);
}

void EngineAnalysisTab::onUsiLogFontIncrease()
{
    updateUsiLogFontSize(1);
}

void EngineAnalysisTab::onUsiLogFontDecrease()
{
    updateUsiLogFontSize(-1);
}

// ★ 追加: USIコマンドバーを構築
void EngineAnalysisTab::buildUsiCommandBar()
{
    m_usiCommandBar = new QWidget(m_usiLogContainer);
    QHBoxLayout* layout = new QHBoxLayout(m_usiCommandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 送信先選択コンボボックス
    m_usiTargetCombo = new QComboBox(m_usiCommandBar);
    m_usiTargetCombo->addItem(QStringLiteral("E1"), 0);
    m_usiTargetCombo->addItem(QStringLiteral("E2"), 1);
    m_usiTargetCombo->addItem(QStringLiteral("E1+E2"), 2);
    m_usiTargetCombo->setMinimumWidth(70);
    m_usiTargetCombo->setToolTip(tr("コマンドの送信先を選択"));

    // コマンド入力欄
    m_usiCommandInput = new QLineEdit(m_usiCommandBar);
    m_usiCommandInput->setPlaceholderText(tr("USIコマンドを入力してEnter"));

    layout->addWidget(m_usiTargetCombo);
    layout->addWidget(m_usiCommandInput, 1);  // stretchで伸縮

    // フォントサイズを適用
    {
        QFont font;
        font.setPointSize(m_usiLogFontSize);
        m_usiTargetCombo->setFont(font);
        m_usiCommandInput->setFont(font);
    }

    m_usiCommandBar->setLayout(layout);

    // シグナル接続
    connect(m_usiCommandInput, &QLineEdit::returnPressed,
            this, &EngineAnalysisTab::onUsiCommandEntered);
}

// ★ 追加: USIコマンド入力処理（Enterキー）
void EngineAnalysisTab::onUsiCommandEntered()
{
    if (!m_usiCommandInput || !m_usiTargetCombo) {
        return;
    }

    QString command = m_usiCommandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    int target = m_usiTargetCombo->currentData().toInt();
    emit usiCommandRequested(target, command);

    // 入力欄をクリア
    m_usiCommandInput->clear();
}

// ★ 追加: エンジン1名変更時のスロット
void EngineAnalysisTab::onEngine1NameChanged()
{
    if (m_usiLogEngine1Label && m_log1) {
        QString name = m_log1->engineName();
        if (name.isEmpty()) {
            name = QStringLiteral("---");
        }
        m_usiLogEngine1Label->setText(QStringLiteral("E1: %1").arg(name));
    }
}

// ★ 追加: エンジン2名変更時のスロット
void EngineAnalysisTab::onEngine2NameChanged()
{
    if (m_usiLogEngine2Label && m_log2) {
        QString name = m_log2->engineName();
        if (name.isEmpty()) {
            name = QStringLiteral("---");
        }
        m_usiLogEngine2Label->setText(QStringLiteral("E2: %1").arg(name));
    }
}

// ★ 追加: 思考タブフォントサイズ変更
void EngineAnalysisTab::updateThinkingFontSize(int delta)
{
    m_thinkingFontSize += delta;
    if (m_thinkingFontSize < 8) m_thinkingFontSize = 8;
    if (m_thinkingFontSize > 24) m_thinkingFontSize = 24;

    QFont font;
    font.setPointSize(m_thinkingFontSize);

    // 上段（EngineInfoWidget）のフォントサイズ変更
    if (m_info1) m_info1->setFontSize(m_thinkingFontSize);
    if (m_info2) m_info2->setFontSize(m_thinkingFontSize);

    // 下段（TableView）のフォントサイズ変更
    if (m_view1) {
        m_view1->setFont(font);
        // 行の高さもフォントサイズに合わせて更新
        const int rowHeight = m_view1->fontMetrics().height() + 4;
        m_view1->verticalHeader()->setDefaultSectionSize(rowHeight);
    }
    if (m_view2) {
        m_view2->setFont(font);
        // 行の高さもフォントサイズに合わせて更新
        const int rowHeight = m_view2->fontMetrics().height() + 4;
        m_view2->verticalHeader()->setDefaultSectionSize(rowHeight);
    }

    // ★ 追加: 設定ファイルに保存
    SettingsService::setThinkingFontSize(m_thinkingFontSize);
}

void EngineAnalysisTab::onThinkingFontIncrease()
{
    updateThinkingFontSize(1);
}

void EngineAnalysisTab::onThinkingFontDecrease()
{
    updateThinkingFontSize(-1);
}

// ★ 追加: コメントツールバーを構築
void EngineAnalysisTab::buildCommentToolbar()
{
    m_commentToolbar = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_commentToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnFontDecrease = new QToolButton(m_commentToolbar);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnFontDecrease->setFixedSize(28, 24);
    connect(m_btnFontDecrease, &QToolButton::clicked, this, &EngineAnalysisTab::onFontDecrease);

    // フォントサイズ増加ボタン
    m_btnFontIncrease = new QToolButton(m_commentToolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    connect(m_btnFontIncrease, &QToolButton::clicked, this, &EngineAnalysisTab::onFontIncrease);

    // undoボタン（元に戻す）
    m_btnCommentUndo = new QToolButton(m_commentToolbar);
    m_btnCommentUndo->setText(QStringLiteral("↩"));
    m_btnCommentUndo->setToolTip(tr("元に戻す (Ctrl+Z)"));
    m_btnCommentUndo->setFixedSize(28, 24);
    connect(m_btnCommentUndo, &QToolButton::clicked, this, &EngineAnalysisTab::onCommentUndo);

    // ★ 追加: redoボタン（やり直す）
    m_btnCommentRedo = new QToolButton(m_commentToolbar);
    m_btnCommentRedo->setText(QStringLiteral("↪"));
    m_btnCommentRedo->setToolTip(tr("やり直す (Ctrl+Y)"));
    m_btnCommentRedo->setFixedSize(28, 24);
    connect(m_btnCommentRedo, &QToolButton::clicked, this, &EngineAnalysisTab::onCommentRedo);

    // ★ 追加: 切り取りボタン
    m_btnCommentCut = new QToolButton(m_commentToolbar);
    m_btnCommentCut->setText(QStringLiteral("✂"));
    m_btnCommentCut->setToolTip(tr("切り取り (Ctrl+X)"));
    m_btnCommentCut->setFixedSize(28, 24);
    connect(m_btnCommentCut, &QToolButton::clicked, this, &EngineAnalysisTab::onCommentCut);

    // ★ 追加: コピーボタン
    m_btnCommentCopy = new QToolButton(m_commentToolbar);
    m_btnCommentCopy->setText(QStringLiteral("📋"));
    m_btnCommentCopy->setToolTip(tr("コピー (Ctrl+C)"));
    m_btnCommentCopy->setFixedSize(28, 24);
    connect(m_btnCommentCopy, &QToolButton::clicked, this, &EngineAnalysisTab::onCommentCopy);

    // ★ 追加: 貼り付けボタン
    m_btnCommentPaste = new QToolButton(m_commentToolbar);
    m_btnCommentPaste->setText(QStringLiteral("📄"));
    m_btnCommentPaste->setToolTip(tr("貼り付け (Ctrl+V)"));
    m_btnCommentPaste->setFixedSize(28, 24);
    connect(m_btnCommentPaste, &QToolButton::clicked, this, &EngineAnalysisTab::onCommentPaste);

    // 「修正中」ラベル（赤字）
    m_editingLabel = new QLabel(tr("修正中"), m_commentToolbar);
    m_editingLabel->setStyleSheet(QStringLiteral("QLabel { color: red; font-weight: bold; }"));
    m_editingLabel->setVisible(false);  // 初期状態は非表示

    // コメント更新ボタン
    m_btnUpdateComment = new QPushButton(tr("コメント更新"), m_commentToolbar);
    m_btnUpdateComment->setToolTip(tr("編集したコメントを棋譜に反映する"));
    m_btnUpdateComment->setFixedHeight(24);
    connect(m_btnUpdateComment, &QPushButton::clicked, this, &EngineAnalysisTab::onUpdateCommentClicked);

    toolbarLayout->addWidget(m_btnFontDecrease);
    toolbarLayout->addWidget(m_btnFontIncrease);
    toolbarLayout->addWidget(m_btnCommentUndo);
    toolbarLayout->addWidget(m_btnCommentRedo);   // ★ 追加
    toolbarLayout->addWidget(m_btnCommentCut);    // ★ 追加
    toolbarLayout->addWidget(m_btnCommentCopy);   // ★ 追加
    toolbarLayout->addWidget(m_btnCommentPaste);  // ★ 追加
    toolbarLayout->addWidget(m_editingLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_btnUpdateComment);

    m_commentToolbar->setLayout(toolbarLayout);
}

// ★ 追加: フォントサイズ更新
void EngineAnalysisTab::updateCommentFontSize(int delta)
{
    m_currentFontSize += delta;
    if (m_currentFontSize < 8) m_currentFontSize = 8;
    if (m_currentFontSize > 24) m_currentFontSize = 24;

    if (m_comment) {
        QFont font = m_comment->font();
        font.setPointSize(m_currentFontSize);
        m_comment->setFont(font);
    }
    
    // ★ 追加: 設定ファイルに保存
    SettingsService::setCommentFontSize(m_currentFontSize);
}

// ★ 追加: コメントのundo（QTextEditのundo機能を使用）
void EngineAnalysisTab::onCommentUndo()
{
    if (!m_comment) return;
    m_comment->undo();
}

// ★ 追加: コメントのredo（やり直す）
void EngineAnalysisTab::onCommentRedo()
{
    if (!m_comment) return;
    m_comment->redo();
}

// ★ 追加: コメントの切り取り
void EngineAnalysisTab::onCommentCut()
{
    if (!m_comment) return;
    m_comment->cut();
}

// ★ 追加: コメントのコピー
void EngineAnalysisTab::onCommentCopy()
{
    if (!m_comment) return;
    m_comment->copy();
}

// ★ 追加: コメントの貼り付け
void EngineAnalysisTab::onCommentPaste()
{
    if (!m_comment) return;
    m_comment->paste();
}

// ★ 追加: URLをHTMLリンクに変換
QString EngineAnalysisTab::convertUrlsToLinks(const QString& text)
{
    QString result = text;
    
    // URLパターン（http, https, ftp）
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((https?://|ftp://)[^\s<>"']+)"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    // すでにリンクになっているか確認するための正規表現
    static const QRegularExpression existingLinkPattern(
        QStringLiteral(R"(<a\s+[^>]*href=)"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    // すでにHTMLにリンクが含まれている場合はそのまま返す
    if (existingLinkPattern.match(result).hasMatch()) {
        return result;
    }
    
    // 改行を<br>に変換
    result.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    
    // URLをリンクに変換
    QRegularExpressionMatchIterator i = urlPattern.globalMatch(result);
    QVector<QPair<int, int>> matches;
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        matches.append(qMakePair(match.capturedStart(), match.capturedLength()));
    }
    
    // 後ろから置換して位置がずれないようにする
    for (qsizetype j = matches.size() - 1; j >= 0; --j) {
        int start = matches[j].first;
        int length = matches[j].second;
        QString url = result.mid(start, length);
        QString link = QStringLiteral("<a href=\"%1\" style=\"color: blue; text-decoration: underline;\">%1</a>").arg(url);
        result.replace(start, length, link);
    }
    
    return result;
}

// ★ 追加: フォントサイズ増加スロット
void EngineAnalysisTab::onFontIncrease()
{
    updateCommentFontSize(1);
}

// ★ 追加: フォントサイズ減少スロット
void EngineAnalysisTab::onFontDecrease()
{
    updateCommentFontSize(-1);
}

// ★ 追加: コメント更新ボタンクリック時のスロット
void EngineAnalysisTab::onUpdateCommentClicked()
{
    if (!m_comment) return;
    
    // HTMLからプレーンテキストを取得
    QString newComment = m_comment->toPlainText();
    
    // シグナルを発行
    emit commentUpdated(m_currentMoveIndex, newComment);
    
    // ★ 編集状態をクリア
    m_originalComment = newComment;
    m_isCommentDirty = false;
    updateEditingIndicator();
}

// ★ 追加: 現在の手数インデックスを設定
void EngineAnalysisTab::setCurrentMoveIndex(int index)
{
    qDebug().noquote()
        << "[EngineAnalysisTab] setCurrentMoveIndex:"
        << " old=" << m_currentMoveIndex
        << " new=" << index;
    m_currentMoveIndex = index;
}

// ★ 追加: コメントテキスト変更時のスロット
void EngineAnalysisTab::onCommentTextChanged()
{
    if (!m_comment) return;
    
    // 現在のテキストと元のテキストを比較
    QString currentText = m_comment->toPlainText();
    bool isDirty = (currentText != m_originalComment);
    
    // ★ デバッグ出力
    qDebug().noquote()
        << "[EngineAnalysisTab] onCommentTextChanged:"
        << " currentText.len=" << currentText.size()
        << " originalComment.len=" << m_originalComment.size()
        << " isDirty=" << isDirty
        << " m_isCommentDirty(before)=" << m_isCommentDirty;
    
    if (m_isCommentDirty != isDirty) {
        m_isCommentDirty = isDirty;
        updateEditingIndicator();
        qDebug().noquote() << "[EngineAnalysisTab] m_isCommentDirty changed to:" << m_isCommentDirty;
    }
}

// ★ 追加: 「修正中」表示の更新
void EngineAnalysisTab::updateEditingIndicator()
{
    if (m_editingLabel) {
        m_editingLabel->setVisible(m_isCommentDirty);
        qDebug().noquote() << "[EngineAnalysisTab] updateEditingIndicator: visible=" << m_isCommentDirty;
    }
}

// ★ 追加: 未保存編集の警告ダイアログ
bool EngineAnalysisTab::confirmDiscardUnsavedComment()
{
    qDebug().noquote()
        << "[EngineAnalysisTab] confirmDiscardUnsavedComment ENTER:"
        << " m_isCommentDirty=" << m_isCommentDirty;
    
    if (!m_isCommentDirty) {
        qDebug().noquote() << "[EngineAnalysisTab] confirmDiscardUnsavedComment: not dirty, returning true";
        return true;  // 変更がなければそのまま移動OK
    }
    
    qDebug().noquote() << "[EngineAnalysisTab] confirmDiscardUnsavedComment: showing QMessageBox...";
    
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("未保存のコメント"),
        tr("コメントが編集されていますが、まだ更新されていません。\n"
           "変更を破棄して移動しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    qDebug().noquote() << "[EngineAnalysisTab] confirmDiscardUnsavedComment: reply=" << reply;
    
    if (reply == QMessageBox::Yes) {
        // 変更を破棄
        m_isCommentDirty = false;
        updateEditingIndicator();
        qDebug().noquote() << "[EngineAnalysisTab] confirmDiscardUnsavedComment: user chose Yes, returning true";
        return true;
    }
    
    qDebug().noquote() << "[EngineAnalysisTab] confirmDiscardUnsavedComment: user chose No, returning false";
    return false;  // 移動をキャンセル
}

// ★ 追加: 編集状態をクリア
void EngineAnalysisTab::clearCommentDirty()
{
    if (m_comment) {
        m_originalComment = m_comment->toPlainText();
    }
    m_isCommentDirty = false;
    updateEditingIndicator();
}

// ★ 追加: エンジン1の読み筋テーブルクリック処理
void EngineAnalysisTab::onView1Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    // 「盤面」列（列4）のクリック時のみ読み筋表示ウィンドウを開く
    if (index.column() == 4) {
        qDebug() << "[EngineAnalysisTab] onView1Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(0, index.row());
    }
}

// ★ 追加: エンジン2の読み筋テーブルクリック処理
void EngineAnalysisTab::onView2Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    // 「盤面」列（列4）のクリック時のみ読み筋表示ウィンドウを開く
    if (index.column() == 4) {
        qDebug() << "[EngineAnalysisTab] onView2Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(1, index.row());
    }
}

// ★ 追加: エンジン情報ウィジェットの列幅変更時の保存
void EngineAnalysisTab::onEngineInfoColumnWidthChanged()
{
    EngineInfoWidget* sender = qobject_cast<EngineInfoWidget*>(QObject::sender());
    if (!sender) return;
    
    int widgetIndex = sender->widgetIndex();
    QList<int> widths = sender->columnWidths();
    
    // 設定ファイルに保存
    SettingsService::setEngineInfoColumnWidths(widgetIndex, widths);
}

// ★ 追加: 思考タブ下段（読み筋テーブル）の列幅変更時の保存
void EngineAnalysisTab::onThinkingViewColumnWidthChanged(int viewIndex)
{
    QTableView* view = (viewIndex == 0) ? m_view1 : m_view2;
    if (!view) return;
    
    QList<int> widths;
    const int colCount = view->horizontalHeader()->count();
    for (int col = 0; col < colCount; ++col) {
        widths.append(view->columnWidth(col));
    }
    
    // 設定ファイルに保存
    SettingsService::setThinkingViewColumnWidths(viewIndex, widths);
}

// ★ 追加: view1の列幅変更スロット
void EngineAnalysisTab::onView1SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    // 設定ファイルから読み込み済みの場合、初期化時のリサイズイベントは無視しない
    // ただし、初期表示の自動調整による保存は避ける必要がある
    // m_thinkingView1WidthsLoadedがtrueの場合のみ、ユーザー操作とみなして保存
    if (m_thinkingView1WidthsLoaded) {
        onThinkingViewColumnWidthChanged(0);
    }
}

// ★ 追加: view2の列幅変更スロット
void EngineAnalysisTab::onView2SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    if (m_thinkingView2WidthsLoaded) {
        onThinkingViewColumnWidthChanged(1);
    }
}

// ★ 追加: CSA通信ログツールバーを構築
void EngineAnalysisTab::buildCsaLogToolbar()
{
    m_csaLogToolbar = new QWidget(m_csaLogContainer);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_csaLogToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnCsaLogFontDecrease = new QToolButton(m_csaLogToolbar);
    m_btnCsaLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnCsaLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnCsaLogFontDecrease->setFixedSize(28, 24);
    connect(m_btnCsaLogFontDecrease, &QToolButton::clicked, this, &EngineAnalysisTab::onCsaLogFontDecrease);

    // フォントサイズ増加ボタン
    m_btnCsaLogFontIncrease = new QToolButton(m_csaLogToolbar);
    m_btnCsaLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnCsaLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnCsaLogFontIncrease->setFixedSize(28, 24);
    connect(m_btnCsaLogFontIncrease, &QToolButton::clicked, this, &EngineAnalysisTab::onCsaLogFontIncrease);

    toolbarLayout->addWidget(m_btnCsaLogFontDecrease);
    toolbarLayout->addWidget(m_btnCsaLogFontIncrease);
    toolbarLayout->addStretch();

    m_csaLogToolbar->setLayout(toolbarLayout);
}

// ★ 追加: CSA通信ログフォントサイズ変更
void EngineAnalysisTab::updateCsaLogFontSize(int delta)
{
    m_csaLogFontSize += delta;
    if (m_csaLogFontSize < 8) m_csaLogFontSize = 8;
    if (m_csaLogFontSize > 24) m_csaLogFontSize = 24;

    // ログ表示エリア
    if (m_csaLog) {
        QFont font = m_csaLog->font();
        font.setPointSize(m_csaLogFontSize);
        m_csaLog->setFont(font);
    }

    // コマンド入力部分も同じフォントサイズに
    if (m_btnCsaSendToServer) {
        QFont font = m_btnCsaSendToServer->font();
        font.setPointSize(m_csaLogFontSize);
        m_btnCsaSendToServer->setFont(font);
    }

    if (m_csaCommandInput) {
        QFont font = m_csaCommandInput->font();
        font.setPointSize(m_csaLogFontSize);
        m_csaCommandInput->setFont(font);
    }

    // SettingsServiceに保存
    SettingsService::setCsaLogFontSize(m_csaLogFontSize);
}

void EngineAnalysisTab::onCsaLogFontIncrease()
{
    updateCsaLogFontSize(1);
}

void EngineAnalysisTab::onCsaLogFontDecrease()
{
    updateCsaLogFontSize(-1);
}

// ★ 追加: CSA通信ログ追記
void EngineAnalysisTab::appendCsaLog(const QString& line)
{
    if (m_csaLog) {
        m_csaLog->appendPlainText(line);
        // 自動スクロール
        QTextCursor cursor = m_csaLog->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_csaLog->setTextCursor(cursor);
    }
}

// ★ 追加: CSA通信ログクリア
void EngineAnalysisTab::clearCsaLog()
{
    if (m_csaLog) {
        m_csaLog->clear();
    }
}

// ★ 追加: CSAコマンド入力バーを構築
void EngineAnalysisTab::buildCsaCommandBar()
{
    m_csaCommandBar = new QWidget(m_csaLogContainer);
    QHBoxLayout* layout = new QHBoxLayout(m_csaCommandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // CSAサーバーへ送信ボタン（ラベル表示用）
    m_btnCsaSendToServer = new QPushButton(tr("CSAサーバーへ送信"), m_csaCommandBar);
    m_btnCsaSendToServer->setEnabled(false);  // クリック不可（ラベルとして表示）
    m_btnCsaSendToServer->setFlat(true);      // フラットスタイル
    m_btnCsaSendToServer->setMinimumWidth(130);

    layout->addWidget(m_btnCsaSendToServer);

    // コマンド入力欄
    m_csaCommandInput = new QLineEdit(m_csaCommandBar);
    m_csaCommandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    layout->addWidget(m_csaCommandInput, 1);  // stretchで伸縮

    // コマンド入力部分のフォントサイズを設定
    {
        QFont cmdFont;
        cmdFont.setPointSize(m_csaLogFontSize);
        m_btnCsaSendToServer->setFont(cmdFont);
        m_csaCommandInput->setFont(cmdFont);
    }

    m_csaCommandBar->setLayout(layout);

    // コマンド入力のEnterキー処理を接続
    connect(m_csaCommandInput, &QLineEdit::returnPressed,
            this, &EngineAnalysisTab::onCsaCommandEntered);
}

// ★ 追加: CSAコマンド入力処理
void EngineAnalysisTab::onCsaCommandEntered()
{
    if (!m_csaCommandInput) {
        return;
    }

    QString command = m_csaCommandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    // CSAサーバーへ送信シグナルを発行
    emit csaRawCommandRequested(command);

    // 入力欄をクリア
    m_csaCommandInput->clear();
}

