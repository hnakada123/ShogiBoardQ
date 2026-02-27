/// @file shogienginethinkingmodel.cpp
/// @brief エンジン思考結果の表示用リストモデルの実装

#include "shogienginethinkingmodel.h"
#include <QColor>
#include <algorithm>
#include <memory>

// ============================================================
// 初期化
// ============================================================

ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject *parent) : AbstractListModel<ShogiInfoRecord>(parent)
{
}

// ============================================================
// Qtモデルインターフェース
// ============================================================

int ShogiEngineThinkingModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 時間・深さ・ノード数・評価値・盤面・読み筋
    return 6;
}

QVariant ShogiEngineThinkingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    // 盤面列（列4）はボタン風の表示
    if (index.column() == 4) {
        if (role == Qt::DisplayRole) {
            return tr("表示");
        }
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        if (role == Qt::BackgroundRole) {
            return QColor(0x3d, 0x8d, 0xe2);
        }
        if (role == Qt::ForegroundRole) {
            return QColor(Qt::white);
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (index.column()) {
    case 0:
        return list[index.row()]->time();
    case 1:
        return list[index.row()]->depth();
    case 2:
        return list[index.row()]->nodes();
    case 3:
        return list[index.row()]->score();
    case 5:
        return list[index.row()]->pv();
    default:
        return QVariant();
    }
}

QVariant ShogiEngineThinkingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return tr("時間");
        case 1:
            return tr("深さ");
        case 2:
            return tr("ノード数");
        case 3:
            return tr("評価値");
        case 4:
            return tr("盤面");
        case 5:
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

QString ShogiEngineThinkingModel::usiPvAt(int row) const
{
    if (row < 0 || row >= list.size()) {
        return QString();
    }
    return list.at(row)->usiPv();
}

const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int row) const
{
    if (row < 0 || row >= list.size()) {
        return nullptr;
    }
    return list.at(row);
}

int ShogiEngineThinkingModel::findRowByMultipv(int multipv) const
{
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->multipv() == multipv) {
            return i;
        }
    }
    return -1;
}

// ============================================================
// MultiPV対応
// ============================================================

void ShogiEngineThinkingModel::updateByMultipv(ShogiInfoRecord* record, int maxMultiPV)
{
    std::unique_ptr<ShogiInfoRecord> newRecord(record);
    if (!newRecord) return;

    const int multipv = newRecord->multipv();
    if (multipv < 1 || multipv > maxMultiPV) {
        return;
    }

    // 同じmultipv値を持つ既存の行を探す
    const int existingRow = findRowByMultipv(multipv);

    if (existingRow >= 0) {
        // 既存の行を更新
        std::unique_ptr<ShogiInfoRecord> oldRecord(list.at(existingRow));
        list[existingRow] = newRecord.release();

        const QModelIndex topLeft = index(existingRow, 0);
        const QModelIndex bottomRight = index(existingRow, columnCount() - 1);
        emit dataChanged(topLeft, bottomRight);
    } else {
        // multipv順に新しい行を挿入
        int insertPos = 0;
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i)->multipv() < multipv) {
                insertPos = i + 1;
            }
        }

        beginInsertRows(QModelIndex(), insertPos, insertPos);
        list.insert(insertPos, newRecord.release());
        endInsertRows();
    }

    // maxMultiPVを超える行を末尾から削除
    while (list.size() > maxMultiPV) {
        const int lastIdx = static_cast<int>(list.size()) - 1;
        beginRemoveRows(QModelIndex(), lastIdx, lastIdx);
        std::unique_ptr<ShogiInfoRecord> removed(list.takeLast());
        endRemoveRows();
    }
}

void ShogiEngineThinkingModel::sortByScore()
{
    if (list.size() <= 1) return;

    beginResetModel();

    std::sort(list.begin(), list.end(), [](const ShogiInfoRecord* a, const ShogiInfoRecord* b) {
        return a->scoreCp() > b->scoreCp();
    });

    endResetModel();
}

void ShogiEngineThinkingModel::trimToMaxRows(int maxRows)
{
    if (maxRows < 0) {
        maxRows = 0;
    }

    while (list.size() > maxRows) {
        const int lastIdx = static_cast<int>(list.size()) - 1;
        beginRemoveRows(QModelIndex(), lastIdx, lastIdx);
        std::unique_ptr<ShogiInfoRecord> removed(list.takeLast());
        endRemoveRows();
    }
}
