#ifndef KIFUBRANCHTREEBUILDER_H
#define KIFUBRANCHTREEBUILDER_H

/// @file kifubranchtreebuilder.h
/// @brief 分岐ツリー構築ビルダークラスの定義


#include <QString>
#include <QList>

class KifuBranchTree;
class KifuBranchNode;
struct KifParseResult;
struct KifLine;

/**
 * @brief 既存データからKifuBranchTreeを構築するビルダークラス
 */
class KifuBranchTreeBuilder
{
public:
    /// 既存の棋譜や表示を変更する前に、変化の親子関係と開始手数を検証する。
    static bool validateVariations(const KifParseResult& result);

    /**
     * @brief 既存のツリーにKifParseResultの内容を構築
     * @param tree 対象ツリー（クリアして再構築）
     * @param result パース結果
     * @param startSfen 開始局面のSFEN
     */
    static bool buildFromKifParseResult(KifuBranchTree* tree,
                                        const KifParseResult& result,
                                        const QString& startSfen);

private:
    KifuBranchTreeBuilder() = default;

    static QList<KifuBranchNode*> addKifLineToTree(KifuBranchTree* tree,
                                                  const KifLine& line,
                                                  QList<KifuBranchNode*> path);
};

#endif // KIFUBRANCHTREEBUILDER_H
