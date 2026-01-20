#ifndef ENGINEOPTIONDESCRIPTIONS_H
#define ENGINEOPTIONDESCRIPTIONS_H

#include <QString>
#include <QHash>
#include <QList>

// オプションカテゴリの列挙型
enum class EngineOptionCategory {
    Basic,          // 基本設定
    Thinking,       // 思考設定
    TimeControl,    // 時間制御
    Book,           // 定跡設定
    GameRule,       // 対局ルール
    Other           // その他
};

// USIエンジンオプションの説明を提供するクラス
// エンジン名に基づいて適切な説明を返す
class EngineOptionDescriptions
{
public:
    // 指定されたエンジン名とオプション名に対する説明を取得する
    // エンジンが対応していない場合やオプション名が見つからない場合は空文字列を返す
    static QString getDescription(const QString& engineName, const QString& optionName);

    // 指定されたエンジン名とオプション名に対する説明が存在するかどうかを確認する
    static bool hasDescription(const QString& engineName, const QString& optionName);

    // 指定されたエンジン名とオプション名に対するカテゴリを取得する
    // 対応していないエンジンの場合は Other を返す
    static EngineOptionCategory getCategory(const QString& engineName, const QString& optionName);

    // カテゴリの表示名を取得する
    static QString getCategoryDisplayName(EngineOptionCategory category);

    // 定義されている全カテゴリのリストを取得する（表示順）
    static QList<EngineOptionCategory> getAllCategories();

    // エンジンが説明文データベースに対応しているかどうかを確認する
    static bool isEngineSupported(const QString& engineName);

private:
    // オプション説明のマップを初期化する
    static void initializeDescriptions();

    // オプションカテゴリのマップを初期化する
    static void initializeCategories();

    // エンジン名がYaneuraOu系かどうかを判定する
    static bool isYaneuraOuEngine(const QString& engineName);

    // エンジン名がGikou系かどうかを判定する
    static bool isGikouEngine(const QString& engineName);

    // オプション名と説明のマップ（YaneuraOu用）
    static QHash<QString, QString> s_yaneuraouDescriptions;

    // オプション名とカテゴリのマップ（YaneuraOu用）
    static QHash<QString, EngineOptionCategory> s_yaneuraouCategories;

    // オプション名と説明のマップ（Gikou用）
    static QHash<QString, QString> s_gikouDescriptions;

    // オプション名とカテゴリのマップ（Gikou用）
    static QHash<QString, EngineOptionCategory> s_gikouCategories;

    // 初期化済みフラグ
    static bool s_initialized;
};

#endif // ENGINEOPTIONDESCRIPTIONS_H
