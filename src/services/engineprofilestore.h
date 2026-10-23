#ifndef ENGINEPROFILESTORE_H
#define ENGINEPROFILESTORE_H

/// @file engineprofilestore.h
/// @brief 登録エンジンの設定（一覧・USIオプション・先読みの設定）の保存形式を知る唯一の場所

#include <QList>
#include <QString>

#include "engineoption.h"
#include "registeredengines.h"

/**
 * @brief 登録エンジンの設定を読み書きする
 *
 * 保存形式（エンジン一覧の配列 Engines と、エンジンごとの EngineOptions/<名前> にあるオプションの配列と
 * GUI の設定）を知るのはここだけで、ほかの部品は設定ファイルを開かずにこの関数を使う。
 * 予約オプション USI_Ponder は、GUI の先読みの設定（PonderPreferences）として扱う。
 */
namespace EngineProfileStore {

/// 予約オプション USI_Ponder の名前（先読みの可否は GUI の設定から送る）
inline constexpr char kPonderOptionName[] = "USI_Ponder";
inline bool isPonderOption(const QString& name) { return name == QLatin1String(kPonderOptionName); }

/// エンジンごとの先読みの設定
struct PonderPreferences {
    bool enabled = false;               ///< GUI が先読みを始めるか
    bool sendUnreportedOption = true;   ///< 報告がなくても USI_Ponder を送るか
    bool defaultEnabled = false;        ///< エンジンが報告した USI_Ponder の既定値（未報告なら false）
};

/// 起動のたびにエンジンへ送る設定
struct LaunchOptions {
    struct Option {
        QString name;
        QString value;
        bool button = false;   ///< button 型（値なしで送る）
    };
    QList<Option> options;              ///< 保存したオプション（USI_Ponder を除く。button 型は on のものだけ）
    bool ponderEnabled = false;
    bool sendUnreportedPonder = true;
};

// --- エンジン一覧 ---
QList<EngineEntry> loadEngines();
/// エンジン一覧を置き換える
void saveEngines(const QList<EngineEntry>& engines);

// --- エンジンごとの設定 ---
/// 保存したオプション（エンジンが報告した順）
QList<EngineOption> loadOptions(const QString& engineName);
/// オプションを置き換える（GUI の設定は残す）
void saveOptions(const QString& engineName, const QList<EngineOption>& options);
/// エンジンごとの設定（オプション・GUI の設定）を消す
void removeEngine(const QString& engineName);

PonderPreferences loadPonder(const QString& engineName);
/// 先読みの設定を保存する（保存したオプションに USI_Ponder があれば、その値も同じにする）
void savePonder(const QString& engineName, bool enabled, bool sendUnreportedOption);

/// 起動時にエンジンへ送る設定
LaunchOptions launchOptions(const QString& engineName);

/**
 * @brief 以前の版がエンジン名の最上位グループに置いた設定を EngineOptions の下へ移す
 *
 * 起動時（GUI と CLI）に1回呼ぶ。登録済みエンジンごとに、USIオプションの配列（size と番号付きのキー）と
 * GUI の設定（gui/...）だけを移し、同じ名前のグループにあるほかの設定には触れない。
 * 新しいグループにすでに設定があるエンジンは移さない。
 * @return 移したエンジンの数
 */
int migrateLegacyOptionGroups();

} // namespace EngineProfileStore

#endif // ENGINEPROFILESTORE_H
