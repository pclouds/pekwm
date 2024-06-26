//
// StatusWindow.cc for pekwm
// Copyright (C) 2004-2005 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"

#include "PWinObj.hh"
#include "PDecor.hh"
#include "StatusWindow.hh"
#include "PScreen.hh"
#include "PTexture.hh"
#include "PixmapHandler.hh"
#include "ScreenResources.hh"
#include "Theme.hh"
#include "Workspaces.hh"

#ifdef DEBUG
#include <iostream>
using std::cerr;
using std::endl;
#endif // DEBUG

StatusWindow *StatusWindow::_instance = NULL;

//! @brief StatusWindow constructor
StatusWindow::StatusWindow(Display *dpy, Theme *theme) :
PDecor(dpy, theme, "STATUSWINDOW"),
_bg(None)
{
	if (_instance != NULL) {
#ifdef DEBUG
		cerr << __FILE__ << "@" << __LINE__ << ": "
				 << "StatusWindow(" << this << ")::StatusWindow()" << endl
				 << " *** _instance allready set: " << _instance << endl;
#endif // DEBUG
	}
	_instance = this;


	// PWinObj attributes
	_type = PWinObj::WO_STATUS;
	_layer = LAYER_NONE; // hack, goes over LAYER_MENU
	_hidden = true; // don't care about it when changing worskpace etc

	XSetWindowAttributes attr;
	attr.event_mask = None;

	_status_wo = new PWinObj(_dpy);
	_status_wo->setWindow(XCreateWindow(_dpy, _window,
												0, 0, 1, 1, 0,
												CopyFromParent, CopyFromParent, CopyFromParent,
												CWEventMask, &attr));

	addChild(_status_wo);
	activateChild(_status_wo);
	_status_wo->mapWindow();

	setTitlebar(STATE_UNSET);
	setFocused(true); // always draw as focused

	Workspaces::instance()->insert(this);
}

//! @brief StatusWindow destructor
StatusWindow::~StatusWindow(void)
{
	Workspaces::instance()->remove(this);

	// remove ourself from the decor manually, no need to reparent and stuff
	_child_list.remove(_status_wo);

	// free resources
	XDestroyWindow(_dpy, _status_wo->getWindow());
	delete _status_wo;

	unloadTheme();

	_instance = NULL;
}

//! @brief Resizes window to fit the text and renders text
//! @param text Text to draw in StatusWindow
//! @param do_center Center the StatusWindow on screen. Defaults to false.
void
StatusWindow::draw(const std::string &text, bool do_center)
{
	uint width, height;
	PFont *font = _theme->getStatusData()->getFont(); // convenience

	width = font->getWidth(text.c_str()) + 10;
	width = width - (width % 10);
	height = font->getHeight()
		+ _theme->getStatusData()->getPad(DIRECTION_UP)
		+ _theme->getStatusData()->getPad(DIRECTION_DOWN);

	if ((width != getChildWidth()) || (height != getChildHeight())) {
		resizeChild(width, height);
		render();
	}

	if (do_center) {
		center();
	}

	font->setColor(_theme->getStatusData()->getColor());
	_status_wo->clear();
	font->draw(_status_wo->getWindow(),
						 (width - font->getWidth(text.c_str())) / 2,
						 _theme->getStatusData()->getPad(DIRECTION_UP),
						 text.c_str());
}

//! @brief Renders and sets background
void
StatusWindow::loadTheme(void)
{
	render();
}

//! @brief Frees theme resources
void
StatusWindow::unloadTheme(void)
{
	ScreenResources::instance()->getPixmapHandler()->returnPixmap(_bg);
}

//! @brief Renders and sets background
void
StatusWindow::render(void)
{
	PixmapHandler *pm = ScreenResources::instance()->getPixmapHandler();
	pm->returnPixmap(_bg);
	_bg = pm->getPixmap(_status_wo->getWidth(), _status_wo->getHeight(),
											PScreen::instance()->getDepth(), false);

	_theme->getStatusData()->getTexture()->render(_bg, 0, 0, _status_wo->getWidth(), _status_wo->getHeight());

	_status_wo->setBackgroundPixmap(_bg);
	_status_wo->clear();
}

//! @brief Centers the StatusWindow on the current head/screen
void
StatusWindow::center(void)
{
	Geometry head;
	PScreen::instance()->getHeadInfo(PScreen::instance()->getCurrHead(), head);

	move((head.width - _gm.width) / 2, (head.height - _gm.height) / 2);
}
