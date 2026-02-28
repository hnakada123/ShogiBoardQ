#ifndef KIFUEXPORTCONTROLLER_H
#define KIFUEXPORTCONTROLLER_H

/// @file kifuexportcontroller.h
/// @brief 棋譜エクスポートコントローラクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPoint>
#include <functional>
#include <optional>

#include "mainwindow.h"  // PlayMode enum
#include "gamerecordmodel.h"  // GameRecordModel::ExportContext
#include "kifutypes.h"  // ResolvedRow

class QWidget;
class QTableWidget;
class QStatusBar;
class GameRecordModel;
class KifuRecordListModel;
class GameInfoPaneController;
class TimeControlController;
class KifuLoadCoordinator;
class GameRecordPresenter;
class MatchCoordinator;
class ReplayController;
class ShogiGameController;
struct ShogiMove;

/**
 * @brief KifuExportController - 棋譜エクスポートの管理クラス
 *
 * MainWindowから棋譜エクスポート関連の処理を分離したクラス。
 * 以下の責務を担当:
 * - 各種形式でのファイル保存（KIF/KI2/CSA/JKF/USEN/USI）
 * - クリップボードへのコピー
 * - USI指し手リストの生成
 * - BOD形式の生成
 */
class KifuExportController : public QObject
{
    Q_OBJECT

public:
    explicit KifuExportController(QWidget* parentWidget, QObject* parent = nullptr);
    ~KifuExportController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------

    /**
     * @brief エクスポートに必要な依存オブジェクトを一括設定
     */
    struct Dependencies {
        GameRecordModel* gameRecord = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        TimeControlController* timeController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        GameRecordPresenter* recordPresenter = nullptr;
        MatchCoordinator* match = nullptr;
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;
        QStatusBar* statusBar = nullptr;

        // データソース（ポインタ）
        QStringList* sfenRecord = nullptr;
        QStringList* usiMoves = nullptr;
        QVector<ResolvedRow>* resolvedRows = nullptr;
        QVector<QString>* commentsByRow = nullptr;

        // 状態値
        QString startSfenStr;
        PlayMode playMode = PlayMode::NotStarted;
        QString humanName1;
        QString humanName2;
        QString engineName1;
        QString engineName2;
        int activeResolvedRow = 0;
        int currentMoveIndex = 0;
        int activePly = 0;
        int currentSelectedPly = 0;
    };

    void setDependencies(const Dependencies& deps);

    /**
     * @brief 準備コールバックを設定
     *
     * クリップボードコピーやファイル保存の前に呼び出されるコールバック。
     * 依存関係の更新などを行う。
     */
    void setPrepareCallback(std::function<void()> callback);

    /**
     * @brief 状態値を更新（呼び出し時点の値を反映）
     */
    void updateState(const QString& startSfen, PlayMode mode,
                     const QString& human1, const QString& human2,
                     const QString& engine1, const QString& engine2,
                     int activeRow, int moveIndex, int aPly, int selPly);

    // --------------------------------------------------------
    // ファイル保存
    // --------------------------------------------------------

    /**
     * @brief 棋譜をファイルに保存（ダイアログ表示）
     * @return 保存したファイルパス（キャンセル時は空）
     */
    QString saveToFile();

    /**
     * @brief 棋譜を上書き保存
     * @param filePath 保存先ファイルパス
     * @return 成功時true
     */
    [[nodiscard]] bool overwriteFile(const QString& filePath);

    // --------------------------------------------------------
    // クリップボードコピー
    // --------------------------------------------------------

    /** @brief KIF形式でクリップボードにコピー */
    [[nodiscard]] bool copyKifToClipboard();

    /** @brief KI2形式でクリップボードにコピー */
    [[nodiscard]] bool copyKi2ToClipboard();

    /** @brief CSA形式でクリップボードにコピー */
    [[nodiscard]] bool copyCsaToClipboard();

    /** @brief USI形式（全手）でクリップボードにコピー */
    [[nodiscard]] bool copyUsiToClipboard();

    /** @brief USI形式（現在の手まで）でクリップボードにコピー */
    [[nodiscard]] bool copyUsiCurrentToClipboard();

    /** @brief JKF形式でクリップボードにコピー */
    [[nodiscard]] bool copyJkfToClipboard();

    /** @brief USEN形式でクリップボードにコピー */
    [[nodiscard]] bool copyUsenToClipboard();

    /** @brief SFEN形式で現在局面をクリップボードにコピー */
    [[nodiscard]] bool copySfenToClipboard();

    /** @brief BOD形式で現在局面をクリップボードにコピー */
    [[nodiscard]] bool copyBodToClipboard();

    /**
     * @brief 指定ディレクトリへ自動保存（ダイアログなし）
     * @param saveDir 保存先ディレクトリ
     * @return 成功時は保存されたファイルパス、失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<QString> autoSaveToDir(const QString& saveDir);

    // --------------------------------------------------------
    // ユーティリティ
    // --------------------------------------------------------

    /**
     * @brief SFENレコードからUSI指し手リストを生成
     */
    QStringList sfenRecordToUsiMoves() const;

    /**
     * @brief ShogiMoveリストからUSI指し手リストを生成
     */
    static QStringList gameMovesToUsiMoves(const QVector<ShogiMove>& moves);

    /**
     * @brief 現在対局中かどうかを判定
     */
    bool isCurrentlyPlaying() const;

    /**
     * @brief 現在の手数を取得
     */
    int currentPly() const;

Q_SIGNALS:
    /**
     * @brief ステータスバーにメッセージを表示
     */
    void statusMessage(const QString& message, int timeout);

private:
    /**
     * @brief USI指し手リストを取得（複数ソースから優先順位で）
     */
    QStringList resolveUsiMoves() const;

    /**
     * @brief GameRecordModel用のExportContextを構築
     */
    GameRecordModel::ExportContext buildExportContext() const;

    /**
     * @brief SFEN文字列から現在の局面データを取得
     */
    struct PositionData {
        QString sfenStr;
        int moveIndex = 0;
        QString lastMoveStr;
    };
    PositionData getCurrentPositionData() const;

    /**
     * @brief BOD形式の文字列を生成
     */
    QString generateBodText(const PositionData& pos) const;

    QWidget* m_parentWidget = nullptr;
    Dependencies m_deps;
    std::function<void()> m_prepareCallback;

    // キャッシュ用
    QStringList m_kifuDataList;
};

#endif // KIFUEXPORTCONTROLLER_H
