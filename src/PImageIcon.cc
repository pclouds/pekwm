//
// PImage.hh for pekwm
// Copyright © 2007-2009 Claes Nästén <me@pekdon.net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include <iostream>

#include "Atoms.hh"
#include "PImageIcon.hh"

using std::cerr;
using std::endl;

//! @brief PImageIcon constructor.
//! @param dpy Display to load icon from.
PImageIcon::PImageIcon(Display *dpy)
  : PImage(dpy)
{
    _type = IMAGE_TYPE_SCALED;
    _has_alpha = true;
    _use_alpha = true;
}

//! @brief PImage destructor.
PImageIcon::~PImageIcon(void)
{
}

//! @brief Load icon from window (if atom is set)
bool
PImageIcon::loadFromWindow(Window win)
{
    bool status = false;

    Atom icon = Atoms::getAtom(NET_WM_ICON);
    uchar *udata = 0;
    long expected = 2;

    // Start reading icon size
    if (AtomUtil::getProperty(win, icon, XA_CARDINAL, expected, &udata)) {
        long *data = reinterpret_cast<long*>(udata);

        uint width = data[0];
        uint height = data[1];

        XFree(udata);

        // Read the actual icon
        expected += width * height;
        if (AtomUtil::getProperty(win, icon, XA_CARDINAL, expected, &udata)) {
            status = true;

            data = reinterpret_cast<long*>(udata);

            _data = new uchar[width * height * 4];
            _width = width;
            _height = height;

            // Assign data, source data is ARGB one pixel per 32bit and
            // destination is RGBA in 4x8bit.
            uchar *p = _data;
            int pixel;
            for (int i = 2; i < expected; i += 1) {
                pixel = data[i]; // in case 64bit system drop the unneeded bits
                *p++ = pixel >> 16 & 0xff;
                *p++ = pixel >> 8 & 0xff;
                *p++ = pixel & 0xff;
                *p++ = pixel >> 24 & 0xff;
            }

            _pixmap = createPixmap(_data, _width, _height);
            _mask =  createMask(_data, _width, _height);

            XFree(udata);
        }
    }

    return status;
}
