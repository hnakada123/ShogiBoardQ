#ifndef ENGINEREGISTRATIONHANDLER_H
#define ENGINEREGISTRATIONHANDLER_H

/// @file engineregistrationhandler.h
/// @brief エンジン登録ハンドラクラスの定義（プロセス管理・USI通信・設定永続化）

#include <QObject>
#include <QProcess>
#include <QSettings>
#include <memory>
#include "engineoptions.h"

struct Engine;

/// エンジン登録の実行処理を担当するハンドラ
class EngineRegistrationHandler : public QObject
{
    Q_OBJECT

public:
    explicit EngineRegistrationHandler(QObject *parent = nullptr);
    ~EngineRegistrationHandler() override;

    /// 設定ファイルからエンジンリストを読み込む
    void loadEnginesFromSettings();

    /// エンジンリストを取得する
    const QList<Engine>& engineList() const;

    /// 指定インデックスのエンジン情報を取得する
    const Engine& engineAt(int index) const;

    /// パスの重複を検査する（重複時は existingName にエンジン名を返す）
    bool isDuplicatePath(const QString& path, QString& existingName) const;

    /// エンジンパスが有効かどうかを検証する
    bool validateEnginePath(const QString& filePath) const;

    /// エンジン登録処理を開始する（プロセス起動→USI送信）
    void startRegistration(const QString& filePath);

    /// 指定インデックスのエンジンを削除し設定を更新する
    void removeEngineAt(int index);

signals:
    /// エンジン登録が完了したとき
    void engineRegistered(const QString& engineName);

    /// エラーが発生したとき
    void errorOccurred(const QString& message);

private slots:
    void processEngineOutput();
    void processEngineErrorOutput();
    void onProcessError(QProcess::ProcessError error);

private:
    struct ValidationResult {
        bool isValid;
        QString errorMessage;
    };

    // プロセスライフサイクル
    void startEngine(const QString& engineFile);
    void cleanupEngineProcess();
    void sendUsiCommand() const;
    void sendQuitCommand() const;

    // USIプロトコル解析
    void parseEngineOutput(const QString& line);
    void processIdName(const QString& line);
    void parseEngineOptionsFromUsiOutput();
    void parseOptionLine(const QString& line);
    ValidationResult checkOptionSyntax(const QString& optionCommand);
    void addOption(const QString& name, const QString& type, const QString& defaultValue,
                   const QString& min, const QString& max, const QString& currentValue);
    void concatenateComboOptionValues();

    // 設定I/O
    Engine readEngineFromSettings(const QSettings& settings) const;
    void saveEnginesToSettingsFile() const;
    void saveEngineToSettings(QSettings& settings, const Engine& engine) const;
    void saveEngineOptionsToSettings() const;
    void removeEnginesGroup() const;
    void removeEngineNameGroup(const QString& removeEngineName) const;

    // エラー設定＋シグナル送出
    void setError(const QString& message);

    // 状態
    std::unique_ptr<QProcess> m_process;
    bool m_errorOccurred = false;
    QStringList m_optionLines;
    QString m_fileName;
    QString m_engineDir;
    QString m_engineIdName;
    QString m_engineIdAuthor;
    QList<Engine> m_engineList;
    QList<EngineOption> m_engineOptions;
    QStringList m_concatenatedOptionValuesList;

    // コマンド文字列の定数
    static constexpr char UsiCommand[] = "usi\n";
    static constexpr char QuitCommand[] = "quit\n";
    static constexpr char IdNamePrefix[] = "id name";
    static constexpr char IdAuthorPrefix[] = "id author";
    static constexpr char OptionNamePrefix[] = "option name";
    static constexpr char UsiOkPrefix[] = "usiok";
};

#endif // ENGINEREGISTRATIONHANDLER_H
