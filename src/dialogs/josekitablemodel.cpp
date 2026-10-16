/// @file josekitablemodel.cpp
/// @brief 定跡ウィンドウの表の実装

#include "josekitablemodel.h"

#include "kifmovetext.h"
#include "kifupresentation.h"
#include "sfenreplay.h"
#include "shogiposition.h"

#include <QLocale>

JosekiTableModel::JosekiTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QString JosekiTableModel::moveText(const QString &usiMove, const QString &sfen)
{
    if (usiMove.isEmpty()) return QString();
    if (usiMove == QStringLiteral("none")) return tr("なし");
    if (KifuPresentation::options().notation == KifuPresentation::Notation::Western)
        return KifuPresentation::move(sfen, usiMove, KifuPresentation::options());

    Position board;
    (void)board.assignSfen(sfen);
    const Turn mover = board.sideToMove();
    const auto move = UsiMove::parse(usiMove);
    if (!move) return (mover == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△")) + usiMove;
    const Piece moving = move->isDrop() ? move->dropPieceFor(mover) : board.pieceAt(move->from);
    return KifMoveText::format(mover, moving, *move);
}

void JosekiTableModel::setMoves(const QString &sfen, const QList<JosekiMove> &moves, bool humanCanPlay)
{
    const int oldRows = static_cast<int>(m_moves.size());
    const int newRows = static_cast<int>(moves.size());
    m_sfen = sfen;
    m_humanCanPlay = humanCanPlay;
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

    if (role == Qt::TextAlignmentRole) {
        switch (column) {
        case Value: case Depth: case Frequency: return int(Qt::AlignRight | Qt::AlignVCenter);
        case Comment: return int(Qt::AlignLeft | Qt::AlignVCenter);
        default: return int(Qt::AlignCenter);
        }
    }
    if (role == Qt::ToolTipRole) {
        switch (column) {
        case Move: return m_humanCanPlay ? move.move + QLatin1Char('\n') + tr("ダブルクリックで着手") : move.move;
        case NextMove: return move.nextMove;
        case Comment: return move.comment;
        default: return {};
        }
    }
    if (role != Qt::DisplayRole) return {};

    QLocale locale = QLocale::system();
    locale.setNumberOptions(QLocale::DefaultNumberOptions);
    switch (column) {
    case No: return index.row() + 1;
    case Play: return m_humanCanPlay ? tr("着手") : QString();
    case Move: return moveText(move.move, m_sfen);
    case NextMove: return moveText(move.nextMove, SfenReplay::positionAfter(m_sfen, move.move));
    case Edit: return tr("編集");
    case Delete: return tr("削除");
    case Value: return locale.toString(move.value);
    case Depth: return locale.toString(move.depth);
    case Frequency: return locale.toString(move.frequency);
    case Comment: return move.comment;
    default: return {};
    }
}

QVariant JosekiTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
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
    case Comment: return tr("コメント");
    default: return {};
    }
}
