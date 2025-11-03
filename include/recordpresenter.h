#ifndef RECORDPRESENTER_H
#define RECORDPRESENTER_H

#include <QObject>
#include <QStringList>
#include <QList>

class KifuRecordListModel;
class RecordPane;
class KifDisplayItem;

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
};

#endif // RECORDPRESENTER_H
