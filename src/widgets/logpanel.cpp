/// @file logpanel.cpp
/// @brief 通信ログのパネルの共通部分の実装

#include "logpanel.h"

#include "applicationfonts.h"
#include "buttonstyles.h"
#include "fontsizecontrol.h"
#include "toolbarwidth.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QTextBlock>
#include <QVBoxLayout>

namespace {
// ログの末尾へ1行足しても、直前の行の文字配置を捨てない。
// QPlainTextDocumentLayout は、直前の行の末尾に段落の区切りと新しい行を足した変化として
// 受け取り、文字の変わらない直前の行も配置し直す。ログは長い行が多く、ドックが裏のタブに
// あっても Qt は表示中として扱うので、追記のたびに2行分の文字配置を計算していた。
class LogDocumentLayout final : public QPlainTextDocumentLayout
{
public:
    using QPlainTextDocumentLayout::QPlainTextDocumentLayout;

protected:
    void documentChanged(int from, int charsRemoved, int charsAdded) override
    {
        const QTextDocument* doc = document();
        if (charsRemoved == 0 && charsAdded > 1 && from + charsAdded == doc->characterCount() - 1
            && doc->characterAt(from) == QChar::ParagraphSeparator) {
            // 変わったのは区切りの後ろ（新しい行）だけとして渡す
            QPlainTextDocumentLayout::documentChanged(from + 1, 0, charsAdded - 1);
            return;
        }
        QPlainTextDocumentLayout::documentChanged(from, charsRemoved, charsAdded);
    }
};

/// 生きているパネル（同じ設定を使うパネルに変更を伝える。UI のスレッドだけで使う）
QList<LogPanel*>& livePanels()
{
    static QList<LogPanel*> panels;
    return panels;
}
} // namespace

LogPanel::LogPanel(const BoundedSetting<int>& fontSize, const Setting<bool>& wrapLines, const Names& names, QWidget* parent)
    : QWidget(parent)
    , m_fontSizeSetting(fontSize)
    , m_wrapLines(wrapLines)
    , m_fontSize(fontSize)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    buildToolbar(names);
    layout->addWidget(m_toolbar);

    m_logView = new QPlainTextEdit(this);
    {
        // QPlainTextEdit が自分で作る文書と同じ設定にして、行の配置だけを差し替える
        auto* document = new QTextDocument(m_logView);
        document->setDocumentLayout(new LogDocumentLayout(document));
        document->setDefaultFont(m_logView->font());
        document->documentLayout()->setPaintDevice(m_logView->viewport());
        m_logView->setDocument(document);
    }
    m_logView->setObjectName(names.view);
    m_logView->setReadOnly(true);
    // 対局・検討を繰り返しても通信履歴でメモリーが増え続けないようにする。
    m_logView->setMaximumBlockCount(5000);
    m_logView->setFont(ApplicationFonts::monospaceFont());
    m_logView->setLineWrapMode(m_wrapAction->isChecked() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    m_logView->document()->setDocumentMargin(6);
    layout->addWidget(m_logView, 1);
    connect(m_logView, &QPlainTextEdit::textChanged, this, &LogPanel::updateLogActions);

    buildCommandBar(names);
    layout->addWidget(m_commandBar);

    applyFontSize();
    updateLogActions();
    livePanels().append(this);
}

LogPanel::~LogPanel()
{
    livePanels().removeOne(this);
}

QList<LogPanel*> LogPanel::panelsSharingFontSize() const
{
    QList<LogPanel*> result;
    for (LogPanel* panel : std::as_const(livePanels())) {
        if (panel != this && qstrcmp(panel->m_fontSizeSetting.key(), m_fontSizeSetting.key()) == 0)
            result.append(panel);
    }
    return result;
}

QList<LogPanel*> LogPanel::panelsSharingWrapLines() const
{
    QList<LogPanel*> result;
    for (LogPanel* panel : std::as_const(livePanels())) {
        if (panel != this && qstrcmp(panel->m_wrapLines.key(), m_wrapLines.key()) == 0) result.append(panel);
    }
    return result;
}

void LogPanel::buildToolbar(const Names& names)
{
    // 幅が狭いドックでは標準の拡張メニューへ操作を退避する。
    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName(names.toolbar);
    m_toolbar->setStyleSheet(ButtonStyles::panelToolButton());

    auto* decrease = new QToolButton(m_toolbar);
    auto* increase = new QToolButton(m_toolbar);
    new FontSizeControl(decrease, increase, &m_fontSize);
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &LogPanel::onFontSizeChanged);
    m_toolbar->addWidget(decrease);
    m_toolbar->addWidget(increase);
    m_toolbar->addSeparator();
    auto* spacer = new QWidget(m_toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_spacerAction = m_toolbar->addWidget(spacer);

    m_wrapAction = m_toolbar->addAction(tr("折り返し"));
    m_wrapAction->setObjectName(names.wrap);
    m_wrapAction->setToolTip(tr("長い行をウィンドウ幅で折り返す"));
    m_wrapAction->setCheckable(true);
    m_wrapAction->setChecked(m_wrapLines());
    connect(m_wrapAction, &QAction::toggled, this, &LogPanel::onWrapToggled);

    m_latestAction = m_toolbar->addAction(tr("最新へ"));
    m_latestAction->setObjectName(names.latest);
    m_latestAction->setToolTip(tr("最新のログへ移動（末尾では新着ログに自動追従）"));
    connect(m_latestAction, &QAction::triggered, this, &LogPanel::scrollToLatest);
    m_toolbar->addSeparator();
    m_copyAction = m_toolbar->addAction(tr("全てコピー"));
    m_copyAction->setObjectName(names.copy);
    m_copyAction->setToolTip(tr("ログ全体をクリップボードへコピー"));
    connect(m_copyAction, &QAction::triggered, this, &LogPanel::copyAll);
    m_clearAction = m_toolbar->addAction(tr("消去"));
    m_clearAction->setObjectName(names.clear);
    m_clearAction->setToolTip(tr("表示中のログを消去"));
    connect(m_clearAction, &QAction::triggered, this, &LogPanel::clear);
    ToolbarWidth::relax(m_toolbar);
}

void LogPanel::buildCommandBar(const Names& names)
{
    m_commandBar = new QWidget(this);
    m_commandLayout = new QHBoxLayout(m_commandBar);
    m_commandLayout->setContentsMargins(0, 0, 0, 0);
    m_commandLayout->setSpacing(4);

    m_commandInput = new QLineEdit(m_commandBar);
    m_commandInput->setObjectName(names.commandInput);
    m_commandInput->setClearButtonEnabled(true);
    m_commandInput->setFont(ApplicationFonts::monospaceFont());

    m_sendButton = new QPushButton(tr("送信"), m_commandBar);
    m_sendButton->setObjectName(names.sendButton);
    m_sendButton->setEnabled(false);

    m_commandLayout->addWidget(m_commandInput, 1);
    m_commandLayout->addWidget(m_sendButton);
    ToolbarWidth::relax(m_commandBar);

    connect(m_commandInput, &QLineEdit::returnPressed, this, &LogPanel::onCommandEntered);
    connect(m_sendButton, &QPushButton::clicked, this, &LogPanel::onCommandEntered);
    connect(m_commandInput, &QLineEdit::textChanged, this, &LogPanel::updateSendButton);
}

void LogPanel::addToolbarWidget(QWidget* widget)
{
    m_toolbar->insertWidget(m_spacerAction, widget);
}

void LogPanel::appendLine(const QString& rawLine, const QColor& color)
{
    if (rawLine.isEmpty()) return;
    // 行数の上限だけでは、長い行でメモリを使い切る（通信の1行は 64KB まで届く）。表示は先頭だけにする（セキュリティ監査 V15）
    constexpr qsizetype kMaxLineChars = 4096;
    const QString line = rawLine.size() <= kMaxLineChars ? rawLine
        : rawLine.left(kMaxLineChars) + tr(" …（%1 文字を省略）").arg(rawLine.size() - kMaxLineChars);

    // 表示用カーソルを動かさず追記する。過去ログの閲覧位置と選択範囲を保つ。
    auto* vertical = m_logView->verticalScrollBar();
    auto* horizontal = m_logView->horizontalScrollBar();
    const int oldVertical = vertical->value();
    const int oldHorizontal = horizontal->value();
    QTextDocument* document = m_logView->document();
    const QTextCursor original = m_logView->textCursor();
    const bool follow = oldVertical == vertical->maximum() && !original.hasSelection();
    // 末尾への追記で動くのは、末尾にある表示用カーソルの端だけ。先頭ログの削除には
    // 表示用カーソル自身が追従するので、末尾にないときは戻す処理を省く
    // （カーソルの位置決めは行の文字配置を毎回計算し直すため重い）。
    const int end = document->characterCount() - 1;
    const bool restoreCursor = original.position() == end || original.anchor() == end;
    // 数値の位置ではなくカーソルで保持し、先頭ログの削除に追従させる。
    // 末尾を選択している場合も追記で選択が伸びないよう、両端を個別に固定する。
    QTextCursor position;
    QTextCursor anchor;
    if (restoreCursor) {
        position = QTextCursor(document);
        position.setPosition(original.position());
        position.setKeepPositionOnInsert(true);
        anchor = QTextCursor(document);
        anchor.setPosition(original.anchor());
        anchor.setKeepPositionOnInsert(true);
    }

    QTextCharFormat format;
    if (color.isValid()) format.setForeground(color);
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    // 編集中に末尾へ移る。編集の外で移ると、最終行の文字配置を計算し直す。
    cursor.movePosition(QTextCursor::End);
    if (!document->isEmpty()) cursor.insertBlock();
    cursor.insertText(line, format);
    cursor.endEditBlock();

    // 行数の上限だけでは、長い行が続くと行の配置のデータで GB のメモリを使う。文字数でも古い行から捨てる（セキュリティ監査 V15）
    constexpr int kMaxDocumentChars = 1'000'000;
    if (document->characterCount() > kMaxDocumentChars) {
        const int target = kMaxDocumentChars * 9 / 10;
        int remaining = document->characterCount();
        QTextBlock oldest = document->begin();
        while (remaining > target && oldest.next().isValid()) {
            remaining -= oldest.length();
            oldest = oldest.next();
        }
        QTextCursor trim(document);
        trim.setPosition(oldest.position(), QTextCursor::KeepAnchor);
        trim.removeSelectedText();
    }

    const int adjustedVertical = vertical->value();
    if (restoreCursor) {
        QTextCursor restored(document);
        restored.setPosition(anchor.position());
        restored.setPosition(position.position(), QTextCursor::KeepAnchor);
        m_logView->setTextCursor(restored);
    }
    vertical->setValue(follow ? vertical->maximum() : adjustedVertical);
    horizontal->setValue(oldHorizontal);
}

void LogPanel::clear()
{
    m_logView->clear();
}

void LogPanel::applyFontSize()
{
    const int size = m_fontSize.fontSize();
    for (QWidget* widget : {static_cast<QWidget*>(m_logView), static_cast<QWidget*>(m_commandInput),
                            static_cast<QWidget*>(m_sendButton)}) {
        QFont font = widget->font();
        font.setPointSize(size);
        widget->setFont(font);
    }
}

void LogPanel::onFontSizeChanged(int size)
{
    applyFontSize();
    // 同じ設定のパネルも同じ大きさにする（同じ大きさなら何もしないので、伝え返しは止まる）
    for (LogPanel* panel : panelsSharingFontSize()) panel->m_fontSize.setFontSize(size);
}

void LogPanel::onCommandEntered()
{
    const QString command = m_commandInput->text().trimmed();
    if (command.isEmpty() || !canSend()) return;
    emit commandEntered(command);
    m_commandInput->clear();
    m_commandInput->setFocus();
}

void LogPanel::updateSendButton()
{
    m_sendButton->setEnabled(canSend() && !m_commandInput->text().trimmed().isEmpty());
}

void LogPanel::onWrapToggled(bool checked)
{
    auto* scroll = m_logView->verticalScrollBar();
    const bool follow = scroll->value() == scroll->maximum() && !m_logView->textCursor().hasSelection();
    m_logView->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    if (follow) scroll->setValue(scroll->maximum());
    m_wrapLines.set(checked);
    // 同じ設定のパネルも合わせる（同じ状態なら toggled が出ないので、伝え返しは止まる）
    for (LogPanel* panel : panelsSharingWrapLines()) panel->m_wrapAction->setChecked(checked);
}

void LogPanel::scrollToLatest()
{
    QTextCursor cursor = m_logView->textCursor();
    cursor.clearSelection();
    m_logView->setTextCursor(cursor);
    m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
}

void LogPanel::copyAll()
{
    QApplication::clipboard()->setText(m_logView->toPlainText());
}

void LogPanel::updateLogActions()
{
    const bool hasLog = !m_logView->document()->isEmpty();
    m_copyAction->setEnabled(hasLog);
    m_clearAction->setEnabled(hasLog);
    m_latestAction->setEnabled(hasLog);
}
