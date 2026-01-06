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
    if (!index.isValid())
        return QVariant();

    // 右寄せ：Evaluation / Difference 列は常に右寄せ（デリゲートが無い場面でも崩れないように）
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == 1 || index.column() == 2) {
            return QVariant::fromValue<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    switch (index.column()) {
    case 0: // 指し手
        return list[index.row()]->currentMove();
    case 1: // 評価値
        return list[index.row()]->evaluationValue();
    case 2: // 差
        return list[index.row()]->evaluationDifference();
    case 3: // 読み筋
        return list[index.row()]->principalVariation();
    default:
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
            return QStringLiteral("指し手");
        case 1:
            return QStringLiteral("評価値");
        case 2:
            return QStringLiteral("差");
        case 3:
            return QStringLiteral("読み筋");
        default:
            // それ以外の場合、空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
