#ifndef POSITIONEDITCONTROLLER_H
#define POSITIONEDITCONTROLLER_H

/// @file positioneditcontroller.h
/// @brief 局面編集モードの開始・終了・盤操作を管理するコントローラ

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPointer>

class ShogiView;
class ShogiBoard;
class ShogiGameController;
class BoardInteractionController;

/**
 * @brief 局面編集モードのライフサイクルと盤面操作を制御する
 *
 * 編集開始時のSFEN決定・盤面初期化、編集終了時のSFEN再構成・モード復帰、
 * および駒配置リセット・平手初期配置・詰将棋初期配置などの操作APIを提供する。
 */
class PositionEditController final : public QObject {
    Q_OBJECT
public:
    explicit PositionEditController(QObject* parent = nullptr) : QObject(parent) {}

    // --- 編集開始コンテキスト ---

    /// 局面編集開始に必要な情報をまとめた構造体
    struct BeginEditContext {
        ShogiView*                 view = nullptr; ///< 盤面ビュー（必須・非所有）
        ShogiGameController*       gc   = nullptr; ///< ゲームコントローラ（必須・非所有）
        BoardInteractionController* bic = nullptr; ///< 操作コントローラ（任意・非所有）
        QString displayedSfen;   ///< 表示中の局面（空なら盤から作る）
    };

    // --- 編集終了コンテキスト ---

    /// 局面編集終了に必要な情報をまとめた構造体
    struct FinishEditContext {
        ShogiView*                 view = nullptr; ///< 盤面ビュー（必須・非所有）
        ShogiGameController*       gc   = nullptr; ///< ゲームコントローラ（必須・非所有）
        BoardInteractionController* bic = nullptr; ///< 操作コントローラ（任意・非所有）
    };

    // --- 編集ライフサイクル ---
    /// 表示中の局面で編集を始める。始めたら true
    bool beginPositionEditing(const BeginEditContext& c);
    /// 編集を終える。戻り値は編集した局面（新しい開始局面になる）。終えなければ空
    QString finishPositionEditing(const FinishEditContext& c);

    // --- 盤面操作API ---

    /// 全駒を駒箱に戻す
    void resetPiecesToBox(ShogiView* view, BoardInteractionController* bic);

    /// 平手初期配置にする
    void setStandardStartPosition(ShogiView* view, BoardInteractionController* bic);

    /// 詰将棋初期配置にする
    void setTsumeShogiStartPosition(ShogiView* view, BoardInteractionController* bic);

    // --- 編集終了ボタン制御 ---
    void showEditExitButtonOnBoard(ShogiView* view);
    void hideEditExitButtonOnBoard(ShogiView* view);

    /// 局面編集中の着手要求を適用し、ドラッグ終了とハイライト更新まで行う
    /// @return 成功すれば true
    bool applyEditMove(const QPoint& from,
                       const QPoint& to,
                       ShogiView* view,
                       ShogiGameController* gc,
                       BoardInteractionController* bic);

public slots:
    void onReturnAllPiecesToBoxTriggered();
    void onFlatHandInitialPositionTriggered();
    void onShogiProblemInitialPositionTriggered();
    void onToggleSideToMoveTriggered();

private:
    QPointer<ShogiView>                 m_view; ///< 編集セッション中の盤面ビュー（非所有）
    QPointer<ShogiGameController>       m_gc; ///< 編集セッション中のゲームコントローラ（非所有）
    QPointer<BoardInteractionController> m_bic; ///< 編集セッション中の操作コントローラ（非所有）
    int m_prevBoardMode = -1; ///< 編集開始前の BoardInteractionController::Mode（未保存時は-1）
};

#endif // POSITIONEDITCONTROLLER_H
