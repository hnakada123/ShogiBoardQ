/// @file automationcommands_board.cpp
/// @brief 自動化 API の board.* メソッド（盤のマスのクリック）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationinput.h"
#include "automationinteractions.h"
#include "automationwidgets.h"
#include "shogiview.h"

namespace {

using AutomationWidgets::requireInteractive;
using AutomationWidgets::requireWindow;

QPoint squareClickPosition(ShogiView* view, const QPoint& square)
{
    // 入力変換を逆引きして、回転・盤サイズ・駒台の表示位置に追従する。
    const QSize cell = view->fieldSize();
    const int stepX = qMax(1, cell.width() / 4);
    const int stepY = qMax(1, cell.height() / 4);
    QRect area;
    for (int y = 0; y < view->height(); y += stepY) {
        for (int x = 0; x < view->width(); x += stepX) {
            if (view->clickedSquare({x, y}) == square) area |= QRect(x, y, 1, 1);
        }
    }
    if (area.isEmpty() || view->clickedSquare(area.center()) != square) {
        throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Square is not displayed on this board"));
    }
    return area.center();
}

} // namespace

void AutomationCommands::registerBoardCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("board.click"), [context](const QJsonObject& params) {
        // 引数は AutomationMethodSpecs の表で検査済み（既定値も補ってある）
        const QString target = params.value(QStringLiteral("target")).toString();
        const int file = params.value(QStringLiteral("file")).toInt();
        const int rank = params.value(QStringLiteral("rank")).toInt();
        if ((file == 10 && rank > 8) || (file == 11 && rank < 2)) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Invalid hand piece coordinate"));
        }
        const QString button = params.value(QStringLiteral("button")).toString();
        QWidget* window = requireWindow(context, target);
        ShogiView* view = nullptr;
        if (window) {
            const auto views = window->findChildren<ShogiView*>();
            for (auto* candidate : views) {
                if (candidate->window() == window && candidate->isVisible()) { view = candidate; break; }
            }
        }
        if (!view) throw AutomationError(ErrorKind::NotFound, QStringLiteral("No visible board in the target window"));
        const QPoint square(file, rank);
        squareClickPosition(view, square); // 不正な座標は応答前に拒否する。
        const auto mouseButton = button == QLatin1String("left") ? Qt::LeftButton : Qt::RightButton;
        // 応答後に対象が非表示になった場合は、背後の盤面へ入力しない（失敗として記録する）
        return AutomationInteractions::queue(
            {view, [view]() { requireInteractive(view); },
             [view, square, mouseButton]() {
                 AutomationInput::sendClick(view, squareClickPosition(view, square), mouseButton);
             }},
            QJsonObject{{QStringLiteral("target"), target}, {QStringLiteral("file"), file},
                        {QStringLiteral("rank"), rank}});
    });
}
