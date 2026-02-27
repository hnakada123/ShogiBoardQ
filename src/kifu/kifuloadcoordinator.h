#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

/// @file kifuloadcoordinator.h
/// @brief 棋譜読み込みコーディネータクラスの定義


#include <QObject>
#include <QTableWidget>
#include <QDockWidget>
#include <QStyledItemDelegate>
#include <functional>

#include "logcategories.h"
#include "kiftosfenconverter.h"
#include "branchtreemanager.h"
#include "shogiview.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"

class KifuBranchTree;
class KifuNavigationState;

/**
 * @brief 棋譜ファイルの読み込みと内部データ構築を統括するコーディネータ
 *
 * KIF/KI2/CSA/JKF/USEN/USI形式の棋譜ファイルを解析し、
 * SFEN列・表示データ・分岐ツリーなど内部データモデルを構築する。
 * 貼り付けテキストの自動判定読み込みにも対応。
 *
 */
class KifuLoadCoordinator : public QObject
{
    Q_OBJECT

public:
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

    /// 分岐あり行にオレンジ背景を描画するデリゲート
    class BranchRowDelegate : public QStyledItemDelegate {
    public:
        explicit BranchRowDelegate(QObject* parent = nullptr);
        ~BranchRowDelegate() override;
        void setMarkers(const QSet<int>* marks) { m_marks = marks; }
        void paint(QPainter* painter,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    private:
        const QSet<int>* m_marks = nullptr;  ///< 分岐あり手数のセット（非所有）
    };
    BranchRowDelegate* m_branchRowDelegate = nullptr;

    // --- 分岐候補（テキスト）側の索引 ---

    /// 分岐候補の情報
    struct BranchCandidate {
        QString text;  ///< 表示テキスト（例: 「▲２六歩(27)」）
        int row;       ///< 解決済み行番号
        int ply;       ///< 手数（1始まり）
    };

    // --- ファイル形式別読み込み ---

    void loadKifuFromFile(const QString& filePath);
    void loadJkfFromFile(const QString& filePath);
    void loadCsaFromFile(const QString& filePath);
    void loadKi2FromFile(const QString& filePath);
    void loadUsenFromFile(const QString& filePath);
    void loadUsiFromFile(const QString& filePath);

    // --- テキスト・局面読み込み ---

    /// 文字列から棋譜を読み込む（棋譜貼り付け機能用、形式を自動判定）
    bool loadKifuFromString(const QString& content);

    /// SFEN形式の局面を読み込む
    bool loadPositionFromSfen(const QString& sfenStr);

    /// BOD形式の局面を読み込む（内部でSFENに変換して処理）
    bool loadPositionFromBod(const QString& bodStr);

    // --- 外部オブジェクト設定 ---

    void setBranchTreeManager(BranchTreeManager* manager);
    void setShogiView(ShogiView* view) { m_shogiView = view; }
    void setBranchTree(KifuBranchTree* tree) { m_branchTree = tree; }
    void setNavigationState(KifuNavigationState* state) { m_navState = state; }

    // --- データアクセス ---

    /// USI指し手リストを取得（CSA出力用）
    const QStringList& kifuUsiMoves() const { return m_kifuUsiMoves; }

    /// USI指し手リストへのポインタを取得（棋譜解析用）
    QStringList* kifuUsiMovesPtr() { return &m_kifuUsiMoves; }

    // --- 分岐管理 ---

    /// 分岐コンテキストをリセット（対局終了時に使用）
    void resetBranchContext();

    /// 分岐ツリーを完全リセット（新規対局開始時に使用）
    void resetBranchTreeForNewGame();

signals:
    /// 棋譜読み込み中のエラーを通知する
    void errorOccurred(const QString& errorMessage);

    /// 棋譜の表示データが準備できた時に通知する
    void displayGameRecord(const QList<KifDisplayItem> disp);

    /// 指定手数での盤面同期とハイライト更新を要求する
    void syncBoardAndHighlightsAtRow(int ply1);

    /// ナビゲーション矢印ボタンの有効化を要求する
    void enableArrowButtons();

    /// 分岐候補のワイヤリング設定を要求する
    void setupBranchCandidatesWiring();

    /// 対局情報がテーブルに反映された時に通知する
    void gameInfoPopulated(const QList<KifGameInfoItem>& items);

    /// 分岐ツリーの構築が完了した時に通知する
    void branchTreeBuilt();

private:
    // --- 状態フラグ ---
    bool m_loadingKifu = false;               ///< 棋譜読み込み中フラグ（分岐更新を抑止）

    // --- UIウィジェット（非所有） ---
    QTableWidget* m_gameInfoTable;            ///< 対局情報テーブル（非所有）
    QDockWidget*  m_gameInfoDock;             ///< 対局情報ドック（非所有）
    BranchTreeManager* m_branchTreeManager = nullptr; ///< 分岐ツリーマネージャー（setBranchTreeManager()経由で設定、非所有）
    QTabWidget* m_tab;                        ///< メインタブウィジェット（非所有）
    ShogiView* m_shogiView = nullptr;         ///< 盤面ビュー（setShogiView()経由で設定、非所有）

    // --- 棋譜データ ---
    QStringList m_kifuUsiMoves;               ///< 棋譜から読み込んだUSI形式の指し手リスト
    QStringList* m_sfenHistory;                ///< 局面SFEN列への参照（非所有）
    QVector<ShogiMove>& m_gameMoves;          ///< ゲーム指し手列への参照（MainWindowと共有）
    QStringList& m_positionStrList;           ///< USI positionコマンド列への参照（MainWindowと共有）
    QList<KifDisplayItem> m_dispMain;         ///< 本譜の表示データスナップショット
    QList<KifDisplayItem> m_dispCurrent;      ///< 現在表示中の表示データ
    QStringList           m_sfenMain;         ///< 本譜のSFEN列スナップショット
    QVector<ShogiMove>    m_gmMain;           ///< 本譜のゲーム指し手スナップショット
    QHash<int, QList<KifLine>> m_variationsByPly; ///< ply→変化リストのマップ
    QList<KifLine> m_variationsSeq;           ///< 変化の入力順リスト
    RecordPane* m_recordPane;                 ///< 棋譜ペイン（非所有）

    // --- ナビゲーション状態（参照で共有） ---
    int& m_activePly;                         ///< 現在のアクティブ手数
    int& m_currentSelectedPly;                ///< 現在選択中の手数
    int& m_currentMoveIndex;                  ///< 現在の指し手インデックス

    // --- モデル ---
    KifuRecordListModel* m_kifuRecordModel;   ///< 棋譜レコードモデル（非所有）
    KifuBranchListModel* m_kifuBranchModel;   ///< 分岐リストモデル（非所有）

    // --- 分岐管理 ---
    int m_branchPlyContext = -1;              ///< 分岐コンテキストの手数（-1は無効）
    QSet<int> m_branchablePlySet;             ///< 現在のラインで分岐可能な手数のセット
    KifuBranchTree* m_branchTree = nullptr;   ///< 分岐ツリー（非所有）
    KifuNavigationState* m_navState = nullptr; ///< ナビゲーション状態（非所有）

    // --- 内部ヘルパ ---
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

    /// 現在表示中のラインの分岐手マーカーをモデルへ反映
    void applyBranchMarksForCurrentLine();

    void applyParsedResultCommon(const QString& filePath, const QString& initialSfen,
                                  const QString& teaiLabel, const KifParseResult& res,
                                  const QString& parseWarn, const char* callerTag);

    // --- 棋譜読み込み共通ロジック ---
    using KifuParseFunc = std::function<bool(const QString&, KifParseResult&, QString*)>;
    using KifuDetectSfenFunc = std::function<QString(const QString&, QString*)>;
    using KifuExtractGameInfoFunc = std::function<QList<KifGameInfoItem>(const QString&)>;

    /// 棋譜読み込みの共通フロー（解析→データ構築→UI反映）
    void loadKifuCommon(const QString& filePath, const char* funcName,
                         const KifuParseFunc& parseFunc,
                         const KifuDetectSfenFunc& detectSfenFunc,
                         const KifuExtractGameInfoFunc& extractGameInfoFunc,
                         bool dumpVariations);
};

#endif // KIFULOADCOORDINATOR_H
