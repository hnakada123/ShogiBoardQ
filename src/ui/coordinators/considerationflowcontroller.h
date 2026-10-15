#ifndef CONSIDERATIONFLOWCONTROLLER_H
#define CONSIDERATIONFLOWCONTROLLER_H

/// @file considerationflowcontroller.h
/// @brief 検討フローコントローラクラスの定義

#include <QObject>
#include <functional>

#include "usimove.h"

class AnalysisSession;
class ShogiEngineThinkingModel;

/**
 * @brief 単一局面の検討開始処理を構成するフローコントローラ
 *
 * 検討タブで選んだエンジン・時間・候補手の数から、検討のセッション（AnalysisSession）を始める。
 */
class ConsiderationFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        AnalysisSession* session = nullptr;  ///< 検討のセッション（必須、非所有）
        std::function<void()> onStarted;  ///< 開始確定通知（任意）
        std::function<void(const QString&)> onError;  ///< エラー表示コールバック（任意）
        ShogiEngineThinkingModel* considerationModel = nullptr;  ///< 検討用思考モデル（非所有）
        std::function<void(bool unlimited, int byoyomiSec)> onTimeSettingsReady;  ///< 時間設定通知（任意）
        std::function<void(int multiPV)> onMultiPVReady;  ///< MultiPV確定通知（任意）
    };

    explicit ConsiderationFlowController(QObject* parent=nullptr);

    /// 検討パラメータ
    struct DirectParams {
        int engineIndex = 0;        ///< エンジンインデックス
        QString engineName;         ///< エンジン名
        bool unlimitedTime = true;  ///< 時間無制限フラグ
        int byoyomiSec = 20;        ///< 検討時間（秒）
        int multiPV = 1;            ///< 候補手の数
        BoardSquare previousTo;     ///< 局面に至った手の移動先（読み筋の「同」の判定に使う）
        QString lastUsiMove;        ///< 開始局面に至った最後のUSI指し手
    };

    /**
     * @brief ダイアログなしで検討を直接開始する
     * @param positionStr 送信する`position ...`文字列
     * @return 検討開始まで到達した場合 true
     */
    bool runDirect(const Deps& d, const DirectParams& params, const QString& positionStr);
};

#endif // CONSIDERATIONFLOWCONTROLLER_H
