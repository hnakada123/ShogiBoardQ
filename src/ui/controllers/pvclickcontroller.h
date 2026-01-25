#ifndef PVCLICKCONTROLLER_H
#define PVCLICKCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "mainwindow.h"  // PlayMode enum

class ShogiEngineThinkingModel;
class UsiCommLogModel;
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
    explicit PvClickController(QObject* parent = nullptr);
    ~PvClickController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setThinkingModels(ShogiEngineThinkingModel* model1, ShogiEngineThinkingModel* model2);
    void setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2);
    void setSfenRecord(QStringList* sfenRecord);
    void setGameMoves(const QVector<ShogiMove>* gameMoves);
    void setUsiMoves(const QStringList* usiMoves);

    // --------------------------------------------------------
    // 状態設定
    // --------------------------------------------------------
    void setPlayMode(PlayMode mode);
    void setPlayerNames(const QString& human1, const QString& human2,
                        const QString& engine1, const QString& engine2);
    void setCurrentSfen(const QString& sfen);
    void setStartSfen(const QString& sfen);
    void setCurrentRecordIndex(int index);

signals:
    void pvDialogClosed(int engineIndex);

public Q_SLOTS:
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
    ShogiEngineThinkingModel* m_modelThinking1 = nullptr;
    ShogiEngineThinkingModel* m_modelThinking2 = nullptr;
    UsiCommLogModel* m_lineEditModel1 = nullptr;
    UsiCommLogModel* m_lineEditModel2 = nullptr;
    QStringList* m_sfenRecord = nullptr;
    const QVector<ShogiMove>* m_gameMoves = nullptr;
    const QStringList* m_usiMoves = nullptr;

    PlayMode m_playMode = PlayMode::NotStarted;
    QString m_humanName1;
    QString m_humanName2;
    QString m_engineName1;
    QString m_engineName2;
    QString m_currentSfenStr;
    QString m_startSfenStr;
    int m_currentRecordIndex = -1;  ///< 現在選択されている棋譜行のインデックス（PV表示では未使用）
};

#endif // PVCLICKCONTROLLER_H
