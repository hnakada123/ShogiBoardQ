#ifndef ANALYSISRESULTHANDLER_H
#define ANALYSISRESULTHANDLER_H

/// @file analysisresulthandler.h
/// @brief 解析結果ハンドラクラスの定義


#include <QCoreApplication>
#include <QString>
#include <optional>

#include "usiinfo.h"
#include <QStringList>
#include "recordline.h"

class KifuAnalysisListModel;
class KifuRecordListModel;
class AnalysisCoordinator;
class AnalysisResultsPanel;

/**
 * @brief 解析結果の蓄積・確定・読み筋表示を担当するハンドラ
 *
 * 棋譜解析の結果（評価値・読み筋）を表へ確定させる（KifuAnalysisController が解析1回ごとに作る）。
 * 一時結果の保持、bestmove受信時の確定処理、読み筋ダイアログ表示を行う。
 */
class AnalysisResultHandler
{
    Q_DECLARE_TR_FUNCTIONS(AnalysisResultHandler)
public:
    /// 外部参照オブジェクト
    struct Refs {
        KifuAnalysisListModel* analysisModel = nullptr;  ///< 解析結果モデル（非所有）
        const RecordLine* line = nullptr;               ///< 実行が所有する解析対象
        AnalysisCoordinator* coord = nullptr;            ///< 解析コーディネータ（非所有）
        AnalysisResultsPanel* presenter = nullptr;   ///< 結果表示プレゼンタ（非所有）
        QString blackPlayerName;                         ///< 先手名
        QString whitePlayerName;                         ///< 後手名
        bool boardFlipped = false;                       ///< GUI本体の盤面反転状態
    };

    /// 解析1回分の参照を受け取る（参照先は解析の実行が持つ）
    explicit AnalysisResultHandler(const Refs& refs);

    /// 解析中の局面（ply）の info を覚える（bestmove で確定する）
    void updatePending(int ply, const UsiInfo& info);

    /// 漢字PVを一時保存する
    void updatePendingPvKanji(const QString& pvKanjiStr);

    /// 保留中の解析結果をモデルへ確定反映する
    void commitPendingResult();

    /// 結果行ダブルクリック時に読み筋盤面ダイアログを表示する
    void showPvBoardDialog(int row);

    /// 最後に確定した手数を返す
    int lastCommittedPly() const { return m_lastCommittedPly; }

    /// 最後に確定した評価値（先手から見た値。定跡・未取得は None）
    const UsiScore& lastCommittedScore() const { return m_lastCommittedScore; }

    /// 確定結果の手数をリセットする
    void resetLastCommitted() { m_lastCommittedPly = -1; }

private:
    Refs m_refs;

    int m_pendingPly = -1;         ///< 一時結果の対象手数（bestmoveで確定）
    UsiScore m_pendingScore;       ///< 一時結果の評価値（手番側から見た値）
    QString m_pendingPv;           ///< 一時結果のUSI PV
    QString m_pendingPvKanji;      ///< 一時結果の漢字PV

    int m_lastCommittedPly = -1;       ///< 最後に確定した手数（GUI同期用）
    UsiScore m_lastCommittedScore;     ///< 最後に確定した評価値（先手から見た値）
    std::optional<int> m_previousCp;   ///< 直前の結果の評価値（増減の計算。比べられなければ nullopt）
};

#endif // ANALYSISRESULTHANDLER_H
