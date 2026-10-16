/// @file imageexportcontroller.cpp
/// @brief 盤と評価値グラフの画像のコピー・保存の実装

#include "imageexportcontroller.h"

#include "boardimageexporter.h"
#include "evaluationchartwidget.h"
#include "shogiview.h"

ImageExportController::ImageExportController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.shogiView && m_deps.evalChart);
}

void ImageExportController::copyBoardToClipboard()
{
    BoardImageExporter::copyToClipboard(m_deps.shogiView);
}

void ImageExportController::copyEvalGraphToClipboard()
{
    BoardImageExporter::copyToClipboard(m_deps.evalChart->chartViewWidget());
}

void ImageExportController::saveBoardImage()
{
    BoardImageExporter::saveImage(m_deps.parentWidget, m_deps.shogiView);
}

void ImageExportController::saveEvalGraphImage()
{
    BoardImageExporter::saveImage(m_deps.parentWidget, m_deps.evalChart->chartViewWidget(),
                                  QStringLiteral("EvalGraph"));
}
