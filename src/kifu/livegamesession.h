#ifndef LIVEGAMESESSION_H
#define LIVEGAMESESSION_H

#include <QObject>
#include <QVector>
#include <QStringList>
#include <QSet>

#include "kifubranchnode.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

class KifuBranchTree;

/**
 * @brief ライブ対局のセッションを管理するクラス
 *
 * 途中局面からの対局開始に対応し、指し手を一時的に記録して
 * 対局終了時に分岐ツリーにコミットする。
 */
class LiveGameSession : public QObject
{
    Q_OBJECT

public:
    explicit LiveGameSession(QObject* parent = nullptr);

    /**
     * @brief ツリーを設定
     */
    void setTree(KifuBranchTree* tree);

    // === セッション開始 ===

    /**
     * @brief 任意の局面から対局を開始
     * @param branchPoint 分岐起点（nullptr = ルートから開始）
     */
    void startFromNode(KifuBranchNode* branchPoint);

    /**
     * @brief 開始局面から対局を開始（新規対局）
     */
    void startFromRoot();

    // === 状態 ===

    /**
     * @brief セッションがアクティブかどうか
     */
    bool isActive() const { return m_active; }

    /**
     * @brief 分岐起点を取得（nullptrなら開始局面から）
     */
    KifuBranchNode* branchPoint() const { return m_branchPoint; }

    /**
     * @brief 分岐起点の手数を取得
     */
    int anchorPly() const;

    /**
     * @brief 分岐起点の局面SFENを取得
     */
    QString anchorSfen() const;

    // === 開始可否チェック ===

    /**
     * @brief 指定ノードから対局を開始できるかどうか
     * @param node チェックするノード
     * @return 終局手でなければtrue
     */
    static bool canStartFrom(KifuBranchNode* node);

    // === 指し手追加 ===

    /**
     * @brief さらに指し手を追加できるかどうか
     */
    bool canAddMove() const { return m_active && !m_hasTerminal; }

    /**
     * @brief 指し手を追加
     * @param move 指し手
     * @param displayText 表示テキスト
     * @param sfen この手を指した後のSFEN
     * @param elapsed 消費時間
     */
    void addMove(const ShogiMove& move, const QString& displayText,
                 const QString& sfen, const QString& elapsed);

    /**
     * @brief 終局手を追加（対局終了時）
     * @param type 終局手の種類
     * @param displayText 表示テキスト
     * @param elapsed 消費時間
     */
    void addTerminalMove(TerminalType type, const QString& displayText,
                         const QString& elapsed);

    // === 確定・破棄 ===

    /**
     * @brief 現在のセッションをKifuBranchTreeに確定
     * @return 新しい分岐の終端ノード
     */
    KifuBranchNode* commit();

    /**
     * @brief セッションを破棄（何も変更しない）
     */
    void discard();

    // === 一時データアクセス ===

    /**
     * @brief 追加した手数
     */
    int moveCount() const { return static_cast<int>(m_moves.size()); }

    /**
     * @brief 総手数（anchorPly + moveCount）
     */
    int totalPly() const;

    /**
     * @brief 追加した手のリストを取得
     */
    QVector<KifDisplayItem> moves() const { return m_moves; }

    /**
     * @brief 最新局面のSFENを取得
     */
    QString currentSfen() const;

    // === 確定時の分岐情報 ===

    /**
     * @brief 分岐を作成するか（途中からの場合true）
     */
    bool willCreateBranch() const;

    /**
     * @brief 確定後の分岐名を取得（"分岐N"）
     */
    QString newLineName() const;

signals:
    /**
     * @brief セッションが開始された
     */
    void sessionStarted(KifuBranchNode* branchPoint);

    /**
     * @brief 指し手が追加された
     */
    void moveAdded(int ply, const QString& displayText);

    /**
     * @brief 終局手が追加された
     */
    void terminalAdded(TerminalType type);

    /**
     * @brief セッションが確定された
     */
    void sessionCommitted(KifuBranchNode* newLineEnd);

    /**
     * @brief セッションが破棄された
     */
    void sessionDiscarded();

    /**
     * @brief 分岐マークが更新された
     * @param branchPlys 分岐がある手数のセット
     */
    void branchMarksUpdated(const QSet<int>& branchPlys);

    /**
     * @brief 棋譜欄モデルの更新が必要
     */
    void recordModelUpdateRequired();

private:
    void reset();
    void computeAndEmitBranchMarks();

    bool m_active = false;
    bool m_hasTerminal = false;
    KifuBranchNode* m_branchPoint = nullptr;
    KifuBranchTree* m_tree = nullptr;

    QVector<KifDisplayItem> m_moves;
    QVector<ShogiMove> m_gameMoves;
    QStringList m_sfens;
};

#endif // LIVEGAMESESSION_H
