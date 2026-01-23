#include "shogienginethinkingmodel.h"
#include <QColor>

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
