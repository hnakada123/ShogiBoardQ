/// @file commenteditorpanel.cpp
/// @brief 棋譜コメント編集パネルクラスの実装

#include "commenteditorpanel.h"
#include "buttonstyles.h"
#include "settingsservice.h"
#include "loggingcategory.h"

#include <QTextEdit>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QSizePolicy>

namespace {
void relaxToolbarWidth(QWidget* toolbar)
{
    if (!toolbar) return;
    toolbar->setMinimumWidth(0);
    QSizePolicy pol = toolbar->sizePolicy();
    pol.setHorizontalPolicy(QSizePolicy::Ignored);
    toolbar->setSizePolicy(pol);
}
} // namespace

CommentEditorPanel::CommentEditorPanel(QObject* parent)
    : QObject(parent)
{
}

CommentEditorPanel::~CommentEditorPanel()
{
    m_commentViewport = nullptr;
}

QWidget* CommentEditorPanel::buildCommentUi(QWidget* parent)
{
    QWidget* commentContainer = new QWidget(parent);
    QVBoxLayout* commentLayout = new QVBoxLayout(commentContainer);
    commentLayout->setContentsMargins(4, 4, 4, 4);
    commentLayout->setSpacing(2);

    buildCommentToolbar(commentContainer);
    commentLayout->addWidget(m_commentToolbar);

    m_comment = new QTextEdit(commentContainer);
    m_comment->setReadOnly(false);
    m_comment->setAcceptRichText(true);
    m_comment->setPlaceholderText(tr("コメントを表示・編集"));
    commentLayout->addWidget(m_comment);

    if (m_comment->viewport()) {
        m_commentViewport = m_comment->viewport();
        m_commentViewport->installEventFilter(this);
    }

    connect(m_comment, &QTextEdit::textChanged,
            this, &CommentEditorPanel::onCommentTextChanged);

    m_currentFontSize = SettingsService::commentFontSize();
    if (m_comment) {
        QFont font = m_comment->font();
        font.setPointSize(m_currentFontSize);
        m_comment->setFont(font);
    }

    return commentContainer;
}

void CommentEditorPanel::setCommentText(const QString& text)
{
    if (m_comment) {
        QString htmlText = convertUrlsToLinks(text);
        m_comment->setHtml(htmlText);
    }
}

void CommentEditorPanel::setCommentHtml(const QString& html)
{
    if (m_comment) {
        qCDebug(lcUi).noquote()
            << "[CommentEditorPanel] setCommentHtml ENTER:"
            << " html.len=" << html.size()
            << " m_isCommentDirty(before)=" << m_isCommentDirty;

        QString processedHtml = convertUrlsToLinks(html);
        m_comment->setHtml(processedHtml);
        m_originalComment = m_comment->toPlainText();

        qCDebug(lcUi).noquote()
            << "[CommentEditorPanel] setCommentHtml:"
            << " m_originalComment.len=" << m_originalComment.size();

        m_isCommentDirty = false;
        updateEditingIndicator();

        qCDebug(lcUi).noquote()
            << "[CommentEditorPanel] setCommentHtml LEAVE:"
            << " m_isCommentDirty=" << m_isCommentDirty;
    }
}

void CommentEditorPanel::setCurrentMoveIndex(int index)
{
    qCDebug(lcUi).noquote()
        << "[CommentEditorPanel] setCurrentMoveIndex:"
        << " old=" << m_currentMoveIndex
        << " new=" << index;
    m_currentMoveIndex = index;
}

bool CommentEditorPanel::confirmDiscardUnsavedComment()
{
    qCDebug(lcUi).noquote()
        << "[CommentEditorPanel] confirmDiscardUnsavedComment ENTER:"
        << " m_isCommentDirty=" << m_isCommentDirty;

    if (!m_isCommentDirty) {
        qCDebug(lcUi).noquote() << "[CommentEditorPanel] confirmDiscardUnsavedComment: not dirty, returning true";
        return true;
    }

    qCDebug(lcUi).noquote() << "[CommentEditorPanel] confirmDiscardUnsavedComment: showing QMessageBox...";

    // 親ウィジェットをQTextEditから取得
    QWidget* parentWidget = m_comment ? m_comment->window() : nullptr;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        parentWidget,
        tr("未保存のコメント"),
        tr("コメントが編集されていますが、まだ更新されていません。\n"
           "変更を破棄して移動しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    qCDebug(lcUi).noquote() << "[CommentEditorPanel] confirmDiscardUnsavedComment: reply=" << reply;

    if (reply == QMessageBox::Yes) {
        m_isCommentDirty = false;
        updateEditingIndicator();
        qCDebug(lcUi).noquote() << "[CommentEditorPanel] confirmDiscardUnsavedComment: user chose Yes, returning true";
        return true;
    }

    qCDebug(lcUi).noquote() << "[CommentEditorPanel] confirmDiscardUnsavedComment: user chose No, returning false";
    return false;
}

void CommentEditorPanel::clearCommentDirty()
{
    if (m_comment) {
        m_originalComment = m_comment->toPlainText();
    }
    m_isCommentDirty = false;
    updateEditingIndicator();
}

// static
QString CommentEditorPanel::convertUrlsToLinks(const QString& text)
{
    QString result = text;

    static const QRegularExpression urlPattern(
        QStringLiteral(R"((https?://|ftp://)[^\s<>"']+)"),
        QRegularExpression::CaseInsensitiveOption
    );

    static const QRegularExpression existingLinkPattern(
        QStringLiteral(R"(<a\s+[^>]*href=)"),
        QRegularExpression::CaseInsensitiveOption
    );

    if (existingLinkPattern.match(result).hasMatch()) {
        return result;
    }

    result.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    QRegularExpressionMatchIterator i = urlPattern.globalMatch(result);
    QVector<QPair<int, int>> matches;
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        matches.append(qMakePair(match.capturedStart(), match.capturedLength()));
    }

    for (qsizetype j = matches.size() - 1; j >= 0; --j) {
        int start = matches[j].first;
        int length = matches[j].second;
        QString url = result.mid(start, length);
        QString link = QStringLiteral("<a href=\"%1\" style=\"color: blue; text-decoration: underline;\">%1</a>").arg(url);
        result.replace(start, length, link);
    }

    return result;
}

bool CommentEditorPanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (!obj || ev->type() == QEvent::Destroy) {
        return QObject::eventFilter(obj, ev);
    }

    if (m_commentViewport && obj == m_commentViewport
        && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton && m_comment) {
            const QString anchor = m_comment->anchorAt(me->pos());
            if (!anchor.isEmpty()) {
                QDesktopServices::openUrl(QUrl(anchor));
                return true;
            }
        }
    }
    return QObject::eventFilter(obj, ev);
}

// ===================== Private slots =====================

void CommentEditorPanel::onFontIncrease()
{
    updateCommentFontSize(1);
}

void CommentEditorPanel::onFontDecrease()
{
    updateCommentFontSize(-1);
}

void CommentEditorPanel::onUpdateCommentClicked()
{
    if (!m_comment) return;

    QString newComment = m_comment->toPlainText();

    emit commentUpdated(m_currentMoveIndex, newComment);

    m_originalComment = newComment;
    m_isCommentDirty = false;
    updateEditingIndicator();
}

void CommentEditorPanel::onCommentTextChanged()
{
    if (!m_comment) return;

    QString currentText = m_comment->toPlainText();
    bool isDirty = (currentText != m_originalComment);

    qCDebug(lcUi).noquote()
        << "[CommentEditorPanel] onCommentTextChanged:"
        << " currentText.len=" << currentText.size()
        << " originalComment.len=" << m_originalComment.size()
        << " isDirty=" << isDirty
        << " m_isCommentDirty(before)=" << m_isCommentDirty;

    if (m_isCommentDirty != isDirty) {
        m_isCommentDirty = isDirty;
        updateEditingIndicator();
        qCDebug(lcUi).noquote() << "[CommentEditorPanel] m_isCommentDirty changed to:" << m_isCommentDirty;
    }
}

void CommentEditorPanel::onCommentUndo()
{
    if (!m_comment) return;
    m_comment->undo();
}

void CommentEditorPanel::onCommentRedo()
{
    if (!m_comment) return;
    m_comment->redo();
}

void CommentEditorPanel::onCommentCut()
{
    if (!m_comment) return;
    m_comment->cut();
}

void CommentEditorPanel::onCommentCopy()
{
    if (!m_comment) return;
    m_comment->copy();
}

void CommentEditorPanel::onCommentPaste()
{
    if (!m_comment) return;
    m_comment->paste();
}

// ===================== Private helpers =====================

void CommentEditorPanel::buildCommentToolbar(QWidget* parentWidget)
{
    m_commentToolbar = new QWidget(parentWidget);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_commentToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    m_btnFontDecrease = new QToolButton(m_commentToolbar);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnFontDecrease->setFixedSize(28, 24);
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnFontDecrease, &QToolButton::clicked, this, &CommentEditorPanel::onFontDecrease);

    m_btnFontIncrease = new QToolButton(m_commentToolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnFontIncrease, &QToolButton::clicked, this, &CommentEditorPanel::onFontIncrease);

    m_btnCommentUndo = new QToolButton(m_commentToolbar);
    m_btnCommentUndo->setText(QStringLiteral("↩"));
    m_btnCommentUndo->setToolTip(tr("元に戻す (Ctrl+Z)"));
    m_btnCommentUndo->setFixedSize(28, 24);
    m_btnCommentUndo->setStyleSheet(ButtonStyles::undoRedo());
    connect(m_btnCommentUndo, &QToolButton::clicked, this, &CommentEditorPanel::onCommentUndo);

    m_btnCommentRedo = new QToolButton(m_commentToolbar);
    m_btnCommentRedo->setText(QStringLiteral("↪"));
    m_btnCommentRedo->setToolTip(tr("やり直す (Ctrl+Y)"));
    m_btnCommentRedo->setFixedSize(28, 24);
    m_btnCommentRedo->setStyleSheet(ButtonStyles::undoRedo());
    connect(m_btnCommentRedo, &QToolButton::clicked, this, &CommentEditorPanel::onCommentRedo);

    m_btnCommentCut = new QToolButton(m_commentToolbar);
    m_btnCommentCut->setText(QStringLiteral("✂"));
    m_btnCommentCut->setToolTip(tr("切り取り (Ctrl+X)"));
    m_btnCommentCut->setFixedSize(28, 24);
    m_btnCommentCut->setStyleSheet(ButtonStyles::editOperation());
    connect(m_btnCommentCut, &QToolButton::clicked, this, &CommentEditorPanel::onCommentCut);

    m_btnCommentCopy = new QToolButton(m_commentToolbar);
    m_btnCommentCopy->setText(QStringLiteral("📋"));
    m_btnCommentCopy->setToolTip(tr("コピー (Ctrl+C)"));
    m_btnCommentCopy->setFixedSize(28, 24);
    m_btnCommentCopy->setStyleSheet(ButtonStyles::editOperation());
    connect(m_btnCommentCopy, &QToolButton::clicked, this, &CommentEditorPanel::onCommentCopy);

    m_btnCommentPaste = new QToolButton(m_commentToolbar);
    m_btnCommentPaste->setText(QStringLiteral("📄"));
    m_btnCommentPaste->setToolTip(tr("貼り付け (Ctrl+V)"));
    m_btnCommentPaste->setFixedSize(28, 24);
    m_btnCommentPaste->setStyleSheet(ButtonStyles::editOperation());
    connect(m_btnCommentPaste, &QToolButton::clicked, this, &CommentEditorPanel::onCommentPaste);

    m_editingLabel = new QLabel(tr("修正中"), m_commentToolbar);
    m_editingLabel->setStyleSheet(QStringLiteral("QLabel { color: red; font-weight: bold; }"));
    m_editingLabel->setVisible(false);

    m_btnUpdateComment = new QPushButton(tr("コメント更新"), m_commentToolbar);
    m_btnUpdateComment->setToolTip(tr("編集したコメントを棋譜に反映する"));
    m_btnUpdateComment->setFixedHeight(24);
    m_btnUpdateComment->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnUpdateComment, &QPushButton::clicked, this, &CommentEditorPanel::onUpdateCommentClicked);

    toolbarLayout->addWidget(m_btnFontDecrease);
    toolbarLayout->addWidget(m_btnFontIncrease);
    toolbarLayout->addWidget(m_btnCommentUndo);
    toolbarLayout->addWidget(m_btnCommentRedo);
    toolbarLayout->addWidget(m_btnCommentCut);
    toolbarLayout->addWidget(m_btnCommentCopy);
    toolbarLayout->addWidget(m_btnCommentPaste);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(m_btnUpdateComment);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(m_editingLabel);
    toolbarLayout->addStretch();

    m_commentToolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_commentToolbar);
}

void CommentEditorPanel::updateCommentFontSize(int delta)
{
    m_currentFontSize += delta;
    if (m_currentFontSize < 8) m_currentFontSize = 8;
    if (m_currentFontSize > 24) m_currentFontSize = 24;

    if (m_comment) {
        QFont font = m_comment->font();
        font.setPointSize(m_currentFontSize);
        m_comment->setFont(font);
    }

    SettingsService::setCommentFontSize(m_currentFontSize);
}

void CommentEditorPanel::updateEditingIndicator()
{
    if (m_editingLabel) {
        m_editingLabel->setVisible(m_isCommentDirty);
        qCDebug(lcUi).noquote() << "[CommentEditorPanel] updateEditingIndicator: visible=" << m_isCommentDirty;
    }
}
