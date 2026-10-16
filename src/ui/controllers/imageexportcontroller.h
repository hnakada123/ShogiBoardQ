#ifndef IMAGEEXPORTCONTROLLER_H
#define IMAGEEXPORTCONTROLLER_H

/// @file imageexportcontroller.h
/// @brief 盤と評価値グラフの画像のコピー・保存

#include <QObject>

class EvaluationChartWidget;
class ShogiView;
class QWidget;

/**
 * @brief メニューの「盤面の画像」「評価値グラフの画像」のコピー・保存を行う
 */
class ImageExportController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;              ///< 保存ダイアログの親
        ShogiView* shogiView = nullptr;               ///< 盤（必須）
        EvaluationChartWidget* evalChart = nullptr;   ///< 評価値グラフ（必須）
    };

    explicit ImageExportController(const Deps& deps, QObject* parent = nullptr);

public slots:
    void copyBoardToClipboard();
    void copyEvalGraphToClipboard();
    void saveBoardImage();
    void saveEvalGraphImage();

private:
    const Deps m_deps;
};

#endif // IMAGEEXPORTCONTROLLER_H
