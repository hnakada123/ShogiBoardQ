#ifndef ANALYSISCOORDINATOR_H
#define ANALYSISCOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QTimer>

class EngineAnalysisTab; // 任意。ツリーハイライト等に使うなら setter で渡す

// ゲーム（または任意の範囲）の位置ごとに USIエンジンで解析を回し、結果を signal で通知する司令塔。
// - エンジンへの具体的な送信は行わず、"requestSendUsiCommand" を emit するだけ（疎結合）
// - エンジン側の出力は onEngineInfoLine/onEngineBestmoveReceived で受け取る（slot）
// - 盤面位置は sfenRecord（各手数に対応した "position sfen ... moves ..." の配列）を参照
class AnalysisCoordinator : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        // 各 ply に対応した "position ..." コマンド列（KifuLoadCoordinator 等で構築済み想定）
        // 例：sfenRecord[ply] が「その手数の局面を再現する position コマンド」
        QStringList* sfenRecord = nullptr;
    };

    struct Options {
        int  startPly   = 0;     // 解析開始 ply（0以上）
        int  endPly     = -1;    // 解析終了 ply（-1 は sfenRecord の末尾まで）
        int  movetimeMs = 1000;  // 1局面あたりの思考時間（ms）
        int  multiPV    = 1;     // MultiPV（setoption で設定）
        bool centerTree = true;  // 解析進捗時にツリーをセンタリングするか
    };

    enum Mode {
        Idle,
        SinglePosition,  // 1局面のみ
        RangePositions   // 指定範囲を順次
    };
    Q_ENUM(Mode)

    explicit AnalysisCoordinator(const Deps& d, QObject* parent = nullptr);

    void setDeps(const Deps& d);
    void setAnalysisTab(EngineAnalysisTab* tab); // 任意（ツリーハイライト用）
    void setOptions(const Options& opt);

    // --- 操作 API ---
    void startAnalyzeRange();          // Options の startPly..endPly を順次解析
    void startAnalyzeSingle(int ply);  // 指定 ply を 1回解析
    void stop();                       // 現在の解析を中断

signals:
    // Coordinator → エンジン送信器（UsiEngine等）へ：USIテキストを送ってください
    void requestSendUsiCommand(const QString& line);

    // 解析ライフサイクル
    void analysisStarted(int startPly, int endPly, AnalysisCoordinator::Mode mode);
    void analysisFinished(AnalysisCoordinator::Mode mode);

    // 進捗（info 行をパースしたサマリ。score は cp or mate のどちらかが有効）
    void analysisProgress(int ply,
                          int depth,
                          int seldepth,
                          int scoreCp,     // 未設定時は INT_MIN
                          int mate,        // 未設定時は 0（詰みなし）/ 正負で手数
                          const QString& pv,
                          const QString& rawInfoLine);

    // 位置コマンドを発行した（デバッグ/ログ用）
    void positionPrepared(int ply, const QString& positionCmd);

public slots:
    // エンジン側の標準出力（"info ..."）を接続してください
    void onEngineInfoLine(const QString& line);
    // エンジン側の "bestmove ..." を接続してください
    void onEngineBestmoveReceived(const QString& line);

    // GUI更新完了後にgoコマンドを送信する（棋譜解析用）
    void sendGoCommand();

    // 現在解析中のply（定跡処理用）
    int currentPly() const { return m_currentPly; }

private:
    struct ParsedInfo {
        int depth    = -1;
        int seldepth = -1;
        int scoreCp  = std::numeric_limits<int>::min(); // INT_MIN=未設定
        int mate     = 0; // 0=未設定/詰み無し、正負で手数（先手勝ち:+ / 後手勝ち:- とする慣例）
        QString pv;
    };

    Deps    m_deps;
    Options m_opt;

    Mode m_mode = Idle;
    bool m_running = false;
    int  m_currentPly = -1;
    QString m_pendingPosCmd;  // sendGoCommand()で使用するpositionコマンド

    QPointer<EngineAnalysisTab> m_analysisTab; // 任意
    QTimer m_stopTimer;  // go infinite後にstopを送信するためのタイマー

    // 内部
    void startRange_();
    void startSingle_(int ply);
    void nextPlyOrFinish_();
    void sendAnalyzeForPly_(int ply);
    static bool parseInfoUSI_(const QString& line, ParsedInfo* out);

    // USI 便利
    void send_(const QString& line); // requestSendUsiCommand をまとめる

private slots:
    // stopタイマーのタイムアウト処理
    void onStopTimerTimeout_();
};

#endif // ANALYSISCOORDINATOR_H
