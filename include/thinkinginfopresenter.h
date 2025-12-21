#ifndef THINKINGINFOPRESENTER_H
#define THINKINGINFOPRESENTER_H

#include <QObject>
#include <QLocale>
#include <QStringList>
#include <QVector>
#include <QPointer>
#include <memory>

class ShogiEngineInfoParser;
class ShogiGameController;

/**
 * @brief 思考情報のGUI表示を管理するPresenterクラス
 *
 * 責務:
 * - info行の解析結果をシグナルで通知
 * - 評価値の計算と更新
 * - 思考情報の通知
 * - USI通信ログの通知
 *
 * 設計方針:
 * - View層（モデル）への直接依存を排除
 * - 全ての更新をシグナル経由で行う
 * - 疎結合な設計によりテスタビリティを向上
 */
class ThinkingInfoPresenter : public QObject
{
    Q_OBJECT

public:
    explicit ThinkingInfoPresenter(QObject* parent = nullptr);
    ~ThinkingInfoPresenter() = default;

    // === 依存関係設定 ===
    
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

    /// 思考開始時の局面SFENを設定
    void setBaseSfen(const QString& sfen);
    
    /// 思考開始時の局面SFENを取得
    QString baseSfen() const;

    // === info処理 ===
    
    /// info行を処理してシグナルを発行
    void processInfoLine(const QString& line);
    
    /// 思考情報のクリアをリクエスト
    void requestClearThinkingInfo();
    
    /// バッファリングされたinfo行をフラッシュ
    void flushInfoBuffer();

    // === 通信ログ ===
    
    /// 送信コマンドをログに通知
    void logSentCommand(const QString& prefix, const QString& command);
    
    /// 受信データをログに通知
    void logReceivedData(const QString& prefix, const QString& data);
    
    /// 標準エラーデータをログに通知
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
    // === 思考情報関連シグナル ===
    
    /// 思考情報更新シグナル（思考タブへの追加用）
    /// pvKanjiStr: 漢字表記の読み筋
    /// usiPv: USI形式の読み筋（スペース区切り）
    /// baseSfen: 思考開始時の局面SFEN
    void thinkingInfoUpdated(const QString& time, const QString& depth,
                             const QString& nodes, const QString& score,
                             const QString& pvKanjiStr, const QString& usiPv,
                             const QString& baseSfen);
    
    /// 思考情報クリアリクエストシグナル
    void clearThinkingInfoRequested();
    
    // === 評価値関連シグナル ===
    
    /// 評価値更新シグナル
    void scoreUpdated(int scoreCp, const QString& scoreStr);
    
    // === エンジン情報関連シグナル ===
    
    /// 探索手更新シグナル
    void searchedMoveUpdated(const QString& move);
    
    /// 探索深さ更新シグナル
    void searchDepthUpdated(const QString& depth);
    
    /// ノード数更新シグナル
    void nodeCountUpdated(const QString& nodes);
    
    /// NPS更新シグナル
    void npsUpdated(const QString& nps);
    
    /// ハッシュ使用率更新シグナル
    void hashUsageUpdated(const QString& hashUsage);
    
    // === 通信ログ関連シグナル ===
    
    /// 通信ログ追加シグナル
    void commLogAppended(const QString& log);

private:
    /// info行を処理する内部メソッド
    void processInfoLineInternal(const QString& line);

    /// シグナル発行ヘルパメソッド
    void emitSearchedHand(const ShogiEngineInfoParser* info);
    void emitDepth(const ShogiEngineInfoParser* info);
    void emitNodes(const ShogiEngineInfoParser* info);
    void emitNps(const ShogiEngineInfoParser* info);
    void emitHashfull(const ShogiEngineInfoParser* info);

    /// 評価値計算
    int calculateScoreInt(const ShogiEngineInfoParser* info) const;
    void updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreInt);
    void updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreInt);
    void updateLastScore(int scoreInt);
    void updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt);

private:
    /// ゲームコントローラ参照
    ShogiGameController* m_gameController = nullptr;

    /// 状態
    bool m_analysisMode = false;
    int m_previousFileTo = 0;
    int m_previousRankTo = 0;
    bool m_ponderEnabled = false;
    
    /// 思考開始時の局面SFEN
    QString m_baseSfen;
    
    /// 思考開始時の手番（true=先手(Player1), false=後手(Player2)）
    bool m_thinkingStartPlayerIsP1 = true;
    
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
