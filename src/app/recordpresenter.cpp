#include "recordpresenter.h"
#include "kifdisplayitem.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "recordpane.h"

#include <QAbstractItemView>
#include <QDebug>
#include <qtableview.h>

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
