#ifndef ENGINEREGISTRATIONDIALOG_H
#define ENGINEREGISTRATIONDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QSettings>
#include "engineoptions.h"

namespace Ui {
class EngineRegistrationDialog;
}

// エンジンの構造体
struct Engine
{
    // エンジン名
    QString name;

    // エンジンの実行ファイル名
    QString path;
};

// エンジン登録ダイアログ
class EngineRegistrationDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit EngineRegistrationDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~EngineRegistrationDialog() override;

private slots:
    // 追加ボタンが押されたときに呼び出されるスロット
    void addEngineFromFileSelection();

    // 削除ボタンが押されたときに呼び出されるスロット
    void removeEngine();

    // 設定ボタンが押されたときに呼び出されるスロット
    void configureEngine();

    // 将棋エンジンの出力を１行ずつ読み取り、エンジン情報やオプション情報を取得する。
    void processEngineOutput();

    // QProcessのエラーが発生したときに呼び出されるスロット
    void onProcessError(const QProcess::ProcessError error);

    // プロセスの標準エラー出力を処理する。
    void processEngineErrorOutput();

signals:
    // MainWindowにエラーを報告するためのシグナル
    void errorReported(const QString& errorMessage);

private:
    Ui::EngineRegistrationDialog *ui;

    // 将棋エンジンのプロセス
    QProcess* m_process;

    // エラーが発生したかどうかを示すフラグ
    bool m_errorOccurred;

    // 将棋エンジンから"option name"行を保存するリスト
    QStringList m_optionLines;

    // 将棋エンジンのファイル名
    QString m_fileName;

    // 将棋エンジンのディレクトリ
    QString m_engineDir;

    // エンジン名
    QString m_engineIdName;

    // エンジンのリスト
    QList<Engine> m_engineList;

    // エンジンオプションのリスト
    //QList<EngineOption> m_optionList;
    QList<EngineOption> m_engineOptions;

    // comboタイプのオプションの文字列リスト
    QStringList m_concatenatedOptionValuesList;

    // コマンド文字列の定数
    static constexpr char UsiCommand[] = "usi\n";
    static constexpr char QuitCommand[] = "quit\n";
    static constexpr char IdNamePrefix[] = "id name";
    static constexpr char OptionNamePrefix[] = "option name";
    static constexpr char UsiOkPrefix[] = "usiok";

    // 文法チェックの結果を保持する構造体
    struct ValidationResult {
        bool isValid;
        QString errorMessage;
    };

    // USI optionコマンドの文法をチェックする関数
    ValidationResult checkOptionSyntax(const QString& optionCommand);

    // シグナル・スロットの接続を行う。
    void initializeSignals() const;

    // 設定ファイルにエンジン名と絶対パス付きの実行ファイル名を書き込む。
    void saveEnginesToSettingsFile() const;

    // usiコマンドの出力からエンジンオプションを取り出す。
    void getEngineOptions();

    // 設定ファイルに追加エンジンのオプションを書き込む。；
    void saveEngineOptionsToSettings() const;

    // 指定されたエンジン情報を設定ファイルに保存する。
    void saveEngineToSettings(QSettings& settings, const Engine& engine) const;

    // 設定ファイルから[Engines]グループを削除する。
    void removeEnginesGroup() const;

    // 設定ファイルからエンジン名グループを削除する。
    void removeEngineNameGroup(const QString& removeEngineName) const;

    // エンジン登録ダイアログから選択したエンジンを削除する。
    void removeSelectedEngineFromList(QString& removeEngineName);

    // comboタイプのオプションの文字列を作成する。
    void concatenateComboOptionValues();

    // 将棋エンジンを起動し、対局開始に関するコマンドを送信する。
    void startAndInitializeEngine(const QString& engineFile);

    // 将棋エンジンを起動する。
    void startEngine(const QString& engineFile);

    // usiコマンドを将棋エンジンに送信する。
    void sendUsiCommand() const;

    // quitコマンドを将棋エンジンに送信する。
    void sendQuitCommand() const;

    // 登録したいエンジンが既に登録されている場合、重複エラーのメッセージを表示する。
    void duplicateEngine();

    // 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込み、GUIのリストウィジェットにエンジン名を追加する。
    void loadEnginesFromSettings();

    // 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込む。
    Engine readEngineFromSettings(const QSettings& settings) const;

    // ファイルのパスが有効かどうかを検証する。
    bool validateEnginePath(const QString& filePath) const;

    // 将棋エンジンからの出力を解析する。
    void parseEngineOutput(const QString& line);

    // エンジン登録が重複している場合、重複エラーのメッセージを表示する。
    void handleDuplicateEngine(const QString& engineName);

    // エンジン名と絶対パスでの実行ファイル名を取得する。
    void processIdName(const QString& line);

    // エンジンオプション構造体に値を設定する。
    void addOption(const QString& name, const QString& type, const QString& defaultValue, const QString& min,
                   const QString& max, const QString& currentValue);

    // 将棋エンジンから送信されたオプション行を解析し、エンジンオプションリストに追加する。
    void parseOptionLine(const QString& line);

    // エラーメッセージを表示する。
    void showErrorMessage(const QString &errorMessage);
};

#endif // ENGINEREGISTRATIONDIALOG_H
