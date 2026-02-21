#ifndef TSUMESHOGIGENERATORDIALOG_H
#define TSUMESHOGIGENERATORDIALOG_H

/// @file tsumeshogigeneratordialog.h
/// @brief 詰将棋局面生成ダイアログクラスの定義

#include <QDialog>
#include <QList>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QToolButton;
class TsumeshogiGenerator;

/**
 * @brief 詰将棋局面生成ダイアログ
 *
 * エンジン選択、生成パラメータ設定、プログレス表示、結果テーブルを
 * 1つのダイアログにまとめる。
 */
class TsumeshogiGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TsumeshogiGeneratorDialog(QWidget* parent = nullptr);
    ~TsumeshogiGeneratorDialog() override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPositionFound(const QString& sfen, const QStringList& pv);
    void onProgressUpdated(int tried, int found, qint64 elapsedMs);
    void onGeneratorFinished();
    void onGeneratorError(const QString& message);
    void onCopySelected();
    void onCopyAll();
    void onFontIncrease();
    void onFontDecrease();
    void showEngineSettingsDialog();

private:
    /// エンジン情報
    struct Engine {
        QString name;
        QString path;
    };

    void setupUi();
    void readEngineNameAndDir();
    void loadSettings();
    void saveSettings();
    void updateFontSize(int delta);
    void applyFontSize();
    void setRunningState(bool running);
    QString formatElapsedTime(qint64 ms) const;

    // エンジン設定
    QComboBox* m_comboEngine = nullptr;
    QPushButton* m_btnEngineSetting = nullptr;

    // 生成設定
    QSpinBox* m_spinTargetMoves = nullptr;
    QSpinBox* m_spinMaxAttack = nullptr;
    QSpinBox* m_spinMaxDefend = nullptr;
    QSpinBox* m_spinAttackRange = nullptr;
    QSpinBox* m_spinTimeout = nullptr;
    QSpinBox* m_spinMaxPositions = nullptr;

    // 制御ボタン
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;

    // プログレス表示
    QLabel* m_labelProgress = nullptr;
    QLabel* m_labelElapsed = nullptr;

    // 結果テーブル
    QTableWidget* m_tableResults = nullptr;

    // フォントボタン
    QToolButton* m_btnFontDecrease = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;

    // コピーボタン
    QPushButton* m_btnCopySelected = nullptr;
    QPushButton* m_btnCopyAll = nullptr;

    // 閉じるボタン
    QPushButton* m_btnClose = nullptr;

    // エンジンリスト
    QList<Engine> m_engineList;

    // ジェネレータ
    std::unique_ptr<TsumeshogiGenerator> m_generator;

    // フォントサイズ
    int m_fontSize = 10;
};

#endif // TSUMESHOGIGENERATORDIALOG_H
