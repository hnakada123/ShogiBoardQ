#ifndef ENGINEOPTIONDESCRIPTIONS_H
#define ENGINEOPTIONDESCRIPTIONS_H

/// @file engineoptiondescriptions.h
/// @brief USIエンジンオプション説明データベースの定義


#include <QString>
#include <QHash>
#include <QList>

/**
 * @brief エンジンオプションのカテゴリ分類
 *
 */
enum class EngineOptionCategory {
    Basic,          ///< 基本設定
    Thinking,       ///< 思考設定
    TimeControl,    ///< 時間制御
    Book,           ///< 定跡設定
    GameRule,       ///< 対局ルール
    Other           ///< その他
};

/**
 * @brief USIエンジンオプションの説明とカテゴリを提供する静的クラス
 *
 * エンジン名に基づいて適切な説明を返す。
 * 対応エンジン: YaneuraOu系、Gikou系。
 *
 */
class EngineOptionDescriptions
{
public:
    /**
     * @brief 指定エンジン・オプションに対する説明を取得する
     * @return 対応するエンジンが未登録またはオプション名が未登録の場合は空文字列
     */
    static QString getDescription(const QString& engineName, const QString& optionName);

    /// エンジン・オプションに対する説明が存在するかを確認する
    static bool hasDescription(const QString& engineName, const QString& optionName);

    /**
     * @brief 指定エンジン・オプションに対するカテゴリを取得する
     * @return 未対応エンジンの場合は Other
     */
    static EngineOptionCategory getCategory(const QString& engineName, const QString& optionName);

    /// カテゴリの表示名を取得する
    static QString getCategoryDisplayName(EngineOptionCategory category);

    /// 定義されている全カテゴリのリストを表示順で取得する
    static QList<EngineOptionCategory> getAllCategories();

    /// エンジンが説明文データベースに対応しているかを確認する
    static bool isEngineSupported(const QString& engineName);

private:
    static void initializeDescriptions();
    static void initializeCategories();

    /// エンジン名がYaneuraOu系かどうかを判定する（大文字小文字を区別しない）
    static bool isYaneuraOuEngine(const QString& engineName);

    /// エンジン名がGikou系かどうかを判定する
    static bool isGikouEngine(const QString& engineName);

    static QHash<QString, QString> s_yaneuraouDescriptions;             ///< YaneuraOu用オプション説明マップ
    static QHash<QString, EngineOptionCategory> s_yaneuraouCategories;  ///< YaneuraOu用カテゴリマップ
    static QHash<QString, QString> s_gikouDescriptions;                 ///< Gikou用オプション説明マップ
    static QHash<QString, EngineOptionCategory> s_gikouCategories;      ///< Gikou用カテゴリマップ
    static bool s_initialized;                                          ///< 初期化済みフラグ
};

#endif // ENGINEOPTIONDESCRIPTIONS_H
