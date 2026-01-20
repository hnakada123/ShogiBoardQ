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
// 主にYaneuraOuの設定項目に対応
class EngineOptionDescriptions
{
public:
    // 指定されたオプション名に対する説明を取得する
    // オプション名が見つからない場合は空文字列を返す
    static QString getDescription(const QString& optionName);

    // 指定されたオプション名に対する説明が存在するかどうかを確認する
    static bool hasDescription(const QString& optionName);

    // 指定されたオプション名に対するカテゴリを取得する
    static EngineOptionCategory getCategory(const QString& optionName);

    // カテゴリの表示名を取得する
    static QString getCategoryDisplayName(EngineOptionCategory category);

    // 定義されている全カテゴリのリストを取得する（表示順）
    static QList<EngineOptionCategory> getAllCategories();

private:
    // オプション説明のマップを初期化する
    static void initializeDescriptions();

    // オプションカテゴリのマップを初期化する
    static void initializeCategories();

    // オプション名と説明のマップ
    static QHash<QString, QString> s_descriptions;

    // オプション名とカテゴリのマップ
    static QHash<QString, EngineOptionCategory> s_categories;

    // 初期化済みフラグ
    static bool s_initialized;
};

#endif // ENGINEOPTIONDESCRIPTIONS_H
