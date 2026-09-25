/// @file boardimagerenderer.cpp
/// @brief SFEN から盤面画像を描画する実装

#include "boardimagerenderer.h"

#include "sfenvalidationservice.h"
#include "shogiboard.h"
#include "shogiview.h"

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QImageWriter>

#include <memory>

namespace {

/// USI の升目（"7g" 等）を筋・段に変換する。駒打ちの元升は false
bool parseSquare(const QString& text, int& file, int& rank)
{
    if (text.size() != 2) return false;
    const int f = text.at(0).digitValue();
    const int r = text.at(1).toLatin1() - 'a' + 1;
    if (f < 1 || f > 9 || r < 1 || r > 9) return false;
    file = f;
    rank = r;
    return true;
}

} // namespace

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
    view.setClockEnabled(false);
    view.setBlackPlayerName(options.blackName);
    view.setWhitePlayerName(options.whiteName);
    view.configureFixedSizing(options.squareSize);
    view.applyBoardAndRender(&board);
    view.setActiveSide(board.currentPlayer() == Turn::Black);
    view.resize(view.sizeHint());

    // ハイライトは非所有なので描画が終わるまで保持する
    std::unique_ptr<ShogiView::FieldHighlight> fromHighlight;
    std::unique_ptr<ShogiView::FieldHighlight> toHighlight;
    const QString move = options.lastMoveUsi.trimmed();
    if (move.size() >= 4) {
        int file = 0;
        int rank = 0;
        if (parseSquare(move.mid(2, 2), file, rank)) {
            toHighlight = std::make_unique<ShogiView::FieldHighlight>(file, rank, QColor(255, 165, 0, 140));
            view.addHighlight(toHighlight.get());
        }
        if (move.at(1) != QLatin1Char('*') && parseSquare(move.left(2), file, rank)) {
            fromHighlight = std::make_unique<ShogiView::FieldHighlight>(file, rank, QColor(255, 255, 0, 110));
            view.addHighlight(fromHighlight.get());
        }
    }

    QImage image = view.toImage();
    view.removeHighlightAllData();
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
