#ifndef KIFUBRANCHTREE_H
#define KIFUBRANCHTREE_H

#include <QObject>
#include <QHash>
#include <QVector>
#include <QSet>

#include "kifubranchnode.h"

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
     * @brief 指定ノードの分岐候補を取得（兄弟ノード）
     */
    QVector<KifuBranchNode*> branchesAt(KifuBranchNode* node) const;

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

    // === コメント ===

    /**
     * @brief ノードのコメントを設定
     */
    void setComment(int nodeId, const QString& comment);

signals:
    /**
     * @brief ツリー構造が変更された
     */
    void treeChanged();

    /**
     * @brief ノードが追加された
     */
    void nodeAdded(KifuBranchNode* node);

    /**
     * @brief ノードが削除された
     */
    void nodeRemoved(int nodeId);

    /**
     * @brief コメントが変更された
     */
    void commentChanged(int nodeId, const QString& comment);

private:
    KifuBranchNode* createNode();
    void collectLinesRecursive(KifuBranchNode* node,
                               QVector<KifuBranchNode*>& currentPath,
                               QVector<BranchLine>& lines,
                               int& lineIndex) const;

    KifuBranchNode* m_root = nullptr;
    QHash<int, KifuBranchNode*> m_nodeById;
    int m_nextNodeId = 1;
};

#endif // KIFUBRANCHTREE_H
