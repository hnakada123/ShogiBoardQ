/// @file gameinfoeditmodel.cpp
/// @brief 対局情報の表の中身と編集の履歴の実装

#include "gameinfoeditmodel.h"

#include "gameinfokeys.h"
#include "kifupresentation.h"

#include <algorithm>

namespace {
bool isBlackPlayerKey(const QString& key)
{
    return key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kShitatePlayer;
}

bool isWhitePlayerKey(const QString& key)
{
    return key == GameInfoKeys::kWhitePlayer || key == GameInfoKeys::kUwatePlayer;
}

// 表の行・履歴の行・反映済みの項目（どれも key と value を持つ）から項目を探す
template <typename Entries>
auto findKey(Entries& entries, const QString& key)
{
    return std::find_if(entries.begin(), entries.end(), [&key](const auto& entry) { return entry.key == key; });
}

template <typename Entries>
void setPlayerNamesIn(Entries& entries, const QString& blackName, const QString& whiteName)
{
    for (auto& entry : entries) {
        if (isBlackPlayerKey(entry.key)) entry.value = blackName;
        else if (isWhitePlayerKey(entry.key)) entry.value = whiteName;
    }
}
} // namespace

GameInfoEditModel::GameInfoEditModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    resetHistory();
}

int GameInfoEditModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int GameInfoEditModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant GameInfoEditModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return index.column() == KeyColumn ? row.key : row.value;
    case PlaceholderRole:
        return index.column() == ValueColumn ? placeholderFor(row.key.trimmed()) : QString();
    case Qt::AccessibleTextRole:
        return presentedText(index.row(), index.column());
    case Qt::ToolTipRole:
        // ツールチップでは棋譜中のHTMLらしい文字列もそのまま表示する。
        return QStringLiteral("<qt>%1</qt>").arg(presentedText(index.row(), index.column())
                                                     .toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
    default:
        return {};
    }
}

bool GameInfoEditModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable)) return false;
    Row& row = m_rows[index.row()];
    QString& text = index.column() == KeyColumn ? row.key : row.value;
    const QString newText = value.toString();
    if (text == newText) return true;
    text = newText;
    // 内容の表記と案内文は項目名で決まるので、行の両方の列を知らせる
    emit dataChanged(this->index(index.row(), KeyColumn), this->index(index.row(), ValueColumn));
    recordEdit();
    return true;
}

Qt::ItemFlags GameInfoEditModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return Qt::NoItemFlags;
    Qt::ItemFlags result = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == ValueColumn || m_rows.at(index.row()).keyEditable) result |= Qt::ItemIsEditable;
    return result;
}

QVariant GameInfoEditModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    return section == KeyColumn ? tr("項目") : section == ValueColumn ? tr("内容") : QVariant();
}

void GameInfoEditModel::setItems(const QList<KifGameInfoItem>& items, bool beforeMatchStart)
{
    m_beforeMatchStart = beforeMatchStart;
    Rows rows;
    rows.reserve(items.size());
    for (const KifGameInfoItem& item : items) rows.append({item.key, item.value, false});
    resetRows(rows);
    m_applied = items;
    resetHistory();
    emit editStateChanged();
}

QList<KifGameInfoItem> GameInfoEditModel::items() const
{
    QList<KifGameInfoItem> result;
    result.reserve(m_rows.size());
    for (const Row& row : m_rows) result.append({row.key, row.value});
    return result;
}

bool GameInfoEditModel::isDirty() const
{
    if (m_rows.size() != m_applied.size()) return true;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).key != m_applied.at(i).key || m_rows.at(i).value != m_applied.at(i).value) return true;
    }
    return false;
}

void GameInfoEditModel::markApplied()
{
    m_applied = items();
    emit editStateChanged();
}

void GameInfoEditModel::discardChanges()
{
    setItems(m_applied, m_beforeMatchStart);
}

void GameInfoEditModel::setAutomaticValue(const QString& key, const QString& value)
{
    const auto current = findKey(m_rows, key);
    if (current == m_rows.end()) {
        beginInsertRows({}, rowCount(), rowCount());
        m_rows.append({key, value, false});
        endInsertRows();
    } else {
        current->value = value;
        const int row = static_cast<int>(current - m_rows.begin());
        emit dataChanged(index(row, KeyColumn), index(row, ValueColumn));
    }
    for (Rows& state : m_history) {
        const auto entry = findKey(state, key);
        if (entry == state.end()) state.append({key, value, false});
        else entry->value = value;
    }
    const auto applied = findKey(m_applied, key);
    if (applied == m_applied.end()) m_applied.append({key, value});
    else applied->value = value;
    emit editStateChanged();
}

void GameInfoEditModel::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    setPlayerNamesIn(m_rows, blackName, whiteName);
    // 自動で合わせた名前は、ほかの項目の元に戻す・やり直すで古い名前に戻さない
    for (Rows& state : m_history) setPlayerNamesIn(state, blackName, whiteName);
    setPlayerNamesIn(m_applied, blackName, whiteName);
    if (!m_rows.isEmpty()) emit dataChanged(index(0, ValueColumn), index(rowCount() - 1, ValueColumn));
    emit editStateChanged();
}

int GameInfoEditModel::appendRow()
{
    const int row = rowCount();
    beginInsertRows({}, row, row);
    m_rows.append({QString(), QString(), true});
    endInsertRows();
    recordEdit();
    return row;
}

void GameInfoEditModel::removeRowAt(int row)
{
    if (row < 0 || row >= rowCount()) return;
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    recordEdit();
}

void GameInfoEditModel::undo()
{
    if (!canUndo()) return;
    resetRows(m_history.at(--m_historyIndex));
    emit editStateChanged();
}

void GameInfoEditModel::redo()
{
    if (!canRedo()) return;
    resetRows(m_history.at(++m_historyIndex));
    emit editStateChanged();
}

void GameInfoEditModel::resetRows(const Rows& rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void GameInfoEditModel::recordEdit()
{
    if (!(m_rows == m_history.at(m_historyIndex))) {
        while (m_history.size() > m_historyIndex + 1) m_history.removeLast();
        m_history.append(m_rows);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
    }
    emit editStateChanged();
}

void GameInfoEditModel::resetHistory()
{
    m_history = {m_rows};
    m_historyIndex = 0;
}

QString GameInfoEditModel::placeholderFor(const QString& key) const
{
    if (key == GameInfoKeys::kGameDate || key == GameInfoKeys::kStartDateTime)
        return m_beforeMatchStart ? tr("未開始（対局開始時に設定）") : tr("未設定");
    if (isBlackPlayerKey(key) || isWhitePlayerKey(key))
        return tr("未設定（名前を入力できます）");
    if (key == GameInfoKeys::kTimeControl)
        return m_beforeMatchStart ? tr("未設定（対局設定から反映）") : tr("未設定");
    if (key == GameInfoKeys::kEvent || key == GameInfoKeys::kSite || key == GameInfoKeys::kNote)
        return tr("任意入力");
    return {};
}

QString GameInfoEditModel::presentedText(int row, int column) const
{
    const Row& entry = m_rows.at(row);
    if (column == KeyColumn) return entry.key.isEmpty() ? QString() : KifuPresentation::infoKey(entry.key);
    if (entry.value.isEmpty()) return placeholderFor(entry.key.trimmed());
    return KifuPresentation::infoValue(entry.key, entry.value);
}
