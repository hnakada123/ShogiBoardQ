#ifndef JOSEKITABLEMODEL_H
#define JOSEKITABLEMODEL_H

/// @file josekitablemodel.h
/// @brief 定跡ウィンドウの表（表示中の局面の定跡手）

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include "josekitypes.h"

/**
 * @brief 表示中の局面の定跡手の表
 *
 * 「着手」「編集」「削除」の列はボタンの文字を返す（ボタンの形はデリゲートが描き、押したときの処理は
 * 表を持つ側が列で決める）。人間が指せないときは「着手」の列を空にする。
 * 行数が変わるときも行の挿入・削除で知らせ、利用者が変えた列幅を保つ。
 */
class JosekiTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { No, Play, Move, NextMove, Edit, Delete, Value, Depth, Frequency, Comment, ColumnCount };

    explicit JosekiTableModel(QObject *parent = nullptr);

    /// 局面と定跡手を入れ替える
    void setMoves(const QString &sfen, const QList<JosekiMove> &moves, bool humanCanPlay);
    const QList<JosekiMove> &moves() const { return m_moves; }
    const QString &sfen() const { return m_sfen; }
    bool humanCanPlay() const { return m_humanCanPlay; }

    /// 定跡手の表記（棋譜欄と同じ書式。"none" は「なし」）。定跡の表は手ごとに独立しているので「同」は使わない
    static QString moveText(const QString &usiMove, const QString &sfen);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QString m_sfen;
    QList<JosekiMove> m_moves;
    bool m_humanCanPlay = true;
};

#endif // JOSEKITABLEMODEL_H
