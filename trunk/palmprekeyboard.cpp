// palmprekeyboard.cpp
//
// Copyright (C) 2010, 2009 Griffin I'Net, Inc.
//
// This file is licensed under the LGPL version 2.1, the text of which should
// be in the LICENSE.txt file, or alternately at this location:
// http://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
//
// DISCLAIMER: This software is released AS-IS with ABSOLUTELY NO WARRANTY OF
// ANY KIND.

#include <fcntl.h>
#include <linux/input.h>

#include "palmprekeyboard.h"
#include "palmprekeyboard_private.h"
#include "sdlscreen.h"
#include "dlgsymbols.h"

#include <QFile>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <SDL/SDL.h>

PalmPreKeyboard::PalmPreKeyboard ( SDLScreen* screen, bool showDebugInfo )
    : QWSKeyboardHandler(QString())
{
    if (showDebugInfo)
        qDebug("PalmPreKeyboard()");

    d = new PalmPreKeyboard_private(this, screen, showDebugInfo);
}

PalmPreKeyboard::~PalmPreKeyboard()
{
    if (d->_showDebugInfo)
        qDebug("~PalmPreKeyboard()");

    delete d;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


class KeyMap : public QHash<int, Qt::Key>
{
public:
    KeyMap();
};

Q_GLOBAL_STATIC(KeyMap, keymap);

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


class KeyMapSDL : public QHash<int, Qt::Key>
{
public:
    KeyMapSDL();
};

Q_GLOBAL_STATIC(KeyMapSDL, keymapsdl);


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


PalmPreKeyboard_private::PalmPreKeyboard_private ( PalmPreKeyboard* keyboardHandler, SDLScreen* screen, bool showDebugInfo )
    : QObject()
{
    _keyboardHandler = keyboardHandler;
    _screen = screen;
    _showDebugInfo = showDebugInfo;

    _shift = Up;
    _square = Up;
    _symbol = Up;

    _sn = NULL;

    _dlgSymbols = NULL;

/*    _fd = open("/dev/input/keypad0", O_RDONLY);

    if (_fd == -1) {
        qDebug("PalmPreKeyboard : Failed to open keyboard handle!");
        return;
    }

    _sn = new QSocketNotifier(_fd, QSocketNotifier::Read);

    // Deactivate direct access to keyboard
    //connect(_sn, SIGNAL(activated(int)), this, SLOT(activity(int)));

    _sn->setEnabled(true);
*/

    QObject::connect((QObject*)screen->d, SIGNAL(sdlKeyboardEvent(int,int,int)), (QObject*)this, SLOT(sdlKeyboardEvent(int,int,int)));  // ? need to cast screen->d otherwise compiler freaks


}


PalmPreKeyboard_private::~PalmPreKeyboard_private()
{
    /*if (_dlgSymbols)
        delete _dlgSymbols;

    if (_sn != NULL) {
        delete _sn;
        close(_fd);
    }*/
}

#ifdef PALMPREKEYBOARD_LOG_UNKNOWN_KEYS
void logKey ( const QString& key )
{
    QFile file("/media/internal/PalmPreKeyboard_unknownKeys.log");

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        return;

    file.write(QString("%1\n").arg(key).toAscii());
}
#endif

void PalmPreKeyboard_private::symbolDialogHidden()
{
    _symbol = Up;
}

void PalmPreKeyboard_private::activity ( int )
{
    struct input_event in;

    _sn->setEnabled(false);

    read(_fd, &in, sizeof(struct input_event));

    if (_showDebugInfo) {
        QString debugText = QString("Keyboard: type %1, code %2, value %3").arg(in.type).arg(in.code).arg(in.value);
        qDebug("%s", (const char*)debugText.toAscii());
    }

#ifdef PALMPREKEYBOARD_LOG_UNKNOWN_KEYS
    logKey(debugText);
#endif

    if (in.type == 1) {
        // key event
        if (in.code == PalmPreKeyboard::Key_Symbol) {
            if (in.value == 1) {
                if (_symbol == Up) {
                    _symbol = Down;

                    // launch symbol window
                    if (!_dlgSymbols)
                    {
                        _dlgSymbols = new dlgSymbols(_screen);
                        connect(_dlgSymbols, SIGNAL(hidden()), this, SLOT(symbolDialogHidden()));
                        QTimer::singleShot(1, _dlgSymbols, SLOT(show()));
                    }

                    _dlgSymbols->show();
                }
                else
                    _symbol = Up;
            }

            if (_showDebugInfo)
                qDebug("sym: %d", in.value);
        } else if (in.code == PalmPreKeyboard::Key_Square) {
            if (in.value == 1) {
                if (_square == Up)
                    _square = Down;
                else if (_square == Down)
                    _square = Locked;
                else
                    _square = Up;
            }

            if (_showDebugInfo)
                qDebug("square: %d", in.value);
        } else if (in.code == PalmPreKeyboard::Key_Shift) {
            if (in.value == 1) {
                if (_shift == Up)
                    _shift = Down;
                else if (_shift == Down)
                    _shift = Locked;
                else
                    _shift = Up;
            }

            if (_showDebugInfo)
                qDebug("shift: %d", in.value);
        } else {
            if ((_square != Up) && (_symbol != Up) && (in.code == PalmPreKeyboard::Key_C)) {
                // screen capture (SQUARE + SYMBOL + C)
                if (in.value == 1) {
                    // initial key down events only
                    uchar* pixels = _screen->base();
                    int pixelCount = _screen->totalSize();

                    QByteArray pixelData = QByteArray(pixelCount, 0);

                    if (_showDebugInfo)
                        qDebug("screenshot size: %d", pixelCount);

                    uchar* pixelBytes = (uchar*)pixelData.data();
                    for(int i=0;i<pixelCount;i++)
                        pixelBytes[i] = pixels[i];

                    QImage image = QImage((uchar*)pixelData.constData(), _screen->width(), _screen->height(), QImage::Format_ARGB32);

                    image.save(QString("/media/internal/screenshot_%1.jpg").arg((int)time(NULL)), "JPEG");
                }
            } else {
                int key;
                int unicode = 0;

                if (_square != Up)
                    key = keymap()->value(in.code | PalmPreKeyboard::Key_SquareMask);
                else
                    key = keymap()->value(in.code);

                if ((key >= Qt::Key_A) && (key <= Qt::Key_Z)) {
                    // alpha key
                    if (_shift != Up)
                        unicode = key;
                    else
                        unicode = key + 'a' - 'A';
                } else if (_square != Up) {
                    unicode = keymap()->value(in.code | PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask);
                } else {
                    unicode = keymap()->value(in.code | PalmPreKeyboard::Key_UnicodeMask);
                }

                if (_showDebugInfo)
                    qDebug("unicode %d, key %d, shift %d", unicode, key, _shift ? 1 : 0);

                _keyboardHandler->processKeyEvent(unicode, key, (_shift != Up) ? Qt::ShiftModifier : Qt::NoModifier, (in.value != 0), (in.value == 2));
            }

            if (_shift == Down)
                _shift = Up;
            if (_square == Down)
                _square = Up;
        }
    }

    _sn->setEnabled(true);
}


void PalmPreKeyboard_private::sdlKeyboardEvent(int type, int sym, int mod)
{

    int key;
    int unicode = 0;

    key = keymapsdl()->value(sym);
    if(key==0)
        return;
    unicode = key;

    // By default the SDL->Qt mapping gives capital letters. Change this if no modifier is pressed
    if((key >= Qt::Key_A) && (key <= Qt::Key_Z))
    {
        if((mod&KMOD_LSHIFT) == 0)
            unicode = key + 'a' - 'A';
    }





    //_keyboardHandler->processKeyEvent(unicode, key, (_shift != Up) ? Qt::ShiftModifier : Qt::NoModifier, (in.value != 0), (in.value == 2));
    Qt::KeyboardModifiers qmod;
    qmod = Qt::NoModifier;
    /*qmod |= (mod==KMOD_LSHIFT)?Qt::ShiftModifier:Qt::NoModifier;
    qmod |= (mod==KMOD_RCTRL)?Qt::ControlModifier:Qt::NoModifier;             // sym key
    qmod |= (mod==KMOD_RALT)?Qt::AltModifier:Qt::NoModifier;             // square key*/
    int qpress = type==SDL_KEYDOWN;

    //printf("KEY %03d %03d %03d   ->   %03d %04d %04d %04X\n",type,sym,mod, qpress, key,unicode,(int)qmod);

    _keyboardHandler->processKeyEvent(unicode, key, qmod, qpress,false);

}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


KeyMap::KeyMap()
{
    insert(PalmPreKeyboard::Key_AtSign,    Qt::Key_At);
    insert(PalmPreKeyboard::Key_Backspace, Qt::Key_Backspace);
    insert(PalmPreKeyboard::Key_Q,         Qt::Key_Q);
    insert(PalmPreKeyboard::Key_W,         Qt::Key_W);
    insert(PalmPreKeyboard::Key_E,         Qt::Key_E);
    insert(PalmPreKeyboard::Key_R,         Qt::Key_R);
    insert(PalmPreKeyboard::Key_T,         Qt::Key_T);
    insert(PalmPreKeyboard::Key_Y,         Qt::Key_Y);
    insert(PalmPreKeyboard::Key_U,         Qt::Key_U);
    insert(PalmPreKeyboard::Key_I,         Qt::Key_I);
    insert(PalmPreKeyboard::Key_O,         Qt::Key_O);
    insert(PalmPreKeyboard::Key_P,         Qt::Key_P);
    insert(PalmPreKeyboard::Key_Return,    Qt::Key_Enter);
    insert(PalmPreKeyboard::Key_A,         Qt::Key_A);
    insert(PalmPreKeyboard::Key_S,         Qt::Key_S);
    insert(PalmPreKeyboard::Key_D,         Qt::Key_D);
    insert(PalmPreKeyboard::Key_F,         Qt::Key_F);
    insert(PalmPreKeyboard::Key_G,         Qt::Key_G);
    insert(PalmPreKeyboard::Key_H,         Qt::Key_H);
    insert(PalmPreKeyboard::Key_J,         Qt::Key_J);
    insert(PalmPreKeyboard::Key_K,         Qt::Key_K);
    insert(PalmPreKeyboard::Key_L,         Qt::Key_L);
    insert(PalmPreKeyboard::Key_Z,         Qt::Key_Z);
    insert(PalmPreKeyboard::Key_X,         Qt::Key_X);
    insert(PalmPreKeyboard::Key_C,         Qt::Key_C);
    insert(PalmPreKeyboard::Key_V,         Qt::Key_V);
    insert(PalmPreKeyboard::Key_B,         Qt::Key_B);
    insert(PalmPreKeyboard::Key_N,         Qt::Key_N);
    insert(PalmPreKeyboard::Key_M,         Qt::Key_M);
    insert(PalmPreKeyboard::Key_Comma,     Qt::Key_Comma);
    insert(PalmPreKeyboard::Key_Period,    Qt::Key_Period);
    insert(PalmPreKeyboard::Key_Space,     Qt::Key_Space);

    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_AtSign,    Qt::Key_0);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Backspace, Qt::Key_Backspace);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Q,         Qt::Key_Slash);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_W,         Qt::Key_Plus);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_E,         Qt::Key_1);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_R,         Qt::Key_2);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_T,         Qt::Key_3);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Y,         Qt::Key_ParenLeft);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_U,         Qt::Key_ParenRight);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_I,         Qt::Key_Percent);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_O,         Qt::Key_QuoteDbl);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_P,         Qt::Key_Equal);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Return,    Qt::Key_Enter);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_A,         Qt::Key_Ampersand);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_S,         Qt::Key_Minus);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_D,         Qt::Key_4);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_F,         Qt::Key_5);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_G,         Qt::Key_6);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_H,         Qt::Key_Dollar);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_J,         Qt::Key_Exclam);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_K,         Qt::Key_Colon);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_L,         Qt::Key_Apostrophe);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Z,         Qt::Key_Asterisk);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_X,         Qt::Key_7);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_C,         Qt::Key_8);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_V,         Qt::Key_9);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_B,         Qt::Key_NumberSign);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_N,         Qt::Key_Question);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_M,         Qt::Key_Semicolon);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Comma,     Qt::Key_Underscore);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Period,    Qt::Key_Period);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Space,     Qt::Key_Space);

    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_AtSign,   (Qt::Key)'@');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Comma,    (Qt::Key)',');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Period,   (Qt::Key)'.');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Space,    (Qt::Key)' ');

    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_AtSign, (Qt::Key)'0');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Q,      (Qt::Key)'/');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_W,      (Qt::Key)'+');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_E,      (Qt::Key)'1');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_R,      (Qt::Key)'2');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_T,      (Qt::Key)'3');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Y,      (Qt::Key)'(');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_U,      (Qt::Key)')');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_I,      (Qt::Key)'%');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_O,      (Qt::Key)'\"');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_P,      (Qt::Key)'=');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_A,      (Qt::Key)'&');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_S,      (Qt::Key)'-');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_D,      (Qt::Key)'4');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_F,      (Qt::Key)'5');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_G,      (Qt::Key)'6');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_H,      (Qt::Key)'$');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_J,      (Qt::Key)'!');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_K,      (Qt::Key)':');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_L,      (Qt::Key)'\'');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Z,      (Qt::Key)'*');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_X,      (Qt::Key)'7');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_C,      (Qt::Key)'8');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_V,      (Qt::Key)'9');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_B,      (Qt::Key)'#');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_N,      (Qt::Key)'?');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_M,      (Qt::Key)';');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Comma,  (Qt::Key)'_');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Period, (Qt::Key)'.');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Space,  (Qt::Key)' ');
}


KeyMapSDL::KeyMapSDL()
{
    // SDL deliver numbers with square+num - nothing to do
    insert(SDLK_0, Qt::Key_0);
    insert(SDLK_1, Qt::Key_1);
    insert(SDLK_2, Qt::Key_2);
    insert(SDLK_3, Qt::Key_3);
    insert(SDLK_4, Qt::Key_4);
    insert(SDLK_5, Qt::Key_5);
    insert(SDLK_6, Qt::Key_6);
    insert(SDLK_7, Qt::Key_7);
    insert(SDLK_8, Qt::Key_8);
    insert(SDLK_9, Qt::Key_9);
    // Straight
    insert(SDLK_a, Qt::Key_A);
    insert(SDLK_b, Qt::Key_B);
    insert(SDLK_c, Qt::Key_C);
    insert(SDLK_d, Qt::Key_D);
    insert(SDLK_e, Qt::Key_E);
    insert(SDLK_f, Qt::Key_F);
    insert(SDLK_g, Qt::Key_G);
    insert(SDLK_h, Qt::Key_H);
    insert(SDLK_i, Qt::Key_I);
    insert(SDLK_j, Qt::Key_J);
    insert(SDLK_k, Qt::Key_K);
    insert(SDLK_l, Qt::Key_L);
    insert(SDLK_m, Qt::Key_M);
    insert(SDLK_n, Qt::Key_N);
    insert(SDLK_o, Qt::Key_O);
    insert(SDLK_p, Qt::Key_P);
    insert(SDLK_q, Qt::Key_Q);
    insert(SDLK_r, Qt::Key_R);
    insert(SDLK_s, Qt::Key_S);
    insert(SDLK_t, Qt::Key_T);
    insert(SDLK_u, Qt::Key_U);
    insert(SDLK_v, Qt::Key_V);
    insert(SDLK_w, Qt::Key_W);
    insert(SDLK_x, Qt::Key_X);
    insert(SDLK_y, Qt::Key_Y);
    insert(SDLK_z, Qt::Key_Z);



    insert(SDLK_BACKSPACE, Qt::Key_Backspace);
    insert(SDLK_TAB, Qt::Key_Tab);
    insert(SDLK_RETURN, Qt::Key_Enter);
    insert(SDLK_ESCAPE, Qt::Key_Escape);
    insert(SDLK_SPACE, Qt::Key_Space);
    insert(SDLK_EXCLAIM, Qt::Key_Exclam);
    insert(SDLK_QUOTEDBL, Qt::Key_QuoteDbl);
    insert(37, Qt::Key_Percent);
    insert(SDLK_DOLLAR, Qt::Key_Dollar);
    insert(SDLK_AMPERSAND, Qt::Key_Ampersand);
    insert(SDLK_QUOTE, Qt::Key_QuoteLeft);
    insert(SDLK_LEFTPAREN, Qt::Key_ParenLeft);
    insert(SDLK_RIGHTPAREN, Qt::Key_ParenRight);
    insert(SDLK_ASTERISK, Qt::Key_Asterisk);
    insert(SDLK_PLUS, Qt::Key_Plus);
    insert(SDLK_COMMA, Qt::Key_Comma);
    insert(SDLK_MINUS, Qt::Key_Minus);
    insert(SDLK_PERIOD, Qt::Key_Period);
    insert(SDLK_SLASH, Qt::Key_Slash);
    insert(SDLK_COLON, Qt::Key_Colon);
    insert(SDLK_SEMICOLON, Qt::Key_Semicolon);
    insert(SDLK_LESS, Qt::Key_Less);
    insert(SDLK_EQUALS, Qt::Key_Equal);
    insert(SDLK_GREATER, Qt::Key_Greater);
    insert(SDLK_QUESTION, Qt::Key_Question);
    insert(SDLK_AT, Qt::Key_At);
    insert(SDLK_LEFTBRACKET, Qt::Key_BracketLeft);
    insert(SDLK_BACKSLASH, Qt::Key_Backslash);
    insert(SDLK_RIGHTBRACKET, Qt::Key_BracketRight);
    insert(SDLK_UNDERSCORE, Qt::Key_Underscore);

    //insert(SDLK_CARET
    //insert(SDLK_BACKQUOTE
    //insert(SDLK_CLEAR, Qt::Key_Delete
    //insert(SDLK_PAUSE
    //insert(SDLK_HASH, Qt::Key_



            /*{ SDLK_1, BUTTON_1},
            { SDLK_2, BUTTON_2},
            { SDLK_3, BUTTON_3},
            { SDLK_4, BUTTON_4},
            { SDLK_5, BUTTON_5},
            { SDLK_6, BUTTON_6},
            { SDLK_7, BUTTON_7},
            { SDLK_8, BUTTON_8},
            { SDLK_9, BUTTON_9},
            { SDLK_KP0, BUTTON_0},
            { SDLK_KP1, BUTTON_1},
            { SDLK_KP2, BUTTON_2},
            { SDLK_KP3, BUTTON_3},
            { SDLK_KP4, BUTTON_4},
            { SDLK_KP5, BUTTON_5},
            { SDLK_KP6, BUTTON_6},
            { SDLK_KP7, BUTTON_7},
            { SDLK_KP8, BUTTON_8},
            { SDLK_KP9, BUTTON_9},*/
            // Letters, space, return, backspace, delete, period, arrows
            /*{ SDLK_a, BUTTON_A},
            { SDLK_b, BUTTON_B},
            { SDLK_c, BUTTON_C},
            { SDLK_d, BUTTON_D},
            { SDLK_e, BUTTON_E},
            { SDLK_f, BUTTON_F},
            { SDLK_g, BUTTON_MTH},
            { SDLK_h, BUTTON_PRG},
            { SDLK_i, BUTTON_CST},
            { SDLK_j, BUTTON_VAR},
            { SDLK_k, BUTTON_UP},
            { SDLK_UP, BUTTON_UP},
            { SDLK_l, BUTTON_NXT},
            { SDLK_m, BUTTON_COLON},
            { SDLK_n, BUTTON_STO},
            { SDLK_o, BUTTON_EVAL},
            { SDLK_p, BUTTON_LEFT},
            { SDLK_LEFT, BUTTON_LEFT},
            { SDLK_q, BUTTON_DOWN},
            { SDLK_DOWN, BUTTON_DOWN},
            { SDLK_r, BUTTON_RIGHT},
            { SDLK_RIGHT, BUTTON_RIGHT},
            { SDLK_s, BUTTON_SIN},
            { SDLK_t, BUTTON_COS},
            { SDLK_u, BUTTON_TAN},
            { SDLK_v, BUTTON_SQRT},
            { SDLK_w, BUTTON_POWER},
            { SDLK_x, BUTTON_INV},
            { SDLK_y, BUTTON_NEG},
            { SDLK_z, BUTTON_EEX},
            { SDLK_SPACE, BUTTON_SPC},
            { SDLK_RETURN, BUTTON_ENTER},
            { SDLK_KP_ENTER, BUTTON_ENTER},
            { SDLK_BACKSPACE, BUTTON_BS},
            { SDLK_DELETE, BUTTON_DEL},
            { SDLK_PERIOD, BUTTON_PERIOD},
            { SDLK_KP_PERIOD, BUTTON_PERIOD},
            // Math operators
            { SDLK_PLUS, BUTTON_PLUS},
            { SDLK_KP_PLUS, BUTTON_PLUS},
            { SDLK_MINUS, BUTTON_MINUS},
            { SDLK_KP_MINUS, BUTTON_MINUS},
            { SDLK_ASTERISK, BUTTON_MUL},
            { SDLK_KP_MULTIPLY, BUTTON_MUL},
            { SDLK_SLASH, BUTTON_DIV},
            { SDLK_KP_DIVIDE, BUTTON_DIV},
            // on, shift, alpha
            { SDLK_ESCAPE, BUTTON_ON},
            { SDLK_LSHIFT, BUTTON_SHL},
            { SDLK_RSHIFT, BUTTON_SHL},
            { SDLK_LCTRL, BUTTON_SHR},
            { SDLK_RCTRL, BUTTON_SHR},
            { SDLK_LALT, BUTTON_ALPHA},
            { SDLK_RALT, BUTTON_ALPHA},

    insert(SDLK_,    Qt::Key_At);
    insert(PalmPreKeyboard::Key_Backspace, Qt::Key_Backspace);
    insert(PalmPreKeyboard::Key_Q,         Qt::Key_Q);
    insert(PalmPreKeyboard::Key_W,         Qt::Key_W);
    insert(PalmPreKeyboard::Key_E,         Qt::Key_E);
    insert(PalmPreKeyboard::Key_R,         Qt::Key_R);
    insert(PalmPreKeyboard::Key_T,         Qt::Key_T);
    insert(PalmPreKeyboard::Key_Y,         Qt::Key_Y);
    insert(PalmPreKeyboard::Key_U,         Qt::Key_U);
    insert(PalmPreKeyboard::Key_I,         Qt::Key_I);
    insert(PalmPreKeyboard::Key_O,         Qt::Key_O);
    insert(PalmPreKeyboard::Key_P,         Qt::Key_P);
    insert(PalmPreKeyboard::Key_Return,    Qt::Key_Enter);
    insert(PalmPreKeyboard::Key_A,         Qt::Key_A);
    insert(PalmPreKeyboard::Key_S,         Qt::Key_S);
    insert(PalmPreKeyboard::Key_D,         Qt::Key_D);
    insert(PalmPreKeyboard::Key_F,         Qt::Key_F);
    insert(PalmPreKeyboard::Key_G,         Qt::Key_G);
    insert(PalmPreKeyboard::Key_H,         Qt::Key_H);
    insert(PalmPreKeyboard::Key_J,         Qt::Key_J);
    insert(PalmPreKeyboard::Key_K,         Qt::Key_K);
    insert(PalmPreKeyboard::Key_L,         Qt::Key_L);
    insert(PalmPreKeyboard::Key_Z,         Qt::Key_Z);
    insert(PalmPreKeyboard::Key_X,         Qt::Key_X);
    insert(PalmPreKeyboard::Key_C,         Qt::Key_C);
    insert(PalmPreKeyboard::Key_V,         Qt::Key_V);
    insert(PalmPreKeyboard::Key_B,         Qt::Key_B);
    insert(PalmPreKeyboard::Key_N,         Qt::Key_N);
    insert(PalmPreKeyboard::Key_M,         Qt::Key_M);
    insert(PalmPreKeyboard::Key_Comma,     Qt::Key_Comma);
    insert(PalmPreKeyboard::Key_Period,    Qt::Key_Period);
    insert(PalmPreKeyboard::Key_Space,     Qt::Key_Space);*/
/*
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_AtSign,    Qt::Key_0);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Backspace, Qt::Key_Backspace);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Q,         Qt::Key_Slash);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_W,         Qt::Key_Plus);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_E,         Qt::Key_1);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_R,         Qt::Key_2);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_T,         Qt::Key_3);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Y,         Qt::Key_ParenLeft);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_U,         Qt::Key_ParenRight);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_I,         Qt::Key_Percent);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_O,         Qt::Key_QuoteDbl);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_P,         Qt::Key_Equal);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Return,    Qt::Key_Enter);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_A,         Qt::Key_Ampersand);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_S,         Qt::Key_Minus);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_D,         Qt::Key_4);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_F,         Qt::Key_5);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_G,         Qt::Key_6);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_H,         Qt::Key_Dollar);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_J,         Qt::Key_Exclam);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_K,         Qt::Key_Colon);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_L,         Qt::Key_Apostrophe);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Z,         Qt::Key_Asterisk);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_X,         Qt::Key_7);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_C,         Qt::Key_8);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_V,         Qt::Key_9);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_B,         Qt::Key_NumberSign);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_N,         Qt::Key_Question);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_M,         Qt::Key_Semicolon);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Comma,     Qt::Key_Underscore);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Period,    Qt::Key_Period);
    insert(PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Space,     Qt::Key_Space);

    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_AtSign,   (Qt::Key)'@');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Comma,    (Qt::Key)',');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Period,   (Qt::Key)'.');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_Space,    (Qt::Key)' ');

    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_AtSign, (Qt::Key)'0');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Q,      (Qt::Key)'/');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_W,      (Qt::Key)'+');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_E,      (Qt::Key)'1');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_R,      (Qt::Key)'2');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_T,      (Qt::Key)'3');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Y,      (Qt::Key)'(');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_U,      (Qt::Key)')');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_I,      (Qt::Key)'%');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_O,      (Qt::Key)'\"');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_P,      (Qt::Key)'=');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_A,      (Qt::Key)'&');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_S,      (Qt::Key)'-');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_D,      (Qt::Key)'4');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_F,      (Qt::Key)'5');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_G,      (Qt::Key)'6');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_H,      (Qt::Key)'$');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_J,      (Qt::Key)'!');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_K,      (Qt::Key)':');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_L,      (Qt::Key)'\'');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Z,      (Qt::Key)'*');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_X,      (Qt::Key)'7');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_C,      (Qt::Key)'8');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_V,      (Qt::Key)'9');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_B,      (Qt::Key)'#');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_N,      (Qt::Key)'?');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_M,      (Qt::Key)';');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Comma,  (Qt::Key)'_');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Period, (Qt::Key)'.');
    insert(PalmPreKeyboard::Key_UnicodeMask | PalmPreKeyboard::Key_SquareMask | PalmPreKeyboard::Key_Space,  (Qt::Key)' ');*/
}

