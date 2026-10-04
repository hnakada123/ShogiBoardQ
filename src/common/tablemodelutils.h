#ifndef TABLEMODELUTILS_H
#define TABLEMODELUTILS_H

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QTableView>

namespace TableModelUtils {
/// 選択を外部のビューと共有しないテーブル用。Qt が置き換えた選択モデルを解放する。
inline void setModel(QTableView* view, QAbstractItemModel* model)
{
    if (!view || view->model() == model) return;
    view->setModel(model);

    // QTableView はヘッダーにもモデルを設定してから選択モデルを共有するため、
    // ヘッダーが一時的に作った選択モデルも不要になる。外部所有のものは触らない。
    for (QObject* owner : {static_cast<QObject*>(view),
                           static_cast<QObject*>(view->horizontalHeader()),
                           static_cast<QObject*>(view->verticalHeader())}) {
        const auto selections = owner->findChildren<QItemSelectionModel*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* selection : selections) {
            if (selection != view->selectionModel()
                && selection != view->horizontalHeader()->selectionModel()
                && selection != view->verticalHeader()->selectionModel())
                selection->deleteLater();
        }
    }
}
}

#endif // TABLEMODELUTILS_H
