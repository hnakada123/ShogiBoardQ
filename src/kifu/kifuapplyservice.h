#ifndef KIFUAPPLYSERVICE_H
#define KIFUAPPLYSERVICE_H

/// @file kifuapplyservice.h
/// @brief 棋譜解析結果のモデル適用サービスの定義

#include <QObject>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QSet>
#include <functional>

#include "kifparsetypes.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

struct KifuLoadResult;
class QTableWidget;
class ShogiView;
class RecordPane;
class BranchTreeManager;
class KifuRecordListModel;
class KifuBranchListModel;
class KifuBranchTree;
class KifuNavigationState;

/**
 * @brief 棋譜解析結果をモデル・UIに適用するサービス
 *
 * KifuLoadCoordinator から「適用層」を分離したクラス。
 * 解析済みデータ（SFEN列・表示データ・分岐）をモデルへ反映し、
 * 必要なUI通知をHooksコールバック経由で発行する。
 *
 * MatchCoordinator系のRefs/Hooksパターンに従い、
 * 状態参照はRefsで、UI通知はHooksで注入する。
 */
class KifuApplyService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief KifuLoadCoordinator の内部状態への参照群
     */
    struct Refs {
        // ナビゲーション状態（直接ポインタ: アドレス不変）
        bool* loadingKifu = nullptr;

        // UIウィジェット（直接ポインタ: コンストラクタで確定）
        QTableWidget* gameInfoTable = nullptr;
        RecordPane* recordPane = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        KifuBranchListModel* kifuBranchModel = nullptr;

        // セッター経由で変化するポインタ（二重ポインタ）
        ShogiView** shogiView = nullptr;
        BranchTreeManager** branchTreeManager = nullptr;
        KifuBranchTree** branchTree = nullptr;
        KifuNavigationState** navState = nullptr;

        // KifuBranchTree 作成時の親オブジェクト
        QObject* treeParent = nullptr;
    };

    /**
     * @brief UI通知・シグナル発行用コールバック群
     */
    struct Hooks {
        std::function<void(const QList<KifDisplayItem>&)> displayGameRecord;
        std::function<void(int)> syncBoardAndHighlightsAtRow;
        std::function<void()> enableArrowButtons;
        std::function<void(const QList<KifGameInfoItem>&)> gameInfoPopulated;
        std::function<void()> branchTreeBuilt;
        std::function<void(const QString&)> errorOccurred;
    };

    explicit KifuApplyService(QObject* parent = nullptr);

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 適用API ---

    /// 解析結果をモデル・UIに一括適用する
    /// @return 適用に成功した場合 true（指し手が無ければ false）
    [[nodiscard]] bool applyParsedResult(const QString& filePath, const QString& initialSfen,
                                         const QString& teaiLabel, const KifParseResult& res,
                                         const QString& parseWarn, const char* callerTag);

    /// SFEN形式の局面を読み込んでモデルに適用する
    [[nodiscard]] bool loadPositionFromSfen(const QString& sfenStr);

    /// BOD形式の局面を読み込んでモデルに適用する
    [[nodiscard]] bool loadPositionFromBod(const QString& bodStr);

    /// 対局情報をテーブルに反映する
    void populateGameInfo(const QList<KifGameInfoItem>& items);

    /// 対局情報から先手/後手名を盤面ビューに反映する
    void applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items);

    /// 現在表示中のラインの分岐手マーカーをモデルへ反映する
    void applyBranchMarksForCurrentLine();

private:
    bool validateParsedResult(const QString& filePath, const QStringList& usiMoves,
                              const QList<KifDisplayItem>& disp,
                              bool hasTerminal, const char* callerTag);
    void applyToRecordView(const KifParseResult& res, const QString& filePath,
                           const QString& teaiLabel, const QString& parseWarn);
    void applyBranchTree(const KifParseResult& res, const QString& initialSfen);
    void addGameInfoTabIfMissing();
    QString findGameInfoValue(const QList<KifGameInfoItem>& items, const QStringList& keys) const;

    Refs m_refs;
    Hooks m_hooks;
};

#endif // KIFUAPPLYSERVICE_H
