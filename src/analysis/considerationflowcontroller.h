#ifndef CONSIDERATIONFLOWCONTROLLER_H
#define CONSIDERATIONFLOWCONTROLLER_H

/// @file considerationflowcontroller.h
/// @brief 検討フローコントローラクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <functional>

class MatchCoordinator;
class ShogiEngineThinkingModel;

/**
 * @brief 単一局面の検討開始処理を構成するフローコントローラ
 *
 * 単一局面の「検討」実行を司る薄いフローコントローラ。
 * MainWindow から UI と司令塔(MatchCoordinator)の橋渡しだけを引き剥がす。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ConsiderationFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    /// @todo remove コメントスタイルガイド適用済み
    struct Deps {
        MatchCoordinator* match = nullptr;  ///< 司令塔（必須、非所有）
        std::function<void(const QString&)> onError;  ///< エラー表示コールバック（任意）
        int multiPV = 1;  ///< 候補手本数
        ShogiEngineThinkingModel* considerationModel = nullptr;  ///< 検討用思考モデル（非所有）
        std::function<void(bool unlimited, int byoyomiSec)> onTimeSettingsReady;  ///< 時間設定通知（任意）
        std::function<void(int multiPV)> onMultiPVReady;  ///< MultiPV確定通知（任意）
    };

    /// @todo remove コメントスタイルガイド適用済み
    explicit ConsiderationFlowController(QObject* parent=nullptr);

    /**
     * @brief 検討パラメータ
     *
     * @todo remove コメントスタイルガイド適用済み
     */
    struct DirectParams {
        int engineIndex = 0;        ///< エンジンインデックス
        QString engineName;         ///< エンジン名
        bool unlimitedTime = true;  ///< 時間無制限フラグ
        int byoyomiSec = 20;        ///< 検討時間（秒）
        int multiPV = 1;            ///< 候補手の数
        int previousFileTo = 0;     ///< 前回移動先の筋（1-9、0は未設定）
        int previousRankTo = 0;     ///< 前回移動先の段（1-9、0は未設定）
        QString lastUsiMove;        ///< 開始局面に至った最後のUSI指し手
    };

    /**
     * @brief ダイアログなしで検討を直接開始する
     * @param d 依存オブジェクト
     * @param params 検討パラメータ
     * @param positionStr 送信する`position ...`文字列
     *
     * ダイアログを表示せず、指定されたパラメータで直接検討を開始します。
     * @todo remove コメントスタイルガイド適用済み
     */
    void runDirect(const Deps& d, const DirectParams& params, const QString& positionStr);

private:
    /// MatchCoordinatorへ検討開始を委譲する
    void startAnalysis(MatchCoordinator* match,
                        const QString& enginePath,
                        const QString& engineName,
                        const QString& positionStr,
                        int byoyomiMs,
                        int multiPV,
                        ShogiEngineThinkingModel* considerationModel,
                        int previousFileTo,
                        int previousRankTo,
                        const QString& lastUsiMove);
};

#endif // CONSIDERATIONFLOWCONTROLLER_H
