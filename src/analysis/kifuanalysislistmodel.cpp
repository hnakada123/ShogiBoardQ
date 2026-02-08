/// @file kifuanalysislistmodel.cpp
/// @brief 棋譜解析結果リストモデルクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "kifuanalysislistmodel.h"
#include <cmath>
#include <QColor>

/// @todo remove コメントスタイルガイド適用済み
KifuAnalysisListModel::KifuAnalysisListModel(QObject *parent) : AbstractListModel<KifuAnalysisResultsDisplay>(parent)
{
}

/// @todo remove コメントスタイルガイド適用済み
int KifuAnalysisListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「指し手」「候補手」「一致」「評価値」「形勢」「差」「盤面」「読み筋」の8列を返す。
    return 8;
}

// 指し手と候補手の一致判定
// 「同　」と「同」の表記揺れを正規化して比較する
static bool isMoveMatch(const QString& currentMove, const QString& candidateMove)
{
    if (currentMove.isEmpty() || candidateMove.isEmpty()) {
        return false;
    }
    
    // 指し手から▲/△の後の部分を抽出
    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");
    
    // 現在の指し手の移動部分を抽出
    QString currentMoveBody;
    qsizetype currentMarkPos = currentMove.indexOf(senteMark);
    if (currentMarkPos < 0) {
        currentMarkPos = currentMove.indexOf(goteMark);
    }
    if (currentMarkPos >= 0) {
        currentMoveBody = currentMove.mid(currentMarkPos);
    } else {
        currentMoveBody = currentMove;
    }
    
    // 候補手の移動部分を抽出
    QString candidateMoveBody;
    qsizetype candidateMarkPos = candidateMove.indexOf(senteMark);
    if (candidateMarkPos < 0) {
        candidateMarkPos = candidateMove.indexOf(goteMark);
    }
    if (candidateMarkPos >= 0) {
        candidateMoveBody = candidateMove.mid(candidateMarkPos);
    } else {
        candidateMoveBody = candidateMove;
    }
    
    // 比較用に正規化
    QString normalizedCurrent = currentMoveBody;
    QString normalizedCandidate = candidateMoveBody;
    
    // 「同」の後の空白を統一して削除（半角空白、全角空白の両方）
    // 「同　」→「同」、「同 」→「同」
    normalizedCurrent.replace(QStringLiteral("同　"), QStringLiteral("同"));
    normalizedCurrent.replace(QStringLiteral("同 "), QStringLiteral("同"));
    normalizedCandidate.replace(QStringLiteral("同　"), QStringLiteral("同"));
    normalizedCandidate.replace(QStringLiteral("同 "), QStringLiteral("同"));
    
    return normalizedCurrent == normalizedCandidate;
}

// 評価値を将棋の形勢表現（互角/やや有利/有利/優勢/勝勢）に変換する
// 評価値は先手視点の値を前提とする（正=先手有利、負=後手有利）
static QString getJudgementString(const QString& evalStr)
{
    if (evalStr.isEmpty() || evalStr == QStringLiteral("-")) {
        return QString();
    }

    // 詰み表示の場合
    if (evalStr.contains(QStringLiteral("詰")) || evalStr.contains(QStringLiteral("mate"))) {
        if (evalStr.startsWith(QStringLiteral("-"))) {
            return QObject::tr("後手勝ち");
        } else {
            return QObject::tr("先手勝ち");
        }
    }

    bool ok = false;
    int score = evalStr.toInt(&ok);
    if (!ok) {
        return QString();
    }

    int absScore = std::abs(score);
    QString advantage;

    if (absScore <= 100) {
        advantage = QObject::tr("互角");
    } else if (absScore <= 300) {
        advantage = QObject::tr("やや有利");
    } else if (absScore <= 600) {
        advantage = QObject::tr("有利");
    } else if (absScore <= 1000) {
        advantage = QObject::tr("優勢");
    } else {
        advantage = QObject::tr("勝勢");
    }

    // 互角の場合は「互角」のみ、それ以外は「先手/後手 + 形勢」
    if (absScore <= 100) {
        return advantage;
    }

    QString side = (score > 0) ? QObject::tr("先手") : QObject::tr("後手");
    return side + advantage;
}

/// @todo remove コメントスタイルガイド適用済み
QVariant KifuAnalysisListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // 盤面列（列6）は特別扱い：ボタン風の表示
    if (index.column() == 6) {
        if (role == Qt::DisplayRole) {
            return tr("表示");
        }
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        if (role == Qt::BackgroundRole) {
            // 青緑系のボタン色（クリック可能であることを示す）
            return QColor(0x20, 0x9c, 0xee);  // 明るい青
        }
        if (role == Qt::ForegroundRole) {
            return QColor(Qt::white);  // 白文字
        }
        return QVariant();
    }

    // 右寄せ：評価値 / 差 列は右寄せ
    if (role == Qt::TextAlignmentRole) {
        // 列3（評価値）と列5（差）を右寄せ
        if (index.column() == 3 || index.column() == 5) {
            return QVariant::fromValue<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        // 列2（一致）は中央寄せ
        if (index.column() == 2) {
            return QVariant::fromValue<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    KifuAnalysisResultsDisplay* item = list[index.row()];

    switch (index.column()) {
    case 0: // 指し手
        return item->currentMove();
    case 1: // 候補手
        return item->candidateMove();
    case 2: // 一致
        if (isMoveMatch(item->currentMove(), item->candidateMove())) {
            return QStringLiteral("◯");
        }
        return QString();
    case 3: // 評価値
        return item->evaluationValue();
    case 4: // 形勢
        return getJudgementString(item->evaluationValue());
    case 5: // 差
        return item->evaluationDifference();
    case 7: // 読み筋
        return item->principalVariation();
    default:
        return QVariant();
    }
}

/// @todo remove コメントスタイルガイド適用済み
QVariant KifuAnalysisListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return tr("指し手");
        case 1:
            return tr("候補手");
        case 2:
            return tr("一致");
        case 3:
            return tr("評価値");
        case 4:
            return tr("形勢");
        case 5:
            return tr("差");
        case 6:
            return tr("盤面");
        case 7:
            return tr("読み筋");
        default:
            return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}
