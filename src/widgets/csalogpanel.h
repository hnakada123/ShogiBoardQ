#ifndef CSALOGPANEL_H
#define CSALOGPANEL_H

/// @file csalogpanel.h
/// @brief CSA通信ログパネルクラスの定義

#include <QObject>
#include <memory>

class QWidget;
class QPlainTextEdit;
class QToolButton;
class QPushButton;
class QLineEdit;

class LogViewFontManager;

/**
 * @brief CSA通信ログの表示・操作を担うパネル
 *
 * EngineAnalysisTab から CSA通信ログ関連の責務を分離したクラス。
 * ツールバー（フォントサイズ）、コマンド入力バー、ログ表示エリアを持つ。
 */
class CsaLogPanel : public QObject
{
    Q_OBJECT

public:
    explicit CsaLogPanel(QObject* parent = nullptr);
    ~CsaLogPanel() override;

    /// CSA通信ログページのUIを構築し返す
    QWidget* buildUi(QWidget* parent);

    /// ログ行を追記
    void append(const QString& line);

    /// ログをクリア
    void clear();

signals:
    /// CSAコマンド送信シグナル
    void csaRawCommandRequested(const QString& command);

private slots:
    void onFontIncrease();
    void onFontDecrease();
    void onCommandEntered();

private:
    void buildToolbar();
    void buildCommandBar();
    void initFontManager();

    QWidget* m_container = nullptr;
    QWidget* m_toolbar = nullptr;
    QPlainTextEdit* m_logView = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;
    QToolButton* m_btnFontDecrease = nullptr;
    int m_fontSize = 10;
    std::unique_ptr<LogViewFontManager> m_fontManager;

    QWidget* m_commandBar = nullptr;
    QPushButton* m_btnSendToServer = nullptr;
    QLineEdit* m_commandInput = nullptr;
};

#endif // CSALOGPANEL_H
