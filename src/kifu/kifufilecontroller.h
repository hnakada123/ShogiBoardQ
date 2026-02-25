#ifndef KIFUFILECONTROLLER_H
#define KIFUFILECONTROLLER_H

/// @file kifufilecontroller.h
/// @brief 棋譜ファイル操作（開く・保存・上書き・貼り付け・自動保存）を担当するクラスの定義

#include <QObject>
#include <QPointer>
#include <functional>

#include "playmode.h"

class QWidget;
class QStatusBar;
class KifuExportController;
class KifuLoadCoordinator;
class KifuPasteDialog;

/**
 * @brief 棋譜ファイルI/O操作を担当するコントローラ
 *
 * 責務:
 * - 棋譜ファイルの選択と読み込み
 * - 棋譜ファイルの保存・上書き保存・自動保存
 * - クリップボードからの棋譜貼り付け
 * - SFEN局面の読み込み
 */
class KifuFileController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;           ///< 親ウィジェット（ダイアログ表示用）
        QStatusBar* statusBar = nullptr;           ///< ステータスバー
        QString* saveFileName = nullptr;           ///< 保存先ファイルパス（外部所有）

        // --- 準備コールバック ---
        std::function<void()> clearUiBeforeKifuLoad;           ///< UI状態クリア
        std::function<void(bool)> setReplayMode;               ///< リプレイモード設定
        std::function<void()> ensurePlayerInfoAndGameInfo;     ///< PlayerInfoWiring+GameInfoController 確保
        std::function<void()> ensureGameRecordModel;           ///< GameRecordModel 確保
        std::function<void()> ensureKifuExportController;      ///< KifuExportController 確保
        std::function<void()> updateKifuExportDependencies;    ///< KifuExportController 依存更新
        std::function<void()> createAndWireKifuLoadCoordinator; ///< KifuLoadCoordinator 生成・配線
        std::function<void()> ensureKifuLoadCoordinatorForLive; ///< ライブ用 KifuLoadCoordinator 確保

        // --- ゲッター ---
        std::function<KifuExportController*()> getKifuExportController; ///< KifuExportController 取得
        std::function<KifuLoadCoordinator*()> getKifuLoadCoordinator;   ///< KifuLoadCoordinator 取得
    };

    explicit KifuFileController(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    /// 棋譜ファイルを選択して開く
    void chooseAndLoadKifuFile();
    /// 名前を付けて保存
    void saveKifuToFile();
    /// 上書き保存
    void overwriteKifuFile();
    /// クリップボードから棋譜を貼り付け
    void pasteKifuFromClipboard();
    /// 棋譜貼り付けダイアログからの取り込み
    void onKifuPasteImportRequested(const QString& content);
    /// SFEN局面集から選択された局面を反映
    void onSfenCollectionPositionSelected(const QString& sfen);

public:
    /// 棋譜自動保存（MatchCoordinator フック用）
    void autoSaveKifuToFile(const QString& saveDir, PlayMode playMode,
                            const QString& humanName1, const QString& humanName2,
                            const QString& engineName1, const QString& engineName2);

private:
    void dispatchKifuLoad(const QString& filePath);

    Deps m_deps;
    QPointer<KifuPasteDialog> m_kifuPasteDialog;
};

#endif // KIFUFILECONTROLLER_H
