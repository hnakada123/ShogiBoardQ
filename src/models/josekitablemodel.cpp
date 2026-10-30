/// @file josekitablemodel.cpp
/// @brief 定跡ウィンドウの表の実装

#include "josekitablemodel.h"

#include "kifupresentation.h"
#include "sfenreplay.h"

#include <QColor>
#include <QLocale>

namespace {

// ツールチップは文字を見て HTML として描くことがあるので、定跡ファイルのコメントなどは文字のまま出す
// （<img> でローカルのファイルを読ませない。セキュリティ監査 S12）
QString plainToolTip(const QString &text)
{
    if (text.isEmpty()) return {};
    return QStringLiteral("<qt>%1</qt>").arg(text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
}

} // namespace

JosekiTableModel::JosekiTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QString JosekiTableModel::moveText(const QString &usiMove, const QString &sfen)
{
    if (usiMove.isEmpty()) return QString();
    return KifuPresentation::move(sfen, usiMove, KifuPresentation::options());
}

bool JosekiTableModel::hasNoReply(const JosekiMove &move)
{
    return !move.nextMove.has_value();
}

QString JosekiTableModel::derivedReplyText(const QString &moveText)
{
    return tr("（%1）").arg(moveText);
}

void JosekiTableModel::setMoves(const QString &sfen, const QList<JosekiMove> &moves, bool humanCanPlay,
                                const LeadingMove &leadingMoveAt)
{
    const int oldRows = static_cast<int>(m_moves.size());
    const int newRows = static_cast<int>(moves.size());
    m_sfen = sfen;
    m_humanCanPlay = humanCanPlay;
    m_frequencyTotal = 0;
    for (const JosekiMove &move : moves) m_frequencyTotal += qMax(move.frequency, 0);
    // 定跡手を指した後の局面（予想応手の表記と導いた応手に使う。表示のたびに指し直さない）
    m_afterSfens.clear();
    m_derivedReplies.clear();
    for (const JosekiMove &move : moves) {
        const QString after = SfenReplay::positionAfter(sfen, move.move);
        m_afterSfens.append(after);
        m_derivedReplies.append(leadingMoveAt && hasNoReply(move) ? leadingMoveAt(after) : QString());
    }
    // 行の数だけを増減させ、残る行は中身を差し替える（列幅と表示位置は保つ）
    if (newRows < oldRows) {
        beginRemoveRows(QModelIndex(), newRows, oldRows - 1);
        m_moves = moves;
        endRemoveRows();
    } else if (newRows > oldRows) {
        beginInsertRows(QModelIndex(), oldRows, newRows - 1);
        m_moves = moves;
        endInsertRows();
    } else {
        m_moves = moves;
    }
    const int kept = qMin(oldRows, newRows);
    if (kept > 0) emit dataChanged(index(0, 0), index(kept - 1, ColumnCount - 1));
}

int JosekiTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_moves.size());
}

int JosekiTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant JosekiTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_moves.size()) return {};
    const JosekiMove &move = m_moves.at(index.row());
    const int column = index.column();
    // 予想応手が「なし」で、次の局面の定跡から応手を導けた行
    const QString &derivedReply = m_derivedReplies.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        switch (column) {
        case Value: case Depth: case Frequency: case Ratio: return int(Qt::AlignRight | Qt::AlignVCenter);
        case Comment: return int(Qt::AlignLeft | Qt::AlignVCenter);
        default: return int(Qt::AlignCenter);
        }
    }
    if (role == Qt::ToolTipRole) {
        switch (column) {
        case Move: return plainToolTip(m_humanCanPlay ? move.move + QLatin1Char('\n') + tr("ダブルクリックで着手") : move.move);
        case NextMove:
            if (derivedReply.isEmpty()) return plainToolTip(move.nextMove.value_or(QString()));
            return plainToolTip(tr("予想応手は「なし」です。\n次の局面の定跡手で出現頻度（同じなら評価値）が最も高い手を表示しています。\n"
                                   "やねうら王はこの手を先読みに使います。") + QLatin1Char('\n') + derivedReply);
        case Comment: return plainToolTip(move.comment);
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (column == NextMove && !derivedReply.isEmpty()) return QColor(Qt::darkGray);
        return {};
    }
    if (role != Qt::DisplayRole) return {};

    QLocale locale = QLocale::system();
    locale.setNumberOptions(QLocale::DefaultNumberOptions);
    switch (column) {
    case No: return index.row() + 1;
    case Play: return m_humanCanPlay ? tr("着手") : QString();
    case Move: return moveText(move.move, m_sfen);
    case NextMove: {
        const QString &after = m_afterSfens.at(index.row());
        if (!derivedReply.isEmpty()) return derivedReplyText(moveText(derivedReply, after));
        return move.nextMove ? moveText(*move.nextMove, after) : tr("なし");
    }
    case Edit: return tr("編集");
    case Delete: return tr("削除");
    case Value: return locale.toString(move.value);
    case Depth: return locale.toString(move.depth);
    case Frequency: return locale.toString(move.frequency);
    case Ratio:
        if (m_frequencyTotal <= 0) return QString();
        return locale.toString(100.0 * qMax(move.frequency, 0) / static_cast<double>(m_frequencyTotal), 'f', 1)
            + QLatin1Char('%');
    case Comment: return move.comment;
    default: return {};
    }
}

QVariant JosekiTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return {};
    if (role == Qt::ToolTipRole && section == Ratio) return tr("この局面の定跡手の出現頻度の合計に対する割合");
    if (role != Qt::DisplayRole) return {};
    switch (section) {
    case No: return tr("No.");
    case Play: return tr("着手");
    case Move: return tr("定跡手");
    case NextMove: return tr("予想応手");
    case Edit: return tr("編集");
    case Delete: return tr("削除");
    case Value: return tr("評価値");
    case Depth: return tr("深さ");
    case Frequency: return tr("出現頻度");
    case Ratio: return tr("比率");
    case Comment: return tr("コメント");
    default: return {};
    }
}
