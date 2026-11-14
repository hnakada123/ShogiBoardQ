#ifndef INCREMENTALTABLEVIEW_H
#define INCREMENTALTABLEVIEW_H

/// @file incrementaltableview.h
/// @brief 行の追加で表示領域全体を描き直さない表（棋譜欄・分岐候補欄・思考表・棋譜解析の結果の表）

#include <QList>
#include <QPair>
#include <QPersistentModelIndex>
#include <QRect>
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
 * - 一番上を表示しているときの、表示領域より下の行の削除（思考表の行数の上限を超えた分）:
 *   見えている行は変わらないので描き直さない。
 * - すべての行の削除（思考表の消去）: 行を描いていた範囲だけを描き直す。
 * - 空の表への追加: 先頭へ続けて足しても、足した行だけを描き直す。
 * - それ以外の変更（削除・並べ替え・リセット・文字サイズや描画クラスの変更など）は、
 *   これまでどおり Qt に任せて全体を描き直す。リセットは、配置をその場でやり直して1回の描き直しにまとめる。
 * - 行の色を交互に変える表では、先頭への追加は全体を描き直す（既にある行の色が入れ替わる）。
 *
 * 画面のずらしを使うには表示領域が不透明である必要がある。スタイルシートで背景色を
 * 付けた表では Qt が表示領域の自動の塗りつぶしを止めるので、整数倍率のときだけ戻す。
 * 小数倍率では表示領域の端が物理画素の途中に来て、塗ると枠線の画素が変わるため戻さない
 * （そのときの画面のずらしは、Qt が全体の描き直しに切り替える）。
 *
 * 縦のスクロールバーは、見た目（つまみの位置と大きさ）が変わらなければ描き直さない。QTableView は行数が
 * 変わるたびにスクロールバーの刻みを入れ直し、Qt はそのたびに描き直しを頼む。行が表示領域に収まっている間は
 * 見た目が変わらないのに、スクロールバーと、その下にある親の部品を1行ごとに描いていた。
 *
 * フォーカスの出入りと、ウィンドウが前面になる・後ろに回るとき（ダイアログを開く・閉じるたび）は、
 * 現在の行と選んだ行だけを描き直す。Qt はどちらも表示領域の全体を描き直す（QAbstractItemView の
 * focusInEvent・focusOutEvent・viewportEvent）。見た目が変わるのは、現在のセル（フォーカスの枠）と
 * 選んだ行（フォーカスの有無・前面かどうかで描き分けるスタイル）だけ。前面と後ろでパレットが違う
 * ときは、これまでどおり全体を描き直す。
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
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void onStructureChanged();
    void onModelReset();
    void flushRowUpdates();
    void onColumnResized(int column, int oldWidth, int newWidth);

private:
    enum class Change { None, Append, Prepend, RemoveBelow, RemoveAll, Other };

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
    /// フォーカスの出入りを Qt に処理させ、見た目の変わる行（現在の行と選んだ行）だけを描き直す
    void changeFocus(QFocusEvent* event, bool gained);
    /// 現在の行と選んだ行（と alsoRow の行）を描き直す
    void updateCurrentAndSelectedRows(const QModelIndex& alsoRow = QModelIndex());

    Change m_change = Change::None;
    int m_firstRow = 0;      ///< 追加した行の先頭（追加後の行番号）
    int m_insertedRows = 0;  ///< 追加した行の数
    QRect m_removedRowsRect; ///< すべての行を消す前に、行を描いていた範囲（表示領域の座標）
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
