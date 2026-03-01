#pragma once

/// @file engineregistrationworker.h
/// @brief エンジン登録ワーカークラスの定義（ワーカースレッドでのQProcess通信）

#include "threadtypes.h"
#include <QObject>
#include <QStringList>

/// ワーカースレッドでエンジンプロセスを起動し、USI通信を行うワーカー
///
/// QThread に moveToThread() して使用する。
/// registerEngine() スロットが呼び出されると、QProcess を生成して
/// USI プロトコルの初期ハンドシェイクを実行し、結果をシグナルで通知する。
class EngineRegistrationWorker : public QObject
{
    Q_OBJECT

public:
    explicit EngineRegistrationWorker(QObject* parent = nullptr);

    /// キャンセルフラグを設定する（registerEngine 呼び出し前にメインスレッドから設定する）
    void setCancelFlag(CancelFlag flag);

public slots:
    /// エンジン登録処理を実行する（ワーカースレッドで実行される）
    void registerEngine(const QString& enginePath);

signals:
    /// エンジン登録が成功したとき
    void engineRegistered(const QString& engineName,
                          const QString& engineAuthor,
                          const QStringList& optionLines);

    /// エンジン登録が失敗したとき
    void registrationFailed(const QString& errorMessage);

    /// 進捗通知
    void progressUpdated(const QString& status);

private:
    CancelFlag m_cancelFlag;

    static constexpr int StartTimeoutMs = 5000;
    static constexpr int UsiOkTimeoutMs = 30000;
    static constexpr int ReadIntervalMs = 200;
    static constexpr int QuitTimeoutMs = 3000;
};
