#ifndef KIFUEXPORTCLIPBOARD_H
#define KIFUEXPORTCLIPBOARD_H

/// @file kifuexportclipboard.h
/// @brief 棋譜クリップボードコピー機能の定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "mainwindow.h"       // PlayMode enum
#include "gamerecordmodel.h"  // GameRecordModel::ExportContext
#include "kifuclipboardservice.h"  // KifuClipboardService::ExportContext

class QWidget;
class GameRecordModel;
class KifuRecordListModel;
class GameInfoPaneController;
class TimeControlController;
class KifuLoadCoordinator;
class MatchCoordinator;
class ReplayController;
class ShogiGameController;

/**
 * @brief KifuExportClipboard - 棋譜クリップボードコピー機能
 *
 * KifuExportControllerからクリップボードコピー関連の処理を分離したクラス。
 * 以下の責務を担当:
 * - 各種形式でのクリップボードコピー（KIF/KI2/CSA/JKF/USEN/USI/SFEN/BOD）
 */
class KifuExportClipboard : public QObject
{
    Q_OBJECT

public:
    explicit KifuExportClipboard(QWidget* parentWidget, QObject* parent = nullptr);

    struct Deps {
        GameRecordModel* gameRecord = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        TimeControlController* timeController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        MatchCoordinator* match = nullptr;
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;

        // データソース（ポインタ）
        QStringList* sfenRecord = nullptr;
        QStringList* usiMoves = nullptr;

        // 状態値
        QString startSfenStr;
        PlayMode playMode = PlayMode::NotStarted;
        QString humanName1;
        QString humanName2;
        QString engineName1;
        QString engineName2;
        int currentMoveIndex = 0;
        int activePly = 0;
        int currentSelectedPly = 0;
    };

    void setDependencies(const Deps& deps);
    void setPrepareCallback(std::function<void()> callback);

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

Q_SIGNALS:
    void statusMessage(const QString& message, int timeout);

private:
    QStringList resolveUsiMoves() const;
    GameRecordModel::ExportContext buildExportContext() const;
    KifuClipboardService::ExportContext buildClipboardContext() const;
    bool setClipboardText(const QString& text, const QString& successMsg);
    bool isCurrentlyPlaying() const;
    int currentPly() const;

    struct PositionData {
        QString sfenStr;
        int moveIndex = 0;
        QString lastMoveStr;
    };
    PositionData getCurrentPositionData() const;
    QString generateBodText(const PositionData& pos) const;

    QWidget* m_parentWidget = nullptr;
    Deps m_deps;
    std::function<void()> m_prepareCallback;
};

#endif // KIFUEXPORTCLIPBOARD_H
