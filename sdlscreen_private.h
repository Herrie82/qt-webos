// sdlscreen_private.h
//
// Copyright (C) 2010, 2009 Griffin I'Net, Inc.
//
// This file is licensed under the LGPL version 2.1, the text of which should
// be in the LICENSE.txt file, or alternately at this location:
// http://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
//
// DISCLAIMER: This software is released AS-IS with ABSOLUTELY NO WARRANTY OF
// ANY KIND.

#ifndef SDLSCREEN_PRIVATE_H
#define SDLSCREEN_PRIVATE_H

#include <QObject>
#include <QMutex>

#include <SDL/SDL.h>

class SDL_Surface;
class SDLScreenThread;
class PalmPreKeyboard;
class PalmPreMouse;

class SDLScreen_private : public QObject
{
Q_OBJECT

public:
    SDLScreen_private ( bool showDebugInfo );
    ~SDLScreen_private();

signals:
    void sdlMouseEvent ( int code, int x, int y, int buttons );
    void sdlKeyboardEvent(int type,int sym,int mod);
  //  void OrientationChangeEvent(int o);

public:
    void sendSdlMouseEvent ( int code, int x, int y, int buttons );
    void sendSdlKeyboardEvent(int type,int sym,int mod);
    void sendOrientationChangeEvent(int o);


    bool _showDebugInfo;

    PalmPreKeyboard* _keyboardHandler;
    PalmPreMouse* _mouseHandler;

    bool _open;

    QMutex _mutex;
    SDL_Surface* _surface;

    SDLScreenThread* _thread;

    SDL_Joystick *joystick;
};

#endif // SDLSCREEN_PRIVATE_H
