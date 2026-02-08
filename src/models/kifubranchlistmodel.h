#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

/// @file kifubranchlistmodel.h
/// @brief 分岐候補欄のリストモデルクラスの定義

#include <QVariant>
#include <QVector>
#include <QRectF>
#include "abstractlistmodel.h"
#include "kifubranchdisplay.h"
#include "kifdisplayitem.h"

/**
 * @brief 分岐候補欄のリストモデル
 *
 * 棋譜の分岐候補手を管理し、表示する。
 * 末尾に「本譜へ戻る」行を任意で追加できる。
 * 内部に分岐グラフ（Node）を保持し、アクティブノードの追跡も行う。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay>
{
    Q_OBJECT
public:
    explicit KifuBranchListModel(QObject *parent = nullptr);

    // --- QAbstractTableModel オーバーライド ---

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // --- 候補操作 API ---

    /// 候補をクリアする（ロック中は無視される）
    void clearBranchCandidates();

    /// KIF表示用データから候補を一括設定する（ロック中は無視される）
    void setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows);

    /// 末尾の「本譜へ戻る」行の表示/非表示を設定する
    void setHasBackToMainRow(bool enabled);
    bool hasBackToMainRow() const;

    /// 指定行が「本譜へ戻る」行かどうか
    bool isBackToMainRow(int row) const;

    /// 「本譜へ戻る」行のインデックス（無効時は -1）
    int backToMainRowIndex() const;

    /// 表示行のラベルを返す
    QString labelAt(int row) const {
        if (row < 0 || row >= m_rows.size()) return {};
        return m_rows.at(row).label;
    }

    /// 「本譜へ戻る」を除いた実質の候補数
    int branchCandidateCount() const {
        int n = rowCount();
        return n - (m_hasBackToMainRow ? 1 : 0);
    }

    /// 「本譜へ戻る」を除いた最初の候補の行インデックス（なければ -1）
    int firstBranchRowIndex() const {
        if (rowCount() == 0) return -1;
        return m_hasBackToMainRow ? (rowCount() >= 2 ? 1 : -1) : 0;
    }

    // --- ハイライト ---

    /// 現在ハイライトする行を設定する（棋譜欄と連動）
    void setCurrentHighlightRow(int row);
    int currentHighlightRow() const;

    // --- カスタムロール ---

    /// カスタムロール定義
    enum Roles {
        DispCountRole = Qt::UserRole + 1,  ///< 行ごとの最大手数（0..N）
    };

    // --- 全消去 ---

    /// 候補リストと分岐グラフを全消去する
    void clear();

    // --- ロック機能 ---

    /// ロック状態を設定する（ロック中は外部からの候補変更を防ぐ）
    void setLocked(bool locked) { m_locked = locked; }
    bool isLocked() const { return m_locked; }

    /// KifuDisplayCoordinator専用: ロックを無視して候補をリセットする
    void resetBranchCandidates();

    /// KifuDisplayCoordinator専用: ロックを無視して候補を更新する
    void updateBranchCandidates(const QList<KifDisplayItem>& rows);

private:
    bool m_locked = false;                  ///< ロック状態
    struct RowItem {
        QString label;                      ///< 表示ラベル
        bool isBackToMain = false;          ///< 「本譜へ戻る」行フラグ
    };
    QVector<RowItem> m_rows;                ///< 表示行データ
    bool m_hasBackToMainRow = false;        ///< 末尾に「本譜へ戻る」を表示するか
    int m_currentHighlightRow = 0;          ///< 現在ハイライトする行

private:
    int m_activeVid = -1;                   ///< アクティブな変化番号
    int m_activePly = -1;                   ///< アクティブなグローバル手数

signals:
    /// アクティブノードが変更された（ビュー再レイアウト等に使用）
    void activeNodeChanged(int nodeId);

    /// アクティブな変化番号/手数が変更された
    void activeVidPlyChanged(int vid, int ply);

private:
    /// 分岐グラフのノード
    struct Node {
        int id = -1;                        ///< ノードID
        int vid = -1;                       ///< 変化番号（0=本譜, 1..=変化）
        int ply = 0;                        ///< グローバル手数
        int row = 0;                        ///< 表示行
        QRectF rect;                        ///< 画面上の矩形
        int prevId = -1;                    ///< 直前ノードID
        QVector<int> nextIds;               ///< 直後ノードIDリスト（分岐なら複数）
    };

    int m_activeNodeId = -1;                ///< 現在アクティブなノードID
    int m_mainVid = 0;                      ///< 本譜の変化番号

    QVector<Node> m_nodes;                  ///< ノード配列（nodeId = index）
    QHash<quint64, int> m_key2node;         ///< (vid<<32)|ply → nodeId の検索マップ

private:
    static quint64 vpKey(int vid, int ply) {
        return (quint64(uint32_t(vid)) << 32) | uint32_t(ply);
    }
    void setActiveNode(int nodeId);
    bool graphFallbackToPly(int targetPly, bool preferPrev);

    QHash<int, QByteArray> roleNames() const override;

    /// 指定行の最大手数を算出する
    int rowMaxPly(int row) const;
};

#endif // KIFUBRANCHLISTMODEL_H
