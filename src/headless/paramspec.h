#ifndef PARAMSPEC_H
#define PARAMSPEC_H

/// @file paramspec.h
/// @brief 操作の引数の表（CLI のオプション・自動化 API の検査・MCP のスキーマで共有する。GUI 非依存）

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * @brief 引数1つの仕様（名前・型・必須・範囲・既定値・説明）
 *
 * 名前は JSON の名前（snake_case）。CLI のオプション名は "_" を "-" にしたもの。
 * 範囲は、整数なら値の範囲、文字列と配列なら長さの範囲。
 */
struct ParamSpec {
    enum class Type { Integer, String, Boolean, StringArray, IntegerArray, Object, Any };

    /// CLI のコマンドラインだけで受け付ける旧名（JSON Schema には出さない）
    struct Alias {
        QString option;                                         ///< オプション名（"--" を除く）
        QJsonValue flagValue = QJsonValue(QJsonValue::Undefined); ///< 値を取らないフラグなら入れる値
        double scale = 1.0;                                     ///< 値に掛ける数（整数は切り上げ。ミリ秒→秒は 0.001）
    };

    QString name;
    Type type = Type::String;
    QString description;
    bool required = false;
    std::optional<qint64> minimum;
    std::optional<qint64> maximum;
    std::optional<qint64> itemMinimum;  ///< IntegerArray の要素の下限
    QJsonValue defaultValue = QJsonValue(QJsonValue::Undefined);
    QStringList choices;                ///< 文字列の選択肢（空なら制限なし）
    QString pattern;                    ///< 文字列（StringArray は各要素）が合う正規表現（JSON Schema の pattern。空なら制限なし）
    QList<ParamSpec> properties;        ///< Object の項目（知らない項目は拒否する）
    QString valueName;                  ///< CLI の使い方に出す値の名前
    QList<Alias> aliases;               ///< CLI の旧名

    ParamSpec& require() { required = true; return *this; }
    ParamSpec& withDefault(const QJsonValue& value) { defaultValue = value; return *this; }
    ParamSpec& range(qint64 low, qint64 high) { minimum = low; maximum = high; return *this; }
    ParamSpec& atLeast(qint64 low) { minimum = low; return *this; }
    ParamSpec& atMost(qint64 high) { maximum = high; return *this; }
    ParamSpec& length(qint64 low, qint64 high) { minimum = low; maximum = high; return *this; }
    ParamSpec& value(const QString& cliValueName) { valueName = cliValueName; return *this; }
    ParamSpec& matching(const QString& regularExpression) { pattern = regularExpression; return *this; }
    /// 値を取る旧名（scale は値に掛ける数）
    ParamSpec& alias(const QString& option, double scale = 1.0) { aliases.append({option, QJsonValue(QJsonValue::Undefined), scale}); return *this; }
    /// 値を取らない旧名（指定すると flag の値になる。例: --no-final-alternatives は false）
    ParamSpec& aliasFlag(const QString& option, bool flag) { aliases.append({option, QJsonValue(flag), 1.0}); return *this; }
};

/// 操作1つ（CLI のコマンド・自動化 API のメソッド）の仕様
struct OperationSpec {
    QString name;
    QString summary;
    QList<ParamSpec> params;

    /// 名前の引数（無ければ nullptr）
    const ParamSpec* param(const QString& paramName) const;
};

namespace ParamSpecs {

// --- 表を書くための部品 ---
ParamSpec integer(const QString& name, const QString& description, qint64 minimum, qint64 maximum);
ParamSpec string(const QString& name, const QString& description);
ParamSpec boolean(const QString& name, const QString& description, bool defaultValue = false);
ParamSpec choice(const QString& name, const QString& description, const QStringList& choices);
ParamSpec stringArray(const QString& name, const QString& description);
ParamSpec integerArray(const QString& name, const QString& description, qint64 itemMinimum);
ParamSpec object(const QString& name, const QString& description, const QList<ParamSpec>& properties);
ParamSpec any(const QString& name, const QString& description);

/// USI の指し手1つ（7g7f・7g7f+・P*5e）の正規表現（moves・last_move の pattern）
QString usiMovePattern();

/// 引数全体の JSON Schema（type object・properties・required・additionalProperties false）
QJsonObject inputSchema(const OperationSpec& operation);
/// describe に出す形（name・summary・input_schema）
QJsonObject toJson(const OperationSpec& operation);

/**
 * @brief JSON の引数を表で検査し、既定値を補った引数を返す
 *
 * 知らない引数・必須の欠け・型・範囲・選択肢を検査する。失敗したら nullopt と error。
 */
std::optional<QJsonObject> validate(const OperationSpec& operation, const QJsonObject& params, QString* error);

} // namespace ParamSpecs

#endif // PARAMSPEC_H
