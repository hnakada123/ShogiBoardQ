#include "kifuanalysislistmodel.h"

//将棋の棋譜解析結果をGUI上で表示するためのモデルクラス
// コンストラクタ
KifuAnalysisListModel::KifuAnalysisListModel(QObject *parent) : AbstractListModel<KifuAnalysisResultsDisplay>(parent)
{
}

// 列数を返す。
int KifuAnalysisListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「指し手」「評価値」「差」「読み筋」の4列を返す。
    return 4;
}

// データを返す。
QVariant KifuAnalysisListModel::data(const QModelIndex &index, int role) const
{
    // roleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }

    // 列番号によって処理を分岐する。
    switch (index.column()) {
    case 0:
        // 指し手を返す。
        return list[index.row()]->currentMove();
    case 1:
        // 評価値を返す。
        return list[index.row()]->evaluationValue();
    case 2:
        // 差を返す。
        return list[index.row()]->evaluationDifference();
    case 3:
        // 読み筋を返す。
        return list[index.row()]->principalVariation();
    default:
        // それ以外の場合は空のQVariantを返す。
        return QVariant();
    }
}

// ヘッダを返す。
QVariant KifuAnalysisListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // roleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    // // 横方向のヘッダが要求された場合
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            // 指し手
            return tr("Move");
        case 1:
            // 評価値
            return tr("Evaluation Value");
        case 2:
            // 差
            return tr("Difference");
        case 3:
            // 読み筋
            return tr("Principal Variation");
        default:
            // それ以外の場合、空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
