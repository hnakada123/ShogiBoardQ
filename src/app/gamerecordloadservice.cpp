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

    // 指し手列（gameMoves）は KifuApplyService が読み込んだ本譜の手に置き換え済み。
    // 局面列とそろったまま残し、途中の局面から対局を始めるときに MatchCoordinator が切り詰める

    // RecordPresenter を遅延初期化
    GameRecordPresenter* presenter = nullptr;
    if (m_deps.ensureRecordPresenter) {
        presenter = m_deps.ensureRecordPresenter();
    }
    if (!presenter) return;

    const int rowCount = static_cast<int>(disp.size());

    // GameRecordModel を遅延初期化してデータ設定
    GameRecordModel* model = nullptr;
    if (m_deps.ensureGameRecordModel) {
        model = m_deps.ensureGameRecordModel();
    }
    if (model) {
        model->initializeFromDisplayItems(disp, rowCount);
    }
    qCDebug(lcApp).noquote() << QStringLiteral("GameRecordModel init: %1 ms").arg(timer.elapsed());

    qCDebug(lcApp).noquote() << "displayGameRecord: initialized with" << rowCount << "entries";

    // Presenter 側に表示と配線を委譲
    QElapsedTimer presenterTimer;
    presenterTimer.start();
    presenter->displayAndWire(disp, m_deps.recordPane);
    qCDebug(lcApp).noquote() << QStringLiteral("displayAndWire: %1 ms").arg(presenterTimer.elapsed());
    qCDebug(lcApp).noquote() << QStringLiteral("displayGameRecord TOTAL: %1 ms").arg(timer.elapsed());
}
