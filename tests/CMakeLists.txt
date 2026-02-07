# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# 親プロジェクトのソースディレクトリ
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# テストに必要なソースファイル
set(TEST_COMMON_SOURCES
    ${SRC_DIR}/kifu/livegamesession.cpp
    ${SRC_DIR}/kifu/kifubranchtree.cpp
    ${SRC_DIR}/kifu/kifubranchnode.cpp
    ${SRC_DIR}/core/shogimove.cpp
)

# テストで使うヘッダファイルのディレクトリ
set(TEST_INCLUDE_DIRS
    ${SRC_DIR}/kifu
    ${SRC_DIR}/core
    ${SRC_DIR}/widgets
    ${SRC_DIR}/common
)

# LiveGameSession テスト
add_executable(tst_livegamesession
    tst_livegamesession.cpp
    ${TEST_COMMON_SOURCES}
)

target_include_directories(tst_livegamesession PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(tst_livegamesession PRIVATE
    Qt6::Test
    Qt6::Widgets
)

# 自動 MOC
set_target_properties(tst_livegamesession PROPERTIES AUTOMOC ON)

add_test(NAME tst_livegamesession COMMAND tst_livegamesession)

# PreStartCleanupHandler 判定ロジックのテスト
add_executable(tst_prestartcleanuphandler
    tst_prestartcleanuphandler.cpp
    ${SRC_DIR}/kifu/kifubranchtree.cpp
    ${SRC_DIR}/kifu/kifubranchnode.cpp
    ${SRC_DIR}/core/shogimove.cpp
)

target_include_directories(tst_prestartcleanuphandler PRIVATE
    ${SRC_DIR}/game
    ${SRC_DIR}/kifu
    ${SRC_DIR}/core
)

target_link_libraries(tst_prestartcleanuphandler PRIVATE
    Qt6::Test
    Qt6::Widgets
)

set_target_properties(tst_prestartcleanuphandler PROPERTIES AUTOMOC ON)

add_test(NAME tst_prestartcleanuphandler COMMAND tst_prestartcleanuphandler)

# KifuDisplayCoordinator のライブ開始時表示テスト
add_executable(tst_kifudisplaycoordinator
    tst_kifudisplaycoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/recordpane.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/branchtreewidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/engineanalysistab.h
    ${SRC_DIR}/ui/coordinators/kifudisplaycoordinator.cpp
    ${SRC_DIR}/kifu/kifubranchtree.cpp
    ${SRC_DIR}/kifu/kifubranchnode.cpp
    ${SRC_DIR}/kifu/livegamesession.cpp
    ${SRC_DIR}/kifu/kifunavigationstate.cpp
    ${SRC_DIR}/navigation/kifunavigationcontroller.cpp
    ${SRC_DIR}/models/kifurecordlistmodel.cpp
    ${SRC_DIR}/models/kifubranchlistmodel.cpp
    ${SRC_DIR}/widgets/kifubranchdisplay.cpp
    ${SRC_DIR}/widgets/kifudisplay.cpp
    ${SRC_DIR}/core/shogimove.cpp
    ${SRC_DIR}/core/shogiutils.cpp
    ${SRC_DIR}/common/errorbus.cpp
)

target_include_directories(tst_kifudisplaycoordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
    ${SRC_DIR}
    ${SRC_DIR}/ui/coordinators
    ${SRC_DIR}/kifu
    ${SRC_DIR}/navigation
    ${SRC_DIR}/models
    ${SRC_DIR}/widgets
    ${SRC_DIR}/core
    ${SRC_DIR}/common
)

target_link_libraries(tst_kifudisplaycoordinator PRIVATE
    Qt6::Test
    Qt6::Widgets
)

set_target_properties(tst_kifudisplaycoordinator PROPERTIES AUTOMOC ON)

add_test(NAME tst_kifudisplaycoordinator COMMAND tst_kifudisplaycoordinator)
