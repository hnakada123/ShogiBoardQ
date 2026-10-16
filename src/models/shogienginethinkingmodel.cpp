/// @file shogienginethinkingmodel.cpp
/// @brief エンジン思考結果の表示用リストモデルの実装

#include "shogienginethinkingmodel.h"
#include "kifupresentation.h"
#include "applicationfonts.h"
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
    return ColumnCount;
}

QVariant ShogiEngineThinkingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= list.size()) {
        return QVariant();
    }

    if (role == Qt::ToolTipRole && index.column() == PvColumn) {
        return QStringLiteral("<qt>%1</qt>").arg(list[index.row()]->displayPv(true).toHtmlEscaped());
    }

    if (role == Qt::FontRole && index.column() == PvColumn
        && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        return ApplicationFonts::japaneseFont(QFont());

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

    const ShogiInfoRecord* record = list.at(index.row());
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
        if (score.bound == UsiScore::Bound::Lower) return QString::number(score.value) + QStringLiteral("++");
        if (score.bound == UsiScore::Bound::Upper) return QString::number(score.value) + QStringLiteral("--");
        return score.value;
    case UsiScore::Kind::Mate:
        return score.mateCountKnown ? mateScoreText(score.value) : tr("詰み");
    }
    return QVariant();
}

QString ShogiEngineThinkingModel::mateScoreText(int plies)
{
    return tr("%1手詰").arg(plies);
}

QString ShogiEngineThinkingModel::boardButtonText()
{
    return tr("表示");
}

QVariant ShogiEngineThinkingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
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
    if (row < 0 || row >= list.size()) {
        return nullptr;
    }
    return list.at(row);
}

std::optional<int> ShogiEngineThinkingModel::findRowByMultipv(int multipv) const
{
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->multipv() == multipv) {
            return i;
        }
    }
    return std::nullopt;
}

// ============================================================
// MultiPV対応
// ============================================================

void ShogiEngineThinkingModel::prependRow(const ThinkingInfoRow& row, int maxRows)
{
    if (!list.isEmpty() && list.first()->hasSameContent(row)) return;
    prependItem(new ShogiInfoRecord(row));
    trimToMaxRows(maxRows);
}

void ShogiEngineThinkingModel::updateByMultipv(const ThinkingInfoRow& row, int maxMultiPV)
{
    const int multipv = row.multipv;
    if (multipv < 1 || multipv > maxMultiPV) return;

    // 同じmultipv値を持つ既存の行を探す
    const auto existingRow = findRowByMultipv(multipv);
    if (existingRow && list.at(*existingRow)->hasSameContent(row)) return;
    auto newRecord = std::make_unique<ShogiInfoRecord>(row);

    if (existingRow.has_value()) {
        // 既存の行を更新
        std::unique_ptr<ShogiInfoRecord> oldRecord(list.at(*existingRow));
        list[*existingRow] = newRecord.release();

        const QModelIndex topLeft = index(*existingRow, 0);
        const QModelIndex bottomRight = index(*existingRow, columnCount() - 1);
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
