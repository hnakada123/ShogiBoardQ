#ifndef THINKINGINFOPRESENTER_H
#define THINKINGINFOPRESENTER_H

#include <QObject>
#include <QLocale>
#include <QStringList>
#include <QVector>
#include <QPointer>
#include <memory>

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiEngineInfoParser;
class ShogiGameController;

/**
 * @brief 思考情報のGUI表示を管理するPresenterクラス
 *
 * 責務:
 * - info行の解析結果をGUIモデルに反映
 * - 評価値の計算と更新
 * - 思考タブへの情報追加
 * - USI通信ログの表示
 *
 * 単一責任原則（SRP）に基づき、GUI表示の更新のみを担当する。
 * USIプロトコルの解釈やプロセス管理は他のクラスに委譲する。
 */
class ThinkingInfoPresenter : public QObject
{
    Q_OBJECT

public:
    explicit ThinkingInfoPresenter(QObject* parent = nullptr);
    ~ThinkingInfoPresenter() = default;

    // === モデル設定 ===
    
    /// USI通信ログモデルを設定
    void setCommLogModel(UsiCommLogModel* model);
    
    /// 思考情報モデルを設定
    void setThinkingModel(ShogiEngineThinkingModel* model);
    
    /// ゲームコントローラを設定
    void setGameController(ShogiGameController* controller);

    // === 状態設定 ===
    
    /// 棋譜解析モードを設定
    void setAnalysisMode(bool mode);
    
    /// 前回の指し手の座標を設定
    void setPreviousMove(int fileTo, int rankTo);
    
    /// クローンした盤面データを設定
    void setClonedBoardData(const QVector<QChar>& boardData);
    
    /// ponderモード有効フラグを設定
    void setPonderEnabled(bool enabled);

    // === info処理 ===
    
    /// info行を処理してGUIを更新
    void processInfoLine(const QString& line);
    
    /// 思考情報をクリア
    void clearThinkingInfo();
    
    /// バッファリングされたinfo行をフラッシュ
    void flushInfoBuffer();

    // === 通信ログ ===
    
    /// 送信コマンドをログに追加
    void logSentCommand(const QString& prefix, const QString& command);
    
    /// 受信データをログに追加
    void logReceivedData(const QString& prefix, const QString& data);
    
    /// 標準エラーデータをログに追加
    void logStderrData(const QString& prefix, const QString& data);

    // === 評価値取得 ===
    
    /// 最後の評価値を取得
    int lastScoreCp() const { return m_lastScoreCp; }
    
    /// 評価値文字列を取得
    QString scoreStr() const { return m_scoreStr; }
    
    /// 漢字PV文字列を取得
    QString pvKanjiStr() const { return m_pvKanjiStr; }

public slots:
    /// info行を受信（バッファリング）
    void onInfoReceived(const QString& line);

signals:
    /// 思考情報更新シグナル
    void thinkingInfoUpdated(const QString& time, const QString& depth,
                             const QString& nodes, const QString& score,
                             const QString& pvKanjiStr);
    
    /// 評価値更新シグナル
    void scoreUpdated(int scoreCp, const QString& scoreStr);

private:
    /// info行を処理する内部メソッド
    void processInfoLineInternal(const QString& line);

    /// GUI更新ヘルパメソッド
    void updateSearchedHand(const ShogiEngineInfoParser* info);
    void updateDepth(const ShogiEngineInfoParser* info);
    void updateNodes(const ShogiEngineInfoParser* info);
    void updateNps(const ShogiEngineInfoParser* info);
    void updateHashfull(const ShogiEngineInfoParser* info);

    /// 評価値計算
    int calculateScoreInt(const ShogiEngineInfoParser* info) const;
    void updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreInt);
    void updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreInt);
    void updateLastScore(int scoreInt);
    void updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt);

private:
    /// モデル参照（QPointerで生存を追跡）
    QPointer<UsiCommLogModel> m_commLogModel;
    QPointer<ShogiEngineThinkingModel> m_thinkingModel;
    ShogiGameController* m_gameController = nullptr;

    /// 状態
    bool m_analysisMode = false;
    int m_previousFileTo = 0;
    int m_previousRankTo = 0;
    bool m_ponderEnabled = false;
    
    /// クローンした盤面データ
    QVector<QChar> m_clonedBoardData;

    /// 評価値
    int m_lastScoreCp = 0;
    QString m_scoreStr;
    QString m_pvKanjiStr;

    /// ロケール（カンマ区切り表示用）
    QLocale m_locale;

    /// バッファリング
    QStringList m_infoBuffer;
    bool m_flushScheduled = false;
};

#endif // THINKINGINFOPRESENTER_H
