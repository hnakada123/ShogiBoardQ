/// @file gamerecordmodelbuilder.cpp
/// @brief GameRecordModel の構築ロジックを集約するビルダーの実装

#include "gamerecordmodelbuilder.h"
#include "gamerecordmodel.h"
#include "commentcoordinator.h"
#include "gamerecordpresenter.h"

#include <QObject>
#include <QLoggingCategory>
#include <functional>

Q_DECLARE_LOGGING_CATEGORY(lcApp)

GameRecordModel* GameRecordModelBuilder::build(const Deps& deps)
{
    auto* model = new GameRecordModel(deps.parent);

    // 外部データストアをバインド
    QList<KifDisplayItem>* liveDispPtr = nullptr;
    if (deps.recordPresenter) {
        liveDispPtr = deps.recordPresenter->liveDispPtr();
    }
    model->bind(liveDispPtr);

    if (deps.branchTree != nullptr) {
        model->setBranchTree(deps.branchTree);
    }
    if (deps.navState != nullptr) {
        model->setNavigationState(deps.navState);
    }

    // CommentCoordinator との接続
    if (deps.commentCoordinator) {
        QObject::connect(model, &GameRecordModel::commentChanged,
                         deps.commentCoordinator, &CommentCoordinator::onGameRecordCommentChanged);

        // コメント更新時のコールバックを設定
        // m_kifu.commentsByRow 同期、RecordPresenter通知、UI更新を自動実行
        deps.commentCoordinator->setRecordPresenter(deps.recordPresenter);
        deps.commentCoordinator->setKifuRecordListModel(deps.kifuRecordModel);
        model->setCommentUpdateCallback(
            std::bind(&CommentCoordinator::onCommentUpdateCallback, deps.commentCoordinator,
                      std::placeholders::_1, std::placeholders::_2));

        // しおり更新時のコールバックを設定
        model->setBookmarkUpdateCallback(
            std::bind(&CommentCoordinator::onBookmarkUpdateCallback, deps.commentCoordinator,
                      std::placeholders::_1, std::placeholders::_2));
    }

    qCDebug(lcApp).noquote() << "GameRecordModelBuilder::build: created and bound";

    return model;
}
