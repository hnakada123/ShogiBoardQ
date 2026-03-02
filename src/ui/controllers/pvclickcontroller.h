#ifndef PVCLICKCONTROLLER_H
#define PVCLICKCONTROLLER_H

/// @file pvclickcontroller.h
/// @brief 読み筋クリックコントローラクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include <functional>

#include "playmode.h"

class ShogiEngineThinkingModel;
class UsiCommLogModel;
class ShogiView;
struct ShogiMove;

/**
 * @brief PvClickController - 読み筋クリック処理クラス
 *
 * MainWindowから読み筋（PV）クリック時の処理を分離したクラス。
 * 以下の責務を担当:
 * - 読み筋行クリック時のデータ取得
 * - USI形式読み筋の解析
 * - PvBoardDialogの表示
 */
class PvClickController : public QObject
{
    Q_OBJECT

public:
    /// MainWindow の動的状態へのポインタ群（onPvRowClicked 呼び出し時に自動同期）
    struct StateRefs {
        const PlayMode* playMode = nullptr;
        const QString* humanName1 = nullptr;
        const QString* humanName2 = nullptr;
        const QString* engineName1 = nullptr;
        const QString* engineName2 = nullptr;
        const QString* currentSfenStr = nullptr;
        const QString* startSfenStr = nullptr;
        const int* currentMoveIndex = nullptr;
        ShogiEngineThinkingModel** considerationModel = nullptr;  ///< ダブルポインタ（外部所有）
        std::function<QStringList*()> sfenRecordGetter;           ///< SFEN履歴の動的取得（MC再生成対応）
    };

    explicit PvClickController(QObject* parent = nullptr);
    ~PvClickController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setThinkingModels(ShogiEngineThinkingModel* model1, ShogiEngineThinkingModel* model2);
    void setConsiderationModel(ShogiEngineThinkingModel* model);
    void setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2);
    void setSfenRecord(QStringList* sfenRecord);
    void setGameMoves(const QList<ShogiMove>* gameMoves);
    void setUsiMoves(const QStringList* usiMoves);
    void setStateRefs(const StateRefs& refs);
    void setShogiView(ShogiView* view);

signals:
    void pvDialogClosed(int engineIndex);

public slots:
    /**
     * @brief 読み筋行がクリックされた時の処理
     * @param engineIndex エンジンインデックス（0 or 1）
     * @param row クリックされた行
     */
    void onPvRowClicked(int engineIndex, int row);

private:
    void onPvDialogFinished(int result);

    /**
     * @brief USI形式の読み筋をログから検索
     */
    QStringList searchUsiMovesFromLog(UsiCommLogModel* logModel) const;

    /**
     * @brief 現在の局面SFENを取得（フォールバック付き）
     */
    QString resolveCurrentSfen(const QString& baseSfen) const;

    /**
     * @brief PlayModeに応じた対局者名を取得
     */
    void resolvePlayerNames(QString& blackName, QString& whiteName) const;

    /**
     * @brief 最後の手をUSI形式で取得
     */
    QString resolveLastUsiMove() const;

private:
    void syncFromRefs();

    ShogiEngineThinkingModel* m_modelThinking1 = nullptr;
    ShogiEngineThinkingModel* m_modelThinking2 = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    UsiCommLogModel* m_lineEditModel1 = nullptr;
    UsiCommLogModel* m_lineEditModel2 = nullptr;
    QStringList* m_sfenHistory = nullptr;
    const QList<ShogiMove>* m_gameMoves = nullptr;
    const QStringList* m_usiMoves = nullptr;
    ShogiView* m_shogiView = nullptr;                    ///< 盤面反転状態取得用

    StateRefs m_stateRefs;                               ///< 動的状態へのポインタ群

    PlayMode m_playMode = PlayMode::NotStarted;
    QString m_humanName1;
    QString m_humanName2;
    QString m_engineName1;
    QString m_engineName2;
    QString m_currentSfenStr;
    QString m_startSfenStr;
    int m_currentRecordIndex = -1;  ///< 現在選択されている棋譜行のインデックス（PV表示では未使用）
    bool m_boardFlipped = false;     ///< GUI本体の盤面反転状態
};

#endif // PVCLICKCONTROLLER_H
