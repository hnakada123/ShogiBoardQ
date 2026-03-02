/// @file kifubranchlistmodel.cpp
/// @brief 分岐候補リストモデルクラスの実装

#include "kifubranchlistmodel.h"
#include "logcategories.h"
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QRegularExpression>

KifuBranchListModel::KifuBranchListModel(QObject *parent)
    : AbstractListModel<KifuBranchDisplay>(parent)
{
}

int KifuBranchListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1; // 「分岐」の1列
}

int KifuBranchListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    // 通常候補数 + 末尾の「本譜へ戻る」1行（有効時）
    return static_cast<int>(list.size()) + (m_hasBackToMainRow ? 1 : 0);
}

QVariant KifuBranchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    const bool isBackRow = (m_hasBackToMainRow && index.row() == list.size());

    // --- カスタムロール: この行(分岐)の最大手数 ---
    if (role == DispCountRole) {
        // 「本譜へ戻る」行は 0 を返しておく（呼び出し側でクランプされる前提）
        if (isBackRow) return 0;
        return rowMaxPly(index.row());
    }

    // --- 表示テキスト ---
    if (role == Qt::DisplayRole) {
        if (isBackRow) {
            return tr("本譜へ戻る");
        }
        if (index.column() == 0) {
            if (index.row() >= 0 && index.row() < list.size() && list[index.row()]) {
                QString text = list[index.row()]->currentMove();

                // 棋譜欄では分岐ありを示すため末尾に '+' を付与しているが、
                // 分岐候補欄では '+' を表示しないようにする
                if (text.endsWith(QLatin1Char('+'))) {
                    text.chop(1);          // 末尾の '+' を削除
                    text = text.trimmed(); // 念のため前後の空白を除去
                }

                return text;
            }
        }
        return QVariant();
    }

    // --- 背景色：現在選択行は黄色 ---
    if (role == Qt::BackgroundRole) {
        if (index.row() == m_currentHighlightRow) {
            static const QBrush kYellowBg(QColor(255, 255, 0));
            return kYellowBg;
        }
        return QVariant();
    }

    // --- 体裁：中央揃え ---
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    if (role == Qt::FontRole && isBackRow) {
        QFont f;
        f.setBold(true);
        return f;
    }

    return QVariant();
}

QVariant KifuBranchListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:  return tr("分岐候補");
        default: return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}

void KifuBranchListModel::clearBranchCandidates()
{
    qCDebug(lcUi).noquote() << "clearBranchCandidates called, list.size was:" << list.size()
                            << "locked=" << m_locked;
    // ロック中は外部からのクリアを無視
    if (m_locked) {
        qCDebug(lcUi).noquote() << "clearBranchCandidates: IGNORED (locked)";
        return;
    }
    beginResetModel();
    qDeleteAll(list);
    list.clear();
    m_hasBackToMainRow = false;
    endResetModel();
}

void KifuBranchListModel::resetBranchCandidates()
{
    qCDebug(lcUi).noquote() << "resetBranchCandidates called";
    beginResetModel();
    qDeleteAll(list);
    list.clear();
    m_hasBackToMainRow = false;
    endResetModel();
}

void KifuBranchListModel::setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows)
{
    QStringList s; s.reserve(rows.size());
    for (qsizetype i=0;i<rows.size();++i) s << rows[i].prettyMove;
    qCDebug(lcUi).noquote() << "setBranchCandidatesFromKif:" << rows.size() << "items:" << s.join(", ")
                            << "locked=" << m_locked;

    // ロック中は外部からの設定を無視
    if (m_locked) {
        qCDebug(lcUi).noquote() << "setBranchCandidatesFromKif: IGNORED (locked)";
        return;
    }

    beginResetModel();
    qDeleteAll(list);
    list.clear();

    // 先頭に「手数（半角/全角）+空白」が付いていたら落とす
    static const auto& kDropHeadNumber = *[]() {
        static const QRegularExpression r(
            QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
        return &r;
    }();

    list.reserve(rows.size());
    for (const auto& k : rows) {
        auto* b = new KifuBranchDisplay();

        QString label = k.prettyMove;       // 例: "3 ▲２六歩(27)" or "▲２六歩(27)"
        label.replace(kDropHeadNumber, QString());
        label = label.trimmed();            // 念のため前後の空白を除去

        b->setCurrentMove(label);
        list.push_back(b);
    }
    endResetModel();
}

void KifuBranchListModel::updateBranchCandidates(const QList<KifDisplayItem>& rows)
{
    QStringList s; s.reserve(rows.size());
    for (qsizetype i=0;i<rows.size();++i) s << rows[i].prettyMove;
    qCDebug(lcUi).noquote() << "updateBranchCandidates:" << rows.size() << "items:" << s.join(", ");

    beginResetModel();
    qDeleteAll(list);
    list.clear();

    // 先頭に「手数（半角/全角）+空白」が付いていたら落とす
    static const auto& kDropHeadNumber = *[]() {
        static const QRegularExpression r(
            QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));
        return &r;
    }();

    list.reserve(rows.size());
    for (const auto& k : rows) {
        auto* b = new KifuBranchDisplay();

        QString label = k.prettyMove;       // 例: "3 ▲２六歩(27)" or "▲２六歩(27)"
        label.replace(kDropHeadNumber, QString());
        label = label.trimmed();            // 念のため前後の空白を除去

        b->setCurrentMove(label);
        list.push_back(b);
    }
    endResetModel();

    // ロックを有効にして、外部からの変更を防ぐ
    m_locked = true;
}

void KifuBranchListModel::setHasBackToMainRow(bool enabled)
{
    if (m_hasBackToMainRow == enabled) return;
    beginResetModel();
    m_hasBackToMainRow = enabled;
    endResetModel();
}

bool KifuBranchListModel::hasBackToMainRow() const
{
    return m_hasBackToMainRow;
}

bool KifuBranchListModel::isBackToMainRow(int row) const
{
    return m_hasBackToMainRow && (row == static_cast<int>(list.size()));
}

int KifuBranchListModel::backToMainRowIndex() const
{
    return m_hasBackToMainRow ? static_cast<int>(list.size()) : -1;
}

void KifuBranchListModel::setActiveNode(int nodeId)
{
    if (nodeId < 0 || nodeId >= m_nodes.size()) return;
    if (m_activeNodeId == nodeId) return;

    m_activeNodeId = nodeId;

    // ここでビュー再描画（全体 or 必要範囲）を促す
    // ※あなたの実装に合わせて適切な index 範囲を渡してください。
    emit dataChanged(index(0,0), index(rowCount()-1, columnCount()-1));

    const auto& n = m_nodes[nodeId];
    qCDebug(lcUi).nospace()
        << "active nodeId=" << nodeId
        << " vid=" << n.vid << " ply=" << n.ply
        << " row=" << n.row << " prev=" << n.prevId
        << " nexts=" << n.nextIds.size();
}

// 罫線（前後エッジ）だけで targetPly を探すフォールバック。
// preferPrev=true のとき、まず prev を見て一致すれば採用、だめなら next 側を探す。
bool KifuBranchListModel::graphFallbackToPly(int targetPly, bool preferPrev)
{
    if (m_activeNodeId < 0) return false;

    const auto tryPrev = [&]()->bool{
        const int p = m_nodes[m_activeNodeId].prevId;
        if (p >= 0 && m_nodes[p].ply == targetPly) {
            setActiveNode(p); return true;
        }
        return false;
    };
    const auto tryNext = [&]()->bool{
        const auto& nx = m_nodes[m_activeNodeId].nextIds;
        for (int id : nx) if (m_nodes[id].ply == targetPly) {
                setActiveNode(id); return true;
            }
        return false;
    };

    if (preferPrev) {
        if (tryPrev()) return true;
        if (tryNext()) return true;
    } else {
        if (tryNext()) return true;
        if (tryPrev()) return true;
    }
    return false;
}

QHash<int, QByteArray> KifuBranchListModel::roleNames() const
{
    QHash<int, QByteArray> r = QAbstractItemModel::roleNames();
    r[DispCountRole] = "dispCount";
    return r;
}

int KifuBranchListModel::rowMaxPly(int row) const
{
    if (row < 0) return 0;

    int maxPly = 0;
    // m_nodes に { row, ply, ... } が入っている前提で、同 row の ply 最大値を求める
    for (const auto& n : std::as_const(m_nodes)) {
        if (n.row == row && n.ply > maxPly) {
            maxPly = n.ply;
        }
    }
    return maxPly;
}

void KifuBranchListModel::clear()
{
    // ビューに一括リセットを通知
    beginResetModel();

    // 候補行（所有権あり）を削除
    qDeleteAll(list);
    list.clear();

    // 「本譜へ戻る」行フラグも初期化
    m_hasBackToMainRow = false;

    // 分岐グラフの状態を初期化
    m_nodes.clear();
    m_activeNodeId = -1;

    endResetModel();
}

void KifuBranchListModel::setCurrentHighlightRow(int row)
{
    if (m_currentHighlightRow == row) {
        return;
    }

    const int oldRow = m_currentHighlightRow;
    m_currentHighlightRow = row;

    // 旧行と新行の背景色を更新
    if (oldRow >= 0 && oldRow < rowCount()) {
        const QModelIndex idx = index(oldRow, 0);
        emit dataChanged(idx, idx, { Qt::BackgroundRole });
    }
    if (row >= 0 && row < rowCount()) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, { Qt::BackgroundRole });
    }
}

int KifuBranchListModel::currentHighlightRow() const
{
    return m_currentHighlightRow;
}
