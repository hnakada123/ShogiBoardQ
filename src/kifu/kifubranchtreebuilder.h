#ifndef KIFUBRANCHTREEBUILDER_H
#define KIFUBRANCHTREEBUILDER_H

/// @file kifubranchtreebuilder.h
/// @brief 分岐ツリー構築ビルダークラスの定義


#include <QString>
#include <QList>

class KifuBranchTree;
class KifuBranchNode;
struct KifParseResult;
struct RecordLine;

/**
 * @brief 取り込んだ棋譜（KifParseResult）から分岐ツリーを作る
 */
class KifuBranchTreeBuilder
{
public:
    /**
     * @brief ツリーを取り込んだ棋譜で作り直す
     * @param tree 対象ツリー（クリアして再構築）
     * @param result 取り込んだ棋譜（開始局面は本譜の startSfen）。変化の分岐元は先に書かれた手順
     *               （KifParseResult::addVariation で足したもの）
     * @return 分岐元が先に書かれた手順でない変化があれば、ツリーを変えずに false
     */
    static bool buildFromKifParseResult(KifuBranchTree* tree, const KifParseResult& result);

private:
    KifuBranchTreeBuilder() = default;

    /// from の後に手順の手を足し、足したノードを返す
    static QList<KifuBranchNode*> addKifLineToTree(KifuBranchTree* tree, const RecordLine& line,
                                                  KifuBranchNode* from);
};

#endif // KIFUBRANCHTREEBUILDER_H
