//
// PImageLoaderJpeg.hh for pekwm
// Copyright (C) 2005-2021 Claes Nästén <pekdon@gmail.com>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#pragma once

#include "config.h"

#ifdef HAVE_IMAGE_JPEG

#include "pekwm.hh"

#include <cstdio>

/**
 * Jpeg Loader class.
 */
namespace PImageLoaderJpeg
{
    const char *getExt(void);
    uchar* load(const std::string &file, size_t &width, size_t &height,
                bool &use_alpha);
}

#endif // HAVE_IMAGE_JPEG
