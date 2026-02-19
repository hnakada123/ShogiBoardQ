#ifndef GAMERECORDPRESENTER_H
#define GAMERECORDPRESENTER_H

/// @file gamerecordpresenter.h
/// @brief 棋譜表示プレゼンタクラスの定義


#include <QObject>
#include <QStringList>
#include <QList>
#include <QPointer>

class KifuRecordListModel;
class RecordPane;
struct KifDisplayItem;
class QTableView;
class GameRecordPresenter : public QObject {
    Q_OBJECT
public:
    struct Deps {
        KifuRecordListModel* model {nullptr};
        RecordPane*          recordPane {nullptr}; // 任意（スクロール初期位置合わせ等に使用）
    };

    explicit GameRecordPresenter(const Deps& d, QObject* parent = nullptr);

    // 依存先が後から揃う場合に備えて更新可能に
    void updateDeps(const Deps& d);

    // モデル初期化
    void clear();

    // 棋譜全体をまとめて描画（ヘッダ＋各手を追加）
    void presentGameRecord(const QList<KifDisplayItem>& disp);

    // 1手分を末尾に追記（対局中のライブ更新でも使用）
    void appendMoveLine(const QString& prettyMove, const QString& elapsedTime);

private:
    Deps        m_d;
    int         m_currentMoveIndex {0};
    QStringList m_kifuDataList; // KIF文字列出力用の蓄積（必要なければ未使用でOK）

public:
    // 追加：コメントを Presenter 側で管理
    void setCommentsByRow(const QStringList& commentsByRow);

    // disp をモデルに反映し、コメントと行数を整えて、選択変更の配線までを一括実行
    void displayAndWire(const QList<KifDisplayItem>& disp,
                        int rowCount,
                        RecordPane* recordPane);

    // 現在選択されている行インデックス（選択無しは -1）
    int currentRow() const;

signals:
    // Presenter 経由で MainWindow に「現在行＋コメント」を通知
    void currentRowChanged(int row, const QString& comment);

private:
    void setCommentsFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount);
    void bindKifuSelection(QTableView* kifuView);
    QString commentForRow(int row) const;

    QPointer<QTableView> m_kifuView;
    QMetaObject::Connection m_connRowChanged;
    QStringList m_commentsByRow;

private slots:
    void onKifuCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);

public:
    // ライブ記録用アイテムを追加（保存）
    void addLiveKifItem(const QString& prettyMove, const QString& elapsedTime);

    // 現在のライブ記録を取得
    const QList<KifDisplayItem>& liveDisp() const { return m_liveDisp; }

    // ライブ記録をクリア
    void clearLiveDisp() { m_liveDisp.clear(); }

private:
    QList<KifDisplayItem> m_liveDisp;
};

#endif // GAMERECORDPRESENTER_H
