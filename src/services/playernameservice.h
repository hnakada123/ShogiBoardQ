#ifndef PLAYERNAMESERVICE_H
#define PLAYERNAMESERVICE_H

/// @file playernameservice.h
/// @brief プレイヤー名解決サービスクラスの定義

#include <QString>
#include "playmode.h"

/// エンジン名のマッピング
struct EngineNameMapping {
    QString model1; ///< m_lineEditModel1 に表示するエンジン名
    QString model2; ///< m_lineEditModel2 に表示するエンジン名
};

/**
 * @brief 対局モードに応じてログ用エンジン名を解決するサービス
 *
 * PlayModeからログ用モデルのエンジン名割り当てを決定する。
 *
 */
class PlayerNameService {
public:
    // --- 名前解決 API ---

    /**
     * @brief ログ用モデル（model1/model2）のエンジン名割り当てを決定する
     * @param mode 対局モード
     * @param engine1 エンジン1の名前
     * @param engine2 エンジン2の名前
     * @return モデル1/モデル2のエンジン名マッピング
     */
    static EngineNameMapping computeEngineModels(PlayMode mode,
                                                 const QString& engine1,
                                                 const QString& engine2);
};

#endif // PLAYERNAMESERVICE_H
