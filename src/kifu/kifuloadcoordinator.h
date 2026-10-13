#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

/// @file kifuloadcoordinator.h
/// @brief 棋譜読み込みコーディネータクラスの定義


#include <QObject>
#include <QTableWidget>
#include <functional>
#include <QFutureWatcher>
#include "kifuloadparser.h"

#include "logcategories.h"
#include "kiftosfenconverter.h"
#include "branchtreemanager.h"
#include "shogiview.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"

class KifuBranchTree;
class KifuNavigationState;
class KifuApplyService;

/**
 * @brief 棋譜ファイルの読み込みと内部データ構築を統括するコーディネータ
 *
 * KIF/KI2/CSA/JKF/USEN/USI形式の棋譜ファイルを解析し、
 * SFEN列・表示データ・分岐ツリーなど内部データモデルを構築する。
 * 貼り付けテキストの自動判定読み込みにも対応。
 *
 * I/O層は KifuFileReader、適用層は KifuApplyService に委譲し、
 * 本クラスは解析→データ構築→UI反映の3段パイプラインの調整役に専念する。
 */
class KifuLoadCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit KifuLoadCoordinator(QTableWidget* gameInfoTable,
                        RecordPane* recordPane,
                        KifuRecordListModel* kifuRecordModel,
                        KifuBranchListModel* kifuBranchModel,
                        QObject* parent=nullptr);

    ~KifuLoadCoordinator() override;

    void loadFileAsync(const QString& filePath);
    void loadTextAsync(const QString& content);
    void cancelLoad();

    // --- 分岐候補（テキスト）側の索引 ---

    /// 分岐候補の情報
    struct BranchCandidate {
        QString text;  ///< 表示テキスト（例: 「▲２六歩(27)」）
        int row;       ///< 解決済み行番号
        int ply;       ///< 手数（1始まり）
    };

    // --- ファイル形式別読み込み ---
    // 戻り値: 読み込みと適用に成功した場合 true（失敗時は errorOccurred を発行済み）

    [[nodiscard]] bool loadKifuFromFile(const QString& filePath);
    [[nodiscard]] bool loadJkfFromFile(const QString& filePath);
    [[nodiscard]] bool loadCsaFromFile(const QString& filePath);
    [[nodiscard]] bool loadKi2FromFile(const QString& filePath);
    [[nodiscard]] bool loadUsenFromFile(const QString& filePath);
    [[nodiscard]] bool loadUsiFromFile(const QString& filePath);

    // --- テキスト・局面読み込み ---

    /// 文字列から棋譜を読み込む（棋譜貼り付け機能用、形式を自動判定）
    [[nodiscard]] bool loadKifuFromString(const QString& content);

    /// SFEN形式の局面を読み込む
    [[nodiscard]] bool loadPositionFromSfen(const QString& sfenStr);

    /// BOD形式の局面を読み込む（内部でSFENに変換して処理）
    [[nodiscard]] bool loadPositionFromBod(const QString& bodStr);

    // --- 外部オブジェクト設定 ---

    void setBranchTreeManager(BranchTreeManager* manager);
    void setShogiView(ShogiView* view) { m_shogiView = view; }
    void setBranchTree(KifuBranchTree* tree) { m_branchTree = tree; }
    void setNavigationState(KifuNavigationState* state) { m_navState = state; }

    // --- 分岐管理 ---

    /// 分岐ツリーを完全リセット（新規対局開始時に使用）
    void resetBranchTreeForNewGame();

signals:
    void loadFinished(bool success);

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
    void initApplyService();

    // --- 状態フラグ ---
    bool m_loadingKifu = false;               ///< 棋譜読み込み中フラグ（分岐更新を抑止）

    // --- UIウィジェット（非所有） ---
    QTableWidget* m_gameInfoTable;            ///< 対局情報テーブル（非所有）
    BranchTreeManager* m_branchTreeManager = nullptr; ///< 分岐ツリーマネージャー
    ShogiView* m_shogiView = nullptr;         ///< 盤面ビュー（非所有）

    // --- 棋譜データ ---
    RecordPane* m_recordPane;                 ///< 棋譜ペイン（非所有）

    // --- モデル ---
    KifuRecordListModel* m_kifuRecordModel;   ///< 棋譜レコードモデル（非所有）
    KifuBranchListModel* m_kifuBranchModel;   ///< 分岐リストモデル（非所有）

    // --- 分岐管理 ---
    KifuBranchTree* m_branchTree = nullptr;   ///< 分岐ツリー（非所有）
    KifuNavigationState* m_navState = nullptr; ///< ナビゲーション状態（非所有）

    // --- 適用サービス ---
    KifuApplyService* m_applyService = nullptr; ///< 適用層サービス（Qt parent所有）

    // --- 内部ヘルパ ---
    bool applyLoadResult(const KifuLoadResult& result);
    void startLoad(const QString& input, bool text);
    QFutureWatcher<KifuLoadResult>* m_loadWatcher = nullptr;
    CancelFlag m_loadCancel;

private slots:
    void onLoadFinished();
};

#endif // KIFULOADCOORDINATOR_H
