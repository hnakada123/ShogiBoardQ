#ifndef MAINWINDOWFOUNDATIONREGISTRY_H
#define MAINWINDOWFOUNDATIONREGISTRY_H

/// @file mainwindowfoundationregistry.h
/// @brief ドメイン横断的に利用される共通基盤 ensure* メソッドを管理するレジストリ
///
/// MainWindowServiceRegistry から抽出された、複数ドメインで共有される
/// Tier 0/1 の ensure* メソッドと、依存のない単純リーフ ensure* メソッドを集約する。
///
/// 設計根拠: docs/dev/ensure-graph-analysis.md Section 8.1 (案A: 共通基盤層抽出)

#include <QObject>

class MainWindow;
class MainWindowServiceRegistry;

/**
 * @brief ドメイン横断的な共通基盤 ensure* メソッドのレジストリ
 *
 * 責務:
 * - 複数ドメイン（UI/Game/Kifu/Analysis/Board）から参照される
 *   共有コンポーネントの遅延初期化を集約する
 * - 依存のない単純リーフ ensure* メソッドも含む
 *
 * ServiceRegistry との関係:
 * - ServiceRegistry が所有し、m_foundation 経由でアクセスする
 * - Foundation のメソッドは他の ensure* を呼ばない（Tier 0/1 のみ）
 */
class MainWindowFoundationRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowFoundationRegistry(MainWindow& mw,
                                         MainWindowServiceRegistry* serviceRegistry,
                                         QObject* parent = nullptr);

    // ===== 共有基盤（複数ドメインが依存） =====
    void ensureUiStatePolicyManager();
    void ensurePlayerInfoWiring();
    void ensurePlayerInfoController();
    void ensureKifuNavigationCoordinator();
    void ensureCommentCoordinator();
    void ensureBoardSyncPresenter();
    void ensureUiNotificationService();
    void ensureEvaluationGraphController();

    // ===== 単純リーフ（Tier 0、他の ensure* への依存なし） =====
    void ensureMenuWiring();
    void ensureLanguageController();
    void ensureDockCreationService();
    void ensurePositionEditController();
    void ensureAnalysisPresenter();
    void ensureJishogiController();
    void ensureNyugyokuHandler();

private:
    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry* m_serviceRegistry;  ///< 親 ServiceRegistry（シグナル接続用）
};

#endif // MAINWINDOWFOUNDATIONREGISTRY_H
