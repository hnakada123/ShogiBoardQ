#include "kifubranchlistmodel.h"

// 棋譜欄を表示するクラス
// コンストラクタ
KifuBranchListModel::KifuBranchListModel(QObject *parent): AbstractListModel<KifuBranchDisplay>(parent)
{
}

// 列数を返す。
int KifuBranchListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「分岐」の1列
    return 1;
}

// データを返す。
QVariant KifuBranchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }

    // 列番号によって処理を分岐する。
    switch (index.column()) {
    case 0:
        // 指し手を返す。
        return list[index.row()]->currentMove();
    default:
        // それ以外の場合は空のQVariantを返す。
        return QVariant();
    }
}

// ヘッダを返す。
QVariant KifuBranchListModel::headerData(int section, Qt::Orientation orientation, int role) const
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
            return tr("Move");
        default:
            // それ以外の場合は空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
