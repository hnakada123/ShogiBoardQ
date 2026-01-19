cmake_minimum_required(VERSION 3.16)

project(ShogiBoardQ VERSION 0.1 LANGUAGES CXX)

# ==== Qt auto features ====
set(CMAKE_AUTOUIC ON)
# .ui は従来どおり ui/ 配下を探索
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)
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
        -Wnull-dereference
        -Wformat=2
        -Wimplicit-fallthrough
        # 未使用系
        -Wunused
        -Wunused-function
        -Wunused-variable
        -Wunused-parameter
    )
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

# ==== Qt packages ====
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Charts Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Charts Network)

# ==== Translations (.ts) ====
set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
    resources/translations/ShogiBoardQ_en.ts
)

# ==== UI (.ui) ====
set(UI_FILES
    ui/mainwindow.ui
    ui/changeenginesettingsdialog.ui
    ui/considerationdialog.ui
    ui/csagamedialog.ui
    ui/engineregistrationdialog.ui
    ui/kifuanalysisdialog.ui
    ui/promotedialog.ui
    ui/startgamedialog.ui
    ui/versiondialog.ui
)

# ==== Sources (after moving .cpp into subdirs) ====
# とりあえず GLOB_RECURSE で楽をします（CONFIGURE_DEPENDS 付き）。
# 将来的には明示列挙に切り替えてOKです。
file(GLOB_RECURSE SRC_APP      CONFIGURE_DEPENDS src/app/*.cpp)
file(GLOB_RECURSE SRC_CORE     CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE SRC_CONTROLLERS     CONFIGURE_DEPENDS src/controllers/*.cpp)
file(GLOB_RECURSE SRC_COORDINATORS     CONFIGURE_DEPENDS src/coordinators/*.cpp)
file(GLOB_RECURSE SRC_VIEWS    CONFIGURE_DEPENDS src/views/*.cpp)
file(GLOB_RECURSE SRC_ENGINE   CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB_RECURSE SRC_MODELS   CONFIGURE_DEPENDS src/models/*.cpp)
file(GLOB_RECURSE SRC_WIDGETS  CONFIGURE_DEPENDS src/widgets/*.cpp)
file(GLOB_RECURSE SRC_DIALOGS  CONFIGURE_DEPENDS src/dialogs/*.cpp)
file(GLOB_RECURSE SRC_SERVICES CONFIGURE_DEPENDS src/services/*.cpp)
file(GLOB_RECURSE SRC_NETWORK  CONFIGURE_DEPENDS src/network/*.cpp)

# ==== Headers (for AUTOMOC) ====
file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# ==== Application Icon ====
# Windows用リソースファイル
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
endif()

# macOS用アイコン
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE shogiboardq.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/shogiboardq.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

# ==== Executable ====
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ShogiBoardQ
        MANUAL_FINALIZATION
        ${SRC_APP}
        ${SRC_CORE}
        ${SRC_CONTROLLERS}
        ${SRC_COORDINATORS}
        ${SRC_VIEWS}
        ${SRC_ENGINE}
        ${SRC_MODELS}
        ${SRC_WIDGETS}
        ${SRC_DIALOGS}
        ${SRC_SERVICES}
        ${SRC_NETWORK}
        ${UI_FILES}
        ${HDRS}
        resources/shogiboardq.qrc
        ${APP_ICON_RESOURCE_WINDOWS}
        ${APP_ICON_MACOS}
    )

    # .qm 生成（出力はビルド側に）
    # 注意: CMAKE_SOURCE_DIR 全体を渡すとビルドディレクトリ内の autogen ファイルも
    # スキャンされ、循環依存が発生するため、ソースディレクトリのみを指定する
    qt_create_translation(QM_FILES
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/ui
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
else()
    # Qt5 フォールバック（基本は Qt6 想定）
    add_executable(ShogiBoardQ
        ${SRC_APP}
        ${SRC_CORE}
        ${SRC_CONTROLLERS}
        ${SRC_COORDINATORS}
        ${SRC_VIEWS}
        ${SRC_ENGINE}
        ${SRC_MODELS}
        ${SRC_WIDGETS}
        ${SRC_DIALOGS}
        ${SRC_SERVICES}
        ${SRC_NETWORK}
        ${UI_FILES}
        resources/shogiboardq.qrc
        ${APP_ICON_RESOURCE_WINDOWS}
        ${APP_ICON_MACOS}
    )
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# ==== Includes / Links ====
# グローバル include_directories はやめ、ターゲットに限定
target_include_directories(ShogiBoardQ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ShogiBoardQ PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
)

# ==== Bundle properties ====
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ShogiBoardQ)
endif()
set_target_properties(ShogiBoardQ PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ==== Install ====
include(GNUInstallDirs)
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==== Finalize (Qt6) ====
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ShogiBoardQ)
endif()
