/*
   Widgets for the Midnight Commander

   Copyright (C) 1994-2016
   Free Software Foundation, Inc.

   Authors:
   Human beings.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file mouse.c
 *  \brief Header: High-level mouse API
 */

#include <config.h>

#include "lib/global.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static int last_buttons_down;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Constructs a mouse event structure. The is the high-level type used
 * with "easy callbacks".
 */
static void
init_mouse_event (mouse_event_t * event, mouse_msg_t msg, const Gpm_Event * global_gpm,
                  const Widget * w)
{
    event->msg = msg;
    event->x = global_gpm->x - w->x - 1;        /* '-1' because Gpm_Event is 1-based. */
    event->y = global_gpm->y - w->y - 1;
    event->count = global_gpm->type & (GPM_SINGLE | GPM_DOUBLE | GPM_TRIPLE);
    event->buttons = global_gpm->buttons;
    event->result.abort = FALSE;
    event->result.repeat = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * This is the low-level mouse handler that's in use when you install
 * an "easy callback".
 *
 * It receives a Gpm_Event event and translates it into a higher level
 * protocol with which it feeds your "easy callback".
 *
 * Tip: for details on the C mouse API, see MC's lib/tty/mouse.h,
 * or GPM's excellent 'info' manual:
 *
 *    http://www.fifi.org/cgi-bin/info2www?(gpm)Event+Types
 */
static int
easy_mouse_translator (Gpm_Event * event, void *data)
{
    Widget *w = WIDGET (data);

    gboolean in_widget;
    gboolean run_click = FALSE;
    mouse_msg_t msg = 0;

    in_widget = mouse_global_in_widget (event, w);

    /*
     * Very special widgets may want to control area outside their bounds.
     * For such widgets you will have to turn on the 'forced_capture' flag.
     * You'll also need, in your mouse handler, to inform the system of
     * events you want to pass on by setting 'event->result.abort' to TRUE.
     */
    if (w->Mouse.forced_capture)
        in_widget = TRUE;

    if ((event->type & GPM_DOWN) != 0)
    {
        if (in_widget)
        {
            if ((event->buttons & GPM_B_UP) != 0)
                msg = MSG_MOUSE_SCROLL_UP;
            else if ((event->buttons & GPM_B_DOWN) != 0)
                msg = MSG_MOUSE_SCROLL_DOWN;
            else
            {
                /* Handle normal buttons: anything but the mouse wheel's.
                 *
                 * (Note that turning on capturing for the mouse wheel
                 * buttons doesn't make sense as they don't generate a
                 * mouse_up event, which means we'd never get uncaptured.)
                 */
                w->Mouse.capture = TRUE;
                msg = MSG_MOUSE_DOWN;

                last_buttons_down = event->buttons;
            }
        }
    }
    else if ((event->type & GPM_UP) != 0)
    {
        /* We trigger the mouse_up event even when !in_widget. That's
         * because, for example, a paint application should stop drawing
         * lines when the button is released even outside the canvas. */
        if (w->Mouse.capture)
        {
            w->Mouse.capture = FALSE;
            msg = MSG_MOUSE_UP;

            if (in_widget)
                run_click = TRUE;

            /*
             * When using xterm, event->buttons reports the buttons' state
             * after the event occurred (meaning that event->buttons is zero,
             * because the mouse button is now released). When using GPM,
             * however, that field reports the button(s) that was released.
             *
             * The following makes xterm behave effectively like GPM:
             */
            if (event->buttons == 0)
                event->buttons = last_buttons_down;
        }
    }
    else if ((event->type & GPM_DRAG) != 0)
    {
        if (w->Mouse.capture)
            msg = MSG_MOUSE_DRAG;
    }
    else if ((event->type & GPM_MOVE) != 0)
    {
        if (in_widget)
            msg = MSG_MOUSE_MOVE;
    }

    if (msg != 0)
    {
        mouse_event_t local;

        init_mouse_event (&local, msg, event, w);

        w->Mouse.callback (w, msg, &local);
        if (run_click)
            w->Mouse.callback (w, MSG_MOUSE_CLICK, &local);

        if (!local.result.abort)
            return local.result.repeat ? MOU_REPEAT : MOU_NORMAL;
    }

    return MOU_UNHANDLED;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Use this to install an "easy mouse callback".
 *
 * (The mouse callback widget_init() accepts is a low-level one; you can
 * pass NULL to it. In the future we'll probably do the opposite: have
 * widget_init() accept the "easy" callback.)
 */
void
set_easy_mouse_callback (Widget * w, easy_mouse_callback cb)
{
    w->mouse = easy_mouse_translator;
    w->Mouse.callback = cb;
}

/* --------------------------------------------------------------------------------------------- */