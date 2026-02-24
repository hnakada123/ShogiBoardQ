#ifndef KIFUBRANCHTREE_H
#define KIFUBRANCHTREE_H

/// @file kifubranchtree.h
/// @brief 分岐ツリーデータモデルクラスの定義


#include <QObject>
#include <QHash>
#include <QVector>
#include <QSet>
#include <QList>

#include "kifubranchnode.h"
#include "kifdisplayitem.h"

class KifuBranchTree;

/**
 * @brief 分岐ラインの情報
 */
struct BranchLine {
    int lineIndex = 0;                      // 0=本譜、1以降=分岐
    QString name;                           // "本譜", "分岐1"...
    QVector<KifuBranchNode*> nodes;         // ノード列（ルートから終端まで）
    int branchPly = 0;                      // 分岐開始手数（本譜は0）
    KifuBranchNode* branchPoint = nullptr;  // 分岐点のノード（本譜はnullptr）
};

/**
 * @brief 分岐構造を表すツリー型データモデル
 *
 * 棋譜の分岐構造をツリーで管理する。
 * UIとは独立したデータモデル。
 */
class KifuBranchTree : public QObject
{
    Q_OBJECT

public:
    explicit KifuBranchTree(QObject* parent = nullptr);
    ~KifuBranchTree() override;

    // === 構築 ===

    /**
     * @brief ツリーをクリアする
     */
    void clear();

    /**
     * @brief 開始局面のSFENを設定してルートノードを作成
     * @param sfen 開始局面のSFEN
     */
    void setRootSfen(const QString& sfen);

    /**
     * @brief 指し手を追加する
     * @param parent 親ノード
     * @param move 指し手
     * @param displayText 表示テキスト
     * @param sfen この手を指した後の局面SFEN
     * @param timeText 消費時間テキスト
     * @return 作成されたノード
     */
    KifuBranchNode* addMove(KifuBranchNode* parent,
                           const ShogiMove& move,
                           const QString& displayText,
                           const QString& sfen,
                           const QString& timeText = QString());

    /**
     * @brief 終局手を追加する
     * @param parent 親ノード
     * @param type 終局手の種類
     * @param displayText 表示テキスト
     * @param timeText 消費時間テキスト
     * @return 作成されたノード
     */
    KifuBranchNode* addTerminalMove(KifuBranchNode* parent,
                                    TerminalType type,
                                    const QString& displayText,
                                    const QString& timeText = QString());

    /**
     * @brief 指し手を追加する（treeChanged シグナルを発火しない）
     *
     * ライブ対局中など、treeChanged による UI 更新を避けたい場合に使用。
     */
    KifuBranchNode* addMoveQuiet(KifuBranchNode* parent,
                                 const ShogiMove& move,
                                 const QString& displayText,
                                 const QString& sfen,
                                 const QString& timeText = QString());

    // === クエリ ===

    /**
     * @brief ルートノードを取得
     */
    KifuBranchNode* root() const { return m_root; }

    /**
     * @brief ノードIDからノードを取得
     */
    KifuBranchNode* nodeAt(int nodeId) const;

    /**
     * @brief 指定ラインの指定手数のノードを取得
     * @param lineEnd ラインの終端ノード
     * @param ply 手数
     */
    KifuBranchNode* findByPlyOnLine(KifuBranchNode* lineEnd, int ply) const;

    /**
     * @brief 本譜のN手目を取得
     */
    KifuBranchNode* findByPlyOnMainLine(int ply) const;

    /**
     * @brief SFENで一致するノードを探す
     * @param sfen 検索するSFEN（手数部分は除いて比較）
     * @return 見つかったノード、見つからない場合はnullptr
     */
    KifuBranchNode* findBySfen(const QString& sfen) const;

    /**
     * @brief ツリーが空かどうか
     */
    bool isEmpty() const { return m_root == nullptr; }

    /**
     * @brief ノード数を取得
     */
    int nodeCount() const { return static_cast<int>(m_nodeById.size()); }

    // === ライン操作 ===

    /**
     * @brief 本譜のノード列を取得
     */
    QVector<KifuBranchNode*> mainLine() const;

    /**
     * @brief ルートから指定ノードまでの経路を取得
     */
    QVector<KifuBranchNode*> pathToNode(KifuBranchNode* node) const;

    /**
     * @brief 全てのラインを取得
     */
    QVector<BranchLine> allLines() const;

    /**
     * @brief ライン数を取得
     */
    int lineCount() const;

    // === 分岐判定 ===

    /**
     * @brief このノードに分岐があるか
     */
    bool hasBranch(KifuBranchNode* node) const;

    /**
     * @brief 指定ラインの分岐あり手数集合を取得
     */
    QSet<int> branchablePlysOnLine(const BranchLine& line) const;

    /**
     * @brief 指定ノードが属するラインインデックスを取得
     * @param node 対象ノード
     * @return ラインインデックス（見つからない場合は-1）
     *
     * ノードが複数のラインに属する場合（分岐点より前）は、
     * そのノードを終端として含むラインを優先して返す。
     * 見つからない場合は、そのノードを含む最初のラインを返す。
     */
    int findLineIndexForNode(KifuBranchNode* node) const;

    // === コメント ===

    /**
     * @brief ノードのコメントを設定
     */
    void setComment(int nodeId, const QString& comment);

    // === データ抽出（ResolvedRow互換）===

    /**
     * @brief 指定ラインの表示アイテムリストを取得
     * @param lineIndex ラインインデックス（0=本譜）
     * @return KifDisplayItemのリスト（インデックス0=開始局面、1以降=指し手）
     */
    QList<KifDisplayItem> getDisplayItemsForLine(int lineIndex) const;

    /**
     * @brief 指定ラインのSFENリストを取得
     * @param lineIndex ラインインデックス（0=本譜）
     * @return SFENのリスト（インデックス0=開始局面、1以降=各手後の局面）
     */
    QStringList getSfenListForLine(int lineIndex) const;

signals:
    /**
     * @brief ツリー構造が変更された
     */
    void treeChanged();

private:
    KifuBranchNode* createNode();
    void collectLinesRecursive(KifuBranchNode* node,
                               QVector<KifuBranchNode*>& currentPath,
                               QVector<BranchLine>& lines,
                               int& lineIndex) const;
    void invalidateLineCache();

    KifuBranchNode* m_root = nullptr;
    QHash<int, KifuBranchNode*> m_nodeById;
    int m_nextNodeId = 1;

    mutable QVector<BranchLine> m_linesCache;
    mutable bool m_linesCacheDirty = true;
};

#endif // KIFUBRANCHTREE_H
