/// @file boardimagerenderer.cpp
/// @brief SFEN から盤面画像を描画する実装

#include "boardimagerenderer.h"

#include "sfenvalidator.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "usimove.h"

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QImageWriter>

#include <memory>
#include <optional>

QImage BoardImageRenderer::render(const QString& sfen, const Options& options, Failure* failure)
{
    const auto fail = [failure](ErrorKind kind, const QString& message) {
        if (failure) *failure = {kind, message};
        return QImage();
    };
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) {
        return fail(ErrorKind::InternalError, QStringLiteral("A QApplication (offscreen is fine) is required to render a board"));
    }
    const SfenValidation validation = SfenValidator::validate(sfen);
    if (validation.normalizedSfen.isEmpty()) {
        return fail(ErrorKind::InvalidArgument,
                    QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));
    }
    // CLI・自動化 API の引数の表も同じ範囲を検査する。表を通さずに呼ばれても盤の範囲の外は描かない
    if (options.squareSize < BoardGeometry::kMinSquareSize || options.squareSize > BoardGeometry::kMaxSquareSize) {
        return fail(ErrorKind::InvalidArgument, QStringLiteral("square_size must be between %1 and %2")
                                                    .arg(BoardGeometry::kMinSquareSize).arg(BoardGeometry::kMaxSquareSize));
    }
    std::optional<UsiMove> lastMove;
    if (!options.lastMoveUsi.isEmpty()) {
        lastMove = UsiMove::parse(options.lastMoveUsi);
        if (!lastMove) {
            return fail(ErrorKind::InvalidArgument,
                        QStringLiteral("last_move is not a USI move: %1").arg(options.lastMoveUsi));
        }
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

    if (lastMove) {
        view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveTo, {QPoint(lastMove->to.file, lastMove->to.rank)});
        // 駒打ちは移動元を強調しない
        if (!lastMove->isDrop())
            view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveFrom,
                                               {QPoint(lastMove->from.file, lastMove->from.rank)});
    }

    QImage image = view.toImage();
    if (image.isNull()) return fail(ErrorKind::RenderError, QStringLiteral("Rendering produced an empty image"));
    return image;
}

QStringList BoardImageRenderer::fileSuffixes()
{
    return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"),
            QStringLiteral("webp")};
}

bool BoardImageRenderer::renderToFile(const QString& sfen, const QString& outputPath, const Options& options,
                                      Failure* failure, QSize* outSize)
{
    const QImage image = render(sfen, options, failure);
    if (image.isNull()) return false;

    QString suffix = QFileInfo(outputPath).suffix().toLower();
    if (suffix == QLatin1String("jpg")) suffix = QStringLiteral("jpeg");
    if (suffix.isEmpty()) suffix = QStringLiteral("png");
    QImageWriter writer(outputPath, suffix.toLatin1());
    if (suffix == QLatin1String("jpeg") || suffix == QLatin1String("webp")) writer.setQuality(95);
    if (!writer.write(image)) {
        if (failure) *failure = {ErrorKind::FileError, QStringLiteral("Failed to write %1: %2").arg(outputPath, writer.errorString())};
        return false;
    }
    if (outSize) *outSize = image.size();
    return true;
}
