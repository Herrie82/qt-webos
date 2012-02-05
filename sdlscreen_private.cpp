// sdlscreen_private.cpp
//
// Copyright (C) 2010, 2009 Griffin I'Net, Inc.
//
// This file is licensed under the LGPL version 2.1, the text of which should
// be in the LICENSE.txt file, or alternately at this location:
// http://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
//
// DISCLAIMER: This software is released AS-IS with ABSOLUTELY NO WARRANTY OF
// ANY KIND.

#include "sdlscreen_private.h"
#include "sdlscreenthread.h"
#include <QEvent>
#include <QWSEvent>
#include <QApplication>
#include <QWSServer>
#undef QT_NO_QWS_TRANSFORMED
#include <QTransformedScreen>


SDLScreen_private::SDLScreen_private ( bool showDebugInfo ) :
    QObject()
{
    _showDebugInfo = showDebugInfo;

    _keyboardHandler = NULL;
    _mouseHandler = NULL;

    _open = false;

    _surface = NULL;

    _thread = new SDLScreenThread(this);

    _thread->start();
}

SDLScreen_private::~SDLScreen_private()
{
    delete _thread;
}

void SDLScreen_private::sendSdlMouseEvent ( int code, int x, int y, int buttons )
{
    emit sdlMouseEvent(code, x, y, buttons);
}

void SDLScreen_private::sendSdlKeyboardEvent(int type,int sym,int mod)
{
    emit sdlKeyboardEvent(type,sym,mod);
}

void SDLScreen_private::sendOrientationChangeEvent(int o)
{
    printf("SDLScreen_private::sendOrientationChangeEvent(int o)\n");
    QTransformedScreen *ts = static_cast<QTransformedScreen*>(QScreen::instance());
    if(ts->transformation()==o)
    {
        printf("Transformation NOT necessary\n");
        return;
    }

    QWSScreenTransformationEvent e;
    e.simpleData.screen = 0;
    e.simpleData.transformation=o;
    //QWSServer::instance()->refresh();
    QList<QWSWindow *> l = QWSServer::instance()->clientWindows();
    printf("Client windows: %d\n",l.size());
    QVector<QWSClient *> notifiedClients;
    for(int i=0;i<l.size();i++)
    {

        printf("%d -> %p %p\n",i,l[i],l[i]->client());


        // We must send the event only once
        if(!notifiedClients.contains(l[i]->client()))
        {
            printf("Notifying client %p\n",l[i]->client());
            l[i]->client()->sendEvent(&e);
            notifiedClients.append(l[i]->client());
        }
    }
    //QWSServer::cli
    // QWSServer::sendMouseEvent(mousePos, state, wheel);
    /*QWSWindow *win = qwsServer->windowAt(pos);

    QWSClient *serverClient = qwsServerPrivate->clientMap.value(-1);
    QWSClient *winClient = win ? win->client() : 0;*/

}
