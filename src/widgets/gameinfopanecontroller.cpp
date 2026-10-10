/// @file gameinfopanecontroller.cpp
/// @brief 対局情報ペインコントローラクラスの実装

#include "gameinfopanecontroller.h"
#include "dialogutils.h"
#include "tablestyles.h"
#include "gamesettings.h"
#include "gameinfokeys.h"

#include <QTableWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QLineEdit>
#include <QShortcut>
#include <QMessageBox>
#include <QClipboard>
#include <QAbstractItemDelegate>

GameInfoPaneController::GameInfoPaneController(QObject* parent)
    : QObject(parent)
    , m_fontSize(GameSettings::gameInfoFontSize())
{
    // 設定値の範囲チェック
    if (m_fontSize < 8)  m_fontSize = 10;
    if (m_fontSize > 24) m_fontSize = 24;

    m_keyColumnWidth = qBound(0, GameSettings::gameInfoKeyColumnWidth(), 600);
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
    m_table->setObjectName(QStringLiteral("gameInfoTable"));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("項目"), tr("内容")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setMinimumSectionSize(60);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(true);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setAccessibleName(tr("対局情報"));
    installValueDelegate();

    // フォントサイズ適用
    applyFontSize();

    mainLayout->addWidget(m_table, 1);

    // セル変更時の接続
    QObject::connect(m_table, &QTableWidget::cellChanged,
                     this, &GameInfoPaneController::onCellChanged);

    const auto addShortcut = [this](QKeySequence::StandardKey key,
                                    void (GameInfoPaneController::*slot)()) {
        auto* shortcut = new QShortcut(key, m_container);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence::Undo, &GameInfoPaneController::undo);
    addShortcut(QKeySequence::Redo, &GameInfoPaneController::redo);
    addShortcut(QKeySequence::Cut, &GameInfoPaneController::cut);
    addShortcut(QKeySequence::Copy, &GameInfoPaneController::copy);
    addShortcut(QKeySequence::Paste, &GameInfoPaneController::paste);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), m_container);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, &GameInfoPaneController::applyChanges);
    }
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &GameInfoPaneController::updateEditingIndicator);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, &GameInfoPaneController::onColumnResized);
    connect(qApp, &QApplication::focusChanged, this, &GameInfoPaneController::observeEditor);
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &GameInfoPaneController::updateEditingIndicator);
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::closeEditor,
            this, &GameInfoPaneController::updateEditingIndicator, Qt::QueuedConnection);
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

QWidget* GameInfoPaneController::containerWidget() const
{
    return m_container;
}

QTableWidget* GameInfoPaneController::tableWidget() const
{
    return m_table;
}

void GameInfoPaneController::setGameInfo(const QList<KifGameInfoItem>& items, bool beforeMatchStart)
{
    if (!m_table) return;
    m_beforeMatchStart = beforeMatchStart;

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

    m_table->blockSignals(false);

    // 元データを保存
    m_originalItems = items;
    m_dirty = false;
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::resetGameInfo()
{
    // 対局者は名前が決まるまで、将棋盤の名前欄と同じく「先手」「後手」と呼ぶ
    setGameInfo({{GameInfoKeys::kGameDate, {}},
                 {GameInfoKeys::kStartDateTime, {}},
                 {GameInfoKeys::kBlackPlayer, tr("先手")},
                 {GameInfoKeys::kWhitePlayer, tr("後手")},
                 {GameInfoKeys::kHandicap, QStringLiteral("平手")},
                 {GameInfoKeys::kTimeControl, {}},
                 {GameInfoKeys::kEvent, {}},
                 {GameInfoKeys::kSite, {}},
                 {GameInfoKeys::kNote, {}}}, true);
}

void GameInfoPaneController::setGameInfoForMatch(const QList<KifGameInfoItem>& automaticItems)
{
    commitPendingEditor();
    QList<KifGameInfoItem> items = automaticItems;
    // 対局者の見出しは平手（先手・後手）と駒落ち（下手・上手）で変わるため、前の対局の行を残さない。
    QSet<QString> replacedKeys = {GameInfoKeys::kEndDateTime, QStringLiteral("消費時間"),
                                 QStringLiteral("結果"),
                                 GameInfoKeys::kBlackPlayer, GameInfoKeys::kWhitePlayer,
                                 GameInfoKeys::kShitatePlayer, GameInfoKeys::kUwatePlayer};
    for (const auto& item : automaticItems) replacedKeys.insert(item.key);
    QSet<QString> retainedKeys;
    for (const auto& item : gameInfo()) {
        const QString key = item.key.trimmed();
        if (key.isEmpty() || replacedKeys.contains(key)) continue;
        items.append(item);
        retainedKeys.insert(key);
    }
    for (const auto& key : {GameInfoKeys::kEvent, GameInfoKeys::kSite, GameInfoKeys::kNote}) {
        if (!retainedKeys.contains(key)) items.append({key, {}});
    }
    setGameInfo(items);
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

void GameInfoPaneController::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    m_beforeMatchStart = false;
    m_originalItems = items;
    m_dirty = false;
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::updatePlayerNames(const QString& blackName, const QString& whiteName)
{
    if (!m_table) return;
    commitPendingEditor();

    // 駒落ちの対局情報では、先手・後手の代わりに下手・上手の行がある
    const auto isBlackKey = [](const QString& key) {
        return key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kShitatePlayer;
    };
    const auto isWhiteKey = [](const QString& key) {
        return key == GameInfoKeys::kWhitePlayer || key == GameInfoKeys::kUwatePlayer;
    };

    m_table->blockSignals(true);

    // 先手（下手）の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && isBlackKey(keyItem->text())) {
            QTableWidgetItem* valItem = m_table->item(row, 1);
            if (valItem) {
                valItem->setText(blackName);
            }
            break;
        }
    }

    // 後手（上手）の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && isWhiteKey(keyItem->text())) {
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
        if (isBlackKey(m_originalItems[i].key)) {
            m_originalItems[i].value = blackName;
        } else if (isWhiteKey(m_originalItems[i].key)) {
            m_originalItems[i].value = whiteName;
        }
    }
    // 自動同期された名前は、他のセルのUndo/Redoで古い名前に戻さない。
    for (auto& state : m_history) {
        for (auto& cell : state.cells) {
            if (isBlackKey(cell.first)) cell.second = blackName;
            else if (isWhiteKey(cell.first)) cell.second = whiteName;
        }
    }
    m_dirty = checkDirty();
    updateTablePresentation();
    updateEditingIndicator();
}

bool GameInfoPaneController::isDirty() const
{
    auto* editor = activeEditor();
    if (editor && m_table->currentItem()
        && editor->text() != m_table->currentItem()->text()) return true;
    return m_dirty;
}

bool GameInfoPaneController::confirmDiscardUnsaved()
{
    commitPendingEditor();
    if (!m_dirty) {
        return true;
    }

    const bool discard = DialogUtils::confirmAction(
        m_container,
        tr("未保存の対局情報"),
        tr("対局情報が編集されていますが、まだ更新されていません。\n"
           "変更を破棄して続行しますか？"),
        tr("破棄して続行"), QMessageBox::Warning);

    if (discard) {
        setGameInfo(m_originalItems, m_beforeMatchStart);
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
        GameSettings::setGameInfoFontSize(m_fontSize);
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

void GameInfoPaneController::applyFontSize()
{
    if (m_table) {
        QFont font = m_table->font();
        font.setPointSize(m_fontSize);
        m_table->setFont(font);
        m_table->setStyleSheet(TableStyles::thinking(m_fontSize) + QStringLiteral(
            "QTableView { alternate-background-color: #f7f9fb; }"
            "QTableView::item { padding: 4px 8px; }"));
        m_table->verticalHeader()->setMinimumSectionSize(m_table->fontMetrics().height() + 10);
        updateTablePresentation();
        updateEditingIndicator();
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
