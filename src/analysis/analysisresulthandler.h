#ifndef ANALYSISRESULTHANDLER_H
#define ANALYSISRESULTHANDLER_H

/// @file analysisresulthandler.h
/// @brief 解析結果ハンドラクラスの定義


#include <QCoreApplication>
#include <QString>
#include <QStringList>

class KifuAnalysisListModel;
class KifuRecordListModel;
class AnalysisCoordinator;
class AnalysisResultsPresenter;

/**
 * @brief 解析結果の蓄積・確定・読み筋表示を担当するハンドラ
 *
 * AnalysisFlowControllerから解析結果処理の責務を分離する。
 * 一時結果の保持、bestmove受信時の確定処理、読み筋ダイアログ表示を行う。
 */
class AnalysisResultHandler
{
    Q_DECLARE_TR_FUNCTIONS(AnalysisResultHandler)
public:
    /// 外部参照オブジェクト
    struct Refs {
        KifuAnalysisListModel* analysisModel = nullptr;  ///< 解析結果モデル（非所有）
        QStringList* sfenHistory = nullptr;              ///< 局面コマンド列（非所有）
        KifuRecordListModel* recordModel = nullptr;      ///< 棋譜表示モデル（非所有）
        QStringList* usiMoves = nullptr;                 ///< USI形式指し手列（非所有）
        AnalysisCoordinator* coord = nullptr;            ///< 解析コーディネータ（非所有）
        AnalysisResultsPresenter* presenter = nullptr;   ///< 結果表示プレゼンタ（非所有）
        QString blackPlayerName;                         ///< 先手名
        QString whitePlayerName;                         ///< 後手名
        bool boardFlipped = false;                       ///< GUI本体の盤面反転状態
    };

    /// 外部参照を更新する
    void setRefs(const Refs& refs);

    /// 一時結果・確定結果の状態をリセットする
    void reset();

    /// 解析進捗データを一時保存する
    void updatePending(int ply, int scoreCp, int mate, const QString& pv);

    /// 漢字PVを一時保存する
    void updatePendingPvKanji(const QString& pvKanjiStr);

    /// 保留中の解析結果をモデルへ確定反映する
    void commitPendingResult();

    /// 結果行ダブルクリック時に読み筋盤面ダイアログを表示する
    void showPvBoardDialog(int row);

    /// 漢字指し手文字列からUSI形式指し手を抽出する
    static QString extractUsiMoveFromKanji(const QString& kanjiMove);

    /// 最後に確定した手数を返す
    int lastCommittedPly() const { return m_lastCommittedPly; }

    /// 最後に確定した評価値を返す
    int lastCommittedScoreCp() const { return m_lastCommittedScoreCp; }

    /// 確定結果の手数をリセットする
    void resetLastCommitted() { m_lastCommittedPly = -1; }

private:
    Refs m_refs;

    int m_pendingPly = -1;         ///< 一時結果の対象手数（bestmoveで確定）
    int m_pendingScoreCp = 0;      ///< 一時結果の評価値
    int m_pendingMate = 0;         ///< 一時結果の詰み手数（0は未設定）
    QString m_pendingPv;           ///< 一時結果のUSI PV
    QString m_pendingPvKanji;      ///< 一時結果の漢字PV

    int m_lastCommittedPly = -1;       ///< 最後に確定した手数（GUI同期用）
    int m_lastCommittedScoreCp = 0;    ///< 最後に確定した評価値
    int m_prevEvalCp = 0;              ///< 前回評価値（差分計算用）
};

#endif // ANALYSISRESULTHANDLER_H
