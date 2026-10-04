/// @file gamerecordmodelbuilder.cpp
/// @brief GameRecordModel の構築ロジックを集約するビルダーの実装

#include "gamerecordmodelbuilder.h"
#include "gamerecordmodel.h"
#include "commentcoordinator.h"
#include "gamerecordpresenter.h"

#include <QObject>
#include <QLoggingCategory>

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
        deps.commentCoordinator->setGameRecordModel(model);
        QObject::connect(model, &GameRecordModel::commentChanged,
                         deps.commentCoordinator, &CommentCoordinator::onGameRecordCommentChanged);

        deps.commentCoordinator->setRecordPresenter(deps.recordPresenter);
        deps.commentCoordinator->setKifuRecordListModel(deps.kifuRecordModel);
        QObject::connect(model, &GameRecordModel::bookmarkChanged,
                         deps.commentCoordinator, &CommentCoordinator::onBookmarkChanged);
    }

    qCDebug(lcApp).noquote() << "GameRecordModelBuilder::build: created and bound";

    return model;
}
