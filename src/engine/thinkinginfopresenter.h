#ifndef THINKINGINFOPRESENTER_H
#define THINKINGINFOPRESENTER_H

/// @file thinkinginfopresenter.h
/// @brief 思考情報GUI表示Presenterクラスの定義


#include <QObject>
#include <QLocale>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QPointer>
#include <memory>

#include "thinkinginforow.h"
#include "usiinfo.h"

class PvNotationFormatter;

/**
 * @brief 思考情報のGUI表示を管理するPresenterクラス
 *
 * 責務:
 * - 解析済みの info（UsiInfo）を表示用の行（ThinkingInfoRow）に変換してシグナルで通知
 * - 評価値の計算と更新
 * - 思考情報の通知
 * - USI通信ログの通知
 *
 * 設計方針:
 * - View層（モデル）への直接依存を排除
 * - 全ての更新をシグナル経由で行う
 * - 疎結合な設計によりテスタビリティを向上
 *
 */
class ThinkingInfoPresenter : public QObject
{
    Q_OBJECT

public:
    explicit ThinkingInfoPresenter(QObject* parent = nullptr);
    ~ThinkingInfoPresenter() override;

    // --- 状態設定 ---
    
    /// 前回の指し手の座標を設定
    void setPreviousMove(int fileTo, int rankTo);

    /// 思考開始時の局面SFENを設定（読み筋はこの局面から日本語にする）
    void setBaseSfen(const QString& sfen);
    void presentCheckmate(const QStringList& pv);
    
    /// 思考開始時の局面SFENを取得
    QString baseSfen() const;

    /// 詰みの評価値の表示文字列（例: "3手詰"）。表の列幅の見積もりにも使う
    static QString mateScoreText(const QString& plies);

    // --- info処理 ---
    
    /// info を処理してシグナルを発行（バッファを通さない）
    void processInfo(const UsiInfo& info);
    
    /// 思考情報のクリアをリクエスト
    void requestClearThinkingInfo();
    
    /// バッファリングされたinfoをフラッシュ
    void flushInfoBuffer();

    // --- 通信ログ ---
    
    /// 送信コマンドをログに通知
    void logSentCommand(const QString& prefix, const QString& command);
    
    /// 受信データをログに通知
    void logReceivedData(const QString& prefix, const QString& data);
    
    /// 標準エラーデータをログに通知
    void logStderrData(const QString& prefix, const QString& data);

    // --- 評価値取得 ---
    
    /// 最後の評価値を取得
    int lastScoreCp() const { return m_lastScoreCp; }
    QString lastScoreMate() const { return m_lastScoreMate; }
    
    /// 評価値文字列を取得
    QString scoreStr() const { return m_scoreStr; }
    
    /// 漢字PV文字列を取得
    QString pvKanjiStr() const { return m_pvKanjiStr; }

public slots:
    /// info を受信（50ms ごとにまとめて処理する）
    void onInfoReceived(const UsiInfo& info);

signals:
    // --- 思考情報関連シグナル ---
    
    /// 思考情報更新シグナル（→ Usi::onThinkingInfoUpdated）
    void thinkingInfoUpdated(const ThinkingInfoRow& row);
    
    /// 思考情報クリアリクエストシグナル（→ Usi::onClearThinkingInfoRequested）
    void clearThinkingInfoRequested();
    
    // --- 評価値関連シグナル ---
    
    // --- エンジン情報関連シグナル ---
    
    /// 探索手更新シグナル（→ Usi::onSearchedMoveUpdated）
    void searchedMoveUpdated(const QString& move);
    
    /// 探索深さ更新シグナル（→ Usi::onSearchDepthUpdated）
    void searchDepthUpdated(const QString& depth);
    
    /// ノード数更新シグナル（→ Usi::onNodeCountUpdated）
    void nodeCountUpdated(const QString& nodes);
    
    /// NPS更新シグナル（→ Usi::onNpsUpdated）
    void npsUpdated(const QString& nps);
    
    /// ハッシュ使用率更新シグナル（→ Usi::onHashUsageUpdated）
    void hashUsageUpdated(const QString& hashUsage);
    
    // --- 通信ログ関連シグナル ---
    
    /// 通信ログ追加シグナル（→ Usi::onCommLogAppended）
    void commLogAppended(const QString& log);

private:
    /// シグナル発行ヘルパメソッド
    void emitSearchedHand();
    void emitDepth(const UsiInfo& info);
    void emitNodes(const UsiInfo& info);
    void emitNps(const UsiInfo& info);
    void emitHashfull(const UsiInfo& info);

    /// 評価値計算（score cp の表示文字列を m_scoreStr に入れ、整数値を返す）
    int updateCpScore(const UsiInfo& info);
    void updateLastScore(int scoreInt);
    /// 評価値・詰み手数を更新し、行に出す評価値の表示文字列と整数値を返す
    void updateEvaluationInfo(const UsiInfo& info, QString& scoreText, int& scoreInt);

private:

    int m_previousFileTo = 0;           ///< 前回の指し手の筋
    int m_previousRankTo = 0;           ///< 前回の指し手の段

    QString m_baseSfen;                 ///< 思考開始時の局面SFEN

    QString m_lastScoreMate;            ///< 最後の主候補の score mate（空は通常評価値）
    int m_lastScoreCp = 0;              ///< 最後の評価値（センチポーン）
    QString m_scoreStr;                 ///< 評価値文字列
    QString m_pvKanjiStr;               ///< 漢字PV文字列

    QLocale m_locale;                   ///< ロケール（カンマ区切り表示用）

    // 探索手の欄の表示。同じ手が何度も届く（MultiPV では候補の数だけの手が繰り返し届く）ので、
    // 局面と表記の設定が同じ間は、探索手（漢字表記と USI形式）ごとに作った表示を覚えておく
    QString m_searchedSfenSource;       ///< 覚えた表示を作ったときの局面SFEN
    int m_searchedNotation = -1;        ///< 覚えた表示を作ったときの表記（KifuPresentation::Notation）
    bool m_searchedAlwaysOrigin = false;
    QHash<QString, QString> m_searchedLabels;  ///< 探索手（漢字表記 + U+0000 + USI形式）→ 表示

    QList<UsiInfo> m_infoBuffer;        ///< infoバッファ
    bool m_flushScheduled = false;      ///< フラッシュ予約済みフラグ
    std::unique_ptr<PvNotationFormatter> m_formatter; ///< 読み筋を漢字表記にする（局面を持つので再利用）
};

#endif // THINKINGINFOPRESENTER_H
