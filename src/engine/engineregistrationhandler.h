#ifndef ENGINEREGISTRATIONHANDLER_H
#define ENGINEREGISTRATIONHANDLER_H

/// @file engineregistrationhandler.h
/// @brief エンジン登録ハンドラクラスの定義（ワーカースレッド管理・USI解析・設定永続化）

#include <QObject>
#include "engineoption.h"
#include "enginelistsettings.h"

class UsiEngineSession;

/// エンジン登録の実行処理を担当するハンドラ
///
/// ワーカースレッドでエンジンプロセスとの通信を行い、
/// USIオプション解析と設定永続化はメインスレッドで実行する。
class EngineRegistrationHandler : public QObject
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_engineregistrationhandler.cpp がオプション解析結果を直接検証する
    friend class TestEngineRegistrationHandler;
#endif

public:
    explicit EngineRegistrationHandler(QObject *parent = nullptr);
    ~EngineRegistrationHandler() override;

    /// 設定ファイルからエンジンリストを読み込む
    void loadEnginesFromSettings();

    /// 登録エンジンの一覧（正本。画面の一覧は enginesChanged で作り直す）
    const QList<EngineListSettings::EngineEntry>& engineList() const;

    /// 指定インデックスのエンジン情報を取得する
    const EngineListSettings::EngineEntry& engineAt(int index) const;

    /// パスの重複を検査する（重複時は existingName にエンジン名を返す）
    bool isDuplicatePath(const QString& path, QString& existingName) const;

    /// エンジンパスが有効かどうかを検証する
    bool validateEnginePath(const QString& filePath) const;

    /// エンジン登録を始める（起動して usi〜usiok で名前とオプションを聞く。結果はシグナルで知らせる）
    void startRegistration(const QString& filePath);

    /// 進行中の登録処理をキャンセルする
    void cancelRegistration();

    /// 指定インデックスのエンジンを削除し設定を更新する
    void removeEngineAt(int index);

    /// 登録処理が進行中かどうかを返す
    bool isRegistrationInProgress() const;

signals:
    /// 登録エンジンの一覧が変わった（読み込み・登録・削除）
    void enginesChanged();
    /// エンジン登録が完了したとき（一覧は先に enginesChanged で知らせる）
    void engineRegistered(const QString& engineName);

    /// エラーが発生したとき
    void errorOccurred(const QString& message);

    /// 登録処理の進行状態が変化したとき
    void registrationInProgressChanged(bool inProgress);

    /// 進捗通知
    void progressUpdated(const QString& status);

private slots:
    void onProbeStarted();
    void onProbeReady();
    void onProbeError(const QString& message);

private:
    /// エンジンが報告した名前・作者・オプションで登録する
    void registerEngine(const QString& engineName, const QString& engineAuthor, const QStringList& optionLines);
    void setRegistrationInProgress(bool inProgress);

    // USIプロトコル解析
    void parseEngineOptionsFromUsiOutput();
    void parseOptionLine(const QString& line);
    void addOption(const QString& name, const QString& type, const QString& defaultValue,
                   const QString& min, const QString& max, const QString& currentValue);

    /// 登録名。同じ id name の登録があれば「名前 (2)」のように番号を付けて重複しない名前にする
    QString uniqueRegisteredName(const QString& idName) const;

    // エラー設定＋シグナル送出
    void setError(const QString& message);

    UsiEngineSession* m_probe = nullptr;   ///< 登録するエンジンへの接続（QObject parent 所有）
    bool m_processStarted = false;         ///< 登録するエンジンのプロセスが起動した
    bool m_registrationInProgress = false;

    // 状態
    bool m_errorOccurred = false;
    QStringList m_optionLines;
    QString m_fileName;
    QString m_engineIdName;
    QString m_engineIdAuthor;
    QList<EngineListSettings::EngineEntry> m_engineList;
    QList<EngineOption> m_engineOptions;
};

#endif // ENGINEREGISTRATIONHANDLER_H
