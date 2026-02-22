cmake_minimum_required(VERSION 3.16)

project(ShogiBoardQ VERSION 0.1 LANGUAGES CXX)

# ==== Qt auto features ====
set(CMAKE_AUTOUIC ON)
# .ui ファイルはソースと同じディレクトリに配置
# 他のディレクトリから ui_*.h をインクルードする場合のための検索パス
set(CMAKE_AUTOUIC_SEARCH_PATHS src/app src/dialogs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==== compile_commands.json 生成 (clang-tidy, cppcheck 用) ====
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==== clang-tidy 設定 ====
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE};
            -checks=-*,clang-diagnostic-unused-*,misc-unused-*,clang-analyzer-deadcode.*,readability-redundant-*;
            -header-filter=.*
        )
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# ==== cppcheck 設定（未使用関数検出用） ====
option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK
            ${CPPCHECK_EXE};
            --enable=warning,unusedFunction;
            --inconclusive;
            --inline-suppr;
            --suppress=missingIncludeSystem;
            --suppress=unmatchedSuppression;
            --suppress=unknownMacro;
            --suppress=syntaxError:*elidelabel.h;
            --suppress=syntaxError:*_autogen/*;
            --quiet
        )
    else()
        message(WARNING "cppcheck not found, static analysis disabled")
    endif()
endif()

# ==== コンパイラ警告オプション ====
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wformat=2
        -Wimplicit-fallthrough
        # 未使用系
        -Wunused
        -Wunused-function
        -Wunused-variable
        -Wunused-parameter
    )
    # Qt6のqCDebug/qCInfo/qCWarningマクロはstream構文（<< "..."）で
    # variadic引数が空になるため、C++17では警告が出る。Qt側のマクロ設計の問題。
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-variadic-macro-arguments-omitted)
    endif()
    # -Wnull-dereference は GCC の最適化（特にインライン展開）により
    # Qt ヘッダやメンバ変数アクセスで誤検知が多発するため無効化
endif()

# --- 厳格なコンパイルオプション
#if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#    add_compile_options(
#        -Wall
#        -Wextra
#        -Wpedantic
#        -Wshadow                    # 変数のシャドウイング
#        -Wconversion                # 暗黙の型変換
#        -Wsign-conversion           # 符号付き/符号なしの変換
#        -Wnon-virtual-dtor          # 仮想デストラクタがない
#        -Wold-style-cast            # Cスタイルキャスト
#        -Wcast-align                # アライメントを増加させるキャスト
#        -Woverloaded-virtual        # 仮想関数の隠蔽
#        -Wnull-dereference          # nullポインタ参照の可能性
#        -Wdouble-promotion          # floatからdoubleへの暗黙変換
#        -Wformat=2                  # printf系のフォーマット文字列チェック
#        -Wimplicit-fallthrough      # switch文のフォールスルー
#    )
#
#    # GCC固有
#    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#        add_compile_options(
#            -Wduplicated-cond       # 重複した条件
#            -Wduplicated-branches   # 重複した分岐
#            -Wlogical-op            # 論理演算の誤り
#            -Wuseless-cast          # 不要なキャスト
#        )
#    endif()
#
#    # Clang固有
#    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#        add_compile_options(
#            -Weverything            # 全ての警告（非常に厳格）
#            -Wno-c++98-compat       # C++98互換性警告を除外
#            -Wno-c++98-compat-pedantic
#            -Wno-padded             # パディング警告を除外（うるさい）
#        )
#    endif()
#endif()
# --- ここまで

# ==== Qt6 packages ====
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Charts Network)

# ==== Translations (.ts) ====
set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
    resources/translations/ShogiBoardQ_en.ts
)

# ==== UI (.ui) ====
set(UI_FILES
    src/app/mainwindow.ui
    src/dialogs/changeenginesettingsdialog.ui
    src/dialogs/considerationdialog.ui
    src/dialogs/csagamedialog.ui
    src/dialogs/engineregistrationdialog.ui
    src/dialogs/kifuanalysisdialog.ui
    src/dialogs/promotedialog.ui
    src/dialogs/startgamedialog.ui
    src/dialogs/versiondialog.ui
)

# ==== Sources (機能別ディレクトリ構造) ====
# GLOB_RECURSE + CONFIGURE_DEPENDS でファイルを自動収集
file(GLOB_RECURSE SRC_APP        CONFIGURE_DEPENDS src/app/*.cpp)
file(GLOB_RECURSE SRC_CORE       CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE SRC_GAME       CONFIGURE_DEPENDS src/game/*.cpp)
file(GLOB_RECURSE SRC_KIFU       CONFIGURE_DEPENDS src/kifu/*.cpp)
file(GLOB_RECURSE SRC_ANALYSIS   CONFIGURE_DEPENDS src/analysis/*.cpp)
file(GLOB_RECURSE SRC_ENGINE     CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB_RECURSE SRC_NETWORK    CONFIGURE_DEPENDS src/network/*.cpp)
file(GLOB_RECURSE SRC_NAVIGATION CONFIGURE_DEPENDS src/navigation/*.cpp)
file(GLOB_RECURSE SRC_BOARD      CONFIGURE_DEPENDS src/board/*.cpp)
file(GLOB_RECURSE SRC_UI         CONFIGURE_DEPENDS src/ui/*.cpp)
file(GLOB_RECURSE SRC_VIEWS      CONFIGURE_DEPENDS src/views/*.cpp)
file(GLOB_RECURSE SRC_WIDGETS    CONFIGURE_DEPENDS src/widgets/*.cpp)
file(GLOB_RECURSE SRC_DIALOGS    CONFIGURE_DEPENDS src/dialogs/*.cpp)
file(GLOB_RECURSE SRC_MODELS     CONFIGURE_DEPENDS src/models/*.cpp)
file(GLOB_RECURSE SRC_SERVICES   CONFIGURE_DEPENDS src/services/*.cpp)
file(GLOB_RECURSE SRC_COMMON     CONFIGURE_DEPENDS src/common/*.cpp)

# ==== Headers (for AUTOMOC - ソースと同じディレクトリに配置) ====
file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

# ==== Application Icon ====
# Windows用リソースファイル
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/platform/app.rc")
endif()

# macOS用アイコン
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE shogiboardq.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/shogiboardq.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

# ==== Executable ====
qt_add_executable(ShogiBoardQ
    MANUAL_FINALIZATION
    ${SRC_APP}
    ${SRC_CORE}
    ${SRC_GAME}
    ${SRC_KIFU}
    ${SRC_ANALYSIS}
    ${SRC_ENGINE}
    ${SRC_NETWORK}
    ${SRC_NAVIGATION}
    ${SRC_BOARD}
    ${SRC_UI}
    ${SRC_VIEWS}
    ${SRC_WIDGETS}
    ${SRC_DIALOGS}
    ${SRC_MODELS}
    ${SRC_SERVICES}
    ${SRC_COMMON}
    ${UI_FILES}
    ${HDRS}
    resources/shogiboardq.qrc
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_ICON_MACOS}
)

# .qm 生成（出力はビルド側に）
# 注意: CMAKE_SOURCE_DIR 全体を渡すとビルドディレクトリ内の autogen ファイルも
# スキャンされ、循環依存が発生するため、ソースディレクトリのみを指定する
# .ui ファイルはソースディレクトリに含まれる
qt_create_translation(QM_FILES
    ${CMAKE_SOURCE_DIR}/src
    ${TS_FILES}
)

# .qm ファイルをビルドターゲットとして追加
add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShogiBoardQ translations)

# .qm ファイルを実行可能ファイルと同じディレクトリにコピー
foreach(QM_FILE ${QM_FILES})
    add_custom_command(TARGET ShogiBoardQ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QM_FILE}
            $<TARGET_FILE_DIR:ShogiBoardQ>
        COMMENT "Copying ${QM_FILE} to output directory"
    )
endforeach()

# ==== Includes / Links ====
# ヘッダーはソースと同じディレクトリに配置
target_include_directories(ShogiBoardQ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu/formats
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/coordinators
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/presenters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(ShogiBoardQ PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
)

# アプリケーションバージョン (CalVer: YYYY.MM.DD)
set(APP_VERSION "2026.02.22")

# リリースビルドではqDebug()出力を無効化
target_compile_definitions(ShogiBoardQ PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
    APP_VERSION="${APP_VERSION}"
)

# ==== Bundle properties ====
set_target_properties(ShogiBoardQ PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ShogiBoardQ
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ==== Testing ====
option(BUILD_TESTING "Build test executables" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==== Install ====
include(GNUInstallDirs)

# 実行ファイル
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# デスクトップエントリ
install(FILES resources/platform/shogiboardq.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# アプリケーションアイコン（256x256）
install(FILES resources/icons/shogiboardq.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
)

# 翻訳ファイル（.qm）
install(FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==== Finalize ====
qt_finalize_executable(ShogiBoardQ)
