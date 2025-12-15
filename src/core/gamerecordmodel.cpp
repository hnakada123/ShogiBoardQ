#include "gamerecordmodel.h"
#include "kiftosfenconverter.h"  // KifGameInfoItem
#include "kifurecordlistmodel.h"
#include "sfenpositiontracer.h"  // USEN出力用

#include <QTableWidget>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QPair>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

// ========================================
// CSA形式出力
// ========================================

// ヘルパ関数: USI指し手をCSA指し手に変換
static QString usiToCsaMove(const QString& usiMove, bool isSente)
{
    // USI形式: "7g7f" (盤上移動), "7g7f+" (盤上移動成り), "P*5e" (駒打ち)
    // CSA形式: "+7776FU" (盤上移動), "+0055FU" (駒打ち)
    
    const QString sign = isSente ? QStringLiteral("+") : QStringLiteral("-");
    
    if (usiMove.size() < 4) return QString();
    
    // 駒打ちの場合
    if (usiMove.at(1) == QLatin1Char('*')) {
        // P*5e → +0055FU
        const QChar pieceChar = usiMove.at(0).toUpper();
        const QString toSquare = usiMove.mid(2, 2);
        
        // USI座標をCSA座標に変換
        const int toFile = toSquare.at(0).toLatin1() - '0';
        const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
        
        // USI駒文字をCSA駒文字に変換
        QString csaPiece;
        switch (pieceChar.toLatin1()) {
            case 'P': csaPiece = QStringLiteral("FU"); break;
            case 'L': csaPiece = QStringLiteral("KY"); break;
            case 'N': csaPiece = QStringLiteral("KE"); break;
            case 'S': csaPiece = QStringLiteral("GI"); break;
            case 'G': csaPiece = QStringLiteral("KI"); break;
            case 'B': csaPiece = QStringLiteral("KA"); break;
            case 'R': csaPiece = QStringLiteral("HI"); break;
            default: return QString();
        }
        
        return QStringLiteral("%1%2%3%4%5")
            .arg(sign)
            .arg(QStringLiteral("00"))  // 駒台
            .arg(QString::number(toFile))
            .arg(QString::number(toRank))
            .arg(csaPiece);
    }
    
    // 盤上移動の場合
    const QString fromSquare = usiMove.left(2);
    const QString toSquare = usiMove.mid(2, 2);
    const bool isPromotion = usiMove.endsWith(QLatin1Char('+'));
    
    // USI座標をCSA座標に変換
    const int fromFile = fromSquare.at(0).toLatin1() - '0';
    const int fromRank = fromSquare.at(1).toLatin1() - 'a' + 1;
    const int toFile = toSquare.at(0).toLatin1() - '0';
    const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
    
    Q_UNUSED(isPromotion);
    
    return QStringLiteral("%1%2%3%4%5")
        .arg(sign)
        .arg(QString::number(fromFile))
        .arg(QString::number(fromRank))
        .arg(QString::number(toFile))
        .arg(QString::number(toRank));
    // 注: 駒の種類は後で追加する必要がある（prettyMoveから取得）
}

// ヘルパ関数: 指し手表記から駒種を抽出してCSA形式に変換
static QString extractCsaPiece(const QString& prettyMove, bool isPromotion)
{
    // prettyMove: "▲７六歩", "△同銀成", "▲５五角不成" など
    QString move = prettyMove;
    
    // 手番記号を除去
    if (move.startsWith(QStringLiteral("▲")) || move.startsWith(QStringLiteral("△"))) {
        move = move.mid(1);
    }
    
    // 「不成」「成」を除去
    if (move.endsWith(QStringLiteral("不成"))) {
        move.chop(2);
    }
    
    // 駒種を取得（末尾の文字）
    QString piece;
    if (move.endsWith(QStringLiteral("成香"))) { return QStringLiteral("NY"); }
    if (move.endsWith(QStringLiteral("成桂"))) { return QStringLiteral("NK"); }
    if (move.endsWith(QStringLiteral("成銀"))) { return QStringLiteral("NG"); }
    
    const QString lastChar = move.right(1);
    
    if (lastChar == QStringLiteral("歩")) {
        return isPromotion ? QStringLiteral("TO") : QStringLiteral("FU");
    } else if (lastChar == QStringLiteral("香")) {
        return isPromotion ? QStringLiteral("NY") : QStringLiteral("KY");
    } else if (lastChar == QStringLiteral("桂")) {
        return isPromotion ? QStringLiteral("NK") : QStringLiteral("KE");
    } else if (lastChar == QStringLiteral("銀")) {
        return isPromotion ? QStringLiteral("NG") : QStringLiteral("GI");
    } else if (lastChar == QStringLiteral("金")) {
        return QStringLiteral("KI");
    } else if (lastChar == QStringLiteral("角")) {
        return isPromotion ? QStringLiteral("UM") : QStringLiteral("KA");
    } else if (lastChar == QStringLiteral("飛")) {
        return isPromotion ? QStringLiteral("RY") : QStringLiteral("HI");
    } else if (lastChar == QStringLiteral("玉") || lastChar == QStringLiteral("王")) {
        return QStringLiteral("OU");
    } else if (lastChar == QStringLiteral("と")) {
        return QStringLiteral("TO");
    } else if (lastChar == QStringLiteral("馬")) {
        return QStringLiteral("UM");
    } else if (lastChar == QStringLiteral("龍") || lastChar == QStringLiteral("竜")) {
        return QStringLiteral("RY");
    } else if (move.contains(QStringLiteral("打"))) {
        // 駒打ちの場合、「打」の直前の文字が駒種
        const int idx = move.indexOf(QStringLiteral("打"));
        if (idx > 0) {
            return extractCsaPiece(move.left(idx), false);
        }
    }
    
    return QString();
}

// ヘルパ関数: prettyMoveが成りの指し手かどうかを判定
static bool isCsaPromotion(const QString& prettyMove, const QString& usiMove)
{
    // USI形式で+がある場合は成り
    if (usiMove.endsWith(QLatin1Char('+'))) {
        return true;
    }
    // prettyMoveで「成」があり「不成」でない場合
    if (prettyMove.endsWith(QStringLiteral("成")) && !prettyMove.endsWith(QStringLiteral("不成"))) {
        return true;
    }
    return false;
}

// ヘルパ関数: 時間テキストからCSA形式の消費時間（秒）を抽出
static int extractCsaTimeSeconds(const QString& timeText)
{
    // timeText: "mm:ss/HH:MM:SS" または "(mm:ss/HH:MM:SS)"
    QString text = timeText.trimmed();
    
    // 括弧を除去
    if (text.startsWith(QLatin1Char('('))) text = text.mid(1);
    if (text.endsWith(QLatin1Char(')'))) text.chop(1);
    
    // "mm:ss/..." の部分を取得
    const int slashIdx = text.indexOf(QLatin1Char('/'));
    if (slashIdx < 0) return 0;
    
    const QString moveTime = text.left(slashIdx).trimmed();
    const QStringList parts = moveTime.split(QLatin1Char(':'));
    if (parts.size() != 2) return 0;
    
    bool ok1, ok2;
    const int minutes = parts[0].toInt(&ok1);
    const int seconds = parts[1].toInt(&ok2);
    if (!ok1 || !ok2) return 0;
    
    return minutes * 60 + seconds;
}

// ヘルパ関数: CSA形式の終局コードを取得
static QString getCsaResultCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);
    
    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("%TORYO");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("%CHUDAN");
    if (move.contains(QStringLiteral("千日手"))) return QStringLiteral("%SENNICHITE");
    if (move.contains(QStringLiteral("持将棋"))) return QStringLiteral("%JISHOGI");
    if (move.contains(QStringLiteral("切れ負け")) || move.contains(QStringLiteral("時間切れ"))) 
        return QStringLiteral("%TIME_UP");
    if (move.contains(QStringLiteral("反則負け"))) return QStringLiteral("%ILLEGAL_MOVE");
    // V3.0: 反則行為（手番側の勝ちを表現）
    if (move.contains(QStringLiteral("先手の反則"))) return QStringLiteral("%+ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("後手の反則"))) return QStringLiteral("%-ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("入玉勝ち")) || move.contains(QStringLiteral("宣言勝ち"))) 
        return QStringLiteral("%KACHI");
    if (move.contains(QStringLiteral("引き分け"))) return QStringLiteral("%HIKIWAKE");
    // V3.0で追加: 最大手数
    if (move.contains(QStringLiteral("最大手数"))) return QStringLiteral("%MAX_MOVES");
    if (move.contains(QStringLiteral("詰み"))) return QStringLiteral("%TSUMI");
    if (move.contains(QStringLiteral("不詰"))) return QStringLiteral("%FUZUMI");
    if (move.contains(QStringLiteral("エラー"))) return QStringLiteral("%ERROR");
    
    return QString();
}

// CSA出力用の盤面追跡クラス
class CsaBoardTracker {
public:
    // 駒の種類（CSA形式）
    enum PieceType {
        EMPTY = 0,
        FU, KY, KE, GI, KI, KA, HI, OU,  // 基本駒
        TO, NY, NK, NG, UM, RY           // 成駒
    };
    
    struct Square {
        PieceType piece = EMPTY;
        bool isSente = true;
    };
    
    Square board[9][9];  // board[file-1][rank-1], 1-indexed access via at()
    
    CsaBoardTracker() {
        initHirate();
    }
    
    void initHirate() {
        // 盤面クリア
        for (int f = 0; f < 9; ++f) {
            for (int r = 0; r < 9; ++r) {
                board[f][r] = {EMPTY, true};
            }
        }
        // 後手の駒（1段目）
        board[0][0] = {KY, false}; board[1][0] = {KE, false}; board[2][0] = {GI, false};
        board[3][0] = {KI, false}; board[4][0] = {OU, false}; board[5][0] = {KI, false};
        board[6][0] = {GI, false}; board[7][0] = {KE, false}; board[8][0] = {KY, false};
        // 後手の飛車・角（2段目）: 飛車は8筋(file=8)、角は2筋(file=2)
        board[7][1] = {HI, false}; board[1][1] = {KA, false};
        // 後手の歩（3段目）
        for (int f = 0; f < 9; ++f) board[f][2] = {FU, false};
        // 先手の歩（7段目）
        for (int f = 0; f < 9; ++f) board[f][6] = {FU, true};
        // 先手の飛車・角（8段目）: 飛車は2筋(file=2)、角は8筋(file=8)
        board[1][7] = {HI, true}; board[7][7] = {KA, true};
        // 先手の駒（9段目）
        board[0][8] = {KY, true}; board[1][8] = {KE, true}; board[2][8] = {GI, true};
        board[3][8] = {KI, true}; board[4][8] = {OU, true}; board[5][8] = {KI, true};
        board[6][8] = {GI, true}; board[7][8] = {KE, true}; board[8][8] = {KY, true};
    }
    
    Square& at(int file, int rank) {
        return board[file - 1][rank - 1];
    }
    
    const Square& at(int file, int rank) const {
        return board[file - 1][rank - 1];
    }
    
    static QString pieceToCSA(PieceType p) {
        switch (p) {
            case FU: return QStringLiteral("FU");
            case KY: return QStringLiteral("KY");
            case KE: return QStringLiteral("KE");
            case GI: return QStringLiteral("GI");
            case KI: return QStringLiteral("KI");
            case KA: return QStringLiteral("KA");
            case HI: return QStringLiteral("HI");
            case OU: return QStringLiteral("OU");
            case TO: return QStringLiteral("TO");
            case NY: return QStringLiteral("NY");
            case NK: return QStringLiteral("NK");
            case NG: return QStringLiteral("NG");
            case UM: return QStringLiteral("UM");
            case RY: return QStringLiteral("RY");
            default: return QString();
        }
    }
    
    static PieceType charToPiece(QChar c) {
        switch (c.toUpper().toLatin1()) {
            case 'P': return FU;
            case 'L': return KY;
            case 'N': return KE;
            case 'S': return GI;
            case 'G': return KI;
            case 'B': return KA;
            case 'R': return HI;
            case 'K': return OU;
            default: return EMPTY;
        }
    }
    
    static PieceType promote(PieceType p) {
        switch (p) {
            case FU: return TO;
            case KY: return NY;
            case KE: return NK;
            case GI: return NG;
            case KA: return UM;
            case HI: return RY;
            default: return p;
        }
    }
    
    // USI形式の指し手を適用し、CSA形式の駒種を返す
    QString applyMove(const QString& usiMove, bool isSente) {
        if (usiMove.size() < 4) return QString();
        
        // 駒打ちの場合
        if (usiMove.at(1) == QLatin1Char('*')) {
            PieceType piece = charToPiece(usiMove.at(0));
            int toFile = usiMove.at(2).toLatin1() - '0';
            int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
            
            if (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9) {
                at(toFile, toRank) = {piece, isSente};
            }
            return pieceToCSA(piece);
        }
        
        // 盤上移動の場合
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        bool isPromo = usiMove.endsWith(QLatin1Char('+'));
        
        if (fromFile < 1 || fromFile > 9 || fromRank < 1 || fromRank > 9 ||
            toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            return QString();
        }
        
        PieceType piece = at(fromFile, fromRank).piece;
        if (isPromo) {
            piece = promote(piece);
        }
        
        // 盤面を更新
        at(toFile, toRank) = {piece, isSente};
        at(fromFile, fromRank) = {EMPTY, true};
        
        return pieceToCSA(piece);
    }
};

QStringList GameRecordModel::toCsaLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    QStringList out;
    
    // ★★★ デバッグ: 入力データの確認 ★★★
    qDebug().noquote() << "[toCsaLines] ========== CSA出力開始 ==========";
    qDebug().noquote() << "[toCsaLines] usiMoves.size() =" << usiMoves.size();
    if (!usiMoves.isEmpty()) {
        qDebug().noquote() << "[toCsaLines] usiMoves[0..min(5,size)] ="
                           << usiMoves.mid(0, qMin(5, usiMoves.size()));
    }
    
    // 盤面追跡用オブジェクト
    CsaBoardTracker boardTracker;
    
    // 1) 文字コード宣言 (V3.0で追加)
    out << QStringLiteral("'CSA encoding=UTF-8");
    
    // 2) バージョン
    out << QStringLiteral("V3.0");
    
    // 3) 棋譜情報を先に収集（対局者名もここから取得）
    const QList<KifGameInfoItem> header = collectGameInfo_(ctx);
    
    // 4) 対局者名
    // まず棋譜情報から対局者名を探す（CSAファイル読み込み時はここに保存される）
    QString blackPlayer, whitePlayer;
    for (const auto& it : header) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key == QStringLiteral("先手") && !val.isEmpty()) {
            blackPlayer = val;
        } else if (key == QStringLiteral("後手") && !val.isEmpty()) {
            whitePlayer = val;
        }
    }
    // 棋譜情報になければresolvePlayerNames_から取得
    if (blackPlayer.isEmpty() || whitePlayer.isEmpty()) {
        QString resolvedBlack, resolvedWhite;
        resolvePlayerNames_(ctx, resolvedBlack, resolvedWhite);
        if (blackPlayer.isEmpty()) blackPlayer = resolvedBlack;
        if (whitePlayer.isEmpty()) whitePlayer = resolvedWhite;
    }
    if (!blackPlayer.isEmpty()) {
        out << QStringLiteral("N+%1").arg(blackPlayer);
    }
    if (!whitePlayer.isEmpty()) {
        out << QStringLiteral("N-%1").arg(whitePlayer);
    }
    
    // 5) 棋譜情報（対局者名以外）
    for (const auto& it : header) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key.isEmpty() || val.isEmpty()) continue;
        
        // 対局者名は既に出力済みなのでスキップ
        if (key == QStringLiteral("先手") || key == QStringLiteral("後手")) {
            continue;
        }
        
        if (key == QStringLiteral("棋戦")) {
            out << QStringLiteral("$EVENT:%1").arg(val);
        } else if (key == QStringLiteral("場所")) {
            out << QStringLiteral("$SITE:%1").arg(val);
        } else if (key == QStringLiteral("開始日時")) {
            out << QStringLiteral("$START_TIME:%1").arg(val);
        } else if (key == QStringLiteral("終了日時")) {
            out << QStringLiteral("$END_TIME:%1").arg(val);
        } else if (key == QStringLiteral("持ち時間")) {
            // V3.0形式: $TIME:秒+秒読み+フィッシャー
            // V2.2形式: $TIME_LIMIT:HH:MM+SS
            // 入力値の形式を判定して適切な形式で出力
            // 現状はV2.2形式の入力が多いため、$TIME_LIMITとして出力
            // （V3.0リーダーは両方読めることが期待される）
            out << QStringLiteral("$TIME_LIMIT:%1").arg(val);
        } else if (key == QStringLiteral("戦型")) {
            out << QStringLiteral("$OPENING:%1").arg(val);
        } else if (key == QStringLiteral("最大手数")) {
            // V3.0で追加
            out << QStringLiteral("$MAX_MOVES:%1").arg(val);
        } else if (key == QStringLiteral("持将棋")) {
            // V3.0で追加
            out << QStringLiteral("$JISHOGI:%1").arg(val);
        } else if (key == QStringLiteral("備考")) {
            // V3.0で追加: \は\\に、改行は\nに変換
            QString noteVal = val;
            noteVal.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));  // 先に\をエスケープ
            noteVal.replace(QStringLiteral("\n"), QStringLiteral("\\n"));   // 次に改行を変換
            out << QStringLiteral("$NOTE:%1").arg(noteVal);
        }
    }
    
    // 6) 開始局面
    // startSfenがデフォルト（平手）かどうかを判定
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const bool isHirate = ctx.startSfen.isEmpty() || ctx.startSfen == defaultSfen 
                          || ctx.startSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
    
    if (isHirate) {
        // 平手初期配置
        out << QStringLiteral("P1-KY-KE-GI-KI-OU-KI-GI-KE-KY");
        out << QStringLiteral("P2 * -HI *  *  *  *  * -KA * ");
        out << QStringLiteral("P3-FU-FU-FU-FU-FU-FU-FU-FU-FU");
        out << QStringLiteral("P4 *  *  *  *  *  *  *  *  * ");
        out << QStringLiteral("P5 *  *  *  *  *  *  *  *  * ");
        out << QStringLiteral("P6 *  *  *  *  *  *  *  *  * ");
        out << QStringLiteral("P7+FU+FU+FU+FU+FU+FU+FU+FU+FU");
        out << QStringLiteral("P8 * +KA *  *  *  *  * +HI * ");
        out << QStringLiteral("P9+KY+KE+GI+KI+OU+KI+GI+KE+KY");
        out << QStringLiteral("+");  // 先手番
    } else {
        // 任意の開始局面（SFEN形式から変換）
        // TODO: SFEN→CSA局面変換の実装（複雑なため省略、平手のみ対応）
        out << QStringLiteral("PI");  // フォールバック: 平手
        out << QStringLiteral("+");
    }
    
    // 7) 本譜の指し手を収集
    const QList<KifDisplayItem> disp = collectMainlineForExport_();
    
    // ★★★ デバッグ: disp の確認 ★★★
    qDebug().noquote() << "[toCsaLines] disp.size() =" << disp.size();
    for (int dbgIdx = 0; dbgIdx < qMin(10, disp.size()); ++dbgIdx) {
        qDebug().noquote() << "[toCsaLines] disp[" << dbgIdx << "].prettyMove ="
                           << disp[dbgIdx].prettyMove;
    }
    
    // 開始局面のコメントを出力
    int startIdx = 0;
    if (!disp.isEmpty() && disp[0].prettyMove.trimmed().isEmpty()) {
        const QString cmt = disp[0].comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                // CSA読み込み時にnormalizeCsaCommentLine_で付加された「• 」を除去
                // （内部表示用の箇条書き記号をCSA出力時には元の形式に戻す）
                if (t.startsWith(QStringLiteral("• "))) {
                    t = t.mid(2);
                }
                if (t.startsWith(QLatin1Char('\''))) {
                    out << t;
                } else {
                    out << (QStringLiteral("'*") + t);
                }
            }
        }
        startIdx = 1;
        qDebug().noquote() << "[toCsaLines] 開始局面エントリあり、startIdx = 1";
    }
    
    // ★★★ デバッグ: ループ開始前 ★★★
    qDebug().noquote() << "[toCsaLines] startIdx =" << startIdx
                       << ", disp.size() =" << disp.size()
                       << ", ループ回数予定 =" << (disp.size() - startIdx);
    
    // 6) 各指し手を出力
    int moveNo = 1;
    bool isSente = true;  // 先手から開始
    QString terminalMove;
    int processedMoves = 0;
    int skippedEmpty = 0;
    int skippedNoUsi = 0;
    
    for (int i = startIdx; i < disp.size(); ++i) {
        const auto& it = disp[i];
        const QString moveText = it.prettyMove.trimmed();
        
        if (moveText.isEmpty()) {
            ++skippedEmpty;
            continue;
        }
        
        // 終局語の判定
        if (isTerminalMove(moveText)) {
            qDebug().noquote() << "[toCsaLines] 終局語検出: moveText =" << moveText;
            terminalMove = moveText;
            const QString resultCode = getCsaResultCode(moveText);
            if (!resultCode.isEmpty()) {
                // 消費時間を追加
                const int timeSec = extractCsaTimeSeconds(it.timeText);
                if (timeSec > 0) {
                    out << QStringLiteral("%1,T%2").arg(resultCode).arg(timeSec);
                } else {
                    out << resultCode;
                }
            }
            break;
        }
        
        // USI指し手を取得（インデックスは moveNo-1）
        QString usiMove;
        if (moveNo - 1 < usiMoves.size()) {
            usiMove = usiMoves.at(moveNo - 1);
        }
        
        // ★★★ デバッグ: 各指し手の詳細 ★★★
        if (processedMoves < 5 || usiMove.isEmpty()) {
            qDebug().noquote() << "[toCsaLines] i=" << i
                               << " moveNo=" << moveNo
                               << " moveText=" << moveText
                               << " usiMove=" << usiMove
                               << " (usiMoves.size=" << usiMoves.size() << ")";
        }
        
        // CSA形式の指し手を構築
        QString csaMove;
        const QString sign = isSente ? QStringLiteral("+") : QStringLiteral("-");
        
        if (!usiMove.isEmpty()) {
            // 駒打ちの場合
            if (usiMove.size() >= 4 && usiMove.at(1) == QLatin1Char('*')) {
                const QChar pieceChar = usiMove.at(0).toUpper();
                const QString toSquare = usiMove.mid(2, 2);
                
                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
                
                // 盤面追跡で駒種を取得（盤面も更新される）
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);
                
                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5")
                        .arg(sign)
                        .arg(QStringLiteral("00"))
                        .arg(QString::number(toFile))
                        .arg(QString::number(toRank))
                        .arg(csaPiece);
                }
            } else if (usiMove.size() >= 4) {
                // 盤上移動の場合
                const QString fromSquare = usiMove.left(2);
                const QString toSquare = usiMove.mid(2, 2);
                
                const int fromFile = fromSquare.at(0).toLatin1() - '0';
                const int fromRank = fromSquare.at(1).toLatin1() - 'a' + 1;
                const int toFile = toSquare.at(0).toLatin1() - '0';
                const int toRank = toSquare.at(1).toLatin1() - 'a' + 1;
                
                // 盤面追跡で駒種を取得（成りも考慮、盤面も更新される）
                const QString csaPiece = boardTracker.applyMove(usiMove, isSente);
                
                if (!csaPiece.isEmpty()) {
                    csaMove = QStringLiteral("%1%2%3%4%5%6")
                        .arg(sign)
                        .arg(QString::number(fromFile))
                        .arg(QString::number(fromRank))
                        .arg(QString::number(toFile))
                        .arg(QString::number(toRank))
                        .arg(csaPiece);
                }
            }
        }
        
        if (csaMove.isEmpty()) {
            // USI指し手がない場合はスキップ
            ++skippedNoUsi;
            if (skippedNoUsi <= 5) {
                qDebug().noquote() << "[toCsaLines] スキップ(USIなし): i=" << i
                                   << " moveNo=" << moveNo
                                   << " moveText=" << moveText;
            }
            ++moveNo;
            isSente = !isSente;
            continue;
        }
        
        // 消費時間を追加（常に出力、0秒でもT0を付加）
        const int timeSec = extractCsaTimeSeconds(it.timeText);
        csaMove += QStringLiteral(",T%1").arg(timeSec);
        
        out << csaMove;
        ++processedMoves;
        
        // コメント出力
        const QString cmt = it.comment.trimmed();
        if (!cmt.isEmpty()) {
            const QStringList lines = cmt.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
            for (const QString& raw : lines) {
                QString t = raw.trimmed();
                if (t.isEmpty()) continue;
                // CSA読み込み時にnormalizeCsaCommentLine_で付加された「• 」を除去
                // （内部表示用の箇条書き記号をCSA出力時には元の形式に戻す）
                if (t.startsWith(QStringLiteral("• "))) {
                    t = t.mid(2);
                }
                if (t.startsWith(QLatin1Char('\''))) {
                    out << t;
                } else {
                    out << (QStringLiteral("'*") + t);
                }
            }
        }
        
        ++moveNo;
        isSente = !isSente;
    }
    
    // ★★★ デバッグ: 結果サマリ ★★★
    qDebug().noquote() << "[toCsaLines] ========== CSA出力終了 ==========";
    qDebug().noquote() << "[toCsaLines] processedMoves =" << processedMoves
                       << ", skippedEmpty =" << skippedEmpty
                       << ", skippedNoUsi =" << skippedNoUsi;
    qDebug().noquote() << "[GameRecordModel] toCsaLines: generated"
                       << out.size() << "lines,"
                       << (moveNo - 1) << "moves";
    
    return out;
}

// ========================================
// JKF形式出力
// ========================================

// ヘルパ関数: 日本語駒名からCSA形式駒種に変換
static QString kanjiToCsaPiece(const QString& kanji)
{
    if (kanji.contains(QStringLiteral("歩"))) return QStringLiteral("FU");
    if (kanji.contains(QStringLiteral("香"))) return QStringLiteral("KY");
    if (kanji.contains(QStringLiteral("桂"))) return QStringLiteral("KE");
    if (kanji.contains(QStringLiteral("銀"))) return QStringLiteral("GI");
    if (kanji.contains(QStringLiteral("金"))) return QStringLiteral("KI");
    if (kanji.contains(QStringLiteral("角"))) return QStringLiteral("KA");
    if (kanji.contains(QStringLiteral("飛"))) return QStringLiteral("HI");
    if (kanji.contains(QStringLiteral("玉")) || kanji.contains(QStringLiteral("王"))) return QStringLiteral("OU");
    if (kanji.contains(QStringLiteral("と"))) return QStringLiteral("TO");
    if (kanji.contains(QStringLiteral("成香"))) return QStringLiteral("NY");
    if (kanji.contains(QStringLiteral("成桂"))) return QStringLiteral("NK");
    if (kanji.contains(QStringLiteral("成銀"))) return QStringLiteral("NG");
    if (kanji.contains(QStringLiteral("馬"))) return QStringLiteral("UM");
    if (kanji.contains(QStringLiteral("龍")) || kanji.contains(QStringLiteral("竜"))) return QStringLiteral("RY");
    return QString();
}

// ヘルパ関数: 全角数字を半角数字に変換
static int zenkakuToNumber(QChar c)
{
    static const QString zenkaku = QStringLiteral("０１２３４５６７８９");
    int idx = zenkaku.indexOf(c);
    if (idx >= 0) return idx;
    if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

// ヘルパ関数: 漢数字を半角数字に変換
static int kanjiToNumber(QChar c)
{
    static const QString kanji = QStringLiteral("〇一二三四五六七八九");
    int idx = kanji.indexOf(c);
    if (idx >= 0) return idx;
    if (c >= QLatin1Char('1') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

// ヘルパ関数: 終局語から JKF special 文字列に変換
static QString japaneseToJkfSpecial(const QString& japanese)
{
    if (japanese.contains(QStringLiteral("投了"))) return QStringLiteral("TORYO");
    if (japanese.contains(QStringLiteral("中断"))) return QStringLiteral("CHUDAN");
    if (japanese.contains(QStringLiteral("王手千日手"))) return QStringLiteral("OUTE_SENNICHITE");
    if (japanese.contains(QStringLiteral("千日手"))) return QStringLiteral("SENNICHITE");
    if (japanese.contains(QStringLiteral("持将棋"))) return QStringLiteral("JISHOGI");
    if (japanese.contains(QStringLiteral("切れ負け"))) return QStringLiteral("TIME_UP");
    if (japanese.contains(QStringLiteral("反則負け")) || japanese.contains(QStringLiteral("反則勝ち"))) return QStringLiteral("ILLEGAL_ACTION");
    if (japanese.contains(QStringLiteral("入玉勝ち"))) return QStringLiteral("KACHI");
    if (japanese.contains(QStringLiteral("引き分け"))) return QStringLiteral("HIKIWAKE");
    if (japanese.contains(QStringLiteral("詰み"))) return QStringLiteral("TSUMI");
    if (japanese.contains(QStringLiteral("不詰"))) return QStringLiteral("FUZUMI");
    return QString();
}

// ヘルパ関数: time文字列（mm:ss/HH:MM:SS）をJKFの time オブジェクトに変換
static QJsonObject parseTimeToJkf(const QString& timeText)
{
    QJsonObject result;
    
    if (timeText.isEmpty()) return result;
    
    // mm:ss/HH:MM:SS 形式をパース
    QString text = timeText;
    // 括弧を除去
    text.remove(QLatin1Char('('));
    text.remove(QLatin1Char(')'));
    text = text.trimmed();
    
    const QStringList parts = text.split(QLatin1Char('/'));
    if (parts.size() >= 1) {
        // 1手の消費時間
        const QStringList nowParts = parts[0].split(QLatin1Char(':'));
        if (nowParts.size() >= 2) {
            QJsonObject now;
            now[QStringLiteral("m")] = nowParts[0].trimmed().toInt();
            now[QStringLiteral("s")] = nowParts[1].trimmed().toInt();
            result[QStringLiteral("now")] = now;
        }
    }
    if (parts.size() >= 2) {
        // 累計消費時間
        const QStringList totalParts = parts[1].split(QLatin1Char(':'));
        if (totalParts.size() >= 3) {
            QJsonObject total;
            total[QStringLiteral("h")] = totalParts[0].trimmed().toInt();
            total[QStringLiteral("m")] = totalParts[1].trimmed().toInt();
            total[QStringLiteral("s")] = totalParts[2].trimmed().toInt();
            result[QStringLiteral("total")] = total;
        }
    }
    
    return result;
}

// ヘルパ関数: preset名からSFEN文字列を判定
static QString sfenToJkfPreset(const QString& sfen)
{
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString hiratePos = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    
    if (sfen.isEmpty() || sfen == defaultSfen || sfen.startsWith(hiratePos)) {
        return QStringLiteral("HIRATE");
    }
    
    // 各種駒落ちの判定（後手番から開始）
    struct PresetDef { const char* preset; const char* pos; };
    static const PresetDef presets[] = {
        {"KY",    "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},   // 香落ち
        {"KY_R",  "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},   // 右香落ち
        {"KA",    "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 角落ち
        {"HI",    "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 飛車落ち
        {"HIKY",  "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},     // 飛香落ち
        {"2",     "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 二枚落ち
        {"3",     "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 三枚落ち
        {"4",     "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},       // 四枚落ち
        {"5",     "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},        // 五枚落ち
        {"5_L",   "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},        // 左五枚落ち
        {"6",     "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},         // 六枚落ち
        {"7_L",   "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},          // 左七枚落ち
        {"7_R",   "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},          // 右七枚落ち
        {"8",     "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},           // 八枚落ち
        {"10",    "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},             // 十枚落ち
    };
    
    const QString boardPart = sfen.section(QLatin1Char(' '), 0, 0);
    for (const auto& p : presets) {
        if (boardPart == QString::fromUtf8(p.pos)) {
            return QString::fromUtf8(p.preset);
        }
    }
    
    return QString(); // カスタム局面
}

// ヘルパ関数: SFEN駒文字からCSA駒種に変換
static QString sfenPieceToJkfKind(QChar c, bool promoted)
{
    QChar upper = c.toUpper();
    QString base;
    if (upper == QLatin1Char('P')) base = QStringLiteral("FU");
    else if (upper == QLatin1Char('L')) base = QStringLiteral("KY");
    else if (upper == QLatin1Char('N')) base = QStringLiteral("KE");
    else if (upper == QLatin1Char('S')) base = QStringLiteral("GI");
    else if (upper == QLatin1Char('G')) base = QStringLiteral("KI");
    else if (upper == QLatin1Char('B')) base = QStringLiteral("KA");
    else if (upper == QLatin1Char('R')) base = QStringLiteral("HI");
    else if (upper == QLatin1Char('K')) base = QStringLiteral("OU");
    else return QString();
    
    if (promoted) {
        if (base == QStringLiteral("FU")) return QStringLiteral("TO");
        if (base == QStringLiteral("KY")) return QStringLiteral("NY");
        if (base == QStringLiteral("KE")) return QStringLiteral("NK");
        if (base == QStringLiteral("GI")) return QStringLiteral("NG");
        if (base == QStringLiteral("KA")) return QStringLiteral("UM");
        if (base == QStringLiteral("HI")) return QStringLiteral("RY");
    }
    return base;
}

// ヘルパ関数: SFENからJKFのdataオブジェクトを構築
static QJsonObject sfenToJkfData(const QString& sfen)
{
    QJsonObject data;
    
    if (sfen.isEmpty()) return data;
    
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() < 3) return data;
    
    const QString boardStr = parts[0];
    const QString turnStr = parts[1];
    const QString handsStr = parts[2];
    
    // 手番 (0=先手, 1=後手)
    data[QStringLiteral("color")] = (turnStr == QStringLiteral("b")) ? 0 : 1;
    
    // 盤面を解析
    // SFEN: 9筋から1筋へ、1段から9段へ
    // JKF board: board[x-1][y-1] が (x,y) のマス
    // board[0] は1筋, board[0][0] は (1,1)
    QJsonArray board;
    for (int x = 0; x < 9; ++x) {
        QJsonArray col;
        for (int y = 0; y < 9; ++y) {
            col.append(QJsonObject()); // 空マス
        }
        board.append(col);
    }
    
    const QStringList rows = boardStr.split(QLatin1Char('/'));
    for (int y = 0; y < qMin(9, rows.size()); ++y) {
        const QString& rowStr = rows[y];
        int x = 8; // SFEN は9筋から開始
        bool promoted = false;
        
        for (int i = 0; i < rowStr.size() && x >= 0; ++i) {
            QChar c = rowStr.at(i);
            
            if (c == QLatin1Char('+')) {
                promoted = true;
                continue;
            }
            
            if (c.isDigit()) {
                x -= c.digitValue();
                promoted = false;
            } else {
                // 駒
                QJsonObject piece;
                piece[QStringLiteral("color")] = c.isUpper() ? 0 : 1;
                piece[QStringLiteral("kind")] = sfenPieceToJkfKind(c, promoted);
                
                // board[x] の y 位置に設定
                QJsonArray col = board[x].toArray();
                col[y] = piece;
                board[x] = col;
                
                --x;
                promoted = false;
            }
        }
    }
    data[QStringLiteral("board")] = board;
    
    // 持駒を解析
    // SFEN hands: 先手の駒（大文字）と後手の駒（小文字）が混在
    // JKF hands: hands[0]=先手, hands[1]=後手
    QJsonObject blackHands;
    QJsonObject whiteHands;
    
    if (handsStr != QStringLiteral("-")) {
        int count = 0;
        for (int i = 0; i < handsStr.size(); ++i) {
            QChar c = handsStr.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                QString kind = sfenPieceToJkfKind(c, false);
                if (!kind.isEmpty()) {
                    if (c.isUpper()) {
                        blackHands[kind] = blackHands[kind].toInt() + count;
                    } else {
                        whiteHands[kind] = whiteHands[kind].toInt() + count;
                    }
                }
                count = 0;
            }
        }
    }
    
    QJsonArray hands;
    hands.append(blackHands);
    hands.append(whiteHands);
    data[QStringLiteral("hands")] = hands;
    
    return data;
}

QJsonObject GameRecordModel::buildJkfHeader_(const ExportContext& ctx) const
{
    QJsonObject header;
    
    const QList<KifGameInfoItem> items = collectGameInfo_(ctx);
    for (const auto& item : items) {
        const QString key = item.key.trimmed();
        const QString val = item.value.trimmed();
        if (!key.isEmpty()) {
            header[key] = val;
        }
    }
    
    return header;
}

QJsonObject GameRecordModel::buildJkfInitial_(const ExportContext& ctx) const
{
    QJsonObject initial;
    
    const QString preset = sfenToJkfPreset(ctx.startSfen);
    if (!preset.isEmpty()) {
        initial[QStringLiteral("preset")] = preset;
    } else {
        // カスタム初期局面の場合
        initial[QStringLiteral("preset")] = QStringLiteral("OTHER");
        
        // SFEN から data オブジェクトを構築
        const QJsonObject data = sfenToJkfData(ctx.startSfen);
        if (!data.isEmpty()) {
            initial[QStringLiteral("data")] = data;
        }
    }
    
    return initial;
}

QJsonObject GameRecordModel::convertMoveToJkf_(const KifDisplayItem& disp, int& prevToX, int& prevToY, int ply) const
{
    QJsonObject result;
    
    QString moveText = disp.prettyMove.trimmed();
    if (moveText.isEmpty()) return result;
    
    // 手番記号を除去して判定
    const bool isSente = moveText.startsWith(QStringLiteral("▲"));
    if (moveText.startsWith(QStringLiteral("▲")) || moveText.startsWith(QStringLiteral("△"))) {
        moveText = moveText.mid(1);
    }
    
    // 終局語の判定
    const QString special = japaneseToJkfSpecial(moveText);
    if (!special.isEmpty()) {
        result[QStringLiteral("special")] = special;
        return result;
    }
    
    // move オブジェクトを構築
    QJsonObject move;
    move[QStringLiteral("color")] = isSente ? 0 : 1;
    
    // 移動先の解析
    int toX = 0, toY = 0;
    bool isSame = false;
    int parsePos = 0;
    
    if (moveText.startsWith(QStringLiteral("同"))) {
        isSame = true;
        toX = prevToX;
        toY = prevToY;
        parsePos = 1;
        // 「同　」のような全角スペースをスキップ
        while (parsePos < moveText.size() && (moveText.at(parsePos).isSpace() || moveText.at(parsePos) == QChar(0x3000))) {
            ++parsePos;
        }
    } else {
        // 全角数字＋漢数字
        if (moveText.size() >= 2) {
            toX = zenkakuToNumber(moveText.at(0));
            toY = kanjiToNumber(moveText.at(1));
            parsePos = 2;
        }
    }
    
    if (toX > 0 && toY > 0) {
        QJsonObject to;
        to[QStringLiteral("x")] = toX;
        to[QStringLiteral("y")] = toY;
        move[QStringLiteral("to")] = to;
        
        if (isSame) {
            move[QStringLiteral("same")] = true;
        }
        
        prevToX = toX;
        prevToY = toY;
    }
    
    // 駒種の解析
    QString remaining = moveText.mid(parsePos);
    QString pieceStr;
    
    // 駒名を抽出（成香、成桂、成銀は2文字）
    if (remaining.startsWith(QStringLiteral("成香")) || 
        remaining.startsWith(QStringLiteral("成桂")) ||
        remaining.startsWith(QStringLiteral("成銀"))) {
        pieceStr = remaining.left(2);
        remaining = remaining.mid(2);
    } else if (!remaining.isEmpty()) {
        pieceStr = remaining.left(1);
        remaining = remaining.mid(1);
    }
    
    const QString csaPiece = kanjiToCsaPiece(pieceStr);
    if (!csaPiece.isEmpty()) {
        move[QStringLiteral("piece")] = csaPiece;
    }
    
    // 修飾語と移動元の解析
    QString relative;
    bool isDrop = false;
    bool isPromote = false;
    bool isNonPromote = false;
    int fromX = 0, fromY = 0;
    
    // 修飾語（左右上引寄直打）
    while (!remaining.isEmpty()) {
        QChar c = remaining.at(0);
        if (c == QChar(0x5DE6)) { // 左
            relative += QLatin1Char('L');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x53F3)) { // 右
            relative += QLatin1Char('R');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x4E0A)) { // 上
            relative += QLatin1Char('U');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5F15)) { // 引
            relative += QLatin1Char('D');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5BC4)) { // 寄
            relative += QLatin1Char('M');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x76F4)) { // 直
            relative += QLatin1Char('C');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x6253)) { // 打
            isDrop = true;
            relative += QLatin1Char('H');
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("成"))) {
            isPromote = true;
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("不成"))) {
            isNonPromote = true;
            remaining = remaining.mid(2);
        } else if (c == QLatin1Char('(')) {
            // 移動元 (xy) を解析
            const int closePos = remaining.indexOf(QLatin1Char(')'));
            if (closePos > 1) {
                const QString fromStr = remaining.mid(1, closePos - 1);
                if (fromStr.size() >= 2) {
                    fromX = fromStr.at(0).digitValue();
                    if (fromX < 0) fromX = zenkakuToNumber(fromStr.at(0));
                    fromY = fromStr.at(1).digitValue();
                    if (fromY < 0) fromY = zenkakuToNumber(fromStr.at(1));
                }
            }
            break;
        } else {
            break;
        }
    }
    
    if (!relative.isEmpty()) {
        move[QStringLiteral("relative")] = relative;
    }
    
    if (fromX > 0 && fromY > 0 && !isDrop) {
        QJsonObject from;
        from[QStringLiteral("x")] = fromX;
        from[QStringLiteral("y")] = fromY;
        move[QStringLiteral("from")] = from;
    }
    
    if (isPromote) {
        move[QStringLiteral("promote")] = true;
    } else if (isNonPromote) {
        move[QStringLiteral("promote")] = false;
    }
    
    result[QStringLiteral("move")] = move;
    
    // 時間情報
    const QJsonObject timeObj = parseTimeToJkf(disp.timeText);
    if (!timeObj.isEmpty()) {
        result[QStringLiteral("time")] = timeObj;
    }
    
    // コメント
    if (!disp.comment.isEmpty()) {
        QJsonArray comments;
        const QStringList lines = disp.comment.split(QRegularExpression(QStringLiteral("\r?\n")));
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            // KIF形式の*プレフィックスを除去
            if (trimmed.startsWith(QLatin1Char('*'))) {
                comments.append(trimmed.mid(1));
            } else if (!trimmed.isEmpty()) {
                comments.append(trimmed);
            }
        }
        if (!comments.isEmpty()) {
            result[QStringLiteral("comments")] = comments;
        }
    }
    
    return result;
}

QJsonArray GameRecordModel::buildJkfMoves_(const QList<KifDisplayItem>& disp) const
{
    QJsonArray moves;
    
    int prevToX = 0, prevToY = 0;
    
    for (int i = 0; i < disp.size(); ++i) {
        const auto& item = disp[i];
        
        if (i == 0 && item.prettyMove.trimmed().isEmpty()) {
            // 開始局面のコメント
            QJsonObject openingMove;
            if (!item.comment.isEmpty()) {
                QJsonArray comments;
                const QStringList lines = item.comment.split(QRegularExpression(QStringLiteral("\r?\n")));
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
            const QJsonObject moveObj = convertMoveToJkf_(item, prevToX, prevToY, i);
            if (!moveObj.isEmpty()) {
                moves.append(moveObj);
            }
        }
    }
    
    return moves;
}

void GameRecordModel::addJkfForks_(QJsonArray& movesArray, int mainRowIndex) const
{
    if (!m_resolvedRows || m_resolvedRows->size() <= 1) {
        return;
    }
    
    // 分岐を収集（startPlyでグループ化）
    QMap<int, QVector<int>> forksByPly; // startPly -> rowIndex のリスト
    
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        const ResolvedRow& row = m_resolvedRows->at(i);
        if (row.parent == mainRowIndex) {
            forksByPly[row.startPly].append(i);
        }
    }
    
    // 各分岐を movesArray の適切な位置に追加
    for (auto it = forksByPly.begin(); it != forksByPly.end(); ++it) {
        const int startPly = it.key();
        const QVector<int>& rowIndices = it.value();
        
        // movesArray 内の対応する位置を探す
        // startPly 手目の指し手は movesArray[startPly] にある
        // （moves[0]=開始局面, moves[1]=1手目, ...）
        if (startPly < movesArray.size()) {
            QJsonObject moveObj = movesArray[startPly].toObject();
            
            QJsonArray forks;
            if (moveObj.contains(QStringLiteral("forks"))) {
                forks = moveObj[QStringLiteral("forks")].toArray();
            }
            
            // 各分岐を forks 配列に追加（深い分岐も再帰的に処理）
            for (int rowIndex : rowIndices) {
                QSet<int> visitedRows;
                visitedRows.insert(mainRowIndex); // 本譜は既に処理済み
                
                const QJsonArray forkMoves = buildJkfForkMovesRecursive_(rowIndex, visitedRows);
                if (!forkMoves.isEmpty()) {
                    forks.append(forkMoves);
                }
            }
            
            if (!forks.isEmpty()) {
                moveObj[QStringLiteral("forks")] = forks;
                movesArray[startPly] = moveObj;
            }
        }
    }
}

QJsonArray GameRecordModel::buildJkfForkMovesRecursive_(int rowIndex, QSet<int>& visitedRows) const
{
    QJsonArray forkMoves;
    
    if (!m_resolvedRows || rowIndex < 0 || rowIndex >= m_resolvedRows->size()) {
        return forkMoves;
    }
    
    // 無限ループ防止
    if (visitedRows.contains(rowIndex)) {
        return forkMoves;
    }
    visitedRows.insert(rowIndex);
    
    const ResolvedRow& row = m_resolvedRows->at(rowIndex);
    const QList<KifDisplayItem>& dispItems = row.disp;
    
    int forkPrevToX = 0, forkPrevToY = 0;
    
    // この分岐の子分岐を収集（手数でグループ化）
    QMap<int, QVector<int>> childForksByPly;
    for (int i = 0; i < m_resolvedRows->size(); ++i) {
        const ResolvedRow& childRow = m_resolvedRows->at(i);
        if (childRow.parent == rowIndex && !visitedRows.contains(i)) {
            childForksByPly[childRow.startPly].append(i);
        }
    }
    
    // startPly 手目以降の指し手を追加
    for (int j = row.startPly; j < dispItems.size(); ++j) {
        const auto& item = dispItems[j];
        if (item.prettyMove.trimmed().isEmpty()) continue;
        
        QJsonObject forkMoveObj = convertMoveToJkf_(item, forkPrevToX, forkPrevToY, j);
        if (forkMoveObj.isEmpty()) continue;
        
        // この手数に子分岐があれば追加
        if (childForksByPly.contains(j)) {
            QJsonArray childForks;
            const QVector<int>& childRowIndices = childForksByPly[j];
            
            for (int childRowIndex : childRowIndices) {
                const QJsonArray childForkMoves = buildJkfForkMovesRecursive_(childRowIndex, visitedRows);
                if (!childForkMoves.isEmpty()) {
                    childForks.append(childForkMoves);
                }
            }
            
            if (!childForks.isEmpty()) {
                forkMoveObj[QStringLiteral("forks")] = childForks;
            }
        }
        
        forkMoves.append(forkMoveObj);
    }
    
    return forkMoves;
}

QStringList GameRecordModel::toJkfLines(const ExportContext& ctx) const
{
    QStringList out;
    
    // JKF のルートオブジェクトを構築
    QJsonObject root;
    
    // 1) header
    root[QStringLiteral("header")] = buildJkfHeader_(ctx);
    
    // 2) initial
    root[QStringLiteral("initial")] = buildJkfInitial_(ctx);
    
    // 3) moves（本譜）
    const QList<KifDisplayItem> disp = collectMainlineForExport_();
    QJsonArray movesArray = buildJkfMoves_(disp);
    
    // 4) 分岐を追加
    if (m_resolvedRows && m_resolvedRows->size() > 1) {
        // 本譜の行インデックスを探す
        int mainRowIndex = -1;
        for (int i = 0; i < m_resolvedRows->size(); ++i) {
            if (m_resolvedRows->at(i).parent < 0) {
                mainRowIndex = i;
                break;
            }
        }
        
        if (mainRowIndex >= 0) {
            addJkfForks_(movesArray, mainRowIndex);
        }
    }
    
    root[QStringLiteral("moves")] = movesArray;
    
    // JSON を文字列に変換
    const QJsonDocument doc(root);
    out << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    qDebug().noquote() << "[GameRecordModel] toJkfLines: generated JKF with"
                       << movesArray.size() << "moves";
    
    return out;
}

// ========================================
// USEN形式出力
// ========================================

// USEN形式の座標系:
// - マス番号: sq = (rank - 1) * 9 + (file - 1)
// - 例: 1一 = 0, 9九 = 80
// - rank: 段（一=1, 二=2, ..., 九=9）
// - file: 筋（1=1, 2=2, ..., 9=9）

// 駒打ち時の駒コード (from_sq = 81 + piece_code):
// 歩=10, 香=11, 桂=12, 銀=13, 金=9, 角=14, 飛=15
static int usiPieceToUsenDropCode(QChar usiPiece)
{
    switch (usiPiece.toUpper().toLatin1()) {
    case 'P': return 10;  // 歩
    case 'L': return 11;  // 香
    case 'N': return 12;  // 桂
    case 'S': return 13;  // 銀
    case 'G': return 9;   // 金
    case 'B': return 14;  // 角
    case 'R': return 15;  // 飛
    default:  return -1;  // 無効
    }
}

// base36文字セット
static const QString kUsenBase36Chars = QStringLiteral("0123456789abcdefghijklmnopqrstuvwxyz");

QString GameRecordModel::intToBase36_(int moveCode)
{
    // 3文字のbase36に変換（範囲: 0-46655）
    if (moveCode < 0 || moveCode > 46655) {
        qWarning() << "[USEN] moveCode out of range:" << moveCode;
        return QStringLiteral("000");
    }
    
    QString result;
    result.reserve(3);
    
    int v2 = moveCode % 36;
    moveCode /= 36;
    int v1 = moveCode % 36;
    moveCode /= 36;
    int v0 = moveCode;
    
    result.append(kUsenBase36Chars.at(v0));
    result.append(kUsenBase36Chars.at(v1));
    result.append(kUsenBase36Chars.at(v2));
    
    return result;
}

QString GameRecordModel::encodeUsiMoveToUsen_(const QString& usiMove)
{
    if (usiMove.isEmpty() || usiMove.size() < 4) {
        return QString();
    }
    
    // 成りフラグを確認
    bool isPromotion = usiMove.endsWith(QLatin1Char('+'));
    
    int from_sq, to_sq;
    
    // 駒打ちの判定 (P*5e 形式)
    if (usiMove.at(1) == QLatin1Char('*')) {
        // 駒打ち: from_sq = 81 + piece_code
        QChar pieceChar = usiMove.at(0);
        int pieceCode = usiPieceToUsenDropCode(pieceChar);
        if (pieceCode < 0) {
            qWarning() << "[USEN] Unknown drop piece:" << pieceChar;
            return QString();
        }
        from_sq = 81 + pieceCode;
        
        // 移動先を解析
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    } else {
        // 盤上移動: 7g7f 形式
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        
        from_sq = (fromRank - 1) * 9 + (fromFile - 1);
        to_sq = (toRank - 1) * 9 + (toFile - 1);
    }
    
    // エンコード: code = (from_sq * 81 + to_sq) * 2 + (成る場合は1)
    int moveCode = (from_sq * 81 + to_sq) * 2 + (isPromotion ? 1 : 0);
    
    return intToBase36_(moveCode);
}

QString GameRecordModel::sfenToUsenPosition_(const QString& sfen)
{
    // 平手初期局面のチェック
    static const QString kHirateSfenPrefix = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("0");  // 平手
    }
    
    // 平手初期局面の場合は "0" を返す
    if (trimmed.startsWith(kHirateSfenPrefix)) {
        return QStringLiteral("0");
    }
    
    // カスタム局面: SFEN -> USEN変換
    // '/' -> '_', ' ' -> '.', '+' -> 'z'
    QString usen = trimmed;
    usen.replace(QLatin1Char('/'), QLatin1Char('_'));
    usen.replace(QLatin1Char(' '), QLatin1Char('.'));
    usen.replace(QLatin1Char('+'), QLatin1Char('z'));
    
    return usen;
}

QString GameRecordModel::getUsenTerminalCode_(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);
    
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("i");
    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("r");
    if (move.contains(QStringLiteral("時間切れ")) || move.contains(QStringLiteral("切れ負け"))) 
        return QStringLiteral("t");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("p");
    if (move.contains(QStringLiteral("持将棋")) || move.contains(QStringLiteral("千日手"))) 
        return QStringLiteral("j");
    
    return QString();
}

QString GameRecordModel::inferUsiFromSfenDiff_(const QString& sfenBefore, const QString& sfenAfter, bool isSente)
{
    // 2つのSFEN間の差分からUSI形式の指し手を推測
    // SFEN形式: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    // 盤面部分を比較して、移動した駒を特定する
    
    if (sfenBefore.isEmpty() || sfenAfter.isEmpty()) {
        return QString();
    }
    
    // SFEN盤面部分を抽出
    QString boardBefore = sfenBefore.section(QLatin1Char(' '), 0, 0);
    QString boardAfter = sfenAfter.section(QLatin1Char(' '), 0, 0);
    
    // 盤面を9x9配列に展開 (promoted pieces use 0x100 flag)
    // board[rank][file] の順序で格納 (rank: 0-8 = 一段〜九段, file: 0-8 = 1筋〜9筋)
    auto expandBoard = [](const QString& board) -> QVector<int> {
        QVector<int> result(81, 0);  // 0 = empty
        int sfenIdx = 0;  // SFEN上のインデックス（左上から右へ、上から下へ）
        bool promoted = false;
        
        for (QChar c : board) {
            if (c == QLatin1Char('/')) continue;
            if (c == QLatin1Char('+')) {
                promoted = true;
                continue;
            }
            if (c.isDigit()) {
                int n = c.toLatin1() - '0';
                sfenIdx += n;
            } else {
                if (sfenIdx < 81) {
                    int rank = sfenIdx / 9;       // 0-8 (一段目〜九段目)
                    int sfenFile = sfenIdx % 9;   // 0-8 (9筋〜1筋の順)
                    int file = 8 - sfenFile;      // 0-8 (1筋〜9筋の順に変換)
                    int boardIdx = rank * 9 + file;
                    
                    int pieceCode = c.toLatin1();
                    if (promoted) pieceCode |= 0x100;  // 成駒フラグ
                    result[boardIdx] = pieceCode;
                }
                sfenIdx++;
                promoted = false;
            }
        }
        return result;
    };
    
    QVector<int> before = expandBoard(boardBefore);
    QVector<int> after = expandBoard(boardAfter);
    
    // 差分を探す
    int fromRank = -1, fromFile = -1;
    int toRank = -1, toFile = -1;
    int movedPiece = 0;
    bool isPromotion = false;
    
    for (int rank = 0; rank < 9; ++rank) {
        for (int file = 0; file < 9; ++file) {
            int idx = rank * 9 + file;
            int beforePiece = before[idx];
            int afterPiece = after[idx];
            
            if (beforePiece != afterPiece) {
                // beforeにあってafterにない → 移動元
                if (beforePiece != 0 && afterPiece == 0) {
                    fromRank = rank;
                    fromFile = file;
                    movedPiece = beforePiece;
                }
                // beforeになくてafterにある → 移動先
                else if (beforePiece == 0 && afterPiece != 0) {
                    toRank = rank;
                    toFile = file;
                    // 成り判定: 移動元が非成りで移動先が成り
                    if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                        isPromotion = true;
                    }
                }
                // 両方に駒がある → 駒取り
                else if (beforePiece != 0 && afterPiece != 0) {
                    // 先手の駒は大文字、後手の駒は小文字
                    char beforeChar = static_cast<char>(beforePiece & 0xFF);
                    char afterChar = static_cast<char>(afterPiece & 0xFF);
                    bool beforeIsSente = (beforeChar >= 'A' && beforeChar <= 'Z');
                    bool afterIsSente = (afterChar >= 'A' && afterChar <= 'Z');
                    
                    if (beforeIsSente != afterIsSente) {
                        if ((isSente && afterIsSente) || (!isSente && !afterIsSente)) {
                            toRank = rank;
                            toFile = file;
                            // 成り判定
                            if ((afterPiece & 0x100) && !(movedPiece & 0x100)) {
                                isPromotion = true;
                            }
                        } else {
                            fromRank = rank;
                            fromFile = file;
                            movedPiece = beforePiece;
                        }
                    }
                }
            }
        }
    }
    
    // 駒打ちの検出（fromRank == -1 で toRank != -1）
    if (fromRank < 0 && toRank >= 0) {
        // 持ち駒から打った
        int piece = after[toRank * 9 + toFile];
        char pieceChar = static_cast<char>(piece & 0xFF);
        QChar usiPiece = QChar(pieceChar).toUpper();
        
        // USI座標: ファイルは1-9、ランクはa-i
        int usiFile = toFile + 1;  // 1-9
        char usiRank = 'a' + toRank;  // a-i
        
        return QStringLiteral("%1*%2%3")
            .arg(usiPiece)
            .arg(usiFile)
            .arg(QChar(usiRank));
    }
    
    // 盤上移動
    if (fromRank >= 0 && toRank >= 0) {
        // USI座標: ファイルは1-9、ランクはa-i
        int fromUsiFile = fromFile + 1;
        char fromUsiRank = 'a' + fromRank;
        int toUsiFile = toFile + 1;
        char toUsiRank = 'a' + toRank;
        
        QString usi = QStringLiteral("%1%2%3%4")
            .arg(fromUsiFile)
            .arg(QChar(fromUsiRank))
            .arg(toUsiFile)
            .arg(QChar(toUsiRank));
        
        if (isPromotion) {
            usi += QLatin1Char('+');
        }
        
        return usi;
    }
    
    return QString();
}

QString GameRecordModel::buildUsenVariation_(int rowIndex, QSet<int>& visitedRows) const
{
    if (!m_resolvedRows || rowIndex < 0 || rowIndex >= m_resolvedRows->size()) {
        return QString();
    }
    
    if (visitedRows.contains(rowIndex)) {
        return QString();
    }
    visitedRows.insert(rowIndex);
    
    const ResolvedRow& row = m_resolvedRows->at(rowIndex);
    const int startPly = row.startPly;
    
    // USENのオフセットは startPly - 1 (0-indexed)
    int offset = startPly - 1;
    if (offset < 0) offset = 0;
    
    QString usen = QStringLiteral("~%1.").arg(offset);
    
    // 指し手をエンコード
    // ResolvedRowからUSI指し手を取得するには、sfenリストを使って再構築するか、
    // または表示用データから推測する必要がある
    // ここでは sfen リストから USI を再構築する
    const QStringList& sfenList = row.sfen;
    QString terminalCode;
    
    // sfenリストが利用可能な場合、連続するSFENからUSI指し手を推測
    // または、disp から prettyMove を使って変換
    // 実際のアプローチ: dispの指し手を使ってUSIに変換する必要があるが、
    // prettyMoveからUSIへの逆変換は複雑なので、ResolvedRow に usiMoves があれば使用
    
    // 注意: ResolvedRow には usiMoves がないので、prettyMove + sfen から推測する
    // より確実なアプローチとして、SfenPositionTracer を使って盤面を追跡し、
    // dispの指し手と照合することでUSIを生成する
    
    // シンプルなアプローチ: disp の各指し手を解析してUSIを推定
    // これは完全ではないが、多くのケースで動作する
    const QList<KifDisplayItem>& disp = row.disp;
    
    for (int i = startPly; i < disp.size(); ++i) {
        const KifDisplayItem& item = disp[i];
        const QString& prettyMove = item.prettyMove;
        
        if (prettyMove.isEmpty()) continue;
        
        // 終局語のチェック
        if (isTerminalMove(prettyMove)) {
            terminalCode = getUsenTerminalCode_(prettyMove);
            break;
        }
        
        // prettyMove から USI を推測（sfenリストを使用）
        // 2つの連続するSFENから差分を見つけてUSIを生成
        if (i < sfenList.size() && (i - 1) >= 0 && (i - 1) < sfenList.size()) {
            // SFEN差分からUSI生成は複雑なので、ここでは簡易実装
            // 実際にはprettyMoveを解析する
        }
        
        // prettyMove からの USI 推定
        // これは GameRecordModel では難しいので、別途 SfenPositionTracer を使う必要がある
        // 今回はスキップし、USI moves を直接渡すアプローチを使用
    }
    
    // 終局コードを追加
    if (!terminalCode.isEmpty()) {
        usen += QStringLiteral(".") + terminalCode;
    }
    
    return usen;
}

QStringList GameRecordModel::toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    QStringList out;
    
    // 1) 初期局面をUSEN形式に変換
    QString position = sfenToUsenPosition_(ctx.startSfen);
    
    // 2) 本譜の指し手をエンコード
    QString mainMoves;
    QString terminalCode;
    
    // 本譜の指し手リストを取得
    const QList<KifDisplayItem> disp = collectMainlineForExport_();
    
    // USI moves が渡された場合はそれを使用
    // そうでない場合は ResolvedRow から取得を試みる
    QStringList mainlineUsi = usiMoves;
    
    if (mainlineUsi.isEmpty() && m_resolvedRows && !m_resolvedRows->isEmpty()) {
        // 本譜行を探す
        for (int i = 0; i < m_resolvedRows->size(); ++i) {
            if (m_resolvedRows->at(i).parent < 0) {
                // 本譜のSFENリストからUSI指し手を推測することは複雑
                // ここでは空のままにする
                break;
            }
        }
    }
    
    // 各USI指し手をUSEN形式にエンコード
    for (const QString& usi : std::as_const(mainlineUsi)) {
        QString encoded = encodeUsiMoveToUsen_(usi);
        if (!encoded.isEmpty()) {
            mainMoves += encoded;
        }
    }
    
    // 終局語のチェック
    if (!disp.isEmpty()) {
        const QString& lastMove = disp.last().prettyMove;
        if (isTerminalMove(lastMove)) {
            terminalCode = getUsenTerminalCode_(lastMove);
        }
    }
    
    // 本譜のUSEN文字列を構築
    QString usen = QStringLiteral("~%1.%2").arg(position, mainMoves);
    
    // 終局コードを追加
    if (!terminalCode.isEmpty()) {
        usen += QStringLiteral(".") + terminalCode;
    }
    
    // 3) 分岐を追加
    if (m_resolvedRows && m_resolvedRows->size() > 1) {
        QSet<int> visitedRows;
        
        // 本譜の行インデックスを探す
        int mainRowIndex = -1;
        for (int i = 0; i < m_resolvedRows->size(); ++i) {
            if (m_resolvedRows->at(i).parent < 0) {
                mainRowIndex = i;
                visitedRows.insert(i);
                break;
            }
        }
        
        // 分岐をvarIndexの順でソート
        QVector<QPair<int, int>> variations; // (sortKey, rowIndex)
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
        
        // 各分岐をUSEN形式で出力
        for (const auto& var : std::as_const(variations)) {
            const int rowIndex = var.second;
            const ResolvedRow& row = m_resolvedRows->at(rowIndex);
            
            // オフセット（分岐開始位置 - 1）
            int offset = row.startPly - 1;
            if (offset < 0) offset = 0;
            
            QString branchMoves;
            QString branchTerminal;
            
            // 分岐のSFENリストからUSI指し手を推測
            // ここではSFENの差分から指し手を推測する実装が必要
            // 今回は簡易的にSFENリストを使用
            const QStringList& sfenList = row.sfen;
            const QList<KifDisplayItem>& branchDisp = row.disp;
            
            // SFENの差分からUSI指し手を推測
            for (int i = row.startPly; i < branchDisp.size(); ++i) {
                const KifDisplayItem& item = branchDisp[i];
                if (item.prettyMove.isEmpty()) continue;
                
                // 終局語チェック
                if (isTerminalMove(item.prettyMove)) {
                    branchTerminal = getUsenTerminalCode_(item.prettyMove);
                    break;
                }
                
                // SFENリストを使ってUSI指し手を推測
                // i-1番目のSFENからi番目のSFENへの差分を計算
                if (i < sfenList.size() && (i - 1) >= 0 && (i - 1) < sfenList.size()) {
                    QString usi = inferUsiFromSfenDiff_(sfenList[i - 1], sfenList[i], (i % 2 != 0));
                    if (!usi.isEmpty()) {
                        QString encoded = encodeUsiMoveToUsen_(usi);
                        if (!encoded.isEmpty()) {
                            branchMoves += encoded;
                        }
                    }
                }
            }
            
            // 分岐のUSEN文字列を構築
            QString branchUsen = QStringLiteral("~%1.%2").arg(offset).arg(branchMoves);
            if (!branchTerminal.isEmpty()) {
                branchUsen += QStringLiteral(".") + branchTerminal;
            }
            
            usen += branchUsen;
            visitedRows.insert(rowIndex);
        }
    }
    
    out << usen;
    
    qDebug().noquote() << "[GameRecordModel] toUsenLines: generated USEN with"
                       << mainMoves.size() / 3 << "mainline moves,"
                       << (m_resolvedRows ? m_resolvedRows->size() - 1 : 0) << "variations";
    
    return out;
}
