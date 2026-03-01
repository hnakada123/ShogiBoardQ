# ==== Test Suite for ShogiBoardQ ====
find_package(Qt6 REQUIRED COMPONENTS Test Widgets Charts Network)

# Common source root
set(SRC ${CMAKE_SOURCE_DIR}/src)

# Stubs for missing symbols (logging categories)
set(TEST_STUBS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stubs.cpp
    ${SRC}/common/dialogutils.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/fontsizehelper.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/kifu/formats/parsecommon.cpp
    ${SRC}/kifu/formats/parsemoveformat.cpp
    ${SRC}/kifu/formats/sfencsapositionconverter.cpp
    ${SRC}/kifu/formats/notationutils.cpp
    ${SRC}/board/sfenpositiontracer.cpp
)

# Common include directories for all tests
set(TEST_INCLUDES
    ${SRC}
    ${SRC}/core
    ${SRC}/game
    ${SRC}/kifu
    ${SRC}/kifu/formats
    ${SRC}/board
    ${SRC}/navigation
    ${SRC}/common
    ${SRC}/models
    ${SRC}/services
    ${SRC}/ui
    ${SRC}/ui/controllers
    ${SRC}/ui/coordinators
    ${SRC}/ui/presenters
    ${SRC}/ui/wiring
    ${SRC}/views
    ${SRC}/widgets
    ${SRC}/dialogs
    ${SRC}/analysis
    ${SRC}/engine
    ${SRC}/network
    ${SRC}/app
)

# Common link libraries
set(TEST_LIBS Qt6::Test Qt6::Widgets Qt6::Charts Qt6::Network Qt6::Concurrent)

# Settings sources (全ドメイン設定ファイル)
set(SETTINGS_SOURCES
    ${SRC}/services/settingscommon.cpp
    ${SRC}/services/settingsservice.cpp
    ${SRC}/services/analysissettings.cpp
    ${SRC}/services/appsettings.cpp
    ${SRC}/services/docksettings.cpp
    ${SRC}/services/enginedialogsettings.cpp
    ${SRC}/services/gamesettings.cpp
    ${SRC}/services/josekisettings.cpp
    ${SRC}/services/networksettings.cpp
    ${SRC}/services/tsumeshogisettings.cpp
)

# Copy fixtures to build directory
file(COPY fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# 共通ソース: EngineMoveValidator + 依存ファイル
set(EMV_SOURCES
    ${SRC}/core/enginemovevalidator.cpp
    ${SRC}/core/fmvattacks.cpp
    ${SRC}/core/fmvbitboard81.cpp
    ${SRC}/core/fmvconverter.cpp
    ${SRC}/core/fmvlegalcore.cpp
    ${SRC}/core/fmvmovegeneration.cpp
    ${SRC}/core/fmvposition.cpp
)

# ---- Helper macro to add a test ----
macro(add_shogi_test NAME)
    add_executable(${NAME} ${ARGN})
    target_include_directories(${NAME} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${NAME} PRIVATE ${TEST_LIBS})
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    add_test(NAME ${NAME} COMMAND ${NAME})
    # ヘッドレスCI環境でもWidgetsテストを実行できるようにする
    set_tests_properties(${NAME} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endmacro()

# ============================================================
# Unit 1: Core Data Structures
# ============================================================
add_shogi_test(tst_coredatastructures
    tst_coredatastructures.cpp
    ${TEST_STUBS}
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${EMV_SOURCES}
    ${SRC}/game/turnmanager.cpp
    ${SRC}/game/shogigamecontroller.cpp
    ${SRC}/game/piecemoverules.cpp
    ${SRC}/common/jishogicalculator.cpp
)

# ============================================================
# Unit 1b: ErrorBus (severity levels)
# ============================================================
add_shogi_test(tst_errorbus
    tst_errorbus.cpp
    ${SRC}/common/errorbus.cpp
)

# ============================================================
# Unit 2: ShogiBoard
# ============================================================
add_shogi_test(tst_shogiboard
    tst_shogiboard.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 2b: ShogiMove
# ============================================================
add_shogi_test(tst_shogimove
    tst_shogimove.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
)

# ============================================================
# Unit 2c: ShogiUtils
# ============================================================
add_shogi_test(tst_shogiutils
    tst_shogiutils.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
)

# ============================================================
# Unit 2d: MoveValidator (comprehensive piece movement tests)
# ============================================================
add_shogi_test(tst_movevalidator
    tst_movevalidator.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 3: SfenPositionTracer
# ============================================================
add_shogi_test(tst_sfentracer
    tst_sfentracer.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 4: ShogiClock
# ============================================================
add_shogi_test(tst_shogiclock
    tst_shogiclock.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/shogiclock.cpp
)

# ============================================================
# Unit 5: KIF / KI2 Converters
# ============================================================
add_shogi_test(tst_kifconverter
    tst_kifconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchtreebuilder.cpp
    ${SRC}/common/errorbus.cpp
)

add_shogi_test(tst_ki2converter
    tst_ki2converter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/ki2tosfenconverter.cpp
    ${SRC}/kifu/formats/ki2lexer.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

# ============================================================
# Unit 6: CSA / JKF / USI / USEN Converters
# ============================================================
add_shogi_test(tst_csaconverter
    tst_csaconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/csalexer.cpp
    ${SRC}/kifu/formats/csatosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_jkfconverter
    tst_jkfconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/jkfmoveparser.cpp
    ${SRC}/kifu/formats/jkftosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_usiconverter
    tst_usiconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/usitosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_usenconverter
    tst_usenconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/usentosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

# ============================================================
# Unit 7: KifuBranchTree + LiveGameSession + Navigation
# ============================================================
add_shogi_test(tst_kifubranchtree
    tst_kifubranchtree.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/core/shogimove.cpp
)

add_shogi_test(tst_livegamesession
    tst_livegamesession.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/core/shogimove.cpp
)

add_shogi_test(tst_navigation
    tst_navigation.cpp
    ${TEST_STUBS}
    ${SRC}/navigation/kifunavigationcontroller.cpp
    ${SRC}/navigation/kifunavigationcontroller_branch.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
)

# ============================================================
# Unit 8: GameRecordModel
# ============================================================
add_shogi_test(tst_gamerecordmodel
    tst_gamerecordmodel.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/gamerecordmodel.cpp
    ${SRC}/kifu/formats/csaexporter.cpp
    ${SRC}/kifu/formats/csaformatter.cpp
    ${SRC}/kifu/formats/jkfexporter.cpp
    ${SRC}/kifu/formats/jkfformatter.cpp
    ${SRC}/kifu/formats/kifexporter.cpp
    ${SRC}/kifu/formats/ki2exporter.cpp
    ${SRC}/kifu/formats/usenexporter.cpp
    ${SRC}/kifu/formats/usiexporter.cpp
    ${SRC}/kifu/formats/ki2tosfenconverter.cpp
    ${SRC}/kifu/formats/ki2lexer.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/kifu/kifucontentbuilder.cpp
)

# ============================================================
# Unit 8b: Kifu Comment Sync (GameRecordModel ↔ liveDisp / BranchTree)
# ============================================================
add_shogi_test(tst_kifu_comment_sync
    tst_kifu_comment_sync.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/gamerecordmodel.cpp
    ${SRC}/kifu/formats/csaexporter.cpp
    ${SRC}/kifu/formats/csaformatter.cpp
    ${SRC}/kifu/formats/jkfexporter.cpp
    ${SRC}/kifu/formats/jkfformatter.cpp
    ${SRC}/kifu/formats/kifexporter.cpp
    ${SRC}/kifu/formats/ki2exporter.cpp
    ${SRC}/kifu/formats/usenexporter.cpp
    ${SRC}/kifu/formats/usiexporter.cpp
    ${SRC}/kifu/formats/ki2tosfenconverter.cpp
    ${SRC}/kifu/formats/ki2lexer.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/kifu/kifucontentbuilder.cpp
)

# ============================================================
# Unit 9: JosekiWindow Settings Persistence
# ============================================================
add_shogi_test(tst_josekiwindow
    tst_josekiwindow.cpp
    ${TEST_STUBS}
    ${SRC}/dialogs/josekiwindow.cpp
    ${SRC}/dialogs/josekiwindowio.cpp
    ${SRC}/dialogs/josekiwindowui.cpp
    ${SRC}/dialogs/josekipresenter.cpp
    ${SRC}/dialogs/josekirepository.cpp
    ${SRC}/dialogs/josekimovedialog.cpp
    ${SRC}/dialogs/josekimoveinputwidget.cpp
    ${SRC}/dialogs/josekimergedialog.cpp
    ${SETTINGS_SOURCES}
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

# ============================================================
# Unit 10: Position Edit → Game Start Flow
# ============================================================
add_shogi_test(tst_positionedit_gamestart
    tst_positionedit_gamestart.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 10b: Preset Game Start → Cleanup Flow
# ============================================================
add_shogi_test(tst_preset_gamestart_cleanup
    tst_preset_gamestart_cleanup.cpp
    test_stubs_cleanup.cpp
    ${TEST_STUBS}
    ${SRC}/game/prestartcleanuphandler.cpp
    ${SRC}/engine/usicommlogmodel.cpp
    ${SRC}/models/kifurecordlistmodel.cpp
    ${SRC}/models/kifubranchlistmodel.cpp
    ${SRC}/widgets/kifudisplay.cpp
    ${SRC}/widgets/kifubranchdisplay.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 11: Integration / Stress Tests
# ============================================================
add_shogi_test(tst_integration
    tst_integration.cpp
    ${TEST_STUBS}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/game/shogigamecontroller.cpp
    ${SRC}/game/piecemoverules.cpp
    ${SRC}/game/turnmanager.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/navigation/kifunavigationcontroller.cpp
    ${SRC}/navigation/kifunavigationcontroller_branch.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/formats/kiflexer.cpp
    ${SRC}/kifu/kifreader.cpp
)

# ============================================================
# Unit 13: USI Protocol Handler
# ============================================================
add_shogi_test(tst_usiprotocolhandler
    tst_usiprotocolhandler.cpp
    test_stubs_usiprotocol.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/engine/usiprotocolhandler.cpp
    ${SRC}/engine/usiprotocolhandler_wait.cpp
    ${SRC}/engine/usimovecoordinateconverter.cpp
)

# ============================================================
# Unit 12: UI Display Consistency (Board / Record / Branch Tree)
# ============================================================
add_shogi_test(tst_ui_display_consistency
    tst_ui_display_consistency.cpp
    test_stubs_ui_display.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/ui/coordinators/kifudisplaycoordinator.cpp
    ${SRC}/ui/coordinators/kifudisplaycoordinator_live.cpp
    ${SRC}/ui/coordinators/kifuselectionsync.cpp
    ${SRC}/ui/presenters/kifudisplaypresenter.cpp
    ${SRC}/navigation/kifunavigationcontroller.cpp
    ${SRC}/navigation/kifunavigationcontroller_branch.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/models/kifurecordlistmodel.cpp
    ${SRC}/models/kifubranchlistmodel.cpp
    ${SRC}/widgets/kifudisplay.cpp
    ${SRC}/widgets/kifubranchdisplay.cpp
    ${SRC}/widgets/recordpane.cpp
    ${SRC}/widgets/recordpaneappearancemanager.cpp
    ${SRC}/widgets/branchtreemanager.cpp
    ${SRC}/widgets/branchtreemanager_draw.cpp
    ${SRC}/widgets/branchtreewidget.cpp
    ${SETTINGS_SOURCES}
    ${SRC}/board/sfenpositiontracer.cpp
)

# ============================================================
# Unit 14: AnalysisFlowController
# ============================================================
add_shogi_test(tst_analysisflow
    tst_analysisflow.cpp
    test_stubs_analysisflow.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/common/fontsizehelper.cpp
    ${SRC}/analysis/analysisflowcontroller.cpp
    ${SRC}/analysis/analysisflowcontroller_position.cpp
    ${SRC}/analysis/analysisresulthandler.cpp
    ${SRC}/analysis/analysiscoordinator.cpp
)

# ============================================================
# Unit 15: Game Start Flow (GameStartCoordinator)
# ============================================================
add_shogi_test(tst_game_start_flow
    tst_game_start_flow.cpp
    test_stubs_game_start_flow.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/game/gamestartcoordinator.cpp
    ${SRC}/game/gamestartoptionsbuilder.cpp
    ${SRC}/services/playernameservice.cpp
)

# ============================================================
# Unit 18: Game End Handler (GameEndHandler)
# ============================================================
add_shogi_test(tst_game_end_handler
    tst_game_end_handler.cpp
    test_stubs_game_end_handler.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/game/gameendhandler.cpp
)

# ============================================================
# Unit 19: Game Start Orchestrator (GameStartOrchestrator)
# ============================================================
add_shogi_test(tst_game_start_orchestrator
    tst_game_start_orchestrator.cpp
    test_stubs_game_start_orchestrator.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/game/gamestartorchestrator.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# EngineMoveValidator テストスイート
# ============================================================

# Unit: Bitboard81
add_shogi_test(tst_fmvbitboard81
    tst_fmvbitboard81.cpp
    ${SRC}/core/fmvbitboard81.cpp
)

# Unit: Converter
add_shogi_test(tst_fmvconverter
    tst_fmvconverter.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/fmvbitboard81.cpp
    ${SRC}/core/fmvconverter.cpp
    ${SRC}/core/fmvposition.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/enginemovevalidator.cpp
    ${SRC}/core/fmvattacks.cpp
    ${SRC}/core/fmvlegalcore.cpp
    ${SRC}/core/fmvmovegeneration.cpp
)

# Unit: Position (do/undo)
add_shogi_test(tst_fmvposition
    tst_fmvposition.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/fmvbitboard81.cpp
    ${SRC}/core/fmvconverter.cpp
    ${SRC}/core/fmvposition.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/enginemovevalidator.cpp
    ${SRC}/core/fmvattacks.cpp
    ${SRC}/core/fmvlegalcore.cpp
    ${SRC}/core/fmvmovegeneration.cpp
)

# Unit: LegalCore
add_shogi_test(tst_fmvlegalcore
    tst_fmvlegalcore.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# Unit: EngineMoveValidator compat (互換ラッパ)
add_shogi_test(tst_enginemovevalidator_compat
    tst_enginemovevalidator_compat.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# Unit: EngineMoveValidator context (Context API)
add_shogi_test(tst_enginemovevalidator_context
    tst_enginemovevalidator_context.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# Perft tests
add_shogi_test(tst_fmv_perft
    tst_fmv_perft.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# Crosscheck: EngineMoveValidator (既知値との比較)
add_shogi_test(tst_enginemovevalidator_crosscheck
    tst_enginemovevalidator_crosscheck.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit: ParseCommon (棋譜共通ユーティリティ)
# ============================================================
add_shogi_test(tst_parsecommon
    tst_parsecommon.cpp
    ${TEST_STUBS}
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 16: Layer Dependency Verification (レイヤ間依存方向の検証)
# ============================================================
add_shogi_test(tst_layer_dependencies
    tst_layer_dependencies.cpp
)
target_compile_definitions(tst_layer_dependencies PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 17: Structural KPI (コード品質指標の自動検証)
# ============================================================
add_shogi_test(tst_structural_kpi
    tst_structural_kpi.cpp
)
target_compile_definitions(tst_structural_kpi PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 20: CSA Protocol (CsaClient / CsaMoveConverter / SfenCsaPositionConverter)
# ============================================================
add_shogi_test(tst_csaprotocol
    tst_csaprotocol.cpp
    ${TEST_STUBS}
    ${SRC}/network/csaclient.cpp
    ${SRC}/network/csaclient_parser.cpp
    ${SRC}/network/csamoveconverter.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/game/shogigamecontroller.cpp
    ${SRC}/game/piecemoverules.cpp
    ${SRC}/game/turnmanager.cpp
    ${SRC}/common/jishogicalculator.cpp
    ${EMV_SOURCES}
)

# ============================================================
# Unit 21: Settings Roundtrip (全ドメイン設定の保存→復元)
# ============================================================
add_shogi_test(tst_settings_roundtrip
    tst_settings_roundtrip.cpp
    test_stubs_settings.cpp
    ${SRC}/common/logcategories.cpp
    ${SETTINGS_SOURCES}
)

# ============================================================
# Unit 22: App Lifecycle Pipeline (起動/終了フロー構造契約)
# ============================================================
add_shogi_test(tst_app_lifecycle_pipeline
    tst_app_lifecycle_pipeline.cpp
)
target_compile_definitions(tst_app_lifecycle_pipeline PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 23: App Game Session (対局開始フロー構造契約)
# ============================================================
add_shogi_test(tst_app_game_session
    tst_app_game_session.cpp
)
target_compile_definitions(tst_app_game_session PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 24: App Kifu Load (棋譜ロードフロー構造契約)
# ============================================================
add_shogi_test(tst_app_kifu_load
    tst_app_kifu_load.cpp
)
target_compile_definitions(tst_app_kifu_load PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 25: App UI State Policy (UiStatePolicyManager 状態遷移テスト)
# ============================================================
add_shogi_test(tst_app_ui_state_policy
    tst_app_ui_state_policy.cpp
    test_stubs_ui_state_policy.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/ui/coordinators/uistatepolicymanager.cpp
    ${SRC}/widgets/recordpaneappearancemanager.cpp
    ${SRC}/services/gamesettings.cpp
    ${SRC}/services/settingscommon.cpp
    ${SRC}/app/mainwindow.ui
)
set_target_properties(tst_app_ui_state_policy PROPERTIES
    AUTOUIC ON
    AUTOUIC_SEARCH_PATHS "${SRC}/app;${SRC}/dialogs"
)

# ============================================================
# Unit 26: App Branch Navigation (BranchNavigationWiring 構造契約テスト)
# ============================================================
add_shogi_test(tst_app_branch_navigation
    tst_app_branch_navigation.cpp
)
target_compile_definitions(tst_app_branch_navigation PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 27: Wiring Contracts (MatchCoordinatorWiring シグナル転送契約テスト)
# ============================================================
add_shogi_test(tst_wiring_contracts
    tst_wiring_contracts.cpp
    test_stubs_wiring_contracts.cpp
    ${SRC}/ui/wiring/matchcoordinatorwiring.cpp
    ${SRC}/common/logcategories.cpp
)

# ============================================================
# Unit 28: MatchCoordinator (対局進行コーディネータ主要フロー)
# ============================================================
add_shogi_test(tst_matchcoordinator
    tst_matchcoordinator.cpp
    test_stubs_matchcoordinator.cpp
    ${SRC}/game/matchcoordinator.cpp
    ${SRC}/game/matchcoordinator_engine.cpp
    ${SRC}/game/matchcoordinator_time.cpp
    ${SRC}/common/logcategories.cpp
)

# ============================================================
# Unit 29: GameStrategy (対局モード別Strategy群テスト)
# ============================================================
add_shogi_test(tst_gamestrategy
    tst_gamestrategy.cpp
    test_stubs_gamestrategy.cpp
    ${SRC}/game/matchcoordinator.cpp
    ${SRC}/game/matchcoordinator_engine.cpp
    ${SRC}/game/matchcoordinator_time.cpp
    ${SRC}/game/humanvshumanstrategy.cpp
    ${SRC}/game/humanvsenginestrategy.cpp
    ${SRC}/game/enginevsenginestrategy.cpp
    ${SRC}/common/logcategories.cpp
)

# ============================================================
# Unit 30: App Error Handling (異常系・防御パターン構造契約テスト)
# ============================================================
add_shogi_test(tst_app_error_handling
    tst_app_error_handling.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)
target_compile_definitions(tst_app_error_handling PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 31: CsaGameWiring 配線契約テスト（ソース解析型）
# ============================================================
add_shogi_test(tst_wiring_csagame
    tst_wiring_csagame.cpp
)
target_compile_definitions(tst_wiring_csagame PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 32: AnalysisTabWiring 配線契約テスト（ソース解析型）
# ============================================================
add_shogi_test(tst_wiring_analysistab
    tst_wiring_analysistab.cpp
)
target_compile_definitions(tst_wiring_analysistab PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 33: ConsiderationWiring 配線契約テスト（ソース解析型）
# ============================================================
add_shogi_test(tst_wiring_consideration
    tst_wiring_consideration.cpp
)
target_compile_definitions(tst_wiring_consideration PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 34: PlayerInfoWiring 配線契約テスト（ソース解析型）
# ============================================================
add_shogi_test(tst_wiring_playerinfo
    tst_wiring_playerinfo.cpp
)
target_compile_definitions(tst_wiring_playerinfo PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 35: Lifecycle Scenario (全フェーズ構造的シナリオテスト)
# ============================================================
add_shogi_test(tst_lifecycle_scenario
    tst_lifecycle_scenario.cpp
)
target_compile_definitions(tst_lifecycle_scenario PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============================================================
# Unit 36: Wiring Slot Coverage (接続漏れ/重複検出テスト、ソース解析型)
# ============================================================
add_shogi_test(tst_wiring_slot_coverage
    tst_wiring_slot_coverage.cpp
)
target_compile_definitions(tst_wiring_slot_coverage PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Benchmark: EngineMoveValidator compat vs Context (not a ctest target)
add_executable(bench_movevalidator
    bench_movevalidator.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/logcategories.cpp
    ${EMV_SOURCES}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
)
target_include_directories(bench_movevalidator PRIVATE ${TEST_INCLUDES})
target_link_libraries(bench_movevalidator PRIVATE ${TEST_LIBS})
target_compile_options(bench_movevalidator PRIVATE -O2)
