#include "gameinfopanecontroller.h"
#include "settingsservice.h"

#include <QTableWidget>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QMessageBox>
#include <QDebug>

GameInfoPaneController::GameInfoPaneController(QObject* parent)
    : QObject(parent)
{
    // 設定からフォントサイズを読み込む
    m_fontSize = SettingsService::gameInfoFontSize();
    if (m_fontSize < 8)  m_fontSize = 10;
    if (m_fontSize > 24) m_fontSize = 24;

    buildUi();
}

GameInfoPaneController::~GameInfoPaneController()
{
    // m_container は親ウィジェットに所有されるため、ここでは削除しない
}

void GameInfoPaneController::buildUi()
{
    // コンテナ作成
    m_container = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ツールバー作成
    buildToolbar();
    mainLayout->addWidget(m_toolbar);

    // テーブル作成
    m_table = new QTableWidget(m_container);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("項目"), tr("内容")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(false);

    // フォントサイズ適用
    applyFontSize();

    mainLayout->addWidget(m_table, 1);

    // セル変更時の接続
    QObject::connect(m_table, &QTableWidget::cellChanged,
                     this, &GameInfoPaneController::onCellChanged);
}

void GameInfoPaneController::buildToolbar()
{
    m_toolbar = new QWidget(m_container);
    QHBoxLayout* layout = new QHBoxLayout(m_toolbar);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnFontDecrease = new QToolButton(m_toolbar);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnFontDecrease->setFixedSize(28, 24);
    QObject::connect(m_btnFontDecrease, &QToolButton::clicked,
                     this, &GameInfoPaneController::decreaseFontSize);

    // フォントサイズ増加ボタン
    m_btnFontIncrease = new QToolButton(m_toolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    QObject::connect(m_btnFontIncrease, &QToolButton::clicked,
                     this, &GameInfoPaneController::increaseFontSize);

    // Undoボタン
    m_btnUndo = new QToolButton(m_toolbar);
    m_btnUndo->setText(QStringLiteral("↩"));
    m_btnUndo->setToolTip(tr("元に戻す (Ctrl+Z)"));
    m_btnUndo->setFixedSize(28, 24);
    QObject::connect(m_btnUndo, &QToolButton::clicked,
                     this, &GameInfoPaneController::undo);

    // Redoボタン
    m_btnRedo = new QToolButton(m_toolbar);
    m_btnRedo->setText(QStringLiteral("↪"));
    m_btnRedo->setToolTip(tr("やり直す (Ctrl+Y)"));
    m_btnRedo->setFixedSize(28, 24);
    QObject::connect(m_btnRedo, &QToolButton::clicked,
                     this, &GameInfoPaneController::redo);

    // 切り取りボタン
    m_btnCut = new QToolButton(m_toolbar);
    m_btnCut->setText(QStringLiteral("✂"));
    m_btnCut->setToolTip(tr("切り取り (Ctrl+X)"));
    m_btnCut->setFixedSize(28, 24);
    QObject::connect(m_btnCut, &QToolButton::clicked,
                     this, &GameInfoPaneController::cut);

    // コピーボタン
    m_btnCopy = new QToolButton(m_toolbar);
    m_btnCopy->setText(QStringLiteral("📋"));
    m_btnCopy->setToolTip(tr("コピー (Ctrl+C)"));
    m_btnCopy->setFixedSize(28, 24);
    QObject::connect(m_btnCopy, &QToolButton::clicked,
                     this, &GameInfoPaneController::copy);

    // 貼り付けボタン
    m_btnPaste = new QToolButton(m_toolbar);
    m_btnPaste->setText(QStringLiteral("📄"));
    m_btnPaste->setToolTip(tr("貼り付け (Ctrl+V)"));
    m_btnPaste->setFixedSize(28, 24);
    QObject::connect(m_btnPaste, &QToolButton::clicked,
                     this, &GameInfoPaneController::paste);

    // 「修正中」ラベル
    m_editingLabel = new QLabel(tr("修正中"), m_toolbar);
    m_editingLabel->setStyleSheet(QStringLiteral("QLabel { color: red; font-weight: bold; }"));
    m_editingLabel->setVisible(false);

    // 更新ボタン
    m_btnUpdate = new QPushButton(tr("対局情報更新"), m_toolbar);
    m_btnUpdate->setToolTip(tr("編集した対局情報を棋譜に反映する"));
    m_btnUpdate->setFixedHeight(24);
    QObject::connect(m_btnUpdate, &QPushButton::clicked,
                     this, &GameInfoPaneController::applyChanges);

    // レイアウトに追加
    layout->addWidget(m_btnFontDecrease);
    layout->addWidget(m_btnFontIncrease);
    layout->addWidget(m_btnUndo);
    layout->addWidget(m_btnRedo);
    layout->addWidget(m_btnCut);
    layout->addWidget(m_btnCopy);
    layout->addWidget(m_btnPaste);
    layout->addWidget(m_editingLabel);
    layout->addStretch();
    layout->addWidget(m_btnUpdate);
}

QWidget* GameInfoPaneController::containerWidget() const
{
    return m_container;
}

QTableWidget* GameInfoPaneController::tableWidget() const
{
    return m_table;
}

void GameInfoPaneController::setGameInfo(const QList<KifGameInfoItem>& items)
{
    if (!m_table) return;

    m_table->blockSignals(true);

    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(items.size()));

    for (qsizetype row = 0; row < items.size(); ++row) {
        const KifGameInfoItem& item = items.at(row);
        auto* keyItem   = new QTableWidgetItem(item.key);
        auto* valueItem = new QTableWidgetItem(item.value);
        // 項目名は編集不可
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(static_cast<int>(row), 0, keyItem);
        m_table->setItem(static_cast<int>(row), 1, valueItem);
    }

    m_table->resizeColumnToContents(0);
    m_table->blockSignals(false);

    // 元データを保存
    m_originalItems = items;
    m_dirty = false;
    updateEditingIndicator();
}

QList<KifGameInfoItem> GameInfoPaneController::gameInfo() const
{
    QList<KifGameInfoItem> items;
    if (!m_table) return items;

    const int rowCount = m_table->rowCount();
    items.reserve(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        QTableWidgetItem* keyItem = m_table->item(r, 0);
        QTableWidgetItem* valueItem = m_table->item(r, 1);

        KifGameInfoItem item;
        item.key = keyItem ? keyItem->text() : QString();
        item.value = valueItem ? valueItem->text() : QString();
        items.append(item);
    }

    return items;
}

QList<KifGameInfoItem> GameInfoPaneController::originalGameInfo() const
{
    return m_originalItems;
}

void GameInfoPaneController::updatePlayerNames(const QString& blackName, const QString& whiteName)
{
    if (!m_table) return;

    m_table->blockSignals(true);

    // 先手の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && keyItem->text() == tr("先手")) {
            QTableWidgetItem* valItem = m_table->item(row, 1);
            if (valItem) {
                valItem->setText(blackName);
            }
            break;
        }
    }

    // 後手の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && keyItem->text() == tr("後手")) {
            QTableWidgetItem* valItem = m_table->item(row, 1);
            if (valItem) {
                valItem->setText(whiteName);
            }
            break;
        }
    }

    m_table->blockSignals(false);

    // 元データも更新
    for (qsizetype i = 0; i < m_originalItems.size(); ++i) {
        if (m_originalItems[i].key == tr("先手")) {
            m_originalItems[i].value = blackName;
        } else if (m_originalItems[i].key == tr("後手")) {
            m_originalItems[i].value = whiteName;
        }
    }
}

bool GameInfoPaneController::isDirty() const
{
    return m_dirty;
}

bool GameInfoPaneController::confirmDiscardUnsaved()
{
    if (!m_dirty) {
        return true;
    }

    QMessageBox::StandardButton reply = QMessageBox::warning(
        m_container,
        tr("未保存の対局情報"),
        tr("対局情報が編集されていますが、まだ更新されていません。\n"
           "変更を破棄して続行しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        undo();
        return true;
    }
    return false;
}

int GameInfoPaneController::fontSize() const
{
    return m_fontSize;
}

void GameInfoPaneController::setFontSize(int size)
{
    if (size < 8)  size = 8;
    if (size > 24) size = 24;

    if (m_fontSize != size) {
        m_fontSize = size;
        applyFontSize();
        SettingsService::setGameInfoFontSize(m_fontSize);
    }
}

void GameInfoPaneController::increaseFontSize()
{
    setFontSize(m_fontSize + 1);
}

void GameInfoPaneController::decreaseFontSize()
{
    setFontSize(m_fontSize - 1);
}

void GameInfoPaneController::undo()
{
    setGameInfo(m_originalItems);
    qDebug().noquote() << "[GameInfoPane] undo: Reverted to original game info";
}

void GameInfoPaneController::redo()
{
    if (!m_table) return;

    // 編集中セルのエディタを取得してredo
    QWidget* editor = m_table->cellWidget(m_table->currentRow(),
                                           m_table->currentColumn());
    if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor)) {
        lineEdit->redo();
    }
}

void GameInfoPaneController::cut()
{
    if (!m_table) return;

    QTableWidgetItem* item = m_table->currentItem();
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        QApplication::clipboard()->setText(item->text());
        item->setText(QString());
    }
}

void GameInfoPaneController::copy()
{
    if (!m_table) return;

    QTableWidgetItem* item = m_table->currentItem();
    if (item) {
        QApplication::clipboard()->setText(item->text());
    }
}

void GameInfoPaneController::paste()
{
    if (!m_table) return;

    QTableWidgetItem* item = m_table->currentItem();
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        QString text = QApplication::clipboard()->text();
        item->setText(text);
    }
}

void GameInfoPaneController::applyChanges()
{
    if (!m_table) return;

    // 現在のテーブル内容を取得
    QList<KifGameInfoItem> currentItems = gameInfo();

    // 元データを更新
    m_originalItems = currentItems;
    m_dirty = false;
    updateEditingIndicator();

    emit gameInfoUpdated(currentItems);

    qDebug().noquote() << "[GameInfoPane] applyChanges: Game info updated, items=" << currentItems.size();
}

void GameInfoPaneController::onCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    const bool wasDirty = m_dirty;
    m_dirty = checkDirty();

    if (wasDirty != m_dirty) {
        updateEditingIndicator();
        emit dirtyStateChanged(m_dirty);
    }
}

void GameInfoPaneController::updateEditingIndicator()
{
    if (m_editingLabel) {
        m_editingLabel->setVisible(m_dirty);
    }
}

void GameInfoPaneController::applyFontSize()
{
    if (m_table) {
        QFont font = m_table->font();
        font.setPointSize(m_fontSize);
        m_table->setFont(font);
        // フォントメトリクスに基づいた固定行高さを設定
        const int rowHeight = m_table->fontMetrics().height() + 4;
        m_table->verticalHeader()->setDefaultSectionSize(rowHeight);
    }
}

bool GameInfoPaneController::checkDirty() const
{
    if (!m_table) return false;

    const int rowCount = m_table->rowCount();
    if (rowCount != m_originalItems.size()) {
        return true;
    }

    for (int r = 0; r < rowCount; ++r) {
        QTableWidgetItem* keyItem = m_table->item(r, 0);
        QTableWidgetItem* valueItem = m_table->item(r, 1);

        QString currentKey = keyItem ? keyItem->text() : QString();
        QString currentValue = valueItem ? valueItem->text() : QString();

        if (r < m_originalItems.size()) {
            if (currentKey != m_originalItems[r].key ||
                currentValue != m_originalItems[r].value) {
                return true;
            }
        }
    }

    return false;
}
