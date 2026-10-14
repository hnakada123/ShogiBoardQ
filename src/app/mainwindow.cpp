/// @file mainwindow.cpp
/// @brief MainWindow の実装（部品の組み立ては MainWindowComposer）

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "mainwindowcomposer.h"

#include <QCloseEvent>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , m_composer(std::make_unique<MainWindowComposer>(*this, *ui))
{
    m_composer->compose();
}

MainWindow::~MainWindow()
{
    m_composer->shutdown();
}

void MainWindow::startAutomationServer(const QString& socketPath)
{
    m_composer->startAutomationServer(socketPath);
}

void MainWindow::quitWithoutConfirmation()
{
    m_skipCloseConfirmation = true;
    if (close()) QCoreApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (!m_skipCloseConfirmation && !m_composer->confirmClose()) {
        e->ignore();
        return;
    }
    QMainWindow::closeEvent(e);
    if (!e->isAccepted()) return;
    m_composer->shutdown();
}
