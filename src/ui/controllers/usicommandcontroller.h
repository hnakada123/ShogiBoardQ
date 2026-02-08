#ifndef USICOMMANDCONTROLLER_H
#define USICOMMANDCONTROLLER_H

/// @file usicommandcontroller.h
/// @brief USIコマンド送信コントローラクラスの定義


#include <QObject>
#include <QString>

class MatchCoordinator;
class EngineAnalysisTab;

/**
 * @brief USI手動送信を担当するコントローラ
 *
 * MainWindowから送信ロジックを分離し、エンジン実行状態のチェックと
 * ログ出力を一箇所で扱う。
 */
class UsiCommandController : public QObject
{
    Q_OBJECT
public:
    explicit UsiCommandController(QObject* parent = nullptr);

    void setMatchCoordinator(MatchCoordinator* match);
    void setAnalysisTab(EngineAnalysisTab* tab);

public slots:
    /**
     * @brief USIコマンドを手動送信
     * @param target 0=E1, 1=E2, 2=両方
     * @param command 送信するUSI文字列
     */
    void sendCommand(int target, const QString& command);

private:
    void appendStatus(const QString& text);

    MatchCoordinator* m_match = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
};

#endif // USICOMMANDCONTROLLER_H
