#include "kifuanalysislistmodel.h"
#include <cmath>
#include <QColor>

//将棋の棋譜解析結果をGUI上で表示するためのモデルクラス
// コンストラクタ
KifuAnalysisListModel::KifuAnalysisListModel(QObject *parent) : AbstractListModel<KifuAnalysisResultsDisplay>(parent)
{
}

// 列数を返す。
int KifuAnalysisListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // 「指し手」「候補手」「一致」「評価値」「形勢」「差」「盤面」「読み筋」の8列を返す。
    return 8;
}

// 指し手と候補手が一致するか判定する
// 「同　」（同＋全角空白）と「同」（空白なし）の表記を考慮して比較
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

// 評価値から形勢文字列を生成する
// 評価値は先手視点（正=先手有利、負=後手有利）
static QString getJudgementString(const QString& evalStr)
{
    if (evalStr.isEmpty() || evalStr == QStringLiteral("-")) {
        return QString();
    }
    
    // 詰み表示の場合
    if (evalStr.contains(QStringLiteral("詰"))) {
        if (evalStr.startsWith(QStringLiteral("-"))) {
            return QStringLiteral("後手勝ち");
        } else {
            return QStringLiteral("先手勝ち");
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
        advantage = QStringLiteral("互角");
    } else if (absScore <= 300) {
        advantage = QStringLiteral("やや有利");
    } else if (absScore <= 600) {
        advantage = QStringLiteral("有利");
    } else if (absScore <= 1000) {
        advantage = QStringLiteral("優勢");
    } else {
        advantage = QStringLiteral("勝勢");
    }
    
    // 互角の場合は「互角」のみ、それ以外は「先手/後手 + 形勢」
    if (absScore <= 100) {
        return advantage;
    }
    
    QString side = (score > 0) ? QStringLiteral("先手") : QStringLiteral("後手");
    return side + advantage;
}

// データを返す。
QVariant KifuAnalysisListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // 盤面列（列6）は特別扱い：ボタン風の表示
    if (index.column() == 6) {
        if (role == Qt::DisplayRole) {
            return QStringLiteral("表示");
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

// ヘッダを返す。
QVariant KifuAnalysisListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // roleが表示用のデータを要求していない場合、空のQVariantを返す。
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    // // 横方向のヘッダが要求された場合
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return QStringLiteral("指し手");
        case 1:
            return QStringLiteral("候補手");
        case 2:
            return QStringLiteral("一致");
        case 3:
            return QStringLiteral("評価値");
        case 4:
            return QStringLiteral("形勢");
        case 5:
            return QStringLiteral("差");
        case 6:
            return QStringLiteral("盤面");
        case 7:
            return QStringLiteral("読み筋");
        default:
            // それ以外の場合、空のQVariantを返す。
            return QVariant();
        }
    } else {
        // 縦方向のヘッダが要求された場合、セクション番号を返す。
        return QVariant(section + 1);
    }
}
