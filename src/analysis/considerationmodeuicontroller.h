#ifndef CONSIDERATIONMODEUICONTROLLER_H
#define CONSIDERATIONMODEUICONTROLLER_H

/// @file considerationmodeuicontroller.h
/// @brief 検討モードUIコントローラクラスの定義


#include <QObject>
#include <QVector>
#include <QString>

class EngineAnalysisTab;
class ShogiView;
class MatchCoordinator;
class ShogiEngineThinkingModel;
class UsiCommLogModel;
class KifuRecordListModel;
struct ShogiMove;

/**
 * @brief 検討モードのUI状態を管理するコントローラ
 *
 * MainWindowから検討モード関連のUI処理を分離し、以下の責務を担う:
 * - 検討モードの開始/終了/待機状態の管理
 * - 矢印表示の更新
 * - MultiPV変更の処理
 * - 検討タブとの連携
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ConsiderationModeUIController : public QObject
{
    Q_OBJECT

public:
    explicit ConsiderationModeUIController(QObject* parent = nullptr);

    // --- 依存オブジェクト設定 ---

    /// 解析タブ参照を設定する（非所有）
    void setAnalysisTab(EngineAnalysisTab* tab);

    /// 盤面ビュー参照を設定する（非所有）
    void setShogiView(ShogiView* view);

    /// 対局司令塔参照を設定する（非所有）
    void setMatchCoordinator(MatchCoordinator* match);

    /// 検討思考モデル参照を設定する（非所有）
    void setConsiderationModel(ShogiEngineThinkingModel* model);

    /// 通信ログモデル参照を設定する（非所有）
    void setCommLogModel(UsiCommLogModel* model);

    /// 現在局面のSFEN文字列を設定する
    void setCurrentSfenStr(const QString& sfen);

    // --- 表示設定 ---

    /// 矢印表示が有効か返す
    bool isShowArrowsEnabled() const { return m_showArrows; }

    /// 矢印表示の有効/無効を切り替える
    void setShowArrowsEnabled(bool enabled);

public slots:
    /**
     * @brief 検討モード開始時の初期化
     * モデルの接続、矢印更新シグナルの設定を行う
     */
    void onModeStarted();

    /**
     * @brief 検討モードの時間設定が確定したとき
     * @param unlimited 時間無制限フラグ
     * @param byoyomiSec 秒読み時間（秒）
     */
    void onTimeSettingsReady(bool unlimited, int byoyomiSec);

    /**
     * @brief 検討モード終了時の処理
     * タイマー停止、矢印クリア、ボタン状態復元
     */
    void onModeEnded();

    /**
     * @brief 検討待機開始時の処理（時間切れ後、次の局面選択待ち）
     * タイマーは停止するが、ボタンは「検討中止」のまま
     */
    void onWaitingStarted();

    /**
     * @brief 検討中にMultiPVが変更されたとき
     * @param value 新しいMultiPV値
     */
    void onMultiPVChanged(int value);

    /**
     * @brief 検討ダイアログでMultiPVが設定されたとき
     * @param multiPV 設定されたMultiPV値
     */
    void onDialogMultiPVReady(int multiPV);

    /**
     * @brief 矢印表示チェックボックスの状態変更時
     * @param checked チェック状態
     */
    void onShowArrowsChanged(bool checked);

    /**
     * @brief 検討モデルから矢印を更新
     */
    void updateArrows();

    /**
     * @brief 検討モード中に局面が変更されたときの処理
     * @param row 新しい行番号
     * @param newPosition 新しい局面のposition文字列
     * @param gameMoves ゲームの指し手リスト（移動先座標取得用、nullの場合は使用しない）
     * @param kifuRecordModel 棋譜モデル（移動先座標取得用、nullの場合は使用しない）
     * @return 局面が更新された場合true
     */
    bool updatePositionIfInConsiderationMode(int row, const QString& newPosition,
                                             const QVector<ShogiMove>* gameMoves,
                                             KifuRecordListModel* kifuRecordModel);

signals:
    /**
     * @brief 検討中止が要求されたとき（→ ConsiderationWiring::stopRequested）
     */
    void stopRequested();

    /**
     * @brief 検討開始が要求されたとき（→ ConsiderationWiring経由でMainWindowへ）
     */
    void startRequested();

    /**
     * @brief 検討中にMultiPV変更が要求されたとき（→ ConsiderationWiring経由でMatchCoordinatorへ）
     * @param value 新しいMultiPV値
     */
    void multiPVChangeRequested(int value);

private:
    /**
     * @brief USI形式の指し手から座標を取得
     * @param usiMove USI形式の指し手（例: "7g7f", "P*3c"）
     * @param fromFile 出力: 移動元の筋（1-9、駒打ちは0）
     * @param fromRank 出力: 移動元の段（1-9、駒打ちは0）
     * @param toFile 出力: 移動先の筋（1-9）
     * @param toRank 出力: 移動先の段（1-9）
     * @return 解析成功ならtrue
     */
    static bool parseUsiMove(const QString& usiMove,
                             int& fromFile, int& fromRank,
                             int& toFile, int& toRank);

    /**
     * @brief 矢印更新用のシグナル接続を設定
     */
    void connectArrowUpdateSignals();

    EngineAnalysisTab* m_analysisTab = nullptr;  ///< 解析タブ（非所有）
    ShogiView* m_shogiView = nullptr;  ///< 盤面ビュー（非所有）
    MatchCoordinator* m_match = nullptr;  ///< 対局司令塔（非所有）
    ShogiEngineThinkingModel* m_considerationModel = nullptr;  ///< 検討モデル（非所有）
    UsiCommLogModel* m_commLogModel = nullptr;  ///< 通信ログモデル（非所有）
    QString m_currentSfenStr;  ///< 現在局面のSFEN（手番判定に使用）
    bool m_showArrows = true;  ///< 矢印表示設定
    bool m_arrowSignalsConnected = false;  ///< 矢印更新シグナル接続済みフラグ
};

#endif // CONSIDERATIONMODEUICONTROLLER_H
