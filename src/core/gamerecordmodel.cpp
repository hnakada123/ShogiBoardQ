#include "gamerecordmodel.h"
#include "kiftosfenconverter.h"  // KifGameInfoItem
#include "kifurecordlistmodel.h"

#include <QTableWidget>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QPair>
#include <algorithm>

// ========================================
// ヘルパ関数
// ========================================

static inline QString fwColonLine(const QString& key, const QString& val)
{
    return QStringLiteral("%1：%2").arg(key, val);
}

// ヘルパ関数: 指し手から手番記号（▲△）を除去
static QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

// ヘルパ関数: KIF形式の時間文字列にフォーマット（括弧付き）
// 仕様: ( m:ss/HH:MM:SS) 形式
static QString formatKifTime(const QString& timeText)
{
    // 既に括弧付きならそのまま返す
    if (timeText.startsWith(QLatin1Char('('))) return timeText;
    // 空なら既定値
    if (timeText.isEmpty()) return QStringLiteral("( 0:00/00:00:00)");
    
    // mm:ss/HH:MM:SS 形式を解析して ( m:ss/HH:MM:SS) 形式に変換
    const QStringList parts = timeText.split(QLatin1Char('/'));
    if (parts.size() == 2) {
        QString moveTime = parts[0];  // mm:ss
        QString totalTime = parts[1]; // HH:MM:SS
        
        // 分の先頭ゼロを除去（例: "00:00" → " 0:00", "01:30" → " 1:30"）
        if (moveTime.length() >= 2 && moveTime.at(0) == QLatin1Char('0')) {
            moveTime = QStringLiteral(" %1").arg(moveTime.mid(1));
        }
        
        return QStringLiteral("(%1/%2)").arg(moveTime, totalTime);
    }
    
    // 解析できない場合はそのまま括弧で囲む
    return QStringLiteral("( %1)").arg(timeText);
}

// ヘルパ関数: 終局語を判定
static bool isTerminalMove(const QString& move)
{
    static const QStringList terminals = {
        QStringLiteral("投了"),
        QStringLiteral("中断"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("不詰")
    };
    const QString stripped = removeTurnMarker(move);
    for (const QString& t : terminals) {
        if (stripped.contains(t)) return true;
    }
    return false;
}

// ヘルパ関数: 終局結果文字列を生成
static QString buildEndingLine(int lastActualMoveNo, const QString& terminalMove)
{
    const QString stripped = removeTurnMarker(terminalMove);
    // 投了なら直前の手番が勝者
    if (stripped.contains(QStringLiteral("投了"))) {
        // lastActualMoveNo が奇数なら先手の手、偶数なら後手の手が最後
        const bool senteWin = (lastActualMoveNo % 2 != 0);
        return QStringLiteral("まで%1手で%2の勝ち")
            .arg(QString::number(lastActualMoveNo), senteWin ? QStringLiteral("先手") : QStringLiteral("後手"));
    }
    // その他の終局は単純に手数のみ
    return QStringLiteral("まで%1手").arg(QString::number(lastActualMoveNo));
}

// ========================================
// コンストラクタ・デストラクタ
// ========================================

GameRecordModel::GameRecordModel(QObject* parent)
    : QObject(parent)
{
}

// ========================================
// 初期化・バインド
// ========================================

void GameRecordModel::bind(QVector<ResolvedRow>* resolvedRows,
                           int* activeResolvedRow,
                           QList<KifDisplayItem>* liveDisp)
{
    m_resolvedRows = resolvedRows;
    m_activeResolvedRow = activeResolvedRow;
    m_liveDisp = liveDisp;

    qDebug().noquote() << "[GameRecordModel] bind:"
                       << " resolvedRows=" << (resolvedRows ? "valid" : "null")
                       << " activeResolvedRow=" << (activeResolvedRow ? *activeResolvedRow : -1)
                       << " liveDisp=" << (liveDisp ? "valid" : "null");
}

void GameRecordModel::initializeFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount)
{
    m_comments.clear();
    m_comments.resize(qMax(0, rowCount));

    // disp からコメントを抽出
    for (int i = 0; i < disp.size() && i < rowCount; ++i) {
        m_comments[i] = disp[i].comment;
    }

    m_isDirty = false;

    qDebug().noquote() << "[GameRecordModel] initializeFromDisplayItems:"
                       << " disp.size=" << disp.size()
                       << " rowCount=" << rowCount
                       << " m_comments.size=" << m_comments.size();
}

void GameRecordModel::clear()
{
    m_comments.clear();
    m_isDirty = false;

    qDebug().noquote() << "[GameRecordModel] clear";
}

// ========================================
// コメント操作
// ========================================

void GameRecordModel::setComment(int ply, const QString& comment)
{
    if (ply < 0) {
        qWarning().noquote() << "[GameRecordModel] setComment: invalid ply=" << ply;
        return;
    }

    // 1) 内部配列を拡張・更新
    ensureCommentCapacity(ply);
    const QString oldComment = m_comments[ply];
    m_comments[ply] = comment;

    // 2) 外部データストアへ同期
    syncToExternalStores_(ply, comment);

    // 3) 変更フラグを設定
    if (oldComment != comment) {
        m_isDirty = true;
        emit commentChanged(ply, comment);
        emit dataChanged();
    }

    qDebug().noquote() << "[GameRecordModel] setComment:"
                       << " ply=" << ply
                       << " comment.len=" << comment.size()
                       << " isDirty=" << m_isDirty;
}

QString GameRecordModel::comment(int ply) const
{
    if (ply >= 0 && ply < m_comments.size()) {
        return m_comments[ply];
    }
    return QString();
}

void GameRecordModel::ensureCommentCapacity(int ply)
{
    while (m_comments.size() <= ply) {
        m_comments.append(QString());
    }
}

int GameRecordModel::activeRow() const
{
    return m_activeResolvedRow ? *m_activeResolvedRow : 0;
}

// ========================================
// 内部ヘルパ：外部データストアへの同期
// ========================================

void GameRecordModel::syncToExternalStores_(int ply, const QString& comment)
{
    // 1) ResolvedRow への同期
    if (m_resolvedRows && m_activeResolvedRow) {
        const int row = *m_activeResolvedRow;
        if (row >= 0 && row < m_resolvedRows->size()) {
            ResolvedRow& rr = (*m_resolvedRows)[row];

            // ResolvedRow::comments を拡張・更新
            while (rr.comments.size() <= ply) {
                rr.comments.append(QString());
            }
            rr.comments[ply] = comment;

            // ResolvedRow::disp[ply].comment も更新
            if (ply >= 0 && ply < rr.disp.size()) {
                rr.disp[ply].comment = comment;
            }

            qDebug().noquote() << "[GameRecordModel] syncToExternalStores_:"
                               << " updated ResolvedRow[" << row << "]"
                               << " comments[" << ply << "]";
        }
    }

    // 2) liveDisp への同期
    if (m_liveDisp) {
        if (ply >= 0 && ply < m_liveDisp->size()) {
            (*m_liveDisp)[ply].comment = comment;
            qDebug().noquote() << "[GameRecordModel] syncToExternalStores_:"
                               << " updated liveDisp[" << ply << "]";
        }
    }
}

// ========================================
// 棋譜出力
// ========================================

QStringList GameRecordModel::toKifLines(const ExportContext& ctx) const
{
    QStringList out;

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = collectGameInfo_(ctx);
    for (const auto& it : header) {
        if (!it.key.trimmed().isEmpty()) {
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
        }
    }
    if (!out.isEmpty()) {
        out << QString();
    }

    // 2) セパレータ
    out << QStringLiteral("手数----指手---------消費時間--");

    // 3) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport_();

    // 4) 分岐ポイントを収集（本譜のどの手に分岐があるか）
    const QSet<int> branchPoints = collectBranchPoints_();

    // 5) 開始局面の処理（prettyMoveが空のエントリ）
    int startIdx = 0;
    if (!disp.isEmpty() && disp[0].prettyMove.trimmed().isEmpty()) {
        // 開始局面のコメントを先に出力
        const QString cmt = disp[0].comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) {
                    out << t;
                } else {
                    out << (QStringLiteral("*") + t);
                }
            }
        }
        startIdx = 1; // 実際の指し手は次から
    }

    // 6) 各指し手を出力
    int moveNo = 1;
    int lastActualMoveNo = 0;
    QString terminalMove;
    
    for (int i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();
        
        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;
        
        // 終局語の判定
        const bool isTerminal = isTerminalMove(moveText);
        
        // 手番記号を除去してKIF形式に変換
        const QString kifMove = removeTurnMarker(moveText);
        
        // 時間フォーマット（括弧付き）
        const QString time = formatKifTime(it.timeText);
        
        // 分岐マークの判定（この手に分岐があるか）
        const QString branchMark = branchPoints.contains(moveNo) ? QStringLiteral("+") : QString();
        
        // KIF形式で出力: "   手数 指し手   (時間)+"
        const QString moveNoStr = QStringLiteral("%1").arg(moveNo, 4);
        out << QStringLiteral("%1 %2   %3%4")
                   .arg(moveNoStr)
                   .arg(kifMove, -12)  // 左詰め12文字
                   .arg(time)
                   .arg(branchMark);
        
        // コメント出力（指し手の後に）
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) {
                    out << t;
                } else {
                    out << (QStringLiteral("*") + t);
                }
            }
        }
        
        if (isTerminal) {
            terminalMove = moveText;
        } else {
            lastActualMoveNo = moveNo;
        }
        ++moveNo;
    }

    // 7) 終了行（本譜のみ）
    out << QString();
    out << buildEndingLine(lastActualMoveNo, terminalMove);

    // 8) 変化（分岐）を出力
    if (m_resolvedRows && m_resolvedRows->size() > 1) {
        // 本譜（parent < 0）の行インデックスを探す
        int mainRowIndex = -1;
        for (int i = 0; i < m_resolvedRows->size(); ++i) {
            if (m_resolvedRows->at(i).parent < 0) {
                mainRowIndex = i;
                break;
            }
        }
        
        if (mainRowIndex >= 0) {
            QSet<int> visitedRows;
            visitedRows.insert(mainRowIndex); // 本譜は既に出力済み
            outputVariationsRecursively_(mainRowIndex, out, visitedRows);
        }
    }

    qDebug().noquote() << "[GameRecordModel] toKifLines (WITH VARIATIONS): generated"
                       << out.size() << "lines,"
                       << (moveNo - 1) << "moves, lastActualMoveNo=" << lastActualMoveNo
                       << "branchPoints=" << branchPoints.size();

    return out;
}

QList<KifDisplayItem> GameRecordModel::collectMainlineForExport_() const
{
    QList<KifDisplayItem> result;

    // 優先1: ResolvedRows からアクティブ行を取得
    if (m_resolvedRows && !m_resolvedRows->isEmpty() && m_activeResolvedRow) {
        int targetRow = *m_activeResolvedRow;

        // 有効範囲外なら本譜行（parent < 0）を探す
        if (targetRow < 0 || targetRow >= m_resolvedRows->size()) {
            targetRow = -1;
            for (int i = 0; i < m_resolvedRows->size(); ++i) {
                if (m_resolvedRows->at(i).parent < 0) {
                    targetRow = i;
                    break;
                }
            }
            if (targetRow < 0) targetRow = 0;
        }

        const ResolvedRow& rr = m_resolvedRows->at(targetRow);
        result = rr.disp;

        // ResolvedRow::comments からコメントをマージ
        for (int i = 0; i < result.size() && i < rr.comments.size(); ++i) {
            if (!rr.comments[i].isEmpty()) {
                result[i].comment = rr.comments[i];
            }
        }
    }
    // 優先2: liveDisp から取得
    else if (m_liveDisp && !m_liveDisp->isEmpty()) {
        result = *m_liveDisp;
    }

    // 最終: 内部コメント配列 (m_comments) から最新コメントをマージ
    // （これが Single Source of Truth）
    for (int i = 0; i < result.size() && i < m_comments.size(); ++i) {
        if (!m_comments[i].isEmpty()) {
            result[i].comment = m_comments[i];
        }
    }

    return result;
}

QList<KifGameInfoItem> GameRecordModel::collectGameInfo_(const ExportContext& ctx)
{
    QList<KifGameInfoItem> items;

    // a) 既存の「対局情報」テーブルがあれば採用
    if (ctx.gameInfoTable && ctx.gameInfoTable->rowCount() > 0) {
        const int rows = ctx.gameInfoTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            const QTableWidgetItem* keyItem   = ctx.gameInfoTable->item(r, 0);
            const QTableWidgetItem* valueItem = ctx.gameInfoTable->item(r, 1);
            if (!keyItem) continue;
            const QString key = keyItem->text().trimmed();
            const QString val = valueItem ? valueItem->text().trimmed() : QString();
            if (!key.isEmpty()) {
                items.push_back({key, val});
            }
        }
        return items;
    }

    // b) 自動生成
    QString black, white;
    resolvePlayerNames_(ctx, black, white);

    items.push_back({
        QStringLiteral("開始日時"),
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm"))
    });
    items.push_back({ QStringLiteral("先手"), black });
    items.push_back({ QStringLiteral("後手"), white });

    const QString sfen = ctx.startSfen.trimmed();
    const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    QString teai = QStringLiteral("平手");
    if (!sfen.isEmpty()) {
        const QString pp = sfen.section(QLatin1Char(' '), 0, 0);
        if (!pp.isEmpty() && pp != initPP) {
            teai = QStringLiteral("その他");
        }
    }
    items.push_back({ QStringLiteral("手合割"), teai });

    return items;
}

void GameRecordModel::resolvePlayerNames_(const ExportContext& ctx, QString& outBlack, QString& outWhite)
{
    switch (ctx.playMode) {
    case HumanVsHuman:
        outBlack = ctx.human1.isEmpty() ? QObject::tr("先手") : ctx.human1;
        outWhite = ctx.human2.isEmpty() ? QObject::tr("後手") : ctx.human2;
        break;
    case EvenHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    case EvenEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case EvenEngineVsEngine:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine1") : ctx.engine1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine2") : ctx.engine2;
        break;
    case HandicapEngineVsHuman:
        outBlack = ctx.engine1.isEmpty() ? QObject::tr("Engine") : ctx.engine1;
        outWhite = ctx.human2.isEmpty()  ? QObject::tr("後手")   : ctx.human2;
        break;
    case HandicapHumanVsEngine:
        outBlack = ctx.human1.isEmpty()  ? QObject::tr("先手")   : ctx.human1;
        outWhite = ctx.engine2.isEmpty() ? QObject::tr("Engine") : ctx.engine2;
        break;
    default:
        outBlack = QObject::tr("先手");
        outWhite = QObject::tr("後手");
        break;
    }
}

// ========================================
// 分岐出力用ヘルパ
// ========================================

QSet<int> GameRecordModel::collectBranchPoints_() const
{
    QSet<int> result;
    
    if (!m_resolvedRows || m_resolvedRows->isEmpty()) {
        return result;
    }
    
    // 本譜（parent == -1 の行）を探す
    int mainRowIndex = -1;
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        if (m_resolvedRows->at(i).parent < 0) {
            mainRowIndex = i;
            break;
        }
    }
    
    if (mainRowIndex < 0) {
        return result;
    }
    
    // 本譜の子（直接の分岐）と、すべての変化の子を調べる
    // 各変化のstartPlyを記録
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        const ResolvedRow& row = m_resolvedRows->at(i);
        
        // 本譜はスキップ
        if (row.parent < 0) continue;
        
        // この変化が本譜から直接分岐している場合
        if (row.parent == mainRowIndex) {
            result.insert(row.startPly);
        }
    }
    
    // さらに、本譜の各手について、その手から分岐する変化があるかを確認
    // （上記で既に取得済みだが、親子関係が複雑な場合の補助）
    
    return result;
}

void GameRecordModel::outputVariation_(int rowIndex, QStringList& out) const
{
    if (!m_resolvedRows || rowIndex < 0 || rowIndex >= m_resolvedRows->size()) {
        return;
    }
    
    const ResolvedRow& row = m_resolvedRows->at(rowIndex);
    
    // 変化ヘッダを出力
    out << QString();
    out << QStringLiteral("変化：%1手").arg(row.startPly);
    
    // この変化から分岐する子変化のstartPlyを収集
    QSet<int> childBranchPoints;
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        if (m_resolvedRows->at(i).parent == rowIndex) {
            childBranchPoints.insert(m_resolvedRows->at(i).startPly);
        }
    }
    
    // 同じ親・同じstartPlyを持ち、varIndexが自分より大きい兄弟変化があるかチェック
    // （この変化の最初の手に+マークを付けるため - 後に来る変化がある場合のみ）
    bool hasSiblingAfterThis = false;
    const int myVarIndex = row.varIndex;
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        if (i == rowIndex) continue; // 自分自身はスキップ
        const ResolvedRow& other = m_resolvedRows->at(i);
        if (other.parent == row.parent && other.startPly == row.startPly) {
            // 同じ親・同じstartPlyの兄弟変化
            // varIndexが自分より大きい（= 後に出力される）場合のみ+マーク
            if (other.varIndex > myVarIndex) {
                hasSiblingAfterThis = true;
                break;
            }
        }
    }
    
    // 変化の指し手を出力
    // disp[0]=開始局面, disp[i]=i手目 なので、startPly手目から出力開始
    const QList<KifDisplayItem>& disp = row.disp;
    
    int moveNo = row.startPly;
    bool isFirstMove = true;
    
    // dispのstartPly番目の要素から出力（disp[startPly]がstartPly手目）
    for (int i = row.startPly; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();
        
        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;
        
        // 手番記号を除去してKIF形式に変換
        const QString kifMove = removeTurnMarker(moveText);
        
        // 時間フォーマット（括弧付き）
        const QString time = formatKifTime(it.timeText);
        
        // 分岐マークの判定
        // 1. この手に子分岐があるか
        // 2. 最初の手で、後に来る兄弟変化があるか
        bool hasBranch = childBranchPoints.contains(moveNo);
        if (isFirstMove && hasSiblingAfterThis) {
            hasBranch = true;
        }
        const QString branchMark = hasBranch ? QStringLiteral("+") : QString();
        
        // KIF形式で出力
        const QString moveNoStr = QStringLiteral("%1").arg(moveNo, 4);
        out << QStringLiteral("%1 %2   %3%4")
                   .arg(moveNoStr)
                   .arg(kifMove, -12)
                   .arg(time)
                   .arg(branchMark);
        
        // コメント出力
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) {
                    out << t;
                } else {
                    out << (QStringLiteral("*") + t);
                }
            }
        }
        
        ++moveNo;
        isFirstMove = false;
    }
}

void GameRecordModel::outputVariationsRecursively_(int parentRowIndex, QStringList& out, QSet<int>& visitedRows) const
{
    Q_UNUSED(parentRowIndex);
    
    if (!m_resolvedRows) {
        return;
    }
    
    // 元のKIFファイルの順序を維持するため、varIndexの順序で出力する
    // varIndex >= 0 の行を varIndex の昇順で収集
    QVector<QPair<int, int>> variations; // (varIndex, rowIndex)
    
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        const ResolvedRow& row = m_resolvedRows->at(i);
        // 本譜（parent < 0）はスキップ、既に訪問済みもスキップ
        if (row.parent < 0 || visitedRows.contains(i)) {
            continue;
        }
        // varIndex が有効な場合はそれを使用、無効な場合は行インデックスを使用
        int sortKey = (row.varIndex >= 0) ? row.varIndex : (1000 + i);
        variations.append(qMakePair(sortKey, i));
    }
    
    // varIndex（または代替キー）でソート（昇順）
    std::sort(variations.begin(), variations.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.first < b.first;
    });
    
    // 各変化を順番に出力
    for (const auto& var : variations) {
        const int rowIndex = var.second;
        
        if (visitedRows.contains(rowIndex)) {
            continue;
        }
        visitedRows.insert(rowIndex);
        
        // この変化を出力
        outputVariation_(rowIndex, out);
    }
}

// ========================================
// KI2形式出力
// ========================================

QStringList GameRecordModel::toKi2Lines(const ExportContext& ctx) const
{
    QStringList out;

    // 1) ヘッダ
    const QList<KifGameInfoItem> header = collectGameInfo_(ctx);
    for (const auto& it : header) {
        if (!it.key.trimmed().isEmpty()) {
            // 消費時間は KI2 では省略
            if (it.key.trimmed() == QStringLiteral("消費時間")) continue;
            out << fwColonLine(it.key.trimmed(), it.value.trimmed());
        }
    }

    // 2) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport_();

    // 3) 開始局面のコメントを先に出力
    int startIdx = 0;
    if (!disp.isEmpty() && disp[0].prettyMove.trimmed().isEmpty()) {
        // 開始局面のコメントを先に出力
        const QString cmt = disp[0].comment.trimmed();
        if (!cmt.isEmpty()) {
            if (!out.isEmpty()) {
                out << QString();  // 空行を入れる
            }
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                const QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                if (t.startsWith(QLatin1Char('*'))) {
                    out << t;
                } else {
                    out << (QStringLiteral("*") + t);
                }
            }
        }
        startIdx = 1; // 実際の指し手は次から
    }

    // 4) 各指し手を出力（KI2形式）
    int moveNo = 1;
    int lastActualMoveNo = 0;
    QString terminalMove;
    QStringList movesOnLine;  // 現在の行に蓄積する指し手
    bool lastMoveHadComment = false;
    
    for (int i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();
        
        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;
        
        // 終局語の判定
        const bool isTerminal = isTerminalMove(moveText);
        
        // KI2形式: 手番記号は維持、(xx)の移動元は削除
        QString ki2Move = moveText;
        // 移動元情報 (xx) を削除
        static const QRegularExpression fromPosPattern(QStringLiteral("\\([0-9０-９]+[0-9０-９]+\\)$"));
        ki2Move.remove(fromPosPattern);
        
        const QString cmt = it.comment.trimmed();
        const bool hasComment = !cmt.isEmpty();
        
        if (hasComment || lastMoveHadComment || isTerminal) {
            // コメントがある場合、または前の手にコメントがあった場合は、
            // 溜まっている指し手を吐き出してから、この指し手を単独行で出力
            if (!movesOnLine.isEmpty()) {
                out << movesOnLine.join(QStringLiteral("    "));
                movesOnLine.clear();
            }
            out << ki2Move;
            
            // コメント出力
            if (hasComment) {
                const QStringList cmtLines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
                for (const QString& raw : cmtLines) {
                    const QString t = raw.trimmed();
                    if (t.isEmpty()) continue;
                    if (t.startsWith(QLatin1Char('*'))) {
                        out << t;
                    } else {
                        out << (QStringLiteral("*") + t);
                    }
                }
            }
            lastMoveHadComment = hasComment;
        } else {
            // コメントがない場合は指し手を蓄積
            movesOnLine.append(ki2Move);
            lastMoveHadComment = false;
        }
        
        if (isTerminal) {
            terminalMove = moveText;
        } else {
            lastActualMoveNo = moveNo;
        }
        ++moveNo;
    }
    
    // 残りの指し手を出力
    if (!movesOnLine.isEmpty()) {
        out << movesOnLine.join(QStringLiteral("    "));
    }

    // 5) 終了行
    out << buildEndingLine(lastActualMoveNo, terminalMove);

    // 6) 変化（分岐）を出力
    if (m_resolvedRows && m_resolvedRows->size() > 1) {
        // 本譜（parent < 0）の行インデックスを探す
        int mainRowIndex = -1;
        for (int i = 0; i < m_resolvedRows->size(); ++i) {
            if (m_resolvedRows->at(i).parent < 0) {
                mainRowIndex = i;
                break;
            }
        }
        
        if (mainRowIndex >= 0) {
            QSet<int> visitedRows;
            visitedRows.insert(mainRowIndex); // 本譜は既に出力済み
            outputKi2VariationsRecursively_(mainRowIndex, out, visitedRows);
        }
    }

    qDebug().noquote() << "[GameRecordModel] toKi2Lines: generated"
                       << out.size() << "lines,"
                       << lastActualMoveNo << "moves";

    return out;
}

void GameRecordModel::outputKi2Variation_(int rowIndex, QStringList& out) const
{
    if (!m_resolvedRows || rowIndex < 0 || rowIndex >= m_resolvedRows->size()) {
        return;
    }
    
    const ResolvedRow& row = m_resolvedRows->at(rowIndex);
    
    // 変化ヘッダを出力
    out << QString();
    out << QStringLiteral("変化：%1手").arg(row.startPly);
    
    // 変化の指し手を出力
    const QList<KifDisplayItem>& disp = row.disp;
    
    QStringList movesOnLine;
    bool lastMoveHadComment = false;
    
    // dispのstartPly番目の要素から出力
    for (int i = row.startPly; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();
        
        // 空の指し手はスキップ
        if (moveText.isEmpty()) continue;
        
        // KI2形式: 手番記号は維持、(xx)の移動元は削除
        QString ki2Move = moveText;
        static const QRegularExpression fromPosPattern(QStringLiteral("\\([0-9０-９]+[0-9０-９]+\\)$"));
        ki2Move.remove(fromPosPattern);
        
        const QString cmt = it.comment.trimmed();
        const bool hasComment = !cmt.isEmpty();
        const bool isTerminal = isTerminalMove(moveText);
        
        if (hasComment || lastMoveHadComment || isTerminal) {
            if (!movesOnLine.isEmpty()) {
                out << movesOnLine.join(QStringLiteral("    "));
                movesOnLine.clear();
            }
            out << ki2Move;
            
            if (hasComment) {
                const QStringList cmtLines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
                for (const QString& raw : cmtLines) {
                    const QString t = raw.trimmed();
                    if (t.isEmpty()) continue;
                    if (t.startsWith(QLatin1Char('*'))) {
                        out << t;
                    } else {
                        out << (QStringLiteral("*") + t);
                    }
                }
            }
            lastMoveHadComment = hasComment;
        } else {
            movesOnLine.append(ki2Move);
            lastMoveHadComment = false;
        }
    }
    
    // 残りの指し手を出力
    if (!movesOnLine.isEmpty()) {
        out << movesOnLine.join(QStringLiteral("    "));
    }
}

void GameRecordModel::outputKi2VariationsRecursively_(int parentRowIndex, QStringList& out, QSet<int>& visitedRows) const
{
    Q_UNUSED(parentRowIndex);
    
    if (!m_resolvedRows) {
        return;
    }
    
    // varIndexの順序で出力する
    QVector<QPair<int, int>> variations; // (varIndex, rowIndex)
    
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        const ResolvedRow& row = m_resolvedRows->at(i);
        if (row.parent < 0 || visitedRows.contains(i)) {
            continue;
        }
        int sortKey = (row.varIndex >= 0) ? row.varIndex : (1000 + i);
        variations.append(qMakePair(sortKey, i));
    }
    
    std::sort(variations.begin(), variations.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.first < b.first;
    });
    
    for (const auto& var : variations) {
        const int rowIndex = var.second;
        
        if (visitedRows.contains(rowIndex)) {
            continue;
        }
        visitedRows.insert(rowIndex);
        
        outputKi2Variation_(rowIndex, out);
    }
}
