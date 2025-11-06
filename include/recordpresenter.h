#ifndef RECORDPRESENTER_H
#define RECORDPRESENTER_H

#include <QObject>
#include <QStringList>
#include <QList>
#include <QPointer>

class KifuRecordListModel;
class RecordPane;
class KifDisplayItem;
class QTableView;
class QItemSelectionModel;

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

    int  currentMoveIndex() const { return m_currentMoveIndex; }

signals:
    void modelReset(); // 必要なら MainWindow 側でハンドリング

private:
    Deps        m_d;
    int         m_currentMoveIndex {0};
    QStringList m_kifuDataList; // KIF文字列出力用の蓄積（必要なければ未使用でOK）

public:
    // 追加：コメントを Presenter 側で管理
    void setCommentsByRow(const QStringList& commentsByRow);
    // 追加：disp からコメント配列を構築（rowCount = sfenRecord->size() or disp.size()+1）
    void setCommentsFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount);

    // 追加：KifuView の選択変更を Presenter が直接受け取り、MainWindow へ signal を転送
    void bindKifuSelection(QTableView* kifuView);

    // 補助：行に対応するコメント取得
    QString commentForRow(int row) const;

    // disp をモデルに反映し、コメントと行数を整えて、選択変更の配線までを一括実行
    void displayAndWire(const QList<KifDisplayItem>& disp,
                        int rowCount,
                        RecordPane* recordPane);

signals:
    // Presenter 経由で MainWindow に「現在行＋コメント」を通知
    void currentRowChanged(int row, const QString& comment);

private:
    QPointer<QTableView> m_kifuView;
    QMetaObject::Connection m_connRowChanged;
    QStringList m_commentsByRow;

private slots:
    void onKifuCurrentRowChanged_(const QModelIndex& current, const QModelIndex& previous);
};

#endif // RECORDPRESENTER_H
