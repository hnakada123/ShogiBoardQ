#ifndef PVCLICKCONTROLLER_H
#define PVCLICKCONTROLLER_H

/// @file pvclickcontroller.h
/// @brief 読み筋の行をクリックしたとき、読み筋を盤で再生するダイアログを開く

#include <QHash>
#include <QObject>
#include <QString>

#include "shogienginethinkingmodel.h"

class KifuDocument;
class KifuNavigationState;
class PvBoardDialog;
class PvText;
class ShogiView;

/**
 * @brief 読み筋を盤で再生するダイアログ（読み筋盤）を開く
 *
 * 思考タブ・検討タブの読み筋の行と、棋譜解析の結果の行（KifuAnalysisController）から開く。
 * 開き方（対局者名は対局情報、盤の向きは本体の盤、局面に至った手の強調）は showPvBoard の1か所で決める。
 * 思考の表の読み筋の局面は、エンジンが読み筋を出したときの局面（ShogiInfoRecord::baseSfen）を使う。
 */
class PvClickController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const KifuNavigationState* navState = nullptr;      ///< 表示中の手順（読み筋の局面に至った手を探す）
        const KifuDocument* document = nullptr;             ///< 対局者名（対局情報。対局を始めると対局者の名前が入る）
        const ShogiView* shogiView = nullptr;               ///< 盤の反転状態
    };

    explicit PvClickController(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief 読み筋 pv を盤で再生するダイアログを開く（閉じると破棄する）
     * @param lastUsiMove 読み筋の局面に至った手（強調する。無ければ空）
     * @param prevSfen その手を指す前の局面（強調の移動元を決める。無ければ空）
     */
    PvBoardDialog* showPvBoard(const PvText& pv, const QString& lastUsiMove, const QString& prevSfen = QString());

signals:
    /// model の行から開いた読み筋ダイアログが閉じた（表の選択を外す）
    void pvDialogClosed(const ShogiEngineThinkingModel* model);

public slots:
    /// 思考の表（model）の row 行目の「盤面」ボタンが押された
    void onPvRowClicked(ShogiEngineThinkingModel* model, int row);

private slots:
    void onPvDialogFinished();

private:
    Deps m_deps;
    QHash<const QObject*, const ShogiEngineThinkingModel*> m_dialogSources;  ///< 開いているダイアログと元の表
};

#endif // PVCLICKCONTROLLER_H
