#include "recordpresenter.h"
#include "kifdisplayitem.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "recordpane.h"

#include <QAbstractItemView>
#include <QDebug>
#include <QTableView>
#include <QItemSelectionModel>
#include <QDebug>

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

    // ヘッダ（開始局面）
    m_d.model->appendItem(new KifuDisplay(
        QStringLiteral("=== 開始局面 ==="),
        QStringLiteral("（1手 / 合計）")
        ));

    // 各手を追加
    for (const auto& it : disp) {
        appendMoveLine(it.prettyMove, it.timeText);
    }

    // 初期選択位置は先頭に寄せる（任意）
    if (m_d.recordPane) {
        if (auto* view = m_d.recordPane->kifuView()) {
            const QModelIndex top = m_d.model->index(0, 0);
            view->setCurrentIndex(top);
            view->scrollTo(top, QAbstractItemView::PositionAtCenter);
        }
    }
}

void GameRecordPresenter::appendMoveLine(const QString& prettyMove, const QString& elapsedTime) {
    const QString last = prettyMove.trimmed();
    if (last.isEmpty()) {
        qDebug() << "[RecordPresenter] skip empty move line";
        return;
    }

    // 「   n ▲７六歩」形式（左寄せ4桁）
    const QString moveNumberStr = QString::number(++m_currentMoveIndex);
    const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
    const QString recordLine = spaces + moveNumberStr + QLatin1Char(' ') + last;

    // KIF出力用（先後記号は除去）
    QString kifuLine = recordLine + QStringLiteral(" ( ") + elapsedTime + QLatin1String(" )");
    kifuLine.remove(QStringLiteral("▲"));
    kifuLine.remove(QStringLiteral("△"));
    m_kifuDataList.append(kifuLine); // 必要なければ放置でOK

    if (m_d.model) {
        m_d.model->appendItem(new KifuDisplay(recordLine, elapsedTime));
    }
}

void GameRecordPresenter::setCommentsByRow(const QStringList& commentsByRow)
{
    m_commentsByRow = commentsByRow;
}

void GameRecordPresenter::setCommentsFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount)
{
    // 以前 MainWindow が行っていたマッピングを Presenter で再現
    // rowCount: 0手目(初期局面)を含む行数
    m_commentsByRow.clear();
    m_commentsByRow.resize(qMax(0, rowCount));

    const int moveCount = disp.size();

    if (moveCount > 0 && !m_commentsByRow.isEmpty()) {
        // 行0のコメントは disp[0] を採用（従来動作踏襲）
        m_commentsByRow[0] = disp[0].comment;
    }

    for (int i = 0; i < moveCount; ++i) {
        const int row = i + 1; // 1手目→行1
        if (row >= 0 && row < m_commentsByRow.size())
            m_commentsByRow[row] = disp[i].comment;
    }
}

QString GameRecordPresenter::commentForRow(int row) const
{
    if (row >= 0 && row < m_commentsByRow.size())
        return m_commentsByRow[row];
    return QString();
}

void GameRecordPresenter::onKifuCurrentRowChanged_(const QModelIndex& current,
                                                   const QModelIndex& /*previous*/)
{
    const int row = current.isValid() ? current.row() : -1;
    const QString cmt = commentForRow(row);
    emit currentRowChanged(row, cmt);
}

void GameRecordPresenter::bindKifuSelection(QTableView* kifuView)
{
    if (!kifuView) {
        qWarning() << "[RecordPresenter] bindKifuSelection: view is null";
        return;
    }
    m_kifuView = kifuView;

    // 既存の接続を解除
    if (m_connRowChanged)
        QObject::disconnect(m_connRowChanged);

    auto* sel = m_kifuView->selectionModel();
    if (!sel) {
        qWarning() << "[RecordPresenter] selectionModel is null";
        return;
    }

    // KifuView の currentRowChanged を Presenter が受け、
    // 行とコメントを整形して currentRowChanged(row, cmt) を発火
    m_connRowChanged = connect(
        sel, &QItemSelectionModel::currentRowChanged,
        this, &GameRecordPresenter::onKifuCurrentRowChanged_,
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
