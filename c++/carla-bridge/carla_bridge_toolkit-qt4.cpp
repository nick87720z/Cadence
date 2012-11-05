/*
 * Carla UI bridge code
 * Copyright (C) 2011-2012 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the COPYING file
 */

#include "carla_bridge_client.hpp"

#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtCore/QTimerEvent>
#include <QtGui/QApplication>
#include <QtGui/QMainWindow>
#include <QtGui/QVBoxLayout>

#ifdef Q_WS_X11
# include <QtGui/QX11EmbedContainer>
#endif

CARLA_BRIDGE_START_NAMESPACE

static int qargc = 0;
static char* qargv[0] = {};

class BridgeApplication : public QApplication
{
public:
    BridgeApplication()
        : QApplication(qargc, qargv, true)
    {
        msgTimer = 0;
        m_client = nullptr;
    }

    void exec(CarlaClient* const client)
    {
        m_client = client;
        msgTimer = startTimer(50);

        QApplication::exec();
    }

protected:
    void timerEvent(QTimerEvent* const event)
    {
        if (event->timerId() == msgTimer)
        {
            if (m_client && ! m_client->oscIdle())
                killTimer(msgTimer);
        }

        QApplication::timerEvent(event);
    }

private:
    int msgTimer;
    CarlaClient* m_client;
};

// -------------------------------------------------------------------------

class CarlaToolkitQt4: public CarlaToolkit
{
public:
    CarlaToolkitQt4(const char* const title)
        : CarlaToolkit(title),
#if defined(BRIDGE_LV2_QT4)
          settings("Cadence", "Carla-Qt4UIs")
#elif defined(BRIDGE_LV2_X11) || defined(BRIDGE_VST_X11)
          settings("Cadence", "Carla-X11UIs")
#elif defined(BRIDGE_LV2_HWND) || defined(BRIDGE_VST_HWND)
          settings("Cadence", "Carla-HWNDUIs")
#else
          settings("Cadence", "Carla-UIs")
#endif
    {
        qDebug("CarlaToolkitQt4::CarlaToolkitQt4(%s)", title);

        app = nullptr;
        window = nullptr;

#ifdef Q_WS_X11
        x11Container = nullptr;
#endif
    }

    ~CarlaToolkitQt4()
    {
        qDebug("CarlaToolkitQt4::~CarlaToolkitQt4()");
        CARLA_ASSERT(! app);

        if (window)
        {
            window->close();
            delete window;
        }
    }

    void init()
    {
        qDebug("CarlaToolkitQt4::init()");
        CARLA_ASSERT(! app);
        CARLA_ASSERT(! window);

        app = new BridgeApplication;

        window = new QMainWindow(nullptr);
        window->resize(10, 10);
        window->hide();
    }

    void exec(CarlaClient* const client, const bool showGui)
    {
        qDebug("CarlaToolkitQt4::exec(%p)", client);
        CARLA_ASSERT(app);
        CARLA_ASSERT(client);

#ifndef BRIDGE_LV2_X11
        QWidget* const widget = (QWidget*)client->getWidget();

        window->setCentralWidget(widget);
        window->adjustSize();

        widget->setParent(window);
        widget->show();
#endif

        if (! client->isResizable())
            window->setFixedSize(window->width(), window->height());

        window->setWindowTitle(m_title);

        if (settings.contains(QString("%1/pos_x").arg(m_title)))
        {
            int posX = settings.value(QString("%1/pos_x").arg(m_title), window->x()).toInt();
            int posY = settings.value(QString("%1/pos_y").arg(m_title), window->y()).toInt();
            window->move(posX, posY);

            if (client->isResizable())
            {
                int width  = settings.value(QString("%1/width").arg(m_title), window->width()).toInt();
                int height = settings.value(QString("%1/height").arg(m_title), window->height()).toInt();
                window->resize(width, height);
            }
        }

        if (showGui)
            show();
        else
            m_client->sendOscUpdate();

        // Main loop
        app->exec(client);
    }

    void quit()
    {
        qDebug("CarlaToolkitQt4::quit()");
        CARLA_ASSERT(app);

        if (window)
        {
            if (m_client)
            {
                settings.setValue(QString("%1/pos_x").arg(m_title), window->x());
                settings.setValue(QString("%1/pos_y").arg(m_title), window->y());
                settings.setValue(QString("%1/width").arg(m_title), window->width());
                settings.setValue(QString("%1/height").arg(m_title), window->height());
                settings.sync();
            }

            window->close();

            delete window;
            window = nullptr;
        }

        m_client = nullptr;

        if (app)
        {
            if (! app->closingDown())
                app->quit();

            delete app;
            app = nullptr;
        }
    }

    void show()
    {
        qDebug("CarlaToolkitQt4::show()");
        CARLA_ASSERT(window);

        if (window)
            window->show();
    }

    void hide()
    {
        qDebug("CarlaToolkitQt4::hide()");
        CARLA_ASSERT(window);

        if (window)
            window->hide();
    }

    void resize(int width, int height)
    {
        qDebug("CarlaToolkitQt4::resize(%i, %i)", width, height);
        CARLA_ASSERT(window);

        if (window)
            window->setFixedSize(width, height);

#ifdef BRIDGE_LV2_X11
        if (x11Container)
            x11Container->setFixedSize(width, height);
#endif
    }

    void* getContainerId()
    {
#ifdef Q_WS_X11
        if (! x11Container)
        {
            x11Container = new QX11EmbedContainer(window);

            window->setCentralWidget(x11Container);
            window->adjustSize();

            x11Container->setParent(window);
            x11Container->show();
        }

        return (void*)x11Container->winId();
#else
        return nullptr;
#endif
    }

private:
    BridgeApplication* app;
    QMainWindow* window;
    QSettings settings;

#ifdef Q_WS_X11
    QX11EmbedContainer* x11Container;
#endif
};

// -------------------------------------------------------------------------

CarlaToolkit* CarlaToolkit::createNew(const char* const title)
{
    return new CarlaToolkitQt4(title);
}

CARLA_BRIDGE_END_NAMESPACE
