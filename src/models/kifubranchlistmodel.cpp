#include "kifubranchlistmodel.h"
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
    return list.size() + (m_hasBackToMainRow ? 1 : 0);
}

QVariant KifuBranchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    // 末尾の「本譜へ戻る」行か？
    const bool isBackRow = (m_hasBackToMainRow && index.row() == list.size());

    if (role == Qt::DisplayRole) {
        if (isBackRow) {
            return tr("本譜へ戻る");
        }
        // 通常候補
        if (index.column() == 0) {
            if (index.row() >= 0 && index.row() < list.size() && list[index.row()]) {
                return list[index.row()]->currentMove();
            }
        }
        return QVariant();
    }

    // ちょっと分かりやすくセンタリング＆太字（お好みで）
    if (role == Qt::TextAlignmentRole && isBackRow) {
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
    beginResetModel();
    qDeleteAll(list);
    list.clear();
    m_hasBackToMainRow = false;
    endResetModel();
}

void KifuBranchListModel::setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows)
{
#ifdef SHOGIBOARDQ_DEBUG_KIF
    QStringList s; s.reserve(rows.size());
    for (int i=0;i<rows.size();++i) s << rows[i].prettyMove;
    qDebug().noquote() << "[BRANCH-MODEL] set" << rows.size() << ":" << s.join(", ");
#endif

    beginResetModel();
    qDeleteAll(list);
    list.clear();

    // 先頭に「手数（半角/全角）+空白」が付いていたら落とす
    static const QRegularExpression kDropHeadNumber(
        QStringLiteral(R"(^\s*[0-9０-９]+\s*)"));

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
    return m_hasBackToMainRow && (row == list.size());
}

int KifuBranchListModel::backToMainRowIndex() const
{
    return m_hasBackToMainRow ? list.size() : -1;
}
