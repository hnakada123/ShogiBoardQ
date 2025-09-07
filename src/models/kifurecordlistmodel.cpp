#include "kifurecordlistmodel.h"

// 棋譜欄を表示するクラス
// コンストラクタ
KifuRecordListModel::KifuRecordListModel(QObject *parent) : AbstractListModel<KifuDisplay>(parent)
{
}

// 列数を返す。
int KifuRecordListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「指し手」「消費時間」の2列
    return 2;
}

// データを返す。
QVariant KifuRecordListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }

    // 列番号によって処理を分岐する。
    switch (index.column()) {
    case 0:
        // 指し手を返す。
        return list[index.row()]->currentMove();
    case 1:
        // 消費時間を返す。
        return list[index.row()]->timeSpent();
    default:
        // それ以外の場合は空のQVariantを返す。
        return QVariant();
    }
}

// ヘッダを返す。
QVariant KifuRecordListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // roleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    // 横方向のヘッダが要求された場合
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            // 指し手
            return tr("指し手");
        case 1:
            // 消費時間
            return tr("消費時間");
        default:
            // それ以外の場合は空のQVariantを返す。
            return QVariant();
        }
    } else {
         // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
