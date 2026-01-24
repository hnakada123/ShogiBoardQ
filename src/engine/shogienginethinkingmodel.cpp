#include "shogienginethinkingmodel.h"
#include <QColor>
#include <algorithm>

// 将棋エンジンの思考結果をGUI上で表示するためのクラス
// コンストラクタ
ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject *parent) : AbstractListModel<ShogiInfoRecord>(parent)
{
}

// 列数を返す。
int ShogiEngineThinkingModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「時間」「深さ」「ノード数」「評価値」「盤面」「読み筋」の6列を返す。
    return 6;
}

// データを返す。
QVariant ShogiEngineThinkingModel::data(const QModelIndex &index, int role) const
{
    // indexが無効の場合、空のQVariantを返す。
    if (!index.isValid()) {
        return QVariant();
    }

    // 盤面列（列4）は特別扱い：ボタン風の表示
    if (index.column() == 4) {
        if (role == Qt::DisplayRole) {
            return tr("表示");
        }
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        if (role == Qt::BackgroundRole) {
            // タブバーと同系色のボタン色（クリック可能であることを示す）
            return QColor(0x3d, 0x8d, 0xe2);  // タブバーの青
        }
        if (role == Qt::ForegroundRole) {
            return QColor(Qt::white);  // 白文字
        }
        return QVariant();
    }

    // それ以外の列はDisplayRoleのみ処理
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    // 列番号によって処理を分岐する。
    switch (index.column()) {
    case 0:
        // 時間を返す。
        return list[index.row()]->time();
    case 1:
        // 深さを返す。
        return list[index.row()]->depth();
    case 2:
        // ノード数を返す。
        return list[index.row()]->nodes();
    case 3:
        // 評価値を返す。
        return list[index.row()]->score();
    case 5:
        // 読み筋を返す。
        return list[index.row()]->pv();
    default:
        // それ以外の場合は空のQVariantを返す。
        return QVariant();
    }
}

// ヘッダを返す。
QVariant ShogiEngineThinkingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // roleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    // 横方向のヘッダが要求された場合
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            // 時間
            return tr("時間");
        case 1:
            // 深さ
            return tr("深さ");
        case 2:
            // ノード数
            return tr("ノード数");
        case 3:
            // 評価値
            return tr("評価値");
        case 4:
            // 盤面
            return tr("盤面");
        case 5:
            // 読み筋
            return tr("読み筋");
        default:
            // それ以外の場合は空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}

// ★ 追加: 指定行のUSI形式の読み筋を取得
QString ShogiEngineThinkingModel::usiPvAt(int row) const
{
    if (row < 0 || row >= list.size()) {
        return QString();
    }
    return list.at(row)->usiPv();
}

// ★ 追加: 指定行のShogiInfoRecordを取得（読み取り専用）
const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int row) const
{
    if (row < 0 || row >= list.size()) {
        return nullptr;
    }
    return list.at(row);
}

// ★ 追加: MultiPVモードで行を更新または挿入する
void ShogiEngineThinkingModel::updateByMultipv(ShogiInfoRecord* record, int maxMultiPV)
{
    if (!record) return;

    const int multipv = record->multipv();
    if (multipv < 1 || multipv > maxMultiPV) {
        // 範囲外のmultipvは無視
        delete record;
        return;
    }

    // 同じmultipv値を持つ既存の行を探す
    int existingRow = -1;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->multipv() == multipv) {
            existingRow = i;
            break;
        }
    }

    if (existingRow >= 0) {
        // 既存の行を更新（データを入れ替え）
        ShogiInfoRecord* oldRecord = list.at(existingRow);
        list[existingRow] = record;
        delete oldRecord;

        // その行のデータが変更されたことを通知
        const QModelIndex topLeft = index(existingRow, 0);
        const QModelIndex bottomRight = index(existingRow, columnCount() - 1);
        emit dataChanged(topLeft, bottomRight);
    } else {
        // 新しい行を挿入（multipv順に挿入）
        int insertPos = 0;
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i)->multipv() < multipv) {
                insertPos = i + 1;
            }
        }

        beginInsertRows(QModelIndex(), insertPos, insertPos);
        list.insert(insertPos, record);
        endInsertRows();
    }

    // maxMultiPVを超える行を削除
    while (list.size() > maxMultiPV) {
        // 最後の行を削除
        const int lastIdx = static_cast<int>(list.size()) - 1;
        beginRemoveRows(QModelIndex(), lastIdx, lastIdx);
        delete list.takeLast();
        endRemoveRows();
    }
}

// ★ 追加: 全行を評価値でソートする（高い順）
void ShogiEngineThinkingModel::sortByScore()
{
    if (list.size() <= 1) return;

    beginResetModel();

    // 評価値の高い順にソート
    std::sort(list.begin(), list.end(), [](const ShogiInfoRecord* a, const ShogiInfoRecord* b) {
        return a->scoreCp() > b->scoreCp();
    });

    endResetModel();
}
