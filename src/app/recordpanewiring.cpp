#include "recordpanewiring.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "navigationcontroller.h"
#include "mainwindow.h"

#include <QModelIndex>
#include <QDebug>

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

        // ★重要★
        // branchActivated は BranchCandidatesController 側で配線する設計なので、
        // ここ（RecordPaneWiring）では接続しません。
        // （以前の MainWindow::onRecordPaneBranchActivated への接続行は削除）
    }

    // RecordPane へのモデル割当て
    m_pane->setModels(m_d.recordModel, m_d.branchModel);

    // ナビゲーションボタン束
    NavigationController::Buttons btns{
        m_pane->firstButton(),
        m_pane->back10Button(),
        m_pane->prevButton(),
        m_pane->nextButton(),
        m_pane->fwd10Button(),
        m_pane->lastButton(),
    };

    // NavigationController 生成（INavigationContext* が必須）
    if (!m_nav) {
        INavigationContext* ctx = m_d.navCtx;
        if (!ctx) ctx = dynamic_cast<INavigationContext*>(m_d.ctx);
        if (!ctx) {
            qWarning() << "[RecordPaneWiring] navCtx is null. NavigationController will not be created.";
        } else {
            m_nav = new NavigationController(btns, ctx);
        }
    }

    // 起動直後の「=== 開始局面 ===」ヘッダを重複なく用意（任意）
    if (m_d.recordModel) {
        const int rows = m_d.recordModel->rowCount();
        bool need = true;
        if (rows > 0) {
            const QModelIndex idx0 = m_d.recordModel->index(0, 0);
            const QString head = m_d.recordModel->data(idx0, Qt::DisplayRole).toString();
            if (head == QStringLiteral("=== 開始局面 ===")) need = false;
        }
        if (need) {
            auto* item = new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                         QStringLiteral("（１手 / 合計）"));
            if (rows == 0) {
                m_d.recordModel->appendItem(item);
            } else {
                m_d.recordModel->prependItem(item);
            }
        }

        // ★ 追加：開始局面（行0）をハイライト
        m_d.recordModel->setCurrentHighlightRow(0);
    }
}
