/// @file shogienginethinkingmodel.cpp
/// @brief エンジン思考結果の表示用リストモデルの実装

#include "shogienginethinkingmodel.h"
#include "scoretext.h"
#include "movefont.h"
#include <algorithm>

// ============================================================
// 初期化
// ============================================================

ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject *parent) : QAbstractTableModel(parent)
{
}

int ShogiEngineThinkingModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

// ============================================================
// Qtモデルインターフェース
// ============================================================

int ShogiEngineThinkingModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 時間・深さ・ノード数・評価値・上下限・盤面・読み筋
    return ColumnCount;
}

QVariant ShogiEngineThinkingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    if (role == Qt::ToolTipRole && index.column() == PvColumn) {
        return QStringLiteral("<qt>%1</qt>").arg(m_rows.at(index.row()).displayPv(true).toHtmlEscaped());
    }

    if (role == Qt::FontRole && index.column() == PvColumn) return MoveFont::role();

    if (index.column() == BoundColumn) {
        if (role == Qt::TextAlignmentRole) return Qt::AlignCenter;
        const UsiScore& score = m_rows.at(index.row()).score();
        if (!score.isCp()) return QVariant();
        if (role == Qt::DisplayRole) {
            if (score.bound == UsiScore::Bound::Lower) return QStringLiteral("++");
            if (score.bound == UsiScore::Bound::Upper) return QStringLiteral("--");
        }
        if (role == Qt::ToolTipRole) {
            if (score.bound == UsiScore::Bound::Lower) return tr("下限値：この値以上");
            if (score.bound == UsiScore::Bound::Upper) return tr("上限値：この値以下");
        }
        return QVariant();
    }

    // 盤面列はボタン風の表示
    if (index.column() == BoardColumn) {
        if (role == Qt::DisplayRole) {
            return boardButtonText();
        }
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    const ShogiInfoRecord* record = &m_rows.at(index.row());
    switch (index.column()) {
    case TimeColumn:
        return record->timeMs() >= 0 ? QVariant(record->timeMs()) : QVariant();
    case DepthColumn:
        return record->depth() >= 0 ? QVariant(record->depth()) : QVariant();
    case NodesColumn:
        return record->nodes() >= 0 ? QVariant(record->nodes()) : QVariant();
    case ScoreColumn:
        return scoreDisplay(record->score());
    case PvColumn:
        return record->displayPv();
    default:
        return QVariant();
    }
}

QVariant ShogiEngineThinkingModel::scoreDisplay(const UsiScore& score)
{
    switch (score.kind) {
    case UsiScore::Kind::None:
        return QVariant();
    case UsiScore::Kind::Cp:
        return score.value;
    case UsiScore::Kind::Mate:
        return ScoreText::mate(score);
    }
    return QVariant();
}

QString ShogiEngineThinkingModel::boardButtonText()
{
    return tr("表示");
}

QVariant ShogiEngineThinkingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::ToolTipRole) {
        if (section == ScoreColumn)
            return tr("探索開始局面の手番側から見た評価値です。正は手番側有利、負は相手側有利です。");
        if (section == BoundColumn)
            return tr("++：下限値（この値以上）\n--：上限値（この値以下）");
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case TimeColumn:
            return tr("時間(ms)");
        case DepthColumn:
            return tr("深さ");
        case NodesColumn:
            return tr("ノード数");
        case ScoreColumn:
            return tr("評価値");
        case BoundColumn:
            return tr("上下限");
        case BoardColumn:
            return tr("盤面");
        case PvColumn:
            return tr("読み筋");
        default:
            return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}

// ============================================================
// レコードアクセス
// ============================================================

const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return nullptr;
    }
    return &m_rows.at(row);
}

std::optional<int> ShogiEngineThinkingModel::findRowByMultipv(int multipv) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).multipv() == multipv) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

// ============================================================
// MultiPV対応
// ============================================================

void ShogiEngineThinkingModel::prependRow(const ThinkingInfoRow& row, int maxRows)
{
    if (!m_rows.isEmpty() && m_rows.first().hasSameContent(row)) return;
    beginInsertRows(QModelIndex(), 0, 0);
    m_rows.prepend(ShogiInfoRecord(row));
    endInsertRows();
    trimToMaxRows(maxRows);
}

void ShogiEngineThinkingModel::updateByMultipv(const ThinkingInfoRow& row, int maxMultiPV)
{
    const int multipv = row.multipv;
    if (multipv < 1 || multipv > maxMultiPV) return;

    // 同じmultipv値を持つ既存の行を探す
    const auto existingRow = findRowByMultipv(multipv);
    if (existingRow && m_rows.at(*existingRow).hasSameContent(row)) return;

    if (existingRow.has_value()) {
        // 既存の行を更新
        m_rows[*existingRow] = ShogiInfoRecord(row);

        const QModelIndex topLeft = index(*existingRow, 0);
        const QModelIndex bottomRight = index(*existingRow, columnCount() - 1);
        emit dataChanged(topLeft, bottomRight);
    } else {
        // multipv順に新しい行を挿入
        int insertPos = 0;
        for (qsizetype i = 0; i < m_rows.size(); ++i) {
            if (m_rows.at(i).multipv() < multipv) {
                insertPos = static_cast<int>(i) + 1;
            }
        }

        beginInsertRows(QModelIndex(), insertPos, insertPos);
        m_rows.insert(insertPos, ShogiInfoRecord(row));
        endInsertRows();
    }

    // maxMultiPVを超える行を末尾から削除
    trimToMaxRows(maxMultiPV);
}

void ShogiEngineThinkingModel::trimToMaxRows(int maxRows)
{
    if (maxRows < 0) {
        maxRows = 0;
    }

    if (m_rows.size() <= maxRows) return;
    beginRemoveRows(QModelIndex(), maxRows, static_cast<int>(m_rows.size()) - 1);
    m_rows.remove(maxRows, m_rows.size() - maxRows);
    endRemoveRows();
}

void ShogiEngineThinkingModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}
