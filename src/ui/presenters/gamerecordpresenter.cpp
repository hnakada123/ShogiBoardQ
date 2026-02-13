/// @file gamerecordpresenter.cpp
/// @brief 棋譜表示プレゼンタクラスの実装

#include "gamerecordpresenter.h"
#include "kifdisplayitem.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "recordpane.h"

#include <QAbstractItemView>
#include "loggingcategory.h"
#include <QTableView>
#include <QItemSelectionModel>

GameRecordPresenter::GameRecordPresenter(const Deps& d, QObject* parent)
    : QObject(parent), m_d(d) {}

void GameRecordPresenter::updateDeps(const Deps& d) {
    m_d = d;
}

void GameRecordPresenter::clear() {
    m_kifuDataList.clear();
    m_currentMoveIndex = 0;
    if (m_d.model) {
        m_d.model->clearAllItems();
        emit modelReset();
    }
}

void GameRecordPresenter::presentGameRecord(const QList<KifDisplayItem>& disp) {
    if (!m_d.model) return;

    clear();

    // 開始局面のコメントを取得（disp[0] がある場合）
    QString openingComment;
    if (!disp.isEmpty()) {
        openingComment = disp.at(0).comment;
    }

    // パフォーマンス改善: 全アイテムを一括追加
    QList<KifuDisplay*> items;
    items.reserve(disp.size());

    // 開始局面のしおりを取得
    QString openingBookmark;
    if (!disp.isEmpty()) {
        openingBookmark = disp.at(0).bookmark;
    }

    // ヘッダ（開始局面）+ コメント + しおり
    items.append(new KifuDisplay(
        tr("=== 開始局面 ==="),
        tr("（１手 / 合計）"),
        openingComment,
        openingBookmark
        ));

    // 各手を追加（dispの先頭は開始局面エントリなのでスキップ）
    for (qsizetype i = 1; i < disp.size(); ++i) {
        const auto& it = disp.at(i);
        const QString prettyMove = it.prettyMove.trimmed();
        if (prettyMove.isEmpty()) continue;

        // 手数の算出（ヘッダ行を除く）
        const int moveNumber = static_cast<int>(items.size());
        const QString moveNumberStr = QString::number(moveNumber);
        const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
        const QString recordLine = spaces + moveNumberStr + QLatin1Char(' ') + prettyMove;

        items.append(new KifuDisplay(recordLine, it.timeText, it.comment, it.bookmark));

        // m_kifuDataList にも追加（互換性のため）
        QString kifuLine = recordLine + QStringLiteral(" ( ") + it.timeText + QLatin1String(" )");
        kifuLine.remove(QStringLiteral("▲"));
        kifuLine.remove(QStringLiteral("△"));
        m_kifuDataList.append(kifuLine);
    }

    // 一括追加（beginInsertRows/endInsertRows は1回だけ）
    m_d.model->appendItems(items);

    // 現在の手数インデックスを更新
    m_currentMoveIndex = static_cast<int>(items.size()) - 1;

    // ハイライトは最後に1回だけ設定（開始局面）
    m_d.model->setCurrentHighlightRow(0);

    // 初期選択位置は先頭に寄せる（任意）
    // ただし、対局中（ナビゲーション無効化中）はsetCurrentIndexを呼ばない
    if (m_d.recordPane) {
        if (!m_d.recordPane->isNavigationDisabled()) {
            if (auto* view = m_d.recordPane->kifuView()) {
                const QModelIndex top = m_d.model->index(0, 0);
                // カレントインデックスと選択の両方を設定（選択行を黄色にするため）
                if (auto* sel = view->selectionModel()) {
                    sel->setCurrentIndex(top, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                }
                view->scrollTo(top, QAbstractItemView::PositionAtCenter);
            }
        }
    }
}

void GameRecordPresenter::appendMoveLine(const QString& prettyMove, const QString& elapsedTime)
{
    const QString last = prettyMove.trimmed();
    if (last.isEmpty()) return;

    // --- 手数の算出 ---
    // 基本は「モデルの現在行数」だが、先頭に「開始局面」「平手」「startpos」などの見出し行が
    // 1行入っている構成のため、これを手数計算から除外する。
    int moveRows = 0;
    if (m_d.model) {
        moveRows = m_d.model->rowCount();

        if (moveRows > 0) {
            const QModelIndex headIdx = m_d.model->index(0, 0);
            const QString headText = m_d.model->data(headIdx, Qt::DisplayRole).toString();

            // 見出し行の代表的な文言を検出して 1 行分を差し引く
            // （必要に応じて追加： "開始局面", "平手", "Handicap", "startpos" 等）
            if (headText.contains(tr("開始局面"))
                || headText.contains(QStringLiteral("平手"))
                || headText.contains(QStringLiteral("startpos"), Qt::CaseInsensitive)) {
                moveRows -= 1;
                if (moveRows < 0) moveRows = 0;
            }
        }
    }

    // 次に付与すべき手数（1始まり）
    const int nextMoveNumber = moveRows + 1;

    // 内部の現在手数インデックスも同期（独自カウンタに依存しない）
    m_currentMoveIndex = nextMoveNumber;

    // 「   n ▲７六歩」形式（左寄せ4桁）
    const QString moveNumberStr = QString::number(m_currentMoveIndex);
    const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
    const QString recordLine = spaces + moveNumberStr + QLatin1Char(' ') + last;

    // KIF 出力用（先後記号は除去）
    QString kifuLine = recordLine + QStringLiteral(" ( ") + elapsedTime + QLatin1String(" )");
    kifuLine.remove(QStringLiteral("▲"));
    kifuLine.remove(QStringLiteral("△"));
    m_kifuDataList.append(kifuLine); // 必要なければそのままでOK

    if (m_d.model) {
        m_d.model->appendItem(new KifuDisplay(recordLine, elapsedTime));

        // 新しく追加した行（最後の行）を黄色でハイライト
        const int newRow = m_d.model->rowCount() - 1;
        m_d.model->setCurrentHighlightRow(newRow);

        // QTableViewの選択モデルでも新しい行を選択（黄色表示のため）
        if (m_kifuView) {
            if (auto* sel = m_kifuView->selectionModel()) {
                const QModelIndex idx = m_d.model->index(newRow, 0);
                sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        } else if (m_d.recordPane) {
            if (auto* view = m_d.recordPane->kifuView()) {
                if (auto* sel = view->selectionModel()) {
                    const QModelIndex idx = m_d.model->index(newRow, 0);
                    sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                }
            }
        }
    }
}

void GameRecordPresenter::appendMoveLineWithComment(const QString& prettyMove, const QString& elapsedTime, const QString& comment)
{
    const QString last = prettyMove.trimmed();
    if (last.isEmpty()) {
        qCDebug(lcUi) << "skip empty move line";
        return;
    }

    // --- 手数の算出 ---
    int moveRows = 0;
    if (m_d.model) {
        moveRows = m_d.model->rowCount();

        if (moveRows > 0) {
            const QModelIndex headIdx = m_d.model->index(0, 0);
            const QString headText = m_d.model->data(headIdx, Qt::DisplayRole).toString();

            if (headText.contains(tr("開始局面"))
                || headText.contains(QStringLiteral("平手"))
                || headText.contains(QStringLiteral("startpos"), Qt::CaseInsensitive)) {
                moveRows -= 1;
                if (moveRows < 0) moveRows = 0;
            }
        }
    }

    const int nextMoveNumber = moveRows + 1;
    m_currentMoveIndex = nextMoveNumber;

    const QString moveNumberStr = QString::number(m_currentMoveIndex);
    const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
    const QString recordLine = spaces + moveNumberStr + QLatin1Char(' ') + last;

    QString kifuLine = recordLine + QStringLiteral(" ( ") + elapsedTime + QLatin1String(" )");
    kifuLine.remove(QStringLiteral("▲"));
    kifuLine.remove(QStringLiteral("△"));
    m_kifuDataList.append(kifuLine);

    if (m_d.model) {
        // コメント付きで KifuDisplay を追加
        m_d.model->appendItem(new KifuDisplay(recordLine, elapsedTime, comment));

        const int newRow = m_d.model->rowCount() - 1;
        m_d.model->setCurrentHighlightRow(newRow);

        // QTableViewの選択モデルでも新しい行を選択（黄色表示のため）
        if (m_kifuView) {
            if (auto* sel = m_kifuView->selectionModel()) {
                const QModelIndex idx = m_d.model->index(newRow, 0);
                sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        } else if (m_d.recordPane) {
            if (auto* view = m_d.recordPane->kifuView()) {
                if (auto* sel = view->selectionModel()) {
                    const QModelIndex idx = m_d.model->index(newRow, 0);
                    sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                }
            }
        }
    }
}

void GameRecordPresenter::setCommentsByRow(const QStringList& commentsByRow)
{
    m_commentsByRow = commentsByRow;
}

void GameRecordPresenter::setCommentsFromDisplayItems(const QList<KifDisplayItem>& disp,
                                                      int rowCount)
{
    // rowCount: 0手目(初期局面)を含む行数
    m_commentsByRow.clear();
    m_commentsByRow.resize(qMax(0, rowCount));

    const int moveCount = static_cast<int>(disp.size());
    if (m_commentsByRow.isEmpty()) {
        return;
    }

    // --- 新しいデータ構造 ---
    // disp[0] = 開始局面エントリ（prettyMove が空、comment に開始局面コメント）
    // disp[1] = 1手目（prettyMove に指し手、comment に1手目のコメント）
    // disp[2] = 2手目（prettyMove に指し手、comment に2手目のコメント）
    // ...
    // つまり、disp[i].comment は i 手目のコメントに対応
    // m_commentsByRow[i] にも disp[i].comment をそのまま割り当てる

    const int rows = static_cast<int>(m_commentsByRow.size());
    for (int r = 0; r < rows; ++r) {
        if (r < moveCount) {
            m_commentsByRow[r] = disp[r].comment;
        } else {
            m_commentsByRow[r].clear();
        }
    }
}

QString GameRecordPresenter::commentForRow(int row) const
{
    if (row >= 0 && row < m_commentsByRow.size())
        return m_commentsByRow[row];
    return QString();
}

void GameRecordPresenter::onKifuCurrentRowChanged(const QModelIndex& current,
                                                   const QModelIndex& previous)
{
    Q_UNUSED(previous)
    const int row = current.isValid() ? current.row() : -1;

    // 選択行が変わったらモデルのハイライト行も更新
    if (m_d.model && row >= 0) {
        m_d.model->setCurrentHighlightRow(row);
    }

    const QString cmt = commentForRow(row);
    emit currentRowChanged(row, cmt);
}

void GameRecordPresenter::bindKifuSelection(QTableView* kifuView)
{
    if (!kifuView) {
        qCWarning(lcUi) << "bindKifuSelection: view is null";
        return;
    }
    m_kifuView = kifuView;

    // 既存の接続を解除
    if (m_connRowChanged)
        QObject::disconnect(m_connRowChanged);

    auto* sel = m_kifuView->selectionModel();
    if (!sel) {
        qCWarning(lcUi) << "selectionModel is null";
        return;
    }

    // KifuView の currentRowChanged を Presenter が受け、
    // 行とコメントを整形して currentRowChanged(row, cmt) を発火
    m_connRowChanged = connect(
        sel, &QItemSelectionModel::currentRowChanged,
        this, &GameRecordPresenter::onKifuCurrentRowChanged,
        Qt::UniqueConnection
        );
}

void GameRecordPresenter::displayAndWire(const QList<KifDisplayItem>& disp,
                                         int rowCount,
                                         RecordPane* recordPane)
{
    // 1) モデルへ反映（既存のまとめ関数）
    presentGameRecord(disp);

    // 2) コメント配列を Presenter 側で構築（既存の補助関数）
    setCommentsFromDisplayItems(disp, rowCount);

    // 3) KifuView の currentRowChanged を Presenter が受けて MainWindow へ signal 転送
    if (recordPane && recordPane->kifuView()) {
        bindKifuSelection(recordPane->kifuView()); // ここで UniqueConnection 済み
    }
}

int GameRecordPresenter::currentRow() const
{
    // KifuView がバインド済みなら、その selectionModel の currentIndex を信頼
    if (m_kifuView) {
        if (auto* sel = m_kifuView->selectionModel()) {
            const QModelIndex idx = sel->currentIndex();
            if (idx.isValid()) return idx.row();
        }
    }
    return -1; // 選択がまだ無い／未バインド
}

void GameRecordPresenter::addLiveKifItem(const QString& prettyMove, const QString& elapsedTime)
{
    KifDisplayItem item;
    item.prettyMove = prettyMove;
    item.timeText   = elapsedTime.isEmpty() ? QStringLiteral("00:00/00:00:00") : elapsedTime;
    m_liveDisp.push_back(item);
}
