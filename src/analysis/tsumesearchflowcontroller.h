#ifndef TSUMESEARCHFLOWCONTROLLER_H
#define TSUMESEARCHFLOWCONTROLLER_H

#include <QObject>
#include <functional>

class QWidget;
class TsumeShogiSearchDialog;
class MatchCoordinator;

class TsumeSearchFlowController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        MatchCoordinator* match = nullptr;     // 司令塔
        // 局面生成に必要な素材（MainWindow から供給）
        const QStringList* sfenRecord = nullptr;  // 棋譜の SFEN 列
        QString            startSfenStr;          // 開始 SFEN（空可）
        QStringList        positionStrList;       // 既存 "position ..." の列
        int                currentMoveIndex = 0;  // 現在の行
        const QStringList* usiMoves = nullptr;    // USI形式の指し手リスト（moves形式用）
        QString            startPositionCmd;      // 開始局面コマンド（"startpos" or "sfen ..."）
        std::function<void(const QString&)> onError; // 任意: エラー表示
    };

    explicit TsumeSearchFlowController(QObject* parent=nullptr);
    void runWithDialog(const Deps& d, QWidget* parent);

private:
    QString buildPositionForMate(const Deps& d) const;
    void startAnalysis(MatchCoordinator* match,
                        const QString& enginePath,
                        const QString& engineName,
                        const QString& positionStr,
                        int byoyomiMs);
};

#endif // TSUMESEARCHFLOWCONTROLLER_H
