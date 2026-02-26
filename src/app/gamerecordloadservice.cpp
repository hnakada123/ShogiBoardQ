#include "gamerecordloadservice.h"

#include <QElapsedTimer>

#include "kifu/gamerecordmodel.h"
#include "kifu/kifdisplayitem.h"
#include "ui/presenters/gamerecordpresenter.h"

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(lcApp)

void GameRecordLoadService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void GameRecordLoadService::loadGameRecord(const QList<KifDisplayItem>& disp)
{
    QElapsedTimer timer;
    timer.start();

    // 棋譜読み込み時は対局で記録したUSI形式指し手リストをクリア
    // （代わりに KifuLoadCoordinator::kifuUsiMovesPtr() を使用する）
    if (m_deps.gameUsiMoves) {
        m_deps.gameUsiMoves->clear();
    }
    if (m_deps.gameMoves) {
        m_deps.gameMoves->clear();
    }

    // RecordPresenter を遅延初期化
    GameRecordPresenter* presenter = nullptr;
    if (m_deps.ensureRecordPresenter) {
        presenter = m_deps.ensureRecordPresenter();
    }
    if (!presenter) return;

    const qsizetype moveCount = disp.size();
    const QStringList* sfen = m_deps.sfenRecord ? m_deps.sfenRecord() : nullptr;
    const int rowCount = (sfen && !sfen->isEmpty())
                             ? static_cast<int>(sfen->size())
                             : static_cast<int>(moveCount + 1);

    // GameRecordModel を遅延初期化してデータ設定
    GameRecordModel* model = nullptr;
    if (m_deps.ensureGameRecordModel) {
        model = m_deps.ensureGameRecordModel();
    }
    if (model) {
        model->initializeFromDisplayItems(disp, rowCount);
    }
    qCDebug(lcApp).noquote() << QStringLiteral("GameRecordModel init: %1 ms").arg(timer.elapsed());

    // commentsByRow の構築
    if (m_deps.commentsByRow) {
        m_deps.commentsByRow->clear();
        m_deps.commentsByRow->resize(rowCount);
        for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
            (*m_deps.commentsByRow)[i] = disp[i].comment;
        }
    }
    qCDebug(lcApp).noquote() << "displayGameRecord: initialized with" << rowCount << "entries";

    // Presenter 側に表示と配線を委譲
    QElapsedTimer presenterTimer;
    presenterTimer.start();
    presenter->displayAndWire(disp, rowCount, m_deps.recordPane);
    qCDebug(lcApp).noquote() << QStringLiteral("displayAndWire: %1 ms").arg(presenterTimer.elapsed());
    qCDebug(lcApp).noquote() << QStringLiteral("displayGameRecord TOTAL: %1 ms").arg(timer.elapsed());
}
