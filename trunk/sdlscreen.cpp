// sdlscreen.cpp
//
// Copyright (C) 2010, 2009 Griffin I'Net, Inc.
//
// This file is licensed under the LGPL version 2.1, the text of which should
// be in the LICENSE.txt file, or alternately at this location:
// http://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
//
// DISCLAIMER: This software is released AS-IS with ABSOLUTELY NO WARRANTY OF
// ANY KIND.

#include <SDL/SDL.h>
#include <PDL.h>

#include "sdlscreen.h"
#include "sdlscreen_private.h"
#include "sdlscreenthread.h"
#include "palmprekeyboard.h"
#include "palmpremouse.h"

#include <QCoreApplication>
#include <QColor>
#include <QRect>
#include <QRegion>
#include <QImage>
#include <QWSServer>
#include <qwsdisplay_qws.h>

#define DEFAULT_WIDTH 320   // default if SDL_GetVideoInfo() fails
#define DEFAULT_HEIGHT 480
#define DEPTH 32


void pdlexit()
{
    printf("pdl quit\n");
    PDL_Quit();
}

Uint32 timer_callback(Uint32 interval, void *_screen_p)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return interval;

/*
    SDLScreen_private *screen_p = (SDLScreen_private*)_screen_p;

    // Mutex
    screen_p->_mutex.lock();

    SDL_PushEvent(SDL_Event *event);



    //screen_p->_mutex.unlock();
    return interval;*/

}


SDLScreen::SDLScreen ( const QString& driver, int display_id )
    : QScreen(display_id)
    //: QTransformedScreen(display_id)

{
    bool showDebugInfo = !qgetenv("QWS_DEBUG").isEmpty();

    if (showDebugInfo)
        qDebug("SDLScreen(%s, %d)", (const char*)driver.toAscii(), display_id);

    printf("SDLScreen constructor: %p\n",this);
    printf("SDLScreen constructor driver: %s\n",driver.toStdString().c_str());

    d = new SDLScreen_private(showDebugInfo);
}

SDLScreen::~SDLScreen()
{
    if (d->_showDebugInfo)
        qDebug("~SDLScreen()");

    delete d;
}

bool SDLScreen::initDevice()
{
    if (d->_showDebugInfo)
        qDebug("SDLScreen::initDevice()");

    // everything is done in connect()

    return true;
}

void SDLScreen::shutdownDevice()
{
    if (d->_showDebugInfo)
        qDebug("SDLScreen::shutdownDevice()");

    // everything is done in disconnect()
}

bool SDLScreen::connect ( const QString& displaySpec )
{
    if (d->_showDebugInfo)
        qDebug("SDLScreen::connect(%s)", (const char*)displaySpec.toAscii());

    if (qgetenv("QWS_KEYBOARD").isEmpty()) {
        QWSServer::instance()->setDefaultKeyboard("None");
        d->_keyboardHandler = new PalmPreKeyboard(this, d->_showDebugInfo);
//        QWSServer::instance()->setKeyboardHandler(d->_keyboardHandler);
    }

    if (qgetenv("QWS_MOUSE_PROTO").isEmpty()) {
        QWSServer::instance()->setDefaultMouse("None");
        d->_mouseHandler = new PalmPreMouse(this, d->_showDebugInfo);
//        QWSServer::instance()->setMouseHandler(d->_mouseHandler);
    }

    // start the PDL library
    PDL_ScreenMetrics ScreenMetrics;
    PDL_Init(0);
    PDL_GetScreenMetrics(&ScreenMetrics);
    printf("Screen size: %d %d\n",ScreenMetrics.horizontalPixels,ScreenMetrics.verticalPixels);
    atexit(pdlexit);

    d->_mutex.lock();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        qDebug("SDL_Init failed!");

        d->_mutex.unlock();
        return false;
    }

    dw = DEFAULT_WIDTH;
    dh = DEFAULT_HEIGHT;
    QScreen::d = DEPTH;     // We -almost- changed our standard use of 'd' for the private data class over this

    const SDL_VideoInfo* vi = SDL_GetVideoInfo();

    if (vi != NULL) {
        if (d->_showDebugInfo)
            qDebug("Video info: %dx%d, bpp = %d", vi->current_w, vi->current_h, vi->vfmt->BitsPerPixel);

        if (vi->current_w > 0) {
            // valid screen dimensions (?)
            dw = vi->current_w;
            dh = vi->current_h;
        }
    }

    d->_surface = SDL_SetVideoMode(dw, dh, QScreen::d, SDL_SWSURFACE);

    if (!d->_surface) {
        qDebug("SDL_SetVideoMode() failed!");
        SDL_Quit();

        d->_mutex.unlock();
        return false;
    }

    data = (uchar*)d->_surface->pixels;
    grayscale = false;

    // w/: The logical screen width/height in pixels.
    // dw/dh: The real screen width/height in pixels.
    w = dw;
    h = dh;
    //h = dh-64;      // Dan: cheat a bit - keep some pixels for debug
    mapsize = dw * dh * sizeof(quint32);

    physWidth = 44;         // experimental numbers resulting in reasonable font sizes
    physHeight = 63;
    physWidth = w / 3;
    physHeight = h / 3;

    pixeltype = QScreen::NormalPixel;
    screencols = 0;
    size = mapsize;
    lstep = dw * sizeof(quint32);


    // Keyboard
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);


    // Add some timer
    if (SDL_Init(SDL_INIT_TIMER) < 0) {
        qDebug("SDL_Init Timer failed!");
        d->_mutex.unlock();
        return false;
    }
    SDL_AddTimer(250,&timer_callback, (void*)d);

    // Joystick
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        qDebug("SDL_Init Joystick failed!");

        d->_mutex.unlock();
        return false;
    }
    printf("Number of joysticks: %d\n",SDL_NumJoysticks());
      // Open joystick
    d->joystick=SDL_JoystickOpen(0);




    QSize s;
    s = mapToDevice(QSize(100,100));
    printf("mapToDevice: %d %d\n",s.width(),s.height());
    s = mapFromDevice(QSize(100,100));
    printf("mapFromDevice: %d %d\n",s.width(),s.height());

    //setTransformation(Rot90);
    //printf("Transforming\n");

    printf("mapToDevice: %d %d\n",s.width(),s.height());
    s = mapFromDevice(QSize(100,100));
    printf("mapFromDevice: %d %d\n",s.width(),s.height());


    d->_open = true;

    d->_mutex.unlock();

    printf("Constructor of sdl\n");

    return true;
}

void SDLScreen::disconnect()
{
    if (d->_showDebugInfo)
        qDebug("SDLScreen::disconnect()");

    if (d->_open)
    {
        d->_mutex.lock();
        SDL_Quit();
        d->_open = false;
        d->_mutex.unlock();
    }
}

void SDLScreen::setMode ( int width, int height, int depth )
{
    //printf("SDLScreen::setMode: %d %d %d\n",width,height,depth);
    if (d->_open) {
        if (d->_showDebugInfo)
            qDebug("setMode %d %d %d", width, height, depth);

        d->_mutex.lock();
        d->_surface = SDL_SetVideoMode(width, height, depth, SDL_SWSURFACE);
        d->_mutex.unlock();

        if (d->_surface == NULL) {
            qDebug("SDL_SetVideoMode failed!");
            QCoreApplication::quit();
        }
    }
}

void SDLScreen::solidFill ( const QColor& color, const QRegion& region )
{
    if (!d->_open)
        return;

    QWSDisplay::grab();
    //printf("SDLScreen::solidFill: %X\n",color.rgba());
    d->_mutex.lock();



    if (SDL_MUSTLOCK(d->_surface)) {
        if (SDL_LockSurface(d->_surface) < 0) {
            d->_mutex.unlock();
            return;
        }
    }

    //QScreen::solidFill(color, region);
    //QScreen::solidFill(QColor(255,0,0,127), region);


    if (SDL_MUSTLOCK(d->_surface))
        SDL_UnlockSurface(d->_surface);

    SDL_Flip(d->_surface);

    d->_mutex.unlock();
    QWSDisplay::ungrab();
}

void SDLScreen::blit ( const QImage& image, const QPoint& topLeftRaw, const QRegion& region )
{
    if (!d->_open)
        return;

    QWSDisplay::grab();
    //printf("SDLScreen::blit: Image %d %d -> %d %d\n",image.width(),image.height(),topLeftRaw.x(),topLeftRaw.y());
    d->_mutex.lock();



    if (SDL_MUSTLOCK(d->_surface)) {
        if (SDL_LockSurface(d->_surface) < 0) {
            d->_mutex.unlock();
            return;
        }
    }

    QScreen::blit(image, topLeftRaw, region);

    if (SDL_MUSTLOCK(d->_surface))
        SDL_UnlockSurface(d->_surface);

    SDL_Flip(d->_surface);

    d->_mutex.unlock();
    QWSDisplay::ungrab();
}

// This is a very ugly hack. When rotating the screen buffer
// QScreenTransformed::blit does a direct blit, but doesn't notify us to flip the surface.
// We hack this on top of blank....
void SDLScreen::blank ( bool on )
{
    if (!d->_open)
        return;
    if(on)
    {
        d->_mutex.lock();
        if (SDL_MUSTLOCK(d->_surface)) {
            if (SDL_LockSurface(d->_surface) < 0) {
                d->_mutex.unlock();
                return;
            }
        }
    }
    else
    {
        if (SDL_MUSTLOCK(d->_surface))
            SDL_UnlockSurface(d->_surface);

        SDL_Flip(d->_surface);

        d->_mutex.unlock();
    }

}


