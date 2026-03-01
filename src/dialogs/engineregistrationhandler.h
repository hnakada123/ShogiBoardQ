#ifndef ENGINEREGISTRATIONHANDLER_H
#define ENGINEREGISTRATIONHANDLER_H

/// @file engineregistrationhandler.h
/// @brief エンジン登録ハンドラクラスの定義（ワーカースレッド管理・USI解析・設定永続化）

#include "threadtypes.h"
#include <QObject>
#include <QSettings>
#include "engineoptions.h"

struct Engine;
class QThread;
class EngineRegistrationWorker;

/// エンジン登録の実行処理を担当するハンドラ
///
/// ワーカースレッドでエンジンプロセスとの通信を行い、
/// USIオプション解析と設定永続化はメインスレッドで実行する。
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

    /// エンジン登録処理を開始する（ワーカースレッドで非同期実行）
    void startRegistration(const QString& filePath);

    /// 進行中の登録処理をキャンセルする
    void cancelRegistration();

    /// 指定インデックスのエンジンを削除し設定を更新する
    void removeEngineAt(int index);

    /// 登録処理が進行中かどうかを返す
    bool isRegistrationInProgress() const;

signals:
    /// エンジン登録が完了したとき
    void engineRegistered(const QString& engineName);

    /// エラーが発生したとき
    void errorOccurred(const QString& message);

    /// 登録処理の進行状態が変化したとき
    void registrationInProgressChanged(bool inProgress);

    /// 進捗通知
    void progressUpdated(const QString& status);

    /// ワーカーへの登録開始シグナル（内部使用）
    void requestRegistration(const QString& filePath);

private slots:
    void onWorkerRegistered(const QString& engineName,
                            const QString& engineAuthor,
                            const QStringList& optionLines);
    void onWorkerFailed(const QString& errorMessage);

private:
    struct ValidationResult {
        bool isValid;
        QString errorMessage;
    };

    // ワーカースレッド管理
    void ensureWorkerThread();
    void setRegistrationInProgress(bool inProgress);

    // USIプロトコル解析
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

    // ワーカースレッド
    QThread* m_workerThread = nullptr;
    EngineRegistrationWorker* m_worker = nullptr;
    CancelFlag m_cancelFlag;
    bool m_registrationInProgress = false;

    // 状態
    bool m_errorOccurred = false;
    QStringList m_optionLines;
    QString m_fileName;
    QString m_engineDir;
    QString m_engineIdName;
    QString m_engineIdAuthor;
    QList<Engine> m_engineList;
    QList<EngineOption> m_engineOptions;
    QStringList m_concatenatedOptionValuesList;
};

#endif // ENGINEREGISTRATIONHANDLER_H
