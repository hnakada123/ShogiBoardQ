#ifndef KIFUBRANCHTREEBUILDER_H
#define KIFUBRANCHTREEBUILDER_H

/// @file kifubranchtreebuilder.h
/// @brief 分岐ツリー構築ビルダークラスの定義


#include <QString>

class KifuBranchTree;
struct KifParseResult;
struct KifLine;

/**
 * @brief 既存データからKifuBranchTreeを構築するビルダークラス
 */
class KifuBranchTreeBuilder
{
public:
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

    static void addKifLineToTree(KifuBranchTree* tree,
                                 const KifLine& line,
                                 int startPly);
};

#endif // KIFUBRANCHTREEBUILDER_H
