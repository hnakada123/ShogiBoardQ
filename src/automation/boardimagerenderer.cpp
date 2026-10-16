/// @file boardimagerenderer.cpp
/// @brief SFEN から盤面画像を描画する実装

#include "boardimagerenderer.h"

#include "sfenvalidationservice.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "usimove.h"

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QImageWriter>

#include <memory>

QImage BoardImageRenderer::render(const QString& sfen, const Options& options, QString* error)
{
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) {
        if (error) *error = QStringLiteral("A QApplication (offscreen is fine) is required to render a board");
        return {};
    }
    const SfenValidation validation = SfenValidationService::validate(sfen);
    if (validation.normalizedSfen.isEmpty()) {
        if (error) *error = validation.errors.join(QStringLiteral("; "));
        return {};
    }
    if (options.squareSize < 20 || options.squareSize > 150) {
        if (error) *error = QStringLiteral("square_size must be between 20 and 150");
        return {};
    }

    ShogiBoard board;
    board.setSfen(validation.normalizedSfen);

    ShogiView view;
    view.setFlipMode(options.flip);
    view.chrome()->setClockEnabled(false);
    view.chrome()->card(Turn::Black)->setName(options.blackName);
    view.chrome()->card(Turn::White)->setName(options.whiteName);
    view.configureFixedSizing(options.squareSize);
    view.applyBoardAndRender(&board);
    view.resize(view.sizeHint());

    if (const auto move = UsiMove::parse(options.lastMoveUsi)) {
        view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveTo, {QPoint(move->to.file, move->to.rank)});
        // 駒打ちは移動元を強調しない
        if (!move->isDrop())
            view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveFrom,
                                               {QPoint(move->from.file, move->from.rank)});
    }

    QImage image = view.toImage();
    if (image.isNull() && error) *error = QStringLiteral("Rendering produced an empty image");
    return image;
}

bool BoardImageRenderer::renderToFile(const QString& sfen, const QString& outputPath, const Options& options,
                                      QString* error, QSize* outSize)
{
    const QImage image = render(sfen, options, error);
    if (image.isNull()) return false;

    QString suffix = QFileInfo(outputPath).suffix().toLower();
    if (suffix == QLatin1String("jpg")) suffix = QStringLiteral("jpeg");
    if (suffix.isEmpty()) suffix = QStringLiteral("png");
    QImageWriter writer(outputPath, suffix.toLatin1());
    if (suffix == QLatin1String("jpeg") || suffix == QLatin1String("webp")) writer.setQuality(95);
    if (!writer.write(image)) {
        if (error) *error = QStringLiteral("Failed to write %1: %2").arg(outputPath, writer.errorString());
        return false;
    }
    if (outSize) *outSize = image.size();
    return true;
}
