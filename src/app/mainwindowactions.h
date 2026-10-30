#ifndef MAINWINDOWACTIONS_H
#define MAINWINDOWACTIONS_H

/// @file mainwindowactions.h
/// @brief 複数の部品にまたがる利用者の操作（局面編集の開始・新規・終了）

#include <QObject>

class AnalysisSession;
class EngineSlots;
class GameSession;
class KifuFileController;
class PositionEditSession;
class QWidget;
class SessionResetCoordinator;

/**
 * @brief 1つの部品では完結しない利用者の操作をまとめる
 *
 * 部品は MainWindowComposer が起動時に全部作ってから渡す（すべて非所有）。
 */
class MainWindowActions : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* window = nullptr;                          ///< 閉じる対象のウィンドウ
        GameSession* gameSession = nullptr;
        PositionEditSession* positionEdit = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認
        SessionResetCoordinator* reset = nullptr;          ///< 編集した局面・平手の開始局面だけの棋譜にする
        AnalysisSession* analysis = nullptr;               ///< 「新規」で検討・詰み探索を止める
        EngineSlots* engines = nullptr;                    ///< 「新規」で対局・棋譜解析のエンジンを止める（省略可）
    };

    explicit MainWindowActions(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 局面編集を始める（棋譜は開始局面だけになるので、未保存の棋譜を先に確認する）
    void beginPositionEditing();
    /// 棋譜を局面編集の局面（開始・終了の時点）だけにする（QObject でない SessionResetCoordinator へのシグナルの受け口）
    void resetRecordForEditedPosition(const QString& sfen);
    /// 「新規」。未保存の棋譜を確認してから初期状態に戻す（startNewRecord）
    void resetToInitialState();
    /// 「終了」。ウィンドウを閉じられたらアプリを終了する
    void requestQuit();

public:
    /**
     * @brief 「新規」の本体（未保存の確認は呼び出し側）
     *
     * 検討・詰み探索を止め、対局の後始末（GameSession::abandonGame。通信対局・終局の状態・連続対局・中断した対局）を
     * してから、対局・棋譜解析のエンジンを止め、平手の開始局面だけの棋譜にする
     */
    void startNewRecord();

private:
    const Deps m_deps;
};

#endif // MAINWINDOWACTIONS_H
