#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

#include <QVariant>
#include <QVector>
#include <QRectF>
#include "abstractlistmodel.h"
#include "kifubranchdisplay.h"
#include "kifdisplayitem.h"

// 分岐候補欄のモデル
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay>
{
    Q_OBJECT
public:
    explicit KifuBranchListModel(QObject *parent = nullptr);

    // 1列（指し手）
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // 候補行 + （必要なら）最後に「本譜へ戻る」
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // ---- 追加API ----
    // 候補クリア
    void clearBranchCandidates();

    // KIF表示用（prettyMove/time）から候補を一括設定
    void setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows);

    // 最下段に「本譜へ戻る」を表示する/しない
    void setHasBackToMainRow(bool enabled);
    bool hasBackToMainRow() const;

    // 指定行が「本譜へ戻る」か？
    bool isBackToMainRow(int row) const;
    // 「本譜へ戻る」行のindex（無効時は -1）
    int backToMainRowIndex() const;

    // 表示行 index のラベルを返す（BackToMain 行ならその文字列が返る）
    QString labelAt(int row) const {
        if (row < 0 || row >= m_rows.size()) return {};
        return m_rows.at(row).label;  // m_rows[i].label を使っている前提
    }

    // BackToMain を除いた“実質の候補数”
    int branchCandidateCount() const {
        int n = rowCount();
        return n - (m_hasBackToMainRow ? 1 : 0);
    }

    // BackToMain を除いた最初の候補の行インデックス（なければ -1）
    int firstBranchRowIndex() const {
        if (rowCount() == 0) return -1;
        return m_hasBackToMainRow ? (rowCount() >= 2 ? 1 : -1) : 0;
    }

    // --- 追加: カスタムロール ---
    enum Roles {
        DispCountRole = Qt::UserRole + 1,  // 行ごとの最大手数（0..N）
    };

    // 候補リスト＋分岐グラフの全消去
    void clear();

private:  
    struct RowItem {
        QString label;
        // …既存のフィールド（variationId など）がある前提…
        bool isBackToMain = false; // 使っていないなら m_hasBackToMainRow だけでもOK
    };
    QVector<RowItem> m_rows;
    bool m_hasBackToMainRow = false;

private:
    int m_activeVid = -1;
    int m_activePly = -1; // グローバル手数

signals:
    void activeNodeChanged(int nodeId);        // 任意（ビューの再レイアウト等）
    void activeVidPlyChanged(int vid, int ply);// 任意（ログ用など）

private:
    struct Node {
        int id = -1;
        int vid = -1;          // 0=本譜, 1..=変化
        int ply = 0;           // グローバル手数
        int row = 0;           // 表示行（Main=0, VarN）
        QRectF rect;           // 画面上の矩形（必要なら）
        int prevId = -1;       // 直前
        QVector<int> nextIds;  // 直後（分岐なら複数）
    };

    // ハイライト内部状態
    int m_activeNodeId = -1;
    int m_mainVid = 0;

    // グラフ保持
    QVector<Node> m_nodes;                // nodeId = index
    QHash<quint64, int> m_key2node;       // key=(vid<<32)|ply  -> nodeId

private:
    static quint64 vpKey(int vid, int ply) {
        return (quint64(uint32_t(vid)) << 32) | uint32_t(ply);
    }
    void setActiveNode_(int nodeId);      // 低レベル切替
    bool graphFallbackToPly_(int targetPly, bool preferPrev);

    // --- 追加: QML等でロール名が欲しい場合に備えて ---
    QHash<int, QByteArray> roleNames() const override;

    // --- 追加: 行の最大手数を算出するヘルパ ---
    int rowMaxPly_(int row) const;
};

#endif // KIFUBRANCHLISTMODEL_H
