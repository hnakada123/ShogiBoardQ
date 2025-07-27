#include "shogienginethinkingmodel.h"

// 将棋エンジンの思考結果をGUI上で表示するためのクラス
// コンストラクタ
ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject *parent) : AbstractListModel<ShogiInfoRecord>(parent)
{
}

// 列数を返す。
int ShogiEngineThinkingModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「時間」「深さ」「ノード数」「評価値」「読み筋」の5列を返す。
    return 5;
}

// データを返す。
QVariant ShogiEngineThinkingModel::data(const QModelIndex &index, int role) const
{
    // indexが無効またはroleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (!index.isValid() || role != Qt::DisplayRole) {
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
    case 4:
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
            return "Time";
        case 1:
            // 深さ
            return "Depth";
        case 2:
            // ノード数
            return "Nodes";
        case 3:
            // 評価値
            return "Score";
        case 4:
            // 読み筋
            return "PV";
        default:
            // それ以外の場合は空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
