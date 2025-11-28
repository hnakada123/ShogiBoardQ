#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

#include <QObject>
#include <QTableWidget>
#include <QDockWidget>
#include <QStyledItemDelegate>

#include "kiftosfenconverter.h"
#include "engineanalysistab.h"
#include "shogiview.h"
#include "recordpane.h"
#include "branchcandidatescontroller.h"
#include "kifurecordlistmodel.h"
#include "kifutypes.h"
#include "branchdisplayplan.h"

class NavigationPresenter;
class EngineAnalysisTab;

class KifuLoadCoordinator : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                        QVector<ResolvedRow>& resolvedRows,
                        QStringList& positionStrList,
                        int& activeResolvedRow,
                        int& activePly,
                        int& currentSelectedPly,
                        int& currentMoveIndex,
                        QStringList* sfenRecord,
                        QTableWidget* gameInfoTable,
                        QDockWidget* gameInfoDock,
                        EngineAnalysisTab* analysisTab,
                        QTabWidget* tab,
                        ShogiView* shogiView,
                        RecordPane* recordPane,
                        KifuRecordListModel* kifuRecordModel,
                        KifuBranchListModel* kifuBranchModel,
                        BranchCandidatesController* branchCtl,
                        QTableView* kifuBranchView,
                        QHash<int, QMap<int, ::BranchCandidateDisplay>>& branchDisplayPlan,
                        QObject* parent=nullptr);

    // --- 装飾（棋譜テーブル マーカー描画） ---
    class BranchRowDelegate : public QStyledItemDelegate {
    public:
        explicit BranchRowDelegate(QObject* parent = nullptr);
        ~BranchRowDelegate() override;        // ← 宣言だけにする
        void setMarkers(const QSet<int>* marks) { m_marks = marks; }
        void paint(QPainter* painter,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    private:
        const QSet<int>* m_marks = nullptr;
    };
    BranchRowDelegate* m_branchRowDelegate = nullptr;

    // --- 分岐候補（テキスト）側の索引 ---
    struct BranchCandidate {
        QString text;  // 「▲２六歩(27)」
        int row;       // resolved 行
        int ply;       // 1始まり
    };

    void loadKifuFromFile(const QString& filePath);

    void loadCsaFromFile(const QString& filePath);

    void setAnalysisTab(EngineAnalysisTab* tab);

    void setBranchCandidatesController(BranchCandidatesController* ctl);

    void updateBranchTreeFromLive(int currentPly);

    // ライブ対局用：分岐Plan・＋/オレンジ強調・候補欄を一括更新
    void rebuildBranchPlanAndMarksForLive(int currentPly);

public slots:
    void applyResolvedRowAndSelect(int row, int selPly);

    // ★ 新規公開：行(row)・手数(ply1)の分岐候補を“表示のみ”更新
    //   （内部で showBranchCandidatesFromPlan を呼ぶラッパー）
    void showBranchCandidates(int row, int ply1);

    // 棋譜テーブルの「本譜行」選択が変わったときに呼ぶ
    void onMainMoveRowChanged(int selPly);

private slots:
    void onBackToMainButtonClicked_();

signals:
    void errorOccurred(const QString& errorMessage);
    void setReplayMode(bool on);
    void displayGameRecord(const QList<KifDisplayItem> disp);
    void syncBoardAndHighlightsAtRow(int ply1);
    void enableArrowButtons();
    void setupBranchCandidatesWiring_();


private:
    bool m_loadingKifu = false;
    QTableWidget* m_gameInfoTable;
    QDockWidget*  m_gameInfoDock;
    EngineAnalysisTab* m_analysisTab;
    QTabWidget* m_tab;
    ShogiView* m_shogiView;
    QStringList m_usiMoves;
    QStringList* m_sfenRecord;
    QVector<ShogiMove>& m_gameMoves;
    QStringList& m_positionStrList;
    QList<KifDisplayItem> m_dispMain;
    QList<KifDisplayItem> m_dispCurrent;
    QStringList           m_sfenMain;
    QVector<ShogiMove>    m_gmMain;
    QHash<int, QList<KifLine>> m_variationsByPly;
    QList<KifLine> m_variationsSeq;
    RecordPane* m_recordPane;
    QVector<ResolvedRow>& m_resolvedRows;
    int& m_activeResolvedRow;
    int& m_activePly;
    int& m_currentSelectedPly;
    int& m_currentMoveIndex;
    KifuRecordListModel* m_kifuRecordModel;
    KifuBranchListModel* m_kifuBranchModel;
    BranchCandidatesController* m_branchCtl;
    QTableView* m_kifuBranchView;
    int m_branchPlyContext = -1;
    QSet<int> m_branchablePlySet;
    QHash<int, QHash<QString, QList<BranchCandidate>>> m_branchIndex;
    // 行(row) → (ply → 表示計画) の保持
    // 例: m_branchDisplayPlan[row][ply]
    QHash<int, QMap<int, BranchCandidateDisplay>>& m_branchDisplayPlan;
    std::unique_ptr<KifuVariationEngine> m_varEngine;
    bool m_branchTreeLocked = false;  // ← 分岐ツリーの追加・変更を禁止するロック
    NavigationPresenter* m_navPresenter = nullptr;

    QString prepareInitialSfen(const QString& filePath, QString& teaiLabel) const;
    void populateGameInfo(const QList<KifGameInfoItem>& items);
    void addGameInfoTabIfMissing();
    void applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items);
    QString findGameInfoValue(const QList<KifGameInfoItem>& items, const QStringList& keys) const;
    void rebuildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal);
    void rebuildGameMoves(const QString& initialSfen, const QStringList& usiMoves);
    void showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly);
    void showBranchCandidatesFromPlan(int row, int ply1);
    void updateKifuBranchMarkersForActiveRow();
    void ensureBranchRowDelegateInstalled();
    void logImportSummary(const QString& filePath,
                          const QStringList& usiMoves,
                          const QList<KifDisplayItem>& disp,
                          const QString& teaiLabel,
                          const QString& warnParse,
                          const QString& warnConvert) const;
    void buildResolvedLinesAfterLoad();
    void dumpBranchSplitReport() const;
    QString rowNameFor_(int row) const;
    QString labelAt_(const ResolvedRow& rr, int ply) const;
    bool prefixEqualsUpTo_(int rowA, int rowB, int p) const;

    void dumpBranchCandidateDisplayPlan() const;
    void ensureResolvedRowsHaveFullSfen();
    void ensureResolvedRowsHaveFullGameMoves();
    void dumpAllRowsSfenTable() const;
    void dumpAllLinesGameMoves() const;
    void buildBranchCandidateDisplayPlan();
    void ensureNavigationPresenter_();

    // 追加（private）
    QList<KifDisplayItem> collectDispFromRecordModel_() const;

    // ★ 追加：現在表示中の行（m_activeResolvedRow）の分岐手をモデルへ反映
    void applyBranchMarksForCurrentLine_();

    void refreshBranchCandidatesUIOnly_(int row, int ply1); // 文脈を変えずUIだけ更新
};

#endif // KIFULOADCOORDINATOR_H
