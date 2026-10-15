#ifndef PVCLICKCONTROLLER_H
#define PVCLICKCONTROLLER_H

/// @file pvclickcontroller.h
/// @brief 読み筋クリックコントローラクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>


#include "matchplayers.h"
#include "recordline.h"

class KifuNavigationState;
struct AppState;
class ShogiEngineThinkingModel;
class ShogiView;

/**
 * @brief PvClickController - 読み筋クリック処理クラス
 *
 * MainWindowから読み筋（PV）クリック時の処理を分離したクラス。
 * 以下の責務を担当:
 * - 読み筋行クリック時のデータ取得
 * - USI形式読み筋の解析
 * - PvBoardDialogの表示
 */
class PvClickController : public QObject
{
    Q_OBJECT

public:
    /// クリックしたときに読む状態（onPvRowClicked で写し取る）
    struct StateRefs {
        const AppState* state = nullptr;                        ///< 対局者・表示中の局面・開始局面
        const KifuNavigationState* navState = nullptr;          ///< 表示中の手順（分岐ツリー）
    };

    explicit PvClickController(QObject* parent = nullptr);
    ~PvClickController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setThinkingModels(ShogiEngineThinkingModel* model1, ShogiEngineThinkingModel* model2);
    void setConsiderationModel(ShogiEngineThinkingModel* model);
    void setStateRefs(const StateRefs& refs);
    void setShogiView(ShogiView* view);

signals:
    void pvDialogClosed(int engineIndex);

public slots:
    /**
     * @brief 読み筋行がクリックされた時の処理
     * @param engineIndex エンジンインデックス（0 or 1）
     * @param row クリックされた行
     */
    void onPvRowClicked(int engineIndex, int row);

private:
    void onPvDialogFinished(int result);

    /**
     * @brief 現在の局面SFENを取得（フォールバック付き）
     */
    QString resolveCurrentSfen(const QString& baseSfen) const;

    /**
     * @brief 対局者名（最後に始めた対局の対局者。名前が無ければ先手・後手）を取得
     */
    void resolvePlayerNames(QString& blackName, QString& whiteName) const;

    /// engineIndex(0/1/2) に対応する思考モデルを返す
    ShogiEngineThinkingModel* modelForEngineIndex(int engineIndex) const;

    /// PVダイアログのハイライト情報（前局面/最終USI手）を解決する
    void resolveHighlightContext(const QString& currentSfen,
                                 QString& lastUsiMove,
                                 QString& prevSfenForHighlight) const;

private:
    void syncFromRefs();

    ShogiEngineThinkingModel* m_modelThinking1 = nullptr;
    ShogiEngineThinkingModel* m_modelThinking2 = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    RecordLine m_line;                                   ///< クリック時点の表示中の手順
    ShogiView* m_shogiView = nullptr;                    ///< 盤面反転状態取得用

    StateRefs m_stateRefs;                               ///< 動的状態へのポインタ群

    MatchPlayers m_players;
    QString m_currentSfenStr;
    QString m_startSfenStr;
    bool m_boardFlipped = false;     ///< GUI本体の盤面反転状態
};

#endif // PVCLICKCONTROLLER_H
