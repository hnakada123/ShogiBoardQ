#ifndef PLAYERNAMESERVICE_H
#define PLAYERNAMESERVICE_H

#include <QString>
#include "playmode.h"

struct PlayerNameMapping {
    QString p1;  // 先手（黒）
    QString p2;  // 後手（白）
};

struct EngineNameMapping {
    QString model1; // m_lineEditModel1 に表示するエンジン名
    QString model2; // m_lineEditModel2 に表示するエンジン名
};

class PlayerNameService {
public:
    // 先手/後手の表示名を決定（人間名/エンジン名のどちらを出すか）
    static PlayerNameMapping computePlayers(PlayMode mode,
                                            const QString& human1,
                                            const QString& human2,
                                            const QString& engine1,
                                            const QString& engine2);

    // ログ用モデル（model1/model2）のエンジン名割り当てを決定
    static EngineNameMapping computeEngineModels(PlayMode mode,
                                                 const QString& engine1,
                                                 const QString& engine2);
};

#endif // PLAYERNAMESERVICE_H
