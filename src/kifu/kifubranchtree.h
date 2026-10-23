#ifndef KIFUBRANCHTREE_H
#define KIFUBRANCHTREE_H

/// @file kifubranchtree.h
/// @brief 分岐ツリーデータモデルクラスの定義


#include <QObject>
#include <QHash>
#include <QList>
#include <QSet>
#include <memory>
#include <optional>

#include "kifubranchnode.h"
#include "recordline.h"
#include "recordentry.h"

class KifuBranchTree;

/**
 * @brief 分岐ラインの情報
 */
struct BranchLine {
    int lineIndex = 0;                      // 0=本譜、1以降=分岐
    QList<KifuBranchNode*> nodes;         // ノード列（ルートから終端まで）
    int branchPly = 0;                      // 分岐開始手数（本譜は0）
    KifuBranchNode* branchPoint = nullptr;  // 分岐点のノード（本譜はnullptr）
};

/**
 * @brief 分岐構造を表すツリー型データモデル
 *
 * 棋譜の分岐構造をツリーで管理する。UIとは独立したデータモデル。
 *
 * 構造の変更（手の追加・子の並べ替え・部分木の削除・作り直し）は、変わった所をノード ID で
 * 型付きのシグナルで知らせる。ノード ID は同じツリーの寿命中は使い回さない。
 * 手の表記・コメント・しおりの変更は構造の変更ではない（棋譜の文書 KifuDocument が知らせる）。
 */
class KifuBranchTree : public QObject
{
    Q_OBJECT

public:
    explicit KifuBranchTree(QObject* parent = nullptr);
    ~KifuBranchTree() override;

    // === 構築 ===

    /**
     * @brief ツリーを空にする（reset を知らせる）
     */
    void clear();

    /**
     * @brief 開始局面だけのツリーに作り直す（reset を知らせる）
     * @param sfen 開始局面のSFEN
     */
    void setRootSfen(const QString& sfen);

    /**
     * @brief 手を parent の子の末尾に足す（nodeAppended を知らせる）
     * @return 作成されたノード（parent が無いか終局の行なら nullptr）
     *
     * 終局の行（投了・中断など）は盤面を変えないため、entry の局面と指し手は使わず、
     * 親と同じ局面と無効な ShogiMove を持つ。これにより終局の行の局面は追加経路
     * （棋譜の読み込み・対局）によらず一定になる。
     */
    KifuBranchNode* addRecord(KifuBranchNode* parent, const RecordEntry& entry);

    /// 葉ノードを削除する（ルートは不可。subtreeRemoved を知らせる）
    bool removeLeaf(KifuBranchNode* node);

    // === 分岐の編集（変更があれば childrenReordered・subtreeRemoved を知らせる） ===

    /// 子ノードを兄弟の中で index の位置へ移す（分岐の並べ替え。先頭の子が本譜側になる）
    bool moveChild(KifuBranchNode* child, int index);

    /// ルートから node までの手順が本譜になるよう、経路上の各手を兄弟の先頭へ移す。
    /// 既に本譜なら何もせず false を返す。
    bool promoteToMainLine(KifuBranchNode* node);

    /// node とその子孫をすべて削除する（ルートは不可）。
    /// 呼び出し側は削除前に現在位置を削除範囲の外へ移しておくこと。
    bool removeSubtree(KifuBranchNode* node);

    /// node を根とする部分木のノード数（node 自身を含む）
    static int subtreeSize(const KifuBranchNode* node);

    /// node を含む変化の先頭の手（兄弟を持つ最も近い祖先、または node 自身）。
    /// 経路上に分岐がなければ nullptr
    static KifuBranchNode* variationHead(KifuBranchNode* node);

    // === 一括更新 ===

    /**
     * @brief 一括更新を開始する（変更の知らせを最も外側の endBatchUpdate() まで遅らせる）
     *
     * 手を足してから表示中の手を移す、といった続けて行う変更で、表示が途中の状態を描かないようにする。
     * 入れ子にできる。endBatchUpdate() では、遅らせた知らせを順に出す。作り直し（clear・setRootSfen）を
     * 含むときは、まとめて reset を1回だけ出す（棋譜の読み込み）。
     */
    void beginBatchUpdate();

    /**
     * @brief 一括更新を終了する
     * @see beginBatchUpdate()
     */
    void endBatchUpdate();

    // === クエリ ===

    /**
     * @brief ルートノードを取得
     */
    KifuBranchNode* root() const { return m_root.get(); }

    /**
     * @brief ノードIDからノードを取得
     */
    KifuBranchNode* nodeAt(int nodeId) const;
    /// lineEndNodeId の手で終わる手順の ply 手目のノード（ply が手順より後なら末尾。手が無ければ nullptr）。
    /// 手順を手の ID で覚えると、分岐を足す・本譜にする・並べ替えても同じ手順を指す
    KifuBranchNode* nodeOnLine(int lineEndNodeId, int ply) const;

    /**
     * @brief 親ノードの子から、同じ指し手を表す既存ノードを探す
     * @param parent 親ノード
     * @param entry 足そうとしている行
     * @return 一致する子ノード。見つからない場合は nullptr
     *
     * 通常の指し手は、手数を除いた局面SFENが一致するか、
     * 有効な ShogiMove 同士が一致すれば同じ手とみなす。
     * 終局手は終局手の種類が一致すれば同じ手とみなす。
     * 同じ手を指し直したときに重複ノードを作らないために使う。
     */
    KifuBranchNode* findMatchingChild(KifuBranchNode* parent, const RecordEntry& entry) const;

    /**
     * @brief ツリーが空かどうか
     */
    bool isEmpty() const { return m_root == nullptr; }

    /**
     * @brief ノード数を取得
     */
    int nodeCount() const { return static_cast<int>(m_nodeById.size()); }

    /**
     * @brief 構造変更（追加・削除・並べ替え・作り直し）のたびに増える値
     *
     * 棋譜の文書が保存した時点と比べて未保存かを決める。表示中の手は、同じ構造をもう知らせたかを見る。
     */
    quint64 revision() const { return m_revision; }

    // === ライン操作 ===

    /**
     * @brief 本譜のノード列を取得
     */
    QList<KifuBranchNode*> mainLine() const;

    /**
     * @brief ルートから指定ノードまでの経路を取得
     */
    QList<KifuBranchNode*> pathToNode(KifuBranchNode* node) const;

    /**
     * @brief ノード列（ルートからの経路やライン）を、ほかの機能が使う手順の値にする
     *
     * 終局行（投了など）は含めない。currentPly は手数の範囲に収める。
     */
    static RecordLine toRecordLine(const QList<KifuBranchNode*>& nodes, int currentPly = 0);

    /**
     * @brief 本譜の手順（終局行を除く）
     */
    RecordLine mainRecordLine() const { return toRecordLine(mainLine()); }

    /**
     * @brief 全てのラインを取得
     */
    QList<BranchLine> allLines() const;

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
     * @brief 指定ノードが属するラインインデックスを取得
     * @param node 対象ノード
     * @return ラインインデックス（見つからない場合はstd::nullopt）
     *
     * ノードが複数のラインに属する場合（分岐点より前）は、
     * そのノードを終端として含むラインを優先して返す。
     * 見つからない場合は、そのノードを含む最初のラインを返す。
     */
    std::optional<int> findLineIndexForNode(KifuBranchNode* node) const;

    // === コメント ===

    /**
     * @brief ノードのコメントを設定
     */
    void setComment(int nodeId, const QString& comment);

    // === データ抽出 ===

    /**
     * @brief 指定ラインの手順（書き出し・CLI 用。末尾の終局の行と、開始局面のコメント・しおりを含む）
     * @param lineIndex ラインインデックス（0=本譜。範囲外なら空の手順）
     */
    RecordLine exportLine(int lineIndex) const;

    /**
     * @brief 指定ラインのSFENリストを取得
     * @param lineIndex ラインインデックス（0=本譜）
     * @return SFENのリスト（インデックス0=開始局面、1以降=各手後の局面）
     */
    QStringList sfenListForLine(int lineIndex) const;

signals:
    /// 手 nodeId を parentId の子の末尾に足した
    void nodeAppended(int parentId, int nodeId);
    /// parentId の子の並びを変えた（本譜にする操作では、その先の手の子の並びも変わる。ライン番号が変わる）
    void childrenReordered(int parentId);
    /// parentId の子 nodeId とその子孫を消した
    void subtreeRemoved(int parentId, int nodeId);
    /// ツリーを作り直した（空にした・開始局面を設定した・一括更新の中で作り直した）
    void reset();

private:
    /// 知らせを遅らせている構造の変更（一括更新の中）
    struct Change {
        enum class Kind { Appended, Reordered, Removed, Reset };
        Kind kind = Kind::Reset;
        int parentId = -1;
        int nodeId = -1;
    };

    std::unique_ptr<KifuBranchNode> createNode();
    void forgetSubtree(const KifuBranchNode* node);
    bool moveChildSilently(KifuBranchNode* child, int index);
    void collectLinesRecursive(KifuBranchNode* node,
                               QList<KifuBranchNode*>& currentPath,
                               QList<BranchLine>& lines,
                               int& lineIndex) const;
    void invalidateLineCache();

    /// 構造の変更を知らせる（一括更新中は endBatchUpdate() まで遅らせる）
    void notify(const Change& change);
    void emitChange(const Change& change);

    std::unique_ptr<KifuBranchNode> m_root;
    QHash<int, KifuBranchNode*> m_nodeById;   ///< ID → ノード（非所有）
    int m_nextNodeId = 1;

    int m_batchDepth = 0;                 ///< 一括更新の入れ子深さ（0=通常）
    QList<Change> m_pendingChanges;       ///< 一括更新中の変更（古い順）

    mutable QList<BranchLine> m_linesCache;
    mutable bool m_linesCacheDirty = true;
    quint64 m_revision = 0;               ///< 構造変更の回数（revision() 参照）
};

#endif // KIFUBRANCHTREE_H
