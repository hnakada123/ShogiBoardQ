#ifndef INCREMENTALTABLEVIEW_H
#define INCREMENTALTABLEVIEW_H

/// @file incrementaltableview.h
/// @brief 行の追加で表示領域全体を描き直さない表（棋譜欄・思考表）

#include <QList>
#include <QPair>
#include <QPersistentModelIndex>
#include <QTableView>

class QAbstractItemDelegate;

/**
 * @brief 末尾・先頭への行の追加では、変わった所だけを描き直す表
 *
 * QTableView は行数が変わると、表示領域全体の描き直しを頼む（doItemsLayout）。
 * 棋譜欄は1手ごとに末尾へ、思考表は思考情報が届くたびに先頭へ行を足すので、
 * 見えている全行を毎回描き直していた。
 *
 * - 末尾への追加: 追加した行（と、その間に内容が変わった行）だけを描き直す。
 * - 先頭への追加（一番上を表示しているとき）: 既にある行を画面のずらしで下げ、
 *   空いた上の行（と、その間に内容が変わった行）だけを描き直す。
 * - それ以外の変更（削除・並べ替え・リセット・文字サイズや描画クラスの変更など）は、
 *   これまでどおり Qt に任せて全体を描き直す。
 *
 * 画面のずらしを使うには表示領域が不透明である必要がある。スタイルシートで背景色を
 * 付けた表では Qt が表示領域の自動の塗りつぶしを止めるので、整数倍率のときだけ戻す。
 * 小数倍率では表示領域の端が物理画素の途中に来て、塗ると枠線の画素が変わるため戻さない
 * （そのときの画面のずらしは、Qt が全体の描き直しに切り替える）。
 *
 * 縦のスクロールバーが出入りして表示領域の幅が変わると、最後の列が伸び縮みする。
 * Qt は列幅の変化を受けて後から表示領域全体の描き直しを頼むが、幅の変化で全体の
 * 描き直しが既に予約されているので（描いた後でもう一度同じ内容を描くことになる）省く。
 */
class IncrementalTableView : public QTableView
{
    Q_OBJECT

public:
    explicit IncrementalTableView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;
    void doItemsLayout() override;

protected:
    bool event(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void onStructureChanged();
    void flushRowUpdates();
    void onColumnResized(int column, int oldWidth, int newWidth);

private:
    enum class Change { None, Append, Prepend, Other };

    /// 配置のやり直しを伴う設定（変わっていれば全体を描き直す）
    struct Settings {
        QFont font;
        QSize iconSize;
        QPersistentModelIndex rootIndex;
        const QStyle* style = nullptr;
        QAbstractItemDelegate* delegate = nullptr;
        QList<QAbstractItemDelegate*> columnDelegates;
        bool operator==(const Settings& other) const;
    };
    Settings currentSettings() const;

    void updateViewportFill();

    Change m_change = Change::None;
    int m_firstRow = 0;      ///< 追加した行の先頭（追加後の行番号）
    int m_insertedRows = 0;  ///< 追加した行の数
    QList<QPair<int, int>> m_changedRows;  ///< 追加の後に内容が変わった行の範囲（追加後の行番号）
    QList<QPersistentModelIndex> m_pendingRows;  ///< 描き直しを後に回した行（先頭と末尾の組）
    bool m_flushQueued = false;
    Settings m_lastLayoutSettings;         ///< 前回の配置のやり直しのときの設定
    QList<QMetaObject::Connection> m_modelConnections;  ///< モデルの変更を見張る接続（Qt の接続とは別に外す）
    bool m_viewportFilled = false;
    QSize m_paintedViewportSize;      ///< 表示領域を前回描いたときの大きさ
    bool m_resizingViewport = false;  ///< 表示領域の大きさの変化を処理している間
};

#endif // INCREMENTALTABLEVIEW_H
