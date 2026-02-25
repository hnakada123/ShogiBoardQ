/// @file recordpanewiring.cpp
/// @brief 棋譜欄配線クラスの実装

#include "recordpanewiring.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "mainwindow.h"
#include "commentcoordinator.h"

#include <QModelIndex>
#include <QItemSelectionModel>
#include <QTableView>
#include "logcategories.h"

RecordPaneWiring::RecordPaneWiring(const Deps& d, QObject* parent)
    : QObject(parent), m_d(d)
{}

void RecordPaneWiring::buildUiAndWire()
{
    if (!m_pane) {
        m_pane = new RecordPane(m_d.parent);

        // MainWindow へ「メイン行変更」を通知（こちらは MainWindow にスロットあり）
        if (auto* mw = qobject_cast<MainWindow*>(m_d.ctx)) {
            QObject::connect(m_pane, &RecordPane::mainRowChanged,
                             mw,     &MainWindow::onRecordPaneMainRowChanged,
                             Qt::UniqueConnection);
        }
        if (m_d.commentCoordinator) {
            QObject::connect(m_pane, &RecordPane::bookmarkEditRequested,
                             m_d.commentCoordinator, &CommentCoordinator::onBookmarkEditRequested,
                             Qt::UniqueConnection);
        }

        //重要
        // branchActivated は KifuDisplayCoordinator 側で配線する設計なので、
        // ここ（RecordPaneWiring）では接続しません。
    }

    // RecordPane へのモデル割当て
    m_pane->setModels(m_d.recordModel, m_d.branchModel);

    // 旧NavigationControllerは廃止。新システム（KifuNavigationController）がボタン接続を担当する。
    // ボタン接続は MainWindow::initializeBranchNavigationClasses() で行われる。

    // 起動直後の「=== 開始局面 ===」ヘッダを重複なく用意（任意）
    if (m_d.recordModel) {
        const int rows = m_d.recordModel->rowCount();
        bool need = true;
        if (rows > 0) {
            const QModelIndex idx0 = m_d.recordModel->index(0, 0);
            const QString head = m_d.recordModel->data(idx0, Qt::DisplayRole).toString();
            if (head == tr("=== 開始局面 ===")) need = false;
        }
        if (need) {
            auto* item = new KifuDisplay(tr("=== 開始局面 ==="),
                                         tr("（１手 / 合計）"));
            if (rows == 0) {
                m_d.recordModel->appendItem(item);
            } else {
                m_d.recordModel->prependItem(item);
            }
        }

        // 開始局面（行0）をハイライト
        m_d.recordModel->setCurrentHighlightRow(0);

        // QTableViewの選択モデルで行0を選択（黄色表示のため）
        if (auto* kifuView = m_pane->kifuView()) {
            if (auto* sel = kifuView->selectionModel()) {
                const QModelIndex idx0 = m_d.recordModel->index(0, 0);
                sel->setCurrentIndex(idx0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                qCDebug(lcUi) << "initial row 0 selected";
            }
        }
    }
}
