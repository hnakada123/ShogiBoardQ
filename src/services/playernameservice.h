#ifndef PLAYERNAMESERVICE_H
#define PLAYERNAMESERVICE_H

/// @file playernameservice.h
/// @brief プレイヤー名解決サービスクラスの定義

#include <QString>
#include "playmode.h"

/// プレイヤー表示名のマッピング
struct PlayerNameMapping {
    QString p1;  ///< 先手（黒）の表示名
    QString p2;  ///< 後手（白）の表示名
};

/// エンジン名のマッピング
struct EngineNameMapping {
    QString model1; ///< m_lineEditModel1 に表示するエンジン名
    QString model2; ///< m_lineEditModel2 に表示するエンジン名
};

/**
 * @brief 対局モードに応じてプレイヤー表示名・エンジン名を解決するサービス
 *
 * PlayModeから先手/後手の表示名（人間名/エンジン名）と
 * ログ用モデルのエンジン名割り当てを決定する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class PlayerNameService {
public:
    // --- 名前解決 API ---

    /**
     * @brief 先手/後手の表示名を決定する
     * @param mode 対局モード
     * @param human1 先手側の人間名
     * @param human2 後手側の人間名
     * @param engine1 先手側のエンジン名
     * @param engine2 後手側のエンジン名
     * @return 先手・後手の表示名マッピング
     */
    static PlayerNameMapping computePlayers(PlayMode mode,
                                            const QString& human1,
                                            const QString& human2,
                                            const QString& engine1,
                                            const QString& engine2);

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
