#ifndef JOSEKITABLEMODEL_H
#define JOSEKITABLEMODEL_H

/// @file josekitablemodel.h
/// @brief 定跡ウィンドウの表（表示中の局面の定跡手）

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QStringList>

#include "josekitypes.h"

/**
 * @brief 表示中の局面の定跡手の表
 *
 * 「着手」「編集」「削除」の列はボタンの文字を返す（ボタンの形はデリゲートが描き、押したときの処理は
 * 表を持つ側が列で決める）。人間が指せないときは「着手」の列を空にする。
 * 予想応手が「なし」の手で、次の局面の定跡から導いた応手を渡されていれば、予想応手の列に灰色の括弧付きで出す
 * （定跡のデータは変えない）。
 * 行数が変わるときも行の挿入・削除で知らせ、利用者が変えた列幅を保つ。
 */
class JosekiTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// Ratio はこの局面の出現頻度の合計に対する割合（合計が0なら空）
    enum Column { No, Play, Move, NextMove, Edit, Delete, Value, Depth, Frequency, Ratio, Comment, ColumnCount };

    explicit JosekiTableModel(QObject *parent = nullptr);

    /// 局面と定跡手を入れ替える。derivedReplies は行ごとの導いた応手（予想応手が「なし」の行に
    /// やねうら王が先読みに使う、次の局面の定跡手。無ければ空。省けばどの行にも出さない）
    void setMoves(const QString &sfen, const QList<JosekiMove> &moves, bool humanCanPlay,
                  const QStringList &derivedReplies = {});
    const QList<JosekiMove> &moves() const { return m_moves; }
    /// 予想応手が無い
    static bool hasNoReply(const JosekiMove &move);
    bool humanCanPlay() const { return m_humanCanPlay; }

    /// 定跡手の表記（KifuPresentation::move。表示言語と表記の設定に合わせる）。定跡の表は手ごとに独立しているので「同」は使わない
    static QString moveText(const QString &usiMove, const QString &sfen);
    /// 導いた応手の表記（moveText を括弧で囲む）
    static QString derivedReplyText(const QString &moveText);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QString m_sfen;
    QList<JosekiMove> m_moves;
    QStringList m_derivedReplies;  ///< 行ごとの導いた応手（無い行は空）
    QStringList m_afterSfens;      ///< 行ごとの、定跡手を指した後の局面（予想応手の表記に使う）
    qint64 m_frequencyTotal = 0;  ///< 定跡手の出現頻度の合計（比率の分母）
    bool m_humanCanPlay = true;
};

#endif // JOSEKITABLEMODEL_H
