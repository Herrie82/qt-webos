// sdlscreenthread.cpp
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
#include <SDL_joystick.h>

#include "sdlscreenthread.h"
#include "sdlscreen_private.h"
//#undef QT_NO_QWS_TRANSFORMED
#include <QTransformedScreen>

SDLScreenThread::SDLScreenThread ( SDLScreen_private* d)
    : QThread()
{
    _d = d;

    _quit = false;
}

SDLScreenThread::~SDLScreenThread()
{
    stop();
}

void SDLScreenThread::stop()
{
    _quit = true;
    wait();
}

void SDLScreenThread::run()
{
    SDL_Event event;
    quint8 buttonstate;
    int ret;
    double acc[3]={0,0,0};

    while(!_quit) {
        if (_d->_open) {
            for(;;) {
                // loop on SDL event queue until empty
                _d->_mutex.lock();

                if (!_d->_open || _quit) {
                    // re-check after aquiring mutex
                    _d->_mutex.unlock();
                    break;
                }

                ret = SDL_PollEvent(&event);
                _d->_mutex.unlock();

                if (!ret)
                    break;

                if ((event.type == SDL_MOUSEBUTTONDOWN) ||
                    (event.type == SDL_MOUSEBUTTONUP) ||
                    (event.type == SDL_MOUSEMOTION)) {
                    // mouse event
                    buttonstate = SDL_GetMouseState(NULL, NULL);

                    _d->sendSdlMouseEvent(event.type, event.motion.x, event.motion.y,
                                         ((buttonstate & SDL_BUTTON(1)) ? 1 : 0)
                                       | ((buttonstate & SDL_BUTTON(2)) ? 2 : 0)
                                       | ((buttonstate & SDL_BUTTON(3)) ? 4 : 0));
                }
                if((event.type == SDL_KEYDOWN) ||
                   (event.type == SDL_KEYUP)) {
                    //_d->sendSdlKeyboardEvent(event.key.keysym,event.key.keysym);
                    _d->sendSdlKeyboardEvent(event.type,event.key.keysym.sym,event.key.keysym.mod);

                }
                //printf("Event %d\n",event.type);

                if(event.type == SDL_JOYAXISMOTION)
                {
                    //printf("Joy %d Axis %d To %lf\n",event.jaxis.which,event.jaxis.axis,event.jaxis.value/32768.0);
                    acc[event.jaxis.axis]=event.jaxis.value/32768.0;
                    //printf("%.3lf %.3lf %.3lf\n",acc[0],acc[1],acc[2]);

                }

                if(event.type == SDL_USEREVENT)
                {
                    //printf("User event!\n");
                    double x,y,z;

                    _d->_mutex.lock();
                    x = SDL_JoystickGetAxis(_d->joystick, 0) / 32768.0;
                    y = SDL_JoystickGetAxis(_d->joystick, 1) / 32768.0;
                    z = SDL_JoystickGetAxis(_d->joystick, 2) / 32768.0;
                    _d->_mutex.unlock();
                    //printf("%lf %lf %lf\n",x,y,z);

                    // Check if roation needed
                    /*if(y>0.7)
                        printf("Rotate 0\n");
                    if(y<-0.7)
                        printf("Rotate 180\n");
                    if(x<-0.7)
                        printf("Rotate 270\n");
                    if(x>0.7)
                        printf("Rotate 90\n");*/
                    //QTransformedScreen *ts = static_cast<QTransformedScreen*>(QTransformedScreen::instance());
                    QTransformedScreen *ts = static_cast<QTransformedScreen*>(QTransformedScreen::instance());
                    if(y>0.7)
                    {
                        //ts->setTransformation(QTransformedScreen::None);
                        _d->sendOrientationChangeEvent(QTransformedScreen::None);
                    }
                    if(y<-0.7)
                    {
                        //ts->setTransformation(QTransformedScreen::Rot180);
                        _d->sendOrientationChangeEvent(QTransformedScreen::Rot180);
                    }
                    if(x<-0.7)
                    {
                        //ts->setTransformation(QTransformedScreen::Rot270);
                        _d->sendOrientationChangeEvent(QTransformedScreen::Rot270);
                    }
                    if(x>0.7)
                    {
                        //ts->setTransformation(QTransformedScreen::Rot90);
                        _d->sendOrientationChangeEvent(QTransformedScreen::Rot90);
                    }



                }

            }
        }

        QThread::msleep(100);
    }
}
