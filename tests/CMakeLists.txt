# ==== Test Suite for ShogiBoardQ ====
find_package(Qt6 REQUIRED COMPONENTS Test Widgets Charts Network)

# Common source root
set(SRC ${CMAKE_SOURCE_DIR}/src)

# Stubs for missing symbols (logging categories)
set(TEST_STUBS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stubs.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/kifu/formats/parsecommon.cpp
    ${SRC}/kifu/formats/sfencsapositionconverter.cpp
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
set(TEST_LIBS Qt6::Test Qt6::Widgets Qt6::Charts Qt6::Network)

# Copy fixtures to build directory
file(COPY fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# ---- Helper macro to add a test ----
macro(add_shogi_test NAME)
    add_executable(${NAME} ${ARGN})
    target_include_directories(${NAME} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${NAME} PRIVATE ${TEST_LIBS})
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    add_test(NAME ${NAME} COMMAND ${NAME})
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
    ${SRC}/core/fastmovevalidator.cpp
    ${SRC}/game/turnmanager.cpp
    ${SRC}/game/shogigamecontroller.cpp
    ${SRC}/common/jishogicalculator.cpp
)

# ============================================================
# Unit 2: ShogiBoard
# ============================================================
add_shogi_test(tst_shogiboard
    tst_shogiboard.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogimove.cpp
)

# ============================================================
# Unit 3: SfenPositionTracer + FastMoveValidator
# ============================================================
add_shogi_test(tst_sfentracer
    tst_sfentracer.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
)

add_shogi_test(tst_fastmovevalidator
    tst_fastmovevalidator.cpp
    ${TEST_STUBS}
    ${SRC}/core/fastmovevalidator.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
)

# ============================================================
# Unit 4: ShogiClock
# ============================================================
add_shogi_test(tst_shogiclock
    tst_shogiclock.cpp
    ${SRC}/core/shogiclock.cpp
)

# ============================================================
# Unit 5: KIF / KI2 Converters
# ============================================================
add_shogi_test(tst_kifconverter
    tst_kifconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_ki2converter
    tst_ki2converter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/ki2tosfenconverter.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

# ============================================================
# Unit 6: CSA / JKF / USI / USEN Converters
# ============================================================
add_shogi_test(tst_csaconverter
    tst_csaconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/csatosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_jkfconverter
    tst_jkfconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/jkftosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_usiconverter
    tst_usiconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/usitosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/kifubranchnode.cpp
)

add_shogi_test(tst_usenconverter
    tst_usenconverter.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/formats/usentosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
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
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
)

# ============================================================
# Unit 8: GameRecordModel
# ============================================================
add_shogi_test(tst_gamerecordmodel
    tst_gamerecordmodel.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/gamerecordmodel.cpp
    ${SRC}/kifu/formats/csaexporter.cpp
    ${SRC}/kifu/formats/jkfexporter.cpp
    ${SRC}/kifu/formats/ki2tosfenconverter.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
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
    ${SRC}/dialogs/josekimovedialog.cpp
    ${SRC}/dialogs/josekimergedialog.cpp
    ${SRC}/services/settingsservice.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
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
# Unit 11: Integration / Stress Tests
# ============================================================
add_shogi_test(tst_integration
    tst_integration.cpp
    ${TEST_STUBS}
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/fastmovevalidator.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/game/shogigamecontroller.cpp
    ${SRC}/game/turnmanager.cpp
    ${SRC}/kifu/kifubranchtree.cpp
    ${SRC}/kifu/kifubranchnode.cpp
    ${SRC}/kifu/livegamesession.cpp
    ${SRC}/kifu/kifunavigationstate.cpp
    ${SRC}/navigation/kifunavigationcontroller.cpp
    ${SRC}/kifu/formats/kiftosfenconverter.cpp
    ${SRC}/kifu/kifreader.cpp
    ${SRC}/board/sfenpositiontracer.cpp
)
