#ifndef KIFUNAVIGATIONSTATE_H
#define KIFUNAVIGATIONSTATE_H

/// @file kifunavigationstate.h
/// @brief 表示中の手（分岐ツリーのカーソル）と、分岐点ごとに選んだ子の記憶

#include <QObject>
#include <QHash>
#include <QList>

#include "recordline.h"

class KifuBranchTree;
class KifuBranchNode;

/**
 * @brief 表示中の手と、分岐点で選んだ子（表示中の手順）を持つ。棋譜の表示はここだけを正とする
 *
 * 現在の手・手数・手順（ライン）・分岐候補は、すべてノード ID から導く（写しを持たない）。
 * 変わるたびに changed() を1回出す。棋譜欄・分岐候補・分岐ツリー・盤・コメント欄・評価値グラフは
 * changed() を受けて、ここから表示を作り直す。
 *
 * ツリーを作り直して表示中の手が無くなったときは、開始局面へ戻る。
 */
class KifuNavigationState : public QObject
{
    Q_OBJECT

public:
    explicit KifuNavigationState(KifuBranchTree* tree, QObject* parent = nullptr);

    KifuBranchTree* tree() const { return m_tree; }

    // --- 表示中の手 ---
    KifuBranchNode* currentNode() const;
    int currentPly() const;
    /// 表示中の局面（終局の行は直前の局面）。ツリーが空なら空文字
    QString currentSfen() const;

    // --- 表示中の手順 ---
    /// 開始局面から、分岐点で選んだ子をたどった手順の終わりまで
    QList<KifuBranchNode*> selectedPath() const;
    /// 表示中の手順の番号（0=本譜、1以降=分岐。KifuBranchTree::allLines() の添字）
    int currentLineIndex() const;
    /// 表示中の手順が本譜か。KifuBranchNode::isMainLine() は親の最初の子かしか見ないので、表示の判定にはこちらを使う
    bool isOnMainLine() const;
    /// 表示中の手順の、手数 currentPly までの手順（終局の行は除く。対局中はライブの手も含む）
    RecordLine currentRecordLine() const;
    /**
     * @brief 表示中の局面を、開始局面からの手順（直前の手を含む）で返す
     *
     * 検討でエンジンに送る局面。読み筋の1手目が直前の手と同じマスなら「同」と書くので、局面だけでなく手順で持つ。
     * ツリーが空なら平手。局面を読めなければ nullopt
     */
    std::optional<UsiPosition> currentUsiPosition() const;
    /// node の次に進む手（選んだ子。選んでいなければ最初の子）
    KifuBranchNode* nextNode(const KifuBranchNode* node) const;

    // --- 分岐候補 ---
    /// 表示中の手と同じ親を持つ手（兄弟が無ければ空）
    QList<KifuBranchNode*> branchCandidatesAtCurrent() const;
    /// 分岐候補欄に「本譜へ戻る」を出すか（分岐候補があり、本譜以外を表示しているとき）
    bool offersBackToMainLine() const;

    // --- 変更（変わったときだけ changed() を1回出す） ---
    /// node へ移り、開始局面から node までの経路を選んだ手順として記憶する
    void setCurrentNode(KifuBranchNode* node);
    /// node へ移り、開始局面から lineEnd（node の子孫）までの経路を記憶する（分岐の手順を指定して移るとき）
    void setCurrentNode(KifuBranchNode* node, KifuBranchNode* lineEnd);
    /// 分岐点での選択を忘れて node へ移る（本譜に戻るとき・ツリーを作り直したとき）
    void resetToNode(KifuBranchNode* node);
    /// 選択を忘れて開始局面へ移る
    void goToRoot();

signals:
    /// 表示中の手または表示中の手順が変わった
    void changed();

private:
    void onTreeChanged();
    void rememberPath(KifuBranchNode* node);
    void apply(int nodeId, QHash<int, int> selections);

    KifuBranchTree* m_tree = nullptr;
    int m_currentNodeId = -1;
    // 分岐点のノードID → 選んだ子のノードID。並べ替えで意味が変わらない。
    // ライン番号はこの経路から表示時に導出し、別の状態として保持しない。
    QHash<int, int> m_selectedChildIds;
    // 最後に changed() を出したときのツリーの構造の版。手を足してから表示中の手を移したときに、
    // 後から届くツリーの変更の知らせで同じ通知を繰り返さないために使う
    quint64 m_notifiedRevision = 0;
};

#endif // KIFUNAVIGATIONSTATE_H
