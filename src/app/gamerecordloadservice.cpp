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

    // 棋譜の正本は分岐ツリー。対局の局面列・指し手列は読み込みでは触らず、
    // 対局開始時に表示中の手順から作り直す（MainWindowGameStartService）

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
