/// @file kifuanalysislistmodel.cpp
/// @brief 棋譜解析の結果の表のモデルの実装

#include "kifuanalysislistmodel.h"
#include "shogienginethinkingmodel.h"
#include "movefont.h"
#include "applicationfonts.h"
#include "kifupresentation.h"
#include <cmath>

KifuAnalysisListModel::KifuAnalysisListModel(QObject *parent) : QAbstractTableModel(parent)
{
}

int KifuAnalysisListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int KifuAnalysisListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return ColumnCount;
}

const KifuAnalysisRow* KifuAnalysisListModel::row(int row) const
{
    if (row < 0 || row >= m_rows.size()) return nullptr;
    return &m_rows.at(row).row;
}

void KifuAnalysisListModel::appendRow(const KifuAnalysisRow& row)
{
    const int position = static_cast<int>(m_rows.size());
    beginInsertRows(QModelIndex(), position, position);
    m_rows.append(Entry{row, {}});
    endInsertRows();
}

void KifuAnalysisListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

// 表の描画と列幅の計算のたびに呼ばれる。表記の設定が前と同じなら、前に作った文字列を返す
QString KifuAnalysisListModel::display(const Entry& entry, DisplayKind kind, bool full) const
{
    const KifuAnalysisRow& r = entry.row;
    return entry.texts.text(kind, full, [&r, kind, full] {
        switch (kind) {
        case MoveDisplay:
            return r.ply == 0 ? tr("=== 開始局面 ===")
                              : KifuPresentation::numberedLabel(
                                    r.ply, KifuPresentation::moveLabel(r.move, r.beforeSfen, r.lastUsiMove, full), r.branch);
        case CandidateDisplay: return KifuPresentation::moveLabel(r.candidateMove, r.candidateSfen, r.candidateUsi, full);
        case PvDisplay: return r.book ? KifuPresentation::bookLabel() : KifuPresentation::pv(r.pv, full);
        case DisplayKindCount: break;
        }
        return QString();
    });
}

// 先手から見た評価値の表示（定跡・未取得は「-」、詰みは思考の表と同じ「3手詰」。勝ち負けは形勢の列）
QVariant KifuAnalysisListModel::evaluationText(const UsiScore& score)
{
    if (!score.isCp() && !score.isMate()) return QStringLiteral("-");
    return ShogiEngineThinkingModel::scoreDisplay(score);
}

// 先手から見た評価値を形勢（互角・やや有利・有利・優勢・勝勢）にする
QString KifuAnalysisListModel::judgementText(const UsiScore& score)
{
    if (score.isMate()) return score.mateSign() > 0 ? tr("先手勝ち") : tr("後手勝ち");
    if (!score.isCp()) return QString();

    const qint64 absScore = std::abs(static_cast<qint64>(score.value));
    // 互角のときは「互角」だけ、それ以外は「先手/後手：形勢」
    if (absScore <= 100) return tr("互角");
    QString advantage;
    if (absScore <= 300) advantage = tr("やや有利");
    else if (absScore <= 600) advantage = tr("有利");
    else if (absScore <= 1000) advantage = tr("優勢");
    else advantage = tr("勝勢");
    const QString side = score.value > 0 ? tr("先手") : tr("後手");
    return tr("%1：%2").arg(side, advantage);
}

QVariant KifuAnalysisListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()
        || index.column() < 0 || index.column() >= columnCount())
        return QVariant();

    const int column = index.column();
    const Entry& entry = m_rows.at(index.row());
    if (role == Qt::FontRole && (column == MoveColumn || column == CandidateColumn || column == PvColumn))
        return MoveFont::role();

    if (role == Qt::ToolTipRole || role == Qt::AccessibleTextRole) {
        const QString value = column == MoveColumn ? display(entry, MoveDisplay, true)
            : column == CandidateColumn ? display(entry, CandidateDisplay, true)
            : column == PvColumn ? display(entry, PvDisplay, true) : data(index, Qt::DisplayRole).toString();
        if (column == BoardColumn) return tr("この局面からの読み筋を盤面で表示します。");
        const QString description = headerData(index.column(), Qt::Horizontal, Qt::ToolTipRole).toString();
        return description.isEmpty() ? value : value + QLatin1Char('\n') + description;
    }

    // 盤面列はボタンの文字
    if (column == BoardColumn) {
        if (role == Qt::DisplayRole) {
            return tr("表示");
        }
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        return QVariant();
    }

    // 評価値と差は右寄せ、一致は中央寄せ
    if (role == Qt::TextAlignmentRole) {
        if (column == EvaluationColumn || column == DifferenceColumn) {
            return QVariant::fromValue<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (column == MatchColumn) {
            return QVariant::fromValue<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    const KifuAnalysisRow& r = entry.row;
    switch (column) {
    case MoveColumn:
        return display(entry, MoveDisplay);
    case CandidateColumn:
        return display(entry, CandidateDisplay);
    case MatchColumn:
        return r.matchesCandidate() ? QStringLiteral("◯") : QString();
    case EvaluationColumn:
        return evaluationText(r.evaluation);
    case JudgementColumn:
        return judgementText(r.evaluation);
    case DifferenceColumn:
        return r.evaluationDifference ? QVariant(*r.evaluationDifference) : QVariant(QStringLiteral("-"));
    case PvColumn:
        return display(entry, PvDisplay);
    default:
        return QVariant();
    }
}

QVariant KifuAnalysisListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::ToolTipRole) {
        switch (section) {
        case MoveColumn: return tr("棋譜で実際に指された手。この手を指した後の局面を評価します。");
        case CandidateColumn: return tr("直前の局面でエンジンが推奨した手。解析範囲の先頭では空欄になります。");
        case MatchColumn: return tr("実際の指し手と候補手が一致すると◯を表示します。");
        case EvaluationColumn: return tr("先手視点の評価値。正は先手有利、負は後手有利です。未取得は「-」で表示します。");
        case JudgementColumn: return tr("評価値から判定した形勢です。");
        case DifferenceColumn: return tr("直前の解析局面からの評価値の増減（先手視点）。比較できない場合は「-」で表示します。");
        case BoardColumn: return tr("クリックまたはEnterキーで読み筋を盤面に表示します。");
        case PvColumn: return tr("この手を指した後の局面からの読み筋です。");
        default: return QVariant();
        }
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case MoveColumn: return tr("指し手");
        case CandidateColumn: return tr("候補手");
        case MatchColumn: return tr("一致");
        case EvaluationColumn: return tr("評価値");
        case JudgementColumn: return tr("形勢");
        case DifferenceColumn: return tr("評価値差");
        case BoardColumn: return tr("盤面");
        case PvColumn: return tr("読み筋");
        default: return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}
