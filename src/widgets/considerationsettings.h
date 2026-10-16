#ifndef CONSIDERATIONSETTINGS_H
#define CONSIDERATIONSETTINGS_H

/// @file considerationsettings.h
/// @brief 検討タブの設定（エンジン・時間・候補手の数・矢印）

#include <QMetaType>
#include <QString>

/// 検討タブで選んだ検討の条件。検討タブはこの値を正本として持ち、変わると settingsChanged で知らせる
struct ConsiderationSettings {
    QString engineName;          ///< 検討に使うエンジン（登録名）
    bool unlimitedTime = true;   ///< 時間無制限で検討する
    int byoyomiSec = 20;         ///< 時間を決めるときの1局面の検討時間（秒）
    int multiPV = 1;             ///< 候補手の数（1〜10）
    bool showArrows = true;      ///< 候補手の矢印を盤に出す

    /// 1局面の検討時間（ms。0 は無制限）
    int byoyomiMs() const { return unlimitedTime ? 0 : byoyomiSec * 1000; }

    bool operator==(const ConsiderationSettings& other) const
    {
        return engineName == other.engineName && unlimitedTime == other.unlimitedTime
            && byoyomiSec == other.byoyomiSec && multiPV == other.multiPV && showArrows == other.showArrows;
    }
    bool operator!=(const ConsiderationSettings& other) const { return !(*this == other); }
};

Q_DECLARE_METATYPE(ConsiderationSettings)

#endif // CONSIDERATIONSETTINGS_H
