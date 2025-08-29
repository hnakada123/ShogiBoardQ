cmake_minimum_required(VERSION 3.16)

project(ShogiBoardQ VERSION 0.1 LANGUAGES CXX)

include_directories(include)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Charts)

set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        include/mainwindow.h
        ui/mainwindow.ui
        ${TS_FILES}
)

set(SOURCE_FILES
        src/apptooltipfilter.cpp
        src/changeenginesettingsdialog.cpp
        src/considarationdailog.cpp
        src/tsumeshogisearchdialog.cpp
        src/elidelabel.cpp
        src/engineregistrationdialog.cpp
        src/globaltooltip.cpp
        src/kiftosfenconverter.cpp
        src/kifuanalysisdialog.cpp
        src/kifuanalysislistmodel.cpp
        src/kifuanalysisresultsdisplay.cpp
        src/kifudisplay.cpp
        src/kifurecordlistmodel.cpp
        src/longlongspinbox.cpp
        src/movevalidator.cpp
        src/promotedialog.cpp
        src/sfenpositiontracer.cpp
        src/shogiboard.cpp
        src/shogiclock.cpp
        src/shogiengineinfoparser.cpp
        src/shogienginethinkingmodel.cpp
        src/shogigamecontroller.cpp
        src/shogiinforecord.cpp
        src/shogimove.cpp
        src/shogiutils.cpp
        src/shogiview.cpp
        src/startgamedialog.cpp
        src/usi.cpp
        src/usicommlogmodel.cpp
        src/versiondialog.cpp
)

set(HEADER_FILES
        include/abstractlistmodel.h
        include/apptooltipfilter.h
        include/changeenginesettingsdialog.h
        include/considarationdialog.h
        include/tsumeshogisearchdialog.h
        include/elidelabel.h
        include/engineoptions.h
        include/engineregistrationdialog.h
        include/enginesettingsconstants.h
        include/globaltooltip.h
        include/kiftosfenconverter.h
        include/kifuanalysisdialog.h
        include/kifuanalysislistmodel.h
        include/kifuanalysisresultsdisplay.h
        include/kifudisplay.h
        include/kifurecordlistmodel.h
        include/legalmovestatus.h
        include/longlongspinbox.h
        include/movevalidator.h
        include/playmode.h
        include/promotedialog.h
        include/sfenpositiontracer.h
        include/shogiboard.h
        include/shogiclock.h
        include/shogiengineinfoparser.h
        include/shogienginethinkingmodel.h
        include/shogigamecontroller.h
        include/shogiinforecord.h
        include/shogimove.h
        include/shogiutils.h
        include/shogiview.h
        include/startgamedialog.h
        include/usi.h
        include/usicommlogmodel.h
        include/versiondialog.h
)

set(UI_FILES
        ui/changeenginesettingsdialog.ui
        ui/considarationdialog.ui
        ui/engineregistrationdialog.ui
        ui/kifuanalysisdialog.ui
        ui/promotedialog.ui
        ui/startgamedialog.ui
        ui/versiondialog.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ShogiBoardQ
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${SOURCE_FILES}
        ${HEADER_FILES}
        ${UI_FILES}
        resources/shogiboardq.qrc
        resources/images/icons/bold.png
        resources/images/icons/chuudan.png
        resources/images/icons/copy.png
        resources/images/icons/create.png
        resources/images/icons/cut.png
        resources/images/icons/dai.png
        resources/images/icons/edit_redo.png
        resources/images/icons/edit_undo.png
        resources/images/icons/exit.png
        resources/images/icons/font.png
        resources/images/icons/gtk-media-forward-ltr.png
        resources/images/icons/gtk-media-forward-rtl.png
        resources/images/icons/gtk-media-next-ltr.png
        resources/images/icons/gtk-media-next-rtl.png
        resources/images/icons/gtk-media-pause.png
        resources/images/icons/gtk-media-play-ltr.png
        resources/images/icons/gtk-media-play-rtr.png
        resources/images/icons/gtk-media-previous-ltr.png
        resources/images/icons/gtk-media-previous-rtl.png
        resources/images/icons/gtk-media-record.png
        resources/images/icons/gtk-media-rewind-ltr.png
        resources/images/icons/gtk-media-rewind-rtl.png
        resources/images/icons/gtk-media-stop.png
        resources/images/icons/info.png
        resources/images/icons/italic.png
        resources/images/icons/kaiseki.png
        resources/images/icons/kaiten.png
        resources/images/icons/kentou.png
        resources/images/icons/kyuu.png
        resources/images/icons/matta.png
        resources/images/icons/new.png
        resources/images/icons/open.png
        resources/images/icons/paste.png
        resources/images/icons/pencil.png
        resources/images/icons/print.png
        resources/images/icons/save.png
        resources/images/icons/save_as.png
        resources/images/icons/shou.png
        resources/images/icons/taikyoku.png
        resources/images/icons/touryou.png
        resources/images/icons/tsumi.png
        resources/images/icons/underline.png
        resources/images/pieces/Gote_fu45.svg
        resources/images/pieces/Gote_gin45.svg
        resources/images/pieces/Gote_gyoku45.svg
        resources/images/pieces/Gote_hi45.svg
        resources/images/pieces/Gote_kaku45.svg
        resources/images/pieces/Gote_kei45.svg
        resources/images/pieces/Gote_kin45.svg
        resources/images/pieces/Gote_kyou45.svg
        resources/images/pieces/Gote_narigin45.svg
        resources/images/pieces/Gote_narikei45.svg
        resources/images/pieces/Gote_narikyou45.svg
        resources/images/pieces/Gote_ou45.svg
        resources/images/pieces/Gote_ryuu45.svg
        resources/images/pieces/Gote_to45.svg
        resources/images/pieces/Gote_uma45.svg
        resources/images/pieces/Sente_fu45.svg
        resources/images/pieces/Sente_gin45.svg
        resources/images/pieces/Sente_gyoku45.svg
        resources/images/pieces/Sente_hi45.svg
        resources/images/pieces/Sente_kaku45.svg
        resources/images/pieces/Sente_kei45.svg
        resources/images/pieces/Sente_kin45.svg
        resources/images/pieces/Sente_kyou45.svg
        resources/images/pieces/Sente_narigin45.svg
        resources/images/pieces/Sente_narikei45.svg
        resources/images/pieces/Sente_narikyou45.svg
        resources/images/pieces/Sente_ou45.svg
        resources/images/pieces/Sente_ryuu45.svg
        resources/images/pieces/Sente_to45.svg
        resources/images/pieces/Sente_uma45.svg
        resources/images/pieces/Gote_fu45.svg
        resources/images/pieces/Gote_gin45.svg
        resources/images/pieces/Gote_gyoku45.svg
        resources/images/pieces/Gote_hi45.svg
        resources/images/pieces/Gote_kaku45.svg
        resources/images/pieces/Gote_kei45.svg
        resources/images/pieces/Gote_kin45.svg
        resources/images/pieces/Gote_kyou45.svg
        resources/images/pieces/Gote_narigin45.svg
        resources/images/pieces/Gote_narikei45.svg
        resources/images/pieces/Gote_narikyou45.svg
        resources/images/pieces/Gote_ou45.svg
        resources/images/pieces/Gote_ryuu45.svg
        resources/images/pieces/Gote_to45.svg
        resources/images/pieces/Gote_uma45.svg
        resources/images/pieces/Sente_fu45.svg
        resources/images/pieces/Sente_gin45.svg
        resources/images/pieces/Sente_gyoku45.svg
        resources/images/pieces/Sente_hi45.svg
        resources/images/pieces/Sente_kaku45.svg
        resources/images/pieces/Sente_kei45.svg
        resources/images/pieces/Sente_kin45.svg
        resources/images/pieces/Sente_kyou45.svg
        resources/images/pieces/Sente_narigin45.svg
        resources/images/pieces/Sente_narikei45.svg
        resources/images/pieces/Sente_narikyou45.svg
        resources/images/pieces/Sente_ou45.svg
        resources/images/pieces/Sente_ryuu45.svg
        resources/images/pieces/Sente_to45.svg
        resources/images/pieces/Sente_uma45.svg
    )

# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ShogiBoardQ APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(ShogiBoardQ SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(ShogiBoardQ
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(ShogiBoardQ PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Charts)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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

include(GNUInstallDirs)
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ShogiBoardQ)
endif()
