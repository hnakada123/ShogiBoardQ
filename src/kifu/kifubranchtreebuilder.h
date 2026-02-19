#ifndef KIFUBRANCHTREEBUILDER_H
#define KIFUBRANCHTREEBUILDER_H

/// @file kifubranchtreebuilder.h
/// @brief 分岐ツリー構築ビルダークラスの定義


#include <QString>
#include <QVector>

class KifuBranchTree;
struct KifParseResult;
struct KifLine;
struct KifDisplayItem;
struct ShogiMove;

/**
 * @brief 既存データからKifuBranchTreeを構築するビルダークラス
 */
class KifuBranchTreeBuilder
{
public:
    /**
     * @brief KifParseResultからツリーを構築（新規パス）
     * @param result パース結果
     * @param startSfen 開始局面のSFEN
     * @return 構築されたツリー（呼び出し側で所有権を持つ）
     */
    static KifuBranchTree* fromKifParseResult(const KifParseResult& result,
                                              const QString& startSfen);

    /**
     * @brief 既存のツリーにKifParseResultの内容を構築
     * @param tree 対象ツリー（クリアして再構築）
     * @param result パース結果
     * @param startSfen 開始局面のSFEN
     */
    static void buildFromKifParseResult(KifuBranchTree* tree,
                                        const KifParseResult& result,
                                        const QString& startSfen);

private:
    KifuBranchTreeBuilder() = default;

    // 内部ヘルパー
    static void addLineToTree(KifuBranchTree* tree,
                              const QList<KifDisplayItem>& disp,
                              const QStringList& sfens,
                              const QVector<ShogiMove>& moves,
                              int startPly,
                              int parentRow);

    static void addKifLineToTree(KifuBranchTree* tree,
                                 const KifLine& line,
                                 int startPly);
};

#endif // KIFUBRANCHTREEBUILDER_H
