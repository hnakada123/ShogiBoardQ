#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

#include <QObject>
#include <QTableWidget>
#include <QDockWidget>
#include <QStyledItemDelegate>
#include <functional>

#include "kiftosfenconverter.h"
#include "engineanalysistab.h"
#include "shogiview.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"

class KifuBranchTree;
class KifuNavigationState;
class EngineAnalysisTab;

class KifuLoadCoordinator : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                        QStringList& positionStrList,
                        int& activePly,
                        int& currentSelectedPly,
                        int& currentMoveIndex,
                        QStringList* sfenRecord,
                        QTableWidget* gameInfoTable,
                        QDockWidget* gameInfoDock,
                        QTabWidget* tab,
                        RecordPane* recordPane,
                        KifuRecordListModel* kifuRecordModel,
                        KifuBranchListModel* kifuBranchModel,
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

    void loadJkfFromFile(const QString& filePath);

    void loadCsaFromFile(const QString& filePath);

    void loadKi2FromFile(const QString& filePath);

    void loadUsenFromFile(const QString& filePath);

    void loadUsiFromFile(const QString& filePath);

    // ★ 追加: 文字列から棋譜を読み込む（棋譜貼り付け機能用）
    bool loadKifuFromString(const QString& content);

    // ★ 追加: SFEN形式の局面を読み込む
    bool loadPositionFromSfen(const QString& sfenStr);

    // ★ 追加: BOD形式の局面を読み込む
    bool loadPositionFromBod(const QString& bodStr);

    void setAnalysisTab(EngineAnalysisTab* tab);

    void setShogiView(ShogiView* view) { m_shogiView = view; }

    // ★ 新規: KifuBranchTreeを設定
    void setBranchTree(KifuBranchTree* tree) { m_branchTree = tree; }

    // ★ 新規: KifuNavigationStateを設定
    void setNavigationState(KifuNavigationState* state) { m_navState = state; }

    // USI指し手リストを取得（CSA出力用）- 棋譜から読み込んだ指し手
    const QStringList& kifuUsiMoves() const { return m_kifuUsiMoves; }
    
    // USI指し手リストへのポインタを取得（棋譜解析用）- 棋譜から読み込んだ指し手
    QStringList* kifuUsiMovesPtr() { return &m_kifuUsiMoves; }

    // ★ 追加：分岐コンテキストをリセット（対局終了時に使用）
    void resetBranchContext();

    // ★ 追加：分岐ツリーを完全リセット（新規対局開始時に使用）
    //   平手・駒落ちなど、「現在の局面」以外から対局を開始する場合に呼び出す
    void resetBranchTreeForNewGame();

signals:
    void errorOccurred(const QString& errorMessage);
    void setReplayMode(bool on);
    void displayGameRecord(const QList<KifDisplayItem> disp);
    void syncBoardAndHighlightsAtRow(int ply1);
    void enableArrowButtons();
    void setupBranchCandidatesWiring();
    void gameInfoPopulated(const QList<KifGameInfoItem>& items);  // ★ 追加
    void branchTreeBuilt();  // ★ 新規: 分岐ツリーが構築された時

private:
    bool m_loadingKifu = false;
    QTableWidget* m_gameInfoTable;
    QDockWidget*  m_gameInfoDock;
    EngineAnalysisTab* m_analysisTab = nullptr;  // setAnalysisTab() 経由で設定
    QTabWidget* m_tab;
    ShogiView* m_shogiView = nullptr;            // setShogiView() 経由で設定
    QStringList m_kifuUsiMoves;  // 棋譜から読み込んだUSI形式の指し手リスト
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
    int& m_activePly;
    int& m_currentSelectedPly;
    int& m_currentMoveIndex;
    KifuRecordListModel* m_kifuRecordModel;
    KifuBranchListModel* m_kifuBranchModel;
    int m_branchPlyContext = -1;
    QSet<int> m_branchablePlySet;
    KifuBranchTree* m_branchTree = nullptr;       // ★ 分岐ツリー
    KifuNavigationState* m_navState = nullptr;    // ★ ナビゲーション状態

    QString prepareInitialSfen(const QString& filePath, QString& teaiLabel) const;
    void populateGameInfo(const QList<KifGameInfoItem>& items);
    void addGameInfoTabIfMissing();
    void applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items);
    QString findGameInfoValue(const QList<KifGameInfoItem>& items, const QStringList& keys) const;
    void rebuildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal);
    void rebuildGameMoves(const QString& initialSfen, const QStringList& usiMoves);
    void updateKifuBranchMarkersForActiveRow();
    void ensureBranchRowDelegateInstalled();
    void logImportSummary(const QString& filePath,
                          const QStringList& usiMoves,
                          const QList<KifDisplayItem>& disp,
                          const QString& teaiLabel,
                          const QString& warnParse,
                          const QString& warnConvert) const;

    // ★ 追加：現在表示中の行（m_activeResolvedRow）の分岐手をモデルへ反映
    void applyBranchMarksForCurrentLine();

    void applyParsedResultCommon(const QString& filePath, const QString& initialSfen,
                                  const QString& teaiLabel, const KifParseResult& res,
                                  const QString& parseWarn, const char* callerTag);

    // 棋譜読み込み用の関数型定義
    using KifuParseFunc = std::function<bool(const QString&, KifParseResult&, QString*)>;
    using KifuDetectSfenFunc = std::function<QString(const QString&, QString*)>;
    using KifuExtractGameInfoFunc = std::function<QList<KifGameInfoItem>(const QString&)>;

    // 棋譜読み込み共通ロジック
    void loadKifuCommon(const QString& filePath, const char* funcName,
                         const KifuParseFunc& parseFunc,
                         const KifuDetectSfenFunc& detectSfenFunc,
                         const KifuExtractGameInfoFunc& extractGameInfoFunc,
                         bool dumpVariations);
};

#endif // KIFULOADCOORDINATOR_H
