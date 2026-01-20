#ifndef CONSIDERATIONFLOWCONTROLLER_H
#define CONSIDERATIONFLOWCONTROLLER_H

#include <QObject>
#include <functional>

class QWidget;
class ConsiderationDialog;
class MatchCoordinator;

/**
 * 単一局面の「検討」実行を司る薄いフローコントローラ。
 * MainWindow から UI と司令塔(MatchCoordinator)の橋渡しだけを引き剥がす。
 */
class ConsiderationFlowController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        MatchCoordinator* match = nullptr;               // 司令塔
        std::function<void(const QString&)> onError;     // 任意: エラー表示
    };

    explicit ConsiderationFlowController(QObject* parent=nullptr);

    /**
     * 検討ダイアログを表示して、OKなら司令塔へ startAnalysis を投げます。
     * @param positionStr 送信する "position ..." 文字列（MainWindow側で既に用意）
     */
    void runWithDialog(const Deps& d, QWidget* parent, const QString& positionStr);

private:
    void startAnalysis(MatchCoordinator* match,
                        const QString& enginePath,
                        const QString& engineName,
                        const QString& positionStr,
                        int byoyomiMs);
};

#endif // CONSIDERATIONFLOWCONTROLLER_H
