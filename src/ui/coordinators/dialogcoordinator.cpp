/// @file dialogcoordinator.cpp
/// @brief ダイアログコーディネータクラスの実装

#include "dialogcoordinator.h"

#include <QWidget>
#include <QMessageBox>
#include "loggingcategory.h"

#include "aboutcoordinator.h"
#include "enginesettingscoordinator.h"
#include "promotionflow.h"
#include "considerationflowcontroller.h"
#include "tsumesearchflowcontroller.h"
#include "analysisflowcontroller.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "kifuanalysislistmodel.h"
#include "engineanalysistab.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "gameinfopanecontroller.h"
#include "kifuloadcoordinator.h"
#include "evaluationchartwidget.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "shogiutils.h"

namespace {
QString extractUsiMoveFromKanjiLabel(const QString& moveLabel, int fallbackFileTo, int fallbackRankTo)
{
    if (moveLabel.isEmpty()) {
        return QString();
    }

    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark  = QStringLiteral("△");
    qsizetype markPos = moveLabel.indexOf(senteMark);
    if (markPos < 0) {
        markPos = moveLabel.indexOf(goteMark);
    }
    if (markPos < 0 || moveLabel.length() <= markPos + 1) {
        return QString();
    }

    const QString afterMark = moveLabel.mid(markPos + 1);
    const bool isDrop = afterMark.contains(QStringLiteral("打"));
    const bool isPromotion = afterMark.contains(QStringLiteral("成")) && !afterMark.contains(QStringLiteral("不成"));

    int fileTo = 0;
    int rankTo = 0;
    if (afterMark.startsWith(QStringLiteral("同"))) {
        fileTo = fallbackFileTo;
        rankTo = fallbackRankTo;
    } else if (afterMark.size() >= 2) {
        fileTo = ShogiUtils::parseFullwidthFile(afterMark.at(0));
        rankTo = ShogiUtils::parseKanjiRank(afterMark.at(1));
    }
    if (fileTo < 1 || fileTo > 9 || rankTo < 1 || rankTo > 9) {
        return QString();
    }
    const QChar toRankAlpha = QChar('a' + rankTo - 1);

    if (isDrop) {
        static const QString pieceChars = QStringLiteral("歩香桂銀金角飛");
        static const QString usiPieces  = QStringLiteral("PLNSGBR");
        QChar pieceUsi;
        for (qsizetype i = 0; i < pieceChars.size(); ++i) {
            if (afterMark.contains(pieceChars.at(i))) {
                pieceUsi = usiPieces.at(i);
                break;
            }
        }
        if (pieceUsi.isNull()) {
            return QString();
        }
        return QStringLiteral("%1*%2%3").arg(pieceUsi).arg(fileTo).arg(toRankAlpha);
    }

    const qsizetype parenStart = afterMark.indexOf(QLatin1Char('('));
    const qsizetype parenEnd   = afterMark.indexOf(QLatin1Char(')'));
    if (parenStart < 0 || parenEnd <= parenStart + 1) {
        return QString();
    }
    const QString srcStr = afterMark.mid(parenStart + 1, parenEnd - parenStart - 1);
    if (srcStr.size() != 2) {
        return QString();
    }
    const int fileFrom = srcStr.at(0).digitValue();
    const int rankFrom = srcStr.at(1).digitValue();
    if (fileFrom < 1 || fileFrom > 9 || rankFrom < 1 || rankFrom > 9) {
        return QString();
    }
    const QChar fromRankAlpha = QChar('a' + rankFrom - 1);

    QString usiMove = QStringLiteral("%1%2%3%4")
        .arg(fileFrom).arg(fromRankAlpha).arg(fileTo).arg(toRankAlpha);
    if (isPromotion) {
        usiMove += QLatin1Char('+');
    }
    return usiMove;
}
}  // namespace

DialogCoordinator::DialogCoordinator(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

DialogCoordinator::~DialogCoordinator() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void DialogCoordinator::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void DialogCoordinator::setGameController(ShogiGameController* gc)
{
    m_gc = gc;
}

void DialogCoordinator::setUsiEngine(Usi* usi)
{
    m_usi = usi;
}

void DialogCoordinator::setLogModel(UsiCommLogModel* logModel)
{
    m_logModel = logModel;
}

void DialogCoordinator::setThinkingModel(ShogiEngineThinkingModel* thinkingModel)
{
    m_thinkingModel = thinkingModel;
}

void DialogCoordinator::setAnalysisModel(KifuAnalysisListModel* model)
{
    m_analysisModel = model;
}

void DialogCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

// --------------------------------------------------------
// 情報ダイアログ
// --------------------------------------------------------

void DialogCoordinator::showVersionInformation()
{
    AboutCoordinator::showVersionDialog(m_parentWidget);
}

void DialogCoordinator::openProjectWebsite()
{
    AboutCoordinator::openProjectWebsite();
}

void DialogCoordinator::showEngineSettingsDialog()
{
    EngineSettingsCoordinator::openDialog(m_parentWidget);
}

// --------------------------------------------------------
// ゲームプレイ関連ダイアログ
// --------------------------------------------------------

bool DialogCoordinator::showPromotionDialog()
{
    return PromotionFlow::askPromote(m_parentWidget);
}

void DialogCoordinator::showGameOverMessage(const QString& title, const QString& message)
{
    QMessageBox::information(m_parentWidget, title, message);
}

// --------------------------------------------------------
// 解析関連ダイアログ
// --------------------------------------------------------

void DialogCoordinator::startConsiderationDirect(const ConsiderationDirectParams& params)
{
    qCDebug(lcUi).noquote() << "startConsiderationDirect: position=" << params.position
                       << "engineIndex=" << params.engineIndex
                       << "unlimitedTime=" << params.unlimitedTime
                       << "byoyomiSec=" << params.byoyomiSec
                       << "multiPV=" << params.multiPV;

    Q_EMIT considerationModeStarted();

    // Flow に一任
    ConsiderationFlowController* flow = new ConsiderationFlowController(this);
    ConsiderationFlowController::Deps d;
    d.match = m_match;
    d.onError = [this](const QString& msg) { showFlowError(msg); };
    d.multiPV = params.multiPV;
    d.considerationModel = params.considerationModel;
    d.onTimeSettingsReady = [this](bool unlimited, int byoyomiSec) {
        Q_EMIT considerationTimeSettingsReady(unlimited, byoyomiSec);
    };
    d.onMultiPVReady = [this](int multiPV) {
        Q_EMIT considerationMultiPVReady(multiPV);
    };

    ConsiderationFlowController::DirectParams directParams;
    directParams.engineIndex = params.engineIndex;
    directParams.engineName = params.engineName;
    directParams.unlimitedTime = params.unlimitedTime;
    directParams.byoyomiSec = params.byoyomiSec;
    directParams.multiPV = params.multiPV;
    directParams.previousFileTo = params.previousFileTo;
    directParams.previousRankTo = params.previousRankTo;
    directParams.lastUsiMove = params.lastUsiMove;

    flow->runDirect(d, directParams, params.position);
}

void DialogCoordinator::showTsumeSearchDialog(const TsumeSearchParams& params)
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialog: currentMoveIndex=" << params.currentMoveIndex;

    Q_EMIT tsumeSearchModeStarted();

    // Flow に一任
    TsumeSearchFlowController* flow = new TsumeSearchFlowController(this);

    TsumeSearchFlowController::Deps d;
    d.match = m_match;
    d.sfenRecord = params.sfenRecord;
    d.startSfenStr = params.startSfenStr;
    d.positionStrList = params.positionStrList;
    d.currentMoveIndex = qMax(0, params.currentMoveIndex);
    d.usiMoves = params.usiMoves;
    d.startPositionCmd = params.startPositionCmd;
    d.onError = [this](const QString& msg) { showFlowError(msg); };

    flow->runWithDialog(d, m_parentWidget);
}

void DialogCoordinator::showKifuAnalysisDialog(const KifuAnalysisParams& params)
{
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialog: activePly=" << params.activePly;

    Q_EMIT analysisModeStarted();

    // Flow の用意（遅延生成）
    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
        // シグナル中継（Flow → DialogCoordinator → MainWindow）
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisProgressReported,
                         this, &DialogCoordinator::analysisProgressReported, Qt::UniqueConnection);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisResultRowSelected,
                         this, &DialogCoordinator::analysisResultRowSelected, Qt::UniqueConnection);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisStopped,
                         this, &DialogCoordinator::analysisModeEnded, Qt::UniqueConnection);
    }

    // 依存を詰めて Flow へ一任
    AnalysisFlowController::Deps d;
    d.sfenRecord = params.sfenRecord;
    d.moveRecords = params.moveRecords;
    d.recordModel = params.recordModel;
    d.analysisModel = m_analysisModel;
    d.analysisTab = m_analysisTab;
    d.usi = m_usi;
    d.logModel = m_logModel;
    d.thinkingModel = m_thinkingModel;
    d.gameController = params.gameController;  // 盤面情報取得用
    d.presenter = params.presenter;  // 結果表示用プレゼンター
    d.activePly = params.activePly;
    d.blackPlayerName = params.blackPlayerName;
    d.whitePlayerName = params.whitePlayerName;
    d.usiMoves = params.usiMoves;
    d.boardFlipped = params.boardFlipped;
    d.displayError = [this](const QString& msg) { showFlowError(msg); };

    m_analysisFlow->runWithDialog(d, m_parentWidget);
}

void DialogCoordinator::setKifuAnalysisContext(const KifuAnalysisContext& ctx)
{
    m_kifuAnalysisCtx = ctx;
}

void DialogCoordinator::showKifuAnalysisDialogFromContext()
{
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext";

    // 評価値グラフをクリア（対局時のグラフが残らないようにする）
    if (m_kifuAnalysisCtx.evalChart) {
        m_kifuAnalysisCtx.evalChart->clearAll();
        qCDebug(lcUi).noquote() << "evaluation chart cleared";
    }

    // パラメータを構築
    KifuAnalysisParams params;
    params.sfenRecord = m_kifuAnalysisCtx.sfenRecord;
    params.moveRecords = m_kifuAnalysisCtx.moveRecords;
    params.recordModel = m_kifuAnalysisCtx.recordModel;
    params.activePly = m_kifuAnalysisCtx.activePly ? *m_kifuAnalysisCtx.activePly : 0;
    params.gameController = m_kifuAnalysisCtx.gameController;

    // USI形式の指し手リストを取得（コンテキストから、またはKifuLoadCoordinatorから）
    // 注: コンテキストのusiMovesが空の場合はKifuLoadCoordinatorから取得
    // 重要: sfenRecordとusiMovesの整合性をチェック（sfenRecord.size() == usiMoves.size() + 1）
    const qsizetype sfenSize = params.sfenRecord ? params.sfenRecord->size() : 0;

    if (m_kifuAnalysisCtx.gameUsiMoves && !m_kifuAnalysisCtx.gameUsiMoves->isEmpty()) {
        // 対局時のUSI形式指し手リストを使用
        const qsizetype usiSize = m_kifuAnalysisCtx.gameUsiMoves->size();
        if (sfenSize == usiSize + 1) {
            params.usiMoves = m_kifuAnalysisCtx.gameUsiMoves;
            qCDebug(lcUi).noquote() << "using ctx.gameUsiMoves (from game), size=" << usiSize;
        } else {
            // 整合性がないためusiMovesを使用しない（フォールバックを使用）
            qCDebug(lcUi).noquote() << "ctx.gameUsiMoves size mismatch: sfenSize=" << sfenSize
                               << " usiSize=" << usiSize << " (expected sfenSize == usiSize + 1)";
        }
    } else if (m_kifuAnalysisCtx.kifuLoadCoordinator) {
        // 棋譜読み込み時のUSI形式指し手リストを使用
        QStringList* kifuUsiMoves = m_kifuAnalysisCtx.kifuLoadCoordinator->kifuUsiMovesPtr();
        const qsizetype usiSize = kifuUsiMoves ? kifuUsiMoves->size() : 0;
        if (sfenSize == usiSize + 1) {
            params.usiMoves = kifuUsiMoves;
            qCDebug(lcUi).noquote() << "using kifuLoadCoordinator->kifuUsiMovesPtr(), size=" << usiSize;
        } else {
            // 整合性がないためusiMovesを使用しない（フォールバックを使用）
            qCDebug(lcUi).noquote() << "kifuUsiMoves size mismatch: sfenSize=" << sfenSize
                               << " usiSize=" << usiSize << " (expected sfenSize == usiSize + 1)";
        }
    }
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext: params.usiMoves="
                       << params.usiMoves << " size=" << (params.usiMoves ? params.usiMoves->size() : -1);

    // 対局者名を取得（GameInfoPaneControllerから）
    if (m_kifuAnalysisCtx.gameInfoController) {
        const QList<KifGameInfoItem> items = m_kifuAnalysisCtx.gameInfoController->gameInfo();
        extractPlayerNames(items, params.blackPlayerName, params.whitePlayerName);
    }

    // プレゼンターを設定
    params.presenter = m_kifuAnalysisCtx.presenter;

    // GUI本体の盤面反転状態を取得
    if (m_kifuAnalysisCtx.getBoardFlipped) {
        params.boardFlipped = m_kifuAnalysisCtx.getBoardFlipped();
    }

    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext:"
                       << "blackPlayerName=" << params.blackPlayerName
                       << "whitePlayerName=" << params.whitePlayerName;

    showKifuAnalysisDialog(params);
}

void DialogCoordinator::extractPlayerNames(const QList<KifGameInfoItem>& gameInfo,
                                           QString& outBlackName, QString& outWhiteName)
{
    for (const KifGameInfoItem& item : gameInfo) {
        if (item.key == QStringLiteral("先手") || item.key == QStringLiteral("下手")) {
            outBlackName = item.value;
        } else if (item.key == QStringLiteral("後手") || item.key == QStringLiteral("上手")) {
            outWhiteName = item.value;
        }
    }
}

void DialogCoordinator::stopKifuAnalysis()
{
    qCDebug(lcUi).noquote() << "stopKifuAnalysis called";
    if (m_analysisFlow) {
        m_analysisFlow->stop();
    }
}

bool DialogCoordinator::isKifuAnalysisRunning() const
{
    return m_analysisFlow && m_analysisFlow->isRunning();
}

// --------------------------------------------------------
// 検討コンテキスト
// --------------------------------------------------------

void DialogCoordinator::setConsiderationContext(const ConsiderationContext& ctx)
{
    m_considerationCtx = ctx;
}

bool DialogCoordinator::startConsiderationFromContext()
{
    qCDebug(lcUi).noquote() << "startConsiderationFromContext";

    // エンジンが選択されているかチェック
    if (m_analysisTab && m_analysisTab->selectedEngineName().isEmpty()) {
        QMessageBox::critical(m_parentWidget, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return false;
    }

    // 手番表示用の設定
    if (m_considerationCtx.gameController && m_considerationCtx.gameMoves && m_considerationCtx.currentMoveIndex) {
        const int moveIdx = *m_considerationCtx.currentMoveIndex;
        const int movesSize = static_cast<int>(m_considerationCtx.gameMoves->size());
        if (movesSize > 0 && moveIdx >= 0 && moveIdx < movesSize) {
            if (m_considerationCtx.gameMoves->at(moveIdx).movingPiece.isUpper())
                m_considerationCtx.gameController->setCurrentPlayer(ShogiGameController::Player1);
            else
                m_considerationCtx.gameController->setCurrentPlayer(ShogiGameController::Player2);
        }
    }

    // 送信する position を決定
    const int currentMoveIdx = m_considerationCtx.currentMoveIndex ? *m_considerationCtx.currentMoveIndex : 0;
    const QString position = buildPositionStringForIndex(currentMoveIdx);

    qCDebug(lcUi).noquote() << "startConsiderationFromContext: position=" << position.left(50);

    // 検討タブ専用モデルを作成（なければ）
    if (m_considerationCtx.considerationModel) {
        if (!(*m_considerationCtx.considerationModel)) {
            *m_considerationCtx.considerationModel = new ShogiEngineThinkingModel(m_parentWidget);
        }
    }

    // 検討タブの設定を保存
    if (m_analysisTab) {
        m_analysisTab->saveConsiderationTabSettings();
    }

    // 検討タブの設定を使用して直接検討を開始
    ConsiderationDirectParams params;
    params.position = position;
    params.engineIndex = m_analysisTab ? m_analysisTab->selectedEngineIndex() : 0;
    params.engineName = m_analysisTab ? m_analysisTab->selectedEngineName() : QString();
    params.unlimitedTime = m_analysisTab ? m_analysisTab->isUnlimitedTime() : true;
    params.byoyomiSec = m_analysisTab ? m_analysisTab->byoyomiSec() : 20;
    params.multiPV = m_analysisTab ? m_analysisTab->considerationMultiPV() : 1;
    params.considerationModel = m_considerationCtx.considerationModel ? *m_considerationCtx.considerationModel : nullptr;

    // 選択中の指し手の移動先を取得（「同」表記のため）
    const int moveIdx = m_considerationCtx.currentMoveIndex ? *m_considerationCtx.currentMoveIndex : 0;
    qCDebug(lcUi).noquote() << "startConsiderationFromContext: moveIdx=" << moveIdx;

    if (m_considerationCtx.gameMoves) {
        const int movesSize = static_cast<int>(m_considerationCtx.gameMoves->size());
        if (moveIdx >= 0 && moveIdx < movesSize) {
            // 対局中の場合: m_gameMoves から直接取得
            const QPoint& toSquare = m_considerationCtx.gameMoves->at(moveIdx).toSquare;
            params.previousFileTo = toSquare.x();
            params.previousRankTo = toSquare.y();
            qCDebug(lcUi).noquote() << "previousFileTo/RankTo (from gameMoves):"
                               << params.previousFileTo << "/" << params.previousRankTo;
        }
    }

    if (params.previousFileTo == 0 && m_considerationCtx.kifuRecordModel && moveIdx > 0) {
        // 棋譜読み込み時: ShogiUtilsを使用して座標を解析（「同」の場合は自動的に遡る）
        const int modelRowCount = m_considerationCtx.kifuRecordModel->rowCount();
        if (moveIdx < modelRowCount) {
            int fileTo = 0, rankTo = 0;
            if (ShogiUtils::parseMoveCoordinateFromModel(m_considerationCtx.kifuRecordModel, moveIdx, &fileTo, &rankTo)) {
                params.previousFileTo = fileTo;
                params.previousRankTo = rankTo;
                qCDebug(lcUi).noquote() << "previousFileTo/RankTo (from recordModel):"
                                   << params.previousFileTo << "/" << params.previousRankTo;
            }
        }
    }

    // 開始局面に至った最後の指し手を取得（読み筋表示ウィンドウのハイライト用）
    // moveIdx=0 は開始局面なので直前の指し手なし
    // moveIdx>=1 の場合、その局面に至った指し手は moveIdx-1 番目の指し手
    if (moveIdx > 0) {
        // 分岐ライン上では、現在ラインのノードを最優先で参照する。
        if (m_considerationCtx.branchTree && m_considerationCtx.navState) {
            const int lineIndex = m_considerationCtx.navState->currentLineIndex();
            const QVector<BranchLine> lines = m_considerationCtx.branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                KifuBranchNode* currentNode = nullptr;
                const BranchLine& line = lines.at(lineIndex);
                for (KifuBranchNode* node : line.nodes) {
                    if (!node) continue;
                    if (node->ply() == moveIdx) {
                        currentNode = node;
                    }
                }

                if (currentNode) {
                    const ShogiMove mv = currentNode->move();
                    if (mv.movingPiece != QLatin1Char(' ')) {
                        params.lastUsiMove = ShogiUtils::moveToUsi(mv);
                        qCDebug(lcUi).noquote() << "lastUsiMove (from branch node move):" << params.lastUsiMove;
                    }
                }
            }
        }

        // move情報が不完全なケースに備え、棋譜表示文字列からUSIを抽出。
        if (params.lastUsiMove.isEmpty() && m_considerationCtx.kifuRecordModel) {
            const int rowCount = m_considerationCtx.kifuRecordModel->rowCount();
            if (moveIdx >= 0 && moveIdx < rowCount) {
                const QString moveLabel =
                    m_considerationCtx.kifuRecordModel->index(moveIdx, 0).data(Qt::DisplayRole).toString();
                params.lastUsiMove = extractUsiMoveFromKanjiLabel(
                    moveLabel, params.previousFileTo, params.previousRankTo);
                qCDebug(lcUi).noquote() << "lastUsiMove (from record label):" << params.lastUsiMove
                                   << " label=" << moveLabel;
            }
        }

        // フォールバック: gameMoves から変換
        if (params.lastUsiMove.isEmpty() && m_considerationCtx.gameMoves) {
            const int moveIdx2 = moveIdx - 1;
            if (moveIdx2 >= 0 && moveIdx2 < m_considerationCtx.gameMoves->size()) {
                params.lastUsiMove = ShogiUtils::moveToUsi(m_considerationCtx.gameMoves->at(moveIdx2));
                qCDebug(lcUi).noquote() << "lastUsiMove (from gameMoves):" << params.lastUsiMove;
            }
        }

        // 対局中: gameUsiMoves から取得
        if (params.lastUsiMove.isEmpty() && m_considerationCtx.gameUsiMoves && !m_considerationCtx.gameUsiMoves->isEmpty()) {
            const int usiIdx = moveIdx - 1;
            if (usiIdx >= 0 && usiIdx < m_considerationCtx.gameUsiMoves->size()) {
                params.lastUsiMove = m_considerationCtx.gameUsiMoves->at(usiIdx);
                qCDebug(lcUi).noquote() << "lastUsiMove (from gameUsiMoves):" << params.lastUsiMove;
            }
        }

        // 棋譜読み込み時: kifuLoadCoordinator から取得
        if (params.lastUsiMove.isEmpty() && m_considerationCtx.kifuLoadCoordinator) {
            const QStringList& kifuUsiMoves = m_considerationCtx.kifuLoadCoordinator->kifuUsiMoves();
            const int usiIdx = moveIdx - 1;
            if (usiIdx >= 0 && usiIdx < kifuUsiMoves.size()) {
                params.lastUsiMove = kifuUsiMoves.at(usiIdx);
                qCDebug(lcUi).noquote() << "lastUsiMove (from kifuLoadCoordinator):" << params.lastUsiMove;
            }
        }
    }

    qCDebug(lcUi).noquote() << "startConsiderationFromContext calling startConsiderationDirect"
                      << "engineIndex=" << params.engineIndex
                      << "engineName=" << params.engineName
                      << "unlimitedTime=" << params.unlimitedTime
                      << "byoyomiSec=" << params.byoyomiSec
                      << "multiPV=" << params.multiPV;
    startConsiderationDirect(params);
    qCDebug(lcUi).noquote() << "startConsiderationFromContext EXIT";
    return true;
}

QString DialogCoordinator::buildPositionStringForIndex(int moveIndex) const
{
    // 平手初期局面のSFEN
    static const QString kHirateSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    const QString startSfen = m_considerationCtx.startSfenStr ? *m_considerationCtx.startSfenStr : QString();
    const QStringList* sfenRecord = m_considerationCtx.sfenRecord;
    const QString currentSfen = m_considerationCtx.currentSfenStr ? m_considerationCtx.currentSfenStr->trimmed() : QString();

    // 分岐を含む現在表示局面を最優先で使用する。
    if (!currentSfen.isEmpty()) {
        if (currentSfen.startsWith(QStringLiteral("position "))) {
            return currentSfen;
        }
        if (currentSfen.startsWith(QStringLiteral("sfen "))) {
            return QStringLiteral("position ") + currentSfen;
        }
        return QStringLiteral("position sfen ") + currentSfen;
    }

    if (!sfenRecord || sfenRecord->isEmpty()) {
        // sfenRecordがない場合は startpos を返す
        if (startSfen.isEmpty() || startSfen == kHirateSfen) {
            return QStringLiteral("position startpos");
        } else {
            return QStringLiteral("position sfen ") + startSfen;
        }
    }

    // 指定インデックスの局面が存在するか確認
    const int idx = qBound(0, moveIndex, static_cast<int>(sfenRecord->size()) - 1);
    const QString indexedSfen = sfenRecord->at(idx);

    // position sfen 形式で返す（指定局面を直接SFENで送信）
    return QStringLiteral("position sfen ") + indexedSfen;
}

// --------------------------------------------------------
// 詰み探索コンテキスト
// --------------------------------------------------------

void DialogCoordinator::setTsumeSearchContext(const TsumeSearchContext& ctx)
{
    m_tsumeSearchCtx = ctx;
}

void DialogCoordinator::showTsumeSearchDialogFromContext()
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext";

    TsumeSearchParams params;
    params.sfenRecord = m_tsumeSearchCtx.sfenRecord;
    params.startSfenStr = m_tsumeSearchCtx.startSfenStr ? *m_tsumeSearchCtx.startSfenStr : QString();
    params.positionStrList = m_tsumeSearchCtx.positionStrList ? *m_tsumeSearchCtx.positionStrList : QStringList();
    params.currentMoveIndex = m_tsumeSearchCtx.currentMoveIndex ? qMax(0, *m_tsumeSearchCtx.currentMoveIndex) : 0;

    // USI形式の指し手リストを取得（棋譜解析と同様のロジック）
    const qsizetype sfenSize = params.sfenRecord ? params.sfenRecord->size() : 0;
    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext: sfenSize=" << sfenSize
                       << "gameUsiMoves.size=" << (m_tsumeSearchCtx.gameUsiMoves ? m_tsumeSearchCtx.gameUsiMoves->size() : -1)
                       << "kifuLoadCoordinator=" << (m_tsumeSearchCtx.kifuLoadCoordinator ? "exists" : "null");

    if (m_tsumeSearchCtx.gameUsiMoves && !m_tsumeSearchCtx.gameUsiMoves->isEmpty()) {
        const qsizetype usiSize = m_tsumeSearchCtx.gameUsiMoves->size();
        qCDebug(lcUi).noquote() << "checking gameUsiMoves: sfenSize=" << sfenSize << " usiSize=" << usiSize;
        if (sfenSize == usiSize + 1) {
            params.usiMoves = m_tsumeSearchCtx.gameUsiMoves;
            qCDebug(lcUi).noquote() << "using gameUsiMoves, size=" << usiSize;
        } else {
            qCDebug(lcUi).noquote() << "gameUsiMoves size mismatch";
        }
    } else if (m_tsumeSearchCtx.kifuLoadCoordinator) {
        QStringList* kifuUsiMoves = m_tsumeSearchCtx.kifuLoadCoordinator->kifuUsiMovesPtr();
        const qsizetype usiSize = kifuUsiMoves ? kifuUsiMoves->size() : 0;
        qCDebug(lcUi).noquote() << "checking kifuUsiMoves: sfenSize=" << sfenSize << " usiSize=" << usiSize;
        if (kifuUsiMoves) {
            qCDebug(lcUi).noquote() << "kifuUsiMoves content:" << *kifuUsiMoves;
        }
        if (sfenSize == usiSize + 1) {
            params.usiMoves = kifuUsiMoves;
            qCDebug(lcUi).noquote() << "using kifuLoadCoordinator->kifuUsiMovesPtr(), size=" << usiSize;
        } else {
            qCDebug(lcUi).noquote() << "kifuUsiMoves size mismatch (expected sfenSize == usiSize + 1)";
        }
    } else {
        qCDebug(lcUi).noquote() << "no usiMoves source available";
    }

    // 開始局面コマンドを決定（平手初期局面の場合は "startpos"、それ以外は "sfen ..."）
    static const QString kHirateSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    if (params.startSfenStr.isEmpty() || params.startSfenStr == kHirateSfen) {
        params.startPositionCmd = QStringLiteral("startpos");
    } else {
        params.startPositionCmd = QStringLiteral("sfen ") + params.startSfenStr;
    }

    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext:"
                       << "usiMoves=" << (params.usiMoves ? QString::number(params.usiMoves->size()) : "null")
                       << "startSfenStr=" << params.startSfenStr.left(50)
                       << "startPositionCmd=" << params.startPositionCmd
                       << "currentMoveIndex=" << params.currentMoveIndex;

    showTsumeSearchDialog(params);
}

// --------------------------------------------------------
// エラー表示
// --------------------------------------------------------

void DialogCoordinator::showErrorMessage(const QString& message)
{
    QMessageBox::critical(m_parentWidget, tr("Error"), message);
}

void DialogCoordinator::showFlowError(const QString& message)
{
    qCWarning(lcUi).noquote() << "Flow error:" << message;
    QMessageBox::warning(m_parentWidget, tr("エラー"), message);
}
