#ifndef CONSIDERATIONFLOWCONTROLLER_H
#define CONSIDERATIONFLOWCONTROLLER_H

#include <QObject>
#include <functional>

class MatchCoordinator;
class ShogiEngineThinkingModel;

/**
 * 単一局面の「検討」実行を司る薄いフローコントローラ。
 * MainWindow から UI と司令塔(MatchCoordinator)の橋渡しだけを引き剥がす。
 */
class ConsiderationFlowController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        MatchCoordinator* match = nullptr;               // 司令塔
        std::function<void(const QString&)> onError;     // 任意: エラー表示
        int multiPV = 1;                                 // 候補手の数
        ShogiEngineThinkingModel* considerationModel = nullptr;  // 検討タブ用モデル
        std::function<void(bool unlimited, int byoyomiSec)> onTimeSettingsReady;  // 時間設定コールバック
        std::function<void(int multiPV)> onMultiPVReady;  // 候補手の数コールバック
    };

    explicit ConsiderationFlowController(QObject* parent=nullptr);

    /**
     * @brief 検討パラメータ
     */
    struct DirectParams {
        int engineIndex = 0;           // エンジンインデックス
        QString engineName;            // エンジン名
        bool unlimitedTime = true;     // 時間無制限フラグ
        int byoyomiSec = 20;           // 検討時間（秒）
        int multiPV = 1;               // 候補手の数
        int previousFileTo = 0;        // 前回の移動先の筋（1-9, 0=未設定）
        int previousRankTo = 0;        // 前回の移動先の段（1-9, 0=未設定）
    };

    /**
     * ダイアログを表示せず、指定されたパラメータで直接検討を開始します。
     * @param d 依存情報
     * @param params 検討パラメータ
     * @param positionStr 送信する "position ..." 文字列
     */
    void runDirect(const Deps& d, const DirectParams& params, const QString& positionStr);

private:
    void startAnalysis(MatchCoordinator* match,
                        const QString& enginePath,
                        const QString& engineName,
                        const QString& positionStr,
                        int byoyomiMs,
                        int multiPV,
                        ShogiEngineThinkingModel* considerationModel,
                        int previousFileTo,
                        int previousRankTo);
};

#endif // CONSIDERATIONFLOWCONTROLLER_H
