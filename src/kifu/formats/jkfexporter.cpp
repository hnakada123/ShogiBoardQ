/// @file jkfexporter.cpp
/// @brief JKF形式棋譜エクスポータクラスの実装

#include "jkfexporter.h"
#include "jkfformatter.h"
#include "gamerecordmodel.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"
#include "logcategories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

static const QRegularExpression& newlineRe()
{
    static const QRegularExpression re(QStringLiteral("\r?\n"));
    return re;
}

// ========================================
// JKF構築用の内部関数
// ========================================

// JKF形式のヘッダ部分を構築
static QJsonObject buildJkfHeader(const GameRecordModel::ExportContext& ctx)
{
    QJsonObject header;

    const QList<KifGameInfoItem> items = GameRecordModel::collectGameInfo(ctx);
    for (const auto& item : items) {
        const QString key = item.key.trimmed();
        const QString val = item.value.trimmed();
        if (!key.isEmpty()) {
            header[key] = val;
        }
    }

    return header;
}

// JKF形式の初期局面部分を構築
static QJsonObject buildJkfInitial(const GameRecordModel::ExportContext& ctx)
{
    QJsonObject initial;

    const QString preset = JkfFormatter::sfenToJkfPreset(ctx.startSfen);
    if (!preset.isEmpty()) {
        initial[QStringLiteral("preset")] = preset;
    } else {
        initial[QStringLiteral("preset")] = QStringLiteral("OTHER");

        const QJsonObject data = JkfFormatter::sfenToJkfData(ctx.startSfen);
        if (!data.isEmpty()) {
            initial[QStringLiteral("data")] = data;
        }
    }

    return initial;
}

// JKF形式の moves 配列を構築（本譜）
static QJsonArray buildJkfMoves(const QList<KifDisplayItem>& disp)
{
    QJsonArray moves;

    int prevToX = 0, prevToY = 0;

    for (qsizetype i = 0; i < disp.size(); ++i) {
        const auto& item = disp[i];

        if (i == 0 && (item.prettyMove.trimmed().isEmpty()
                       || item.prettyMove.contains(QStringLiteral("開始局面")))) {
            QJsonObject openingMove;
            if (!item.comment.isEmpty()) {
                QJsonArray comments;
                const QStringList lines = item.comment.split(newlineRe());
                for (const QString& line : lines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QLatin1Char('*'))) {
                        comments.append(trimmed.mid(1));
                    } else if (!trimmed.isEmpty()) {
                        comments.append(trimmed);
                    }
                }
                if (!comments.isEmpty()) {
                    openingMove[QStringLiteral("comments")] = comments;
                }
            }
            moves.append(openingMove);
        } else {
            const QJsonObject moveObj = JkfFormatter::convertMoveToJkf(item, prevToX, prevToY, static_cast<int>(i));
            if (!moveObj.isEmpty()) {
                moves.append(moveObj);
            }
        }
    }

    return moves;
}

// KifuBranchNode から JKF形式の分岐指し手配列を構築
static QJsonArray buildJkfForkMovesFromNode(KifuBranchNode* node, QSet<int>& visitedNodes)
{
    QJsonArray forkMoves;

    if (node == nullptr) {
        return forkMoves;
    }

    if (visitedNodes.contains(node->nodeId())) {
        return forkMoves;
    }
    visitedNodes.insert(node->nodeId());

    int forkPrevToX = 0, forkPrevToY = 0;

    KifuBranchNode* current = node;
    while (current != nullptr) {
        if (visitedNodes.contains(current->nodeId()) && current != node) {
            break;
        }
        if (current != node) {
            visitedNodes.insert(current->nodeId());
        }

        const QString moveText = current->displayText().trimmed();
        if (moveText.isEmpty()) {
            if (current->childCount() > 0) {
                current = current->childAt(0);
                continue;
            }
            break;
        }

        KifDisplayItem item;
        item.prettyMove = current->displayText();
        item.comment = current->comment();
        item.timeText = current->timeText();

        QJsonObject forkMoveObj = JkfFormatter::convertMoveToJkf(item, forkPrevToX, forkPrevToY, current->ply());
        if (forkMoveObj.isEmpty()) {
            if (current->childCount() > 0) {
                current = current->childAt(0);
                continue;
            }
            break;
        }

        if (current->hasBranch()) {
            const QVector<KifuBranchNode*>& children = current->children();
            if (children.size() > 1) {
                QJsonArray childForks;
                for (int i = 1; i < children.size(); ++i) {
                    QJsonArray childForkMoves = buildJkfForkMovesFromNode(children.at(i), visitedNodes);
                    if (!childForkMoves.isEmpty()) {
                        childForks.append(childForkMoves);
                    }
                }
                if (!childForks.isEmpty()) {
                    forkMoveObj[QStringLiteral("forks")] = childForks;
                }
            }
        }

        forkMoves.append(forkMoveObj);

        if (current->childCount() > 0) {
            current = current->childAt(0);
        } else {
            break;
        }
    }

    return forkMoves;
}

// KifuBranchTree から JKF形式の分岐を追加
static void addJkfForksFromTree(QJsonArray& movesArray, const KifuBranchTree* tree)
{
    if (tree == nullptr || tree->isEmpty()) {
        return;
    }

    QVector<KifuBranchNode*> mainLineNodes = tree->mainLine();

    for (KifuBranchNode* node : std::as_const(mainLineNodes)) {
        if (!node->hasBranch()) {
            continue;
        }

        const QVector<KifuBranchNode*>& children = node->children();

        if (children.size() <= 1) {
            continue;
        }

        int ply = node->ply();
        if (ply >= movesArray.size()) {
            continue;
        }

        QJsonObject moveObj = movesArray[ply].toObject();

        QJsonArray forks;
        if (moveObj.contains(QStringLiteral("forks"))) {
            forks = moveObj[QStringLiteral("forks")].toArray();
        }

        for (int i = 1; i < children.size(); ++i) {
            KifuBranchNode* branchChild = children.at(i);
            QSet<int> visitedNodes;
            visitedNodes.insert(node->nodeId());

            QJsonArray forkMovesArr = buildJkfForkMovesFromNode(branchChild, visitedNodes);
            if (!forkMovesArr.isEmpty()) {
                forks.append(forkMovesArr);
            }
        }

        if (!forks.isEmpty()) {
            moveObj[QStringLiteral("forks")] = forks;
            movesArray[ply] = moveObj;
        }
    }
}

// ========================================
// JkfExporter 本体
// ========================================

QStringList JkfExporter::exportLines(const GameRecordModel& model,
                                     const GameRecordModel::ExportContext& ctx)
{
    QStringList out;

    QJsonObject root;

    // 1) header
    root[QStringLiteral("header")] = buildJkfHeader(ctx);

    // 2) initial
    root[QStringLiteral("initial")] = buildJkfInitial(ctx);

    // 3) moves（本譜）
    const QList<KifDisplayItem> disp = model.collectMainlineForExport();
    QJsonArray movesArray = buildJkfMoves(disp);

    // 4) 分岐を追加（KifuBranchTree から）
    const KifuBranchTree* tree = model.branchTree();
    if (tree != nullptr && !tree->isEmpty()) {
        addJkfForksFromTree(movesArray, tree);
    }

    root[QStringLiteral("moves")] = movesArray;

    const QJsonDocument doc(root);
    out << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qCDebug(lcKifu).noquote() << "toJkfLines: generated JKF with"
                              << movesArray.size() << "moves";

    return out;
}
