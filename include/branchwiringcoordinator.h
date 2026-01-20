#ifndef BRANCHWIRINGCOORDINATOR_H
#define BRANCHWIRINGCOORDINATOR_H

#include <QObject>
#include <QModelIndex>

class RecordPane;
class KifuBranchListModel;
class KifuVariationEngine;
class BranchCandidatesController;
class KifuLoadCoordinator;

class BranchWiringCoordinator : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        RecordPane*              recordPane = nullptr;
        KifuBranchListModel*     branchModel = nullptr;    // nullなら内部で生成
        KifuVariationEngine*     variationEngine = nullptr;
        KifuLoadCoordinator*     kifuLoader = nullptr;     // applyResolvedRowAndSelect の委譲先
        QObject*                 parent = nullptr;
    };

    explicit BranchWiringCoordinator(const Deps& d);

    // MainWindow::setupBranchView_ 相当
    void setupBranchView();

    // MainWindow::setupBranchCandidatesWiring_ 相当
    void setupBranchCandidatesWiring();

    // ★ 追加：後から Loader を差し替えるための軽量 setter
    void setKifuLoader(KifuLoadCoordinator* loader);

public slots:
    // MainWindow::onRecordPaneBranchActivated_ 相当
    void onRecordPaneBranchActivated(const QModelIndex& index);

    // MainWindow::onBranchPlanActivated_ 相当
    void onBranchPlanActivated(int row, int ply1);

    // MainWindow::onBranchNodeActivated_ 相当
    void onBranchNodeActivated(int row, int ply);

private:
    void applyResolvedRowAndSelect(int row, int selPly);

private:
    RecordPane*                  m_recordPane = nullptr;
    KifuBranchListModel*         m_branchModel = nullptr;
    KifuVariationEngine*         m_varEngine = nullptr;
    KifuLoadCoordinator*         m_loader = nullptr;

    BranchCandidatesController*  m_branchCtl = nullptr;
};

#endif // BRANCHWIRINGCOORDINATOR_H
