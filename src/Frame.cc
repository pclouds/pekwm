//
// Frame.cc for pekwm
// Copyright (C) 2002-2005 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"
#include "PWinObj.hh"
#include "PDecor.hh"
#include "Frame.hh"

#include "PScreen.hh"
#include "Config.hh"
#include "ActionHandler.hh"
#include "AutoProperties.hh"
#include "Client.hh"
#include "Viewport.hh"
#include "ScreenResources.hh"
#include "StatusWindow.hh"
#include "Workspaces.hh"
#include "WindowManager.hh"
#include "KeyGrabber.hh"
#ifdef HARBOUR
#include "Harbour.hh"
#endif // HARBOUR

#ifdef MENUS
#include "PMenu.hh"
#endif //MENUS

#include "Theme.hh"

#include <algorithm>
#include <functional>
#include <cstdio> // for snprintf

extern "C" {
#include <X11/Xatom.h>
}

#ifdef DEBUG
#include <iostream>
using std::cerr;
using std::endl;
#endif // DEBUG
using std::string;
using std::list;
using std::vector;
using std::mem_fun;
using std::find;

//! @brief Frame constructor
Frame::Frame(WindowManager *wm, Client *client, AutoProperty *ap) :
PDecor(wm->getScreen()->getDpy(), wm->getTheme(),
(wm->getAutoProperties()->findDecorProperty(client->getClassHint()) == NULL)
			 ? PDecor::DEFAULT_DECOR_NAME
			 : wm->getAutoProperties()->findDecorProperty(client->getClassHint())->getName()),
_wm(wm),
_id(0), _client(NULL), _class_hint(NULL),
_old_decor_state(0)
{
	// setup basic pointers
	_scr = _wm->getScreen();

	// PWinObj attributes
	_type = WO_FRAME;

	// PDecor attributes
	_decor_cfg_child_move_overloaded = true;
	_decor_cfg_bpr_replay_pointer = true;
	_decor_cfg_bpr_al_title = MOUSE_ACTION_LIST_TITLE_FRAME;
	_decor_cfg_bpr_al_child = MOUSE_ACTION_LIST_CHILD_FRAME;

	// grab buttons so that we can reply them
  for (uint i = 0; i < BUTTON_NO; ++i) {
    XGrabButton(_dpy, i, AnyModifier, _window,
                True, ButtonPressMask|ButtonReleaseMask,
                GrabModeSync, GrabModeAsync, None, None);
  }

	// get unique id of the frame, if the client didn't have an id
	if (_wm->isStartup() == false) {
		long id;

		if (AtomUtil::getLong(client->getWindow(),
													PekwmAtoms::instance()->getAtom(PEKWM_FRAME_ID), id)) {
			_id = id;
		}

	} else {
		_id = _wm->findUniqueFrameId();
	}

	// get the clients class_hint
	_class_hint = new ClassHint();
	*_class_hint = *client->getClassHint();

	_scr->grabServer();

	// We don't send any ConfigurRequests during setup, we send one when we
	// are finished to minimize traffic and confusion
	client->setConfigureRequestLock(true);

	// Before setting position and size up we make sure the decor state
	// of the client match the decore state of the framewidget
 if (!client->hasBorder())
    setBorder(STATE_UNSET);
 if (!client->hasTitlebar())
    setTitlebar(STATE_UNSET);

	// We first get the size of the window as it will be needed when placing
	// the window with the help of WinGravity.
	resizeChild(client->getWidth(), client->getHeight());

	// setup position
	bool place = false;
	if (client->isViewable() || client->isPlaced()) {
		moveChild(client->getX(), client->getY());
	} else if (client->setPUPosition()) {
		calcGravityPosition(client->getXSizeHints()->win_gravity,
												client->getX(), client->getY(), _gm.x, _gm.y);
		move(_gm.x, _gm.y);
	} else {
		place = Config::instance()->isPlaceNew();
	}

	// override both position and size with autoproperties
	if (ap != NULL) {
		if (ap->isMask(AP_FRAME_GEOMETRY|AP_CLIENT_GEOMETRY)) {
			setupAPGeometry(client, ap);

			if (ap->frame_gm_mask&(XValue|YValue) ||
					ap->client_gm_mask&(XValue|YValue)) {
				place = false;
			}
		}

		if (ap->isMask(AP_VIEWPORT)) {
			Viewport *vp = Workspaces::instance()->getViewport(client->getWorkspace());
			if (vp != NULL) {
				// get position on that viewport then add viewport base position
				if (ap->viewport_col < vp->getCols()) {
					_gm.x -= vp->getCol(this) * PScreen::instance()->getWidth();
					_gm.x += ap->viewport_col * PScreen::instance()->getWidth();
				}
				if (ap->viewport_row < vp->getRows()) {
					_gm.y -= vp->getRow(this) * PScreen::instance()->getHeight();
					_gm.y += ap->viewport_row * PScreen::instance()->getHeight();
				}
				move(_gm.x, _gm.y);

				place = false;
			}
		}

		if (ap->isMask(AP_PLACE_NEW)) {
			place = ap->place_new;
		}
	}

	// still need a position?
	if (place) {
		Workspaces::instance()->placeWo(this, client->getTransientWindow());
	}

	_old_gm = _gm;
	_old_decor_state = client->getDecorState();

	_scr->ungrabServer(true); // ungrab and sync

	// needs to be done before the workspace insert
	setLayer(client->getLayer());

	// I add these to the list before I insert the client into the frame to
	// be able to skip an extra updateClientList
	_wm->addToFrameList(this);
	Workspaces::instance()->insert(this);

	// now insert the client in the frame we created, do not give it focus.
	addChild(client);
	activateChild(client);

	// set the window states, shaded, maximized...
	getState(client);

	// FIXME: This should be checked even if viewports are enabled.
	if ((Config::instance()->getViewportCols() < 2) &&
			(Config::instance()->getViewportRows() < 2)) {
		if (_client->hasStrut() == false) {
			if (fixGeometry()) {
					move(_gm.x, _gm.y);
					resize(_gm.width, _gm.height);
				}
			}
	} else {
		Viewport *vp = Workspaces::instance()->getViewport(_client->getWorkspace());
		if (vp != NULL) {
			vp->makeWOInsideVirtual(this);
		}
	}

	client->setConfigureRequestLock(false);
	client->configureRequestSend();

	// figure out if we should be hidden or not, do not read autoprops
	setWorkspace(_client->getWorkspace());

	_wo_list.push_back(this);
}

//! @brief Frame destructor
Frame::~Frame(void)
{
	// remove from lists
	_wo_list.remove(this);
	Workspaces::instance()->remove(this);
	_wm->removeFromFrameList(this);

	if (_class_hint) {
		delete _class_hint;
	}

	Workspaces::instance()->updateClientStackingList(true, true);
}

// START - PWinObj interface.

//! @brief Iconifies the Frame.
void
Frame::iconify(void)
{
	if (_iconified)
		return;
	_iconified = true;

	unmapWindow();
}

//! @brief Toggles the Frame's sticky state
void
Frame::stick(void)
{
	_client->setSticky(_sticky); // FIXME: FRAME
	_client->stick();

	_sticky = !_sticky;

	// make sure it's visible/hidden
	setWorkspace(Workspaces::instance()->getActive());
}

// event handlers

//! @brief
ActionEvent*
Frame::handleMotionEvent(XMotionEvent *ev)
{
	// This is true when we have a title button pressed and then we don't want
	// to be able to drag windows around, therefore we ignore the event
	if (_button != NULL) {
		return NULL;
	}

	ActionEvent *ae = NULL;
	uint button = _scr->getButtonFromState(ev->state);

	if (ev->window == getTitleWindow()) {
		ae = ActionHandler::findMouseAction(button, ev->state, MOUSE_EVENT_MOTION,
																				Config::instance()->getMouseActionList(MOUSE_ACTION_LIST_TITLE_FRAME));
	} else if (ev->window == _client->getWindow()) {
		ae = ActionHandler::findMouseAction(button, ev->state, MOUSE_EVENT_MOTION,
																				Config::instance()->getMouseActionList(MOUSE_ACTION_LIST_CHILD_FRAME));
	} else if (ev->subwindow != None) {
		uint pos = getBorderPosition(ev->subwindow);
		if (pos != BORDER_NO_POS) {
			list<ActionEvent> *bl = Config::instance()->getBorderListFromPosition(pos);
			ae = ActionHandler::findMouseAction(button, ev->state, MOUSE_EVENT_MOTION, bl);
		}
	}

	// check motion threshold
	if ((ae != NULL) && (ae->threshold > 0)) {
		if (!ActionHandler::checkAEThreshold(ev->x_root, ev->y_root,
																			_pointer_x, _pointer_y, ae->threshold)) {
			ae = NULL;
		}
	}

	return ae;
}

//! @brief
ActionEvent*
Frame::handleEnterEvent(XCrossingEvent *ev)
{
	ActionEvent *ae = NULL;
	list<ActionEvent> *al = NULL;

	if (ev->window == getTitleWindow()) {
		al = Config::instance()->getMouseActionList(MOUSE_ACTION_LIST_TITLE_FRAME);

	} else if (ev->subwindow == _client->getWindow()) {
		al = Config::instance()->getMouseActionList(MOUSE_ACTION_LIST_CHILD_FRAME);

	} else {
		uint pos = getBorderPosition(ev->window);
		if (pos != BORDER_NO_POS) {
			al = Config::instance()->getBorderListFromPosition(pos);
		}
	}

	if (al != NULL) {
		ae = ActionHandler::findMouseAction(BUTTON_ANY, ev->state, MOUSE_EVENT_ENTER, al);
	}

	return ae;
}

//! @brief
ActionEvent*
Frame::handleLeaveEvent(XCrossingEvent *ev)
{
	ActionEvent *ae;

	MouseActionListName ln = MOUSE_ACTION_LIST_TITLE_FRAME;
	if (ev->window == _client->getWindow()) {
		ln = MOUSE_ACTION_LIST_CHILD_FRAME;
	}

	ae = ActionHandler::findMouseAction(BUTTON_ANY, ev->state, MOUSE_EVENT_LEAVE,
																			Config::instance()->getMouseActionList(ln));

	return ae;
}

//! @brief
ActionEvent*
Frame::handleMapRequest(XMapRequestEvent *ev)
{
	if ((_client == NULL) || (ev->window != _client->getWindow())) {
		return NULL;
	}

	if (!_sticky && (_workspace != Workspaces::instance()->getActive())) {
#ifdef DEBUG
		cerr << __FILE__ << "@" << __LINE__ << ": "
				 << "Ignoring MapRequest, not on current workspace!" << endl;
#endif // DEBUG
		return NULL;
	}

	mapWindow();

	return NULL;
}

//! @brief
ActionEvent*
Frame::handleUnmapEvent(XUnmapEvent *ev)
{
	list<PWinObj*>::iterator it(_child_list.begin());
	for (; it != _child_list.end(); ++it) {
		if (*(*it) == ev->window) {
			(*it)->handleUnmapEvent(ev);
			break;
		}
	}

	return NULL;
}

// END - PWinObj interface.

#ifdef HAVE_SHAPE
//! @brief
void
Frame::handleShapeEvent(XAnyEvent *ev)
{
	if ((_client == NULL) || (ev->window != _client->getWindow())) {
		return;
	}
	_client->setShaped(setShape(_client->isShaped()));
}
#endif // HAVE_SHAPE

// START - PDecor interface.

//! @brief Adds child to the frame.
void
Frame::addChild (PWinObj *child)
{
  PDecor::addChild (child);
  AtomUtil::setLong (child->getWindow (),
                     PekwmAtoms::instance ()->getAtom (PEKWM_FRAME_ID), _id);
  child->lower ();
}

//! @brief Removes child from the frame.
void
Frame::removeChild (PWinObj *child, bool do_delete)
{
  PDecor::removeChild (child, do_delete);
}

//! @brief Acttivates child in Frame.
void
Frame::activateChild (PWinObj *child)
{
  // sync the frame state with the client only if we already had a client
  if (_client && (_client != child))
    applyState (static_cast<Client*> (child));

  _client = static_cast<Client*> (child);
  PDecor::activateChild (child);

  // setShape uses current active child, so we need to activate the child
  // before setting shape
#ifdef HAVE_SHAPE
  if (PScreen::instance ()->hasExtensionShape ())
    _client->setShaped (setShape (_client->isShaped ()));
#endif // HAVE_SHAPE

  if (_focused)
    child->giveInputFocus ();

  Workspaces::instance ()->updateClientStackingList (true, true);
}

//! @brief
void
Frame::updatedChildOrder(void)
{
	titleClear();

	list<PWinObj*>::iterator it(_child_list.begin());
	for (; it != _child_list.end(); ++it) {
		titleAdd(static_cast<Client*>(*it)->getTitle());
	}

	updatedActiveChild();
}

//! @brief
void
Frame::updatedActiveChild(void)
{
	titleSetActive(0);

	list<PWinObj*>::iterator it(_child_list.begin());
	for (uint i = 0; it != _child_list.end(); ++i, ++it) {
		if (_child == *it) {
			titleSetActive(i);
			break;
		}
	}

	renderTitle();
}

//! @brief
void
Frame::getDecorInfo(char *buf, uint size)
{
	uint width, height;
	if (_client != NULL) {
		calcSizeInCells(width, height);
	} else {
		width = _gm.width;
		height = _gm.height;
	}
	snprintf(buf, size, "%d+%d+%d+%d", width, height,
					 _gm.x + Workspaces::instance()->getActiveViewport()->getX(),
					 _gm.y + Workspaces::instance()->getActiveViewport()->getY());
}

//! @brief
void
Frame::setShaded(StateAction sa)
{
	bool shaded = isShaded();
	PDecor::setShaded(sa);
	if (shaded != isShaded()) {
		_client->setShade(isShaded());
		_client->updateEwmhStates();
	}
}

//! @brief
int
Frame::resizeHorzStep(int diff) const
{
	if (_client == NULL) {
		return diff;
	}

	int diff_ret = 0;
	uint min = _gm.width - getChildWidth();
	if (min == 0) { // borderless windows, we don't want X errors
		min = 1;
	}
	const XSizeHints *hints = _client->getXSizeHints(); // convenience

	// if we have ResizeInc hint set we use it instead of pixel diff
	if (hints->flags&PResizeInc) {
		if (diff > 0) {
			diff_ret = hints->width_inc;
		} else if ((_gm.width - hints->width_inc) >= min) {
			diff_ret = -hints->width_inc;
		}
	} else if ((_gm.width + diff) >= min) {
		diff_ret = diff;
	}

	// check max/min size hints
	if (diff > 0) {
		if ((hints->flags&PMaxSize) &&
				((getChildWidth() + diff) > unsigned(hints->max_width))) {
			diff_ret = _gm.width - hints->max_width + min;
		}
	} else if ((hints->flags&PMinSize) &&
						 ((getChildWidth() + diff) < unsigned(hints->min_width))) {
		diff_ret = _gm.width - hints->min_width + min;
	}

	return diff_ret;
}

//! @brief
int
Frame::resizeVertStep(int diff) const
{
	if (_client == NULL) {
		return diff;
	}

	int diff_ret = 0;
	uint min = _gm.height - getChildHeight();
	if (min == 0) { // borderless windows, we don't want X errors
		min = 1;
	}
	const XSizeHints *hints = _client->getXSizeHints(); // convenience

	// if we have ResizeInc hint set we use it instead of pixel diff
	if (hints->flags&PResizeInc) {
		if (diff > 0) {
			diff_ret = hints->height_inc;
		} else if ((_gm.height - hints->height_inc) >= min) {
			diff_ret = -hints->height_inc;
		}
	} else {
		diff_ret = diff;
	}

	// check max/min size hints
	if (diff > 0) {
		if ((hints->flags&PMaxSize) &&
				((getChildHeight() + diff) > unsigned(hints->max_height))) {
			diff_ret = _gm.height - hints->max_height + min;
		}
	} else if ((hints->flags&PMinSize) &&
						 ((getChildHeight() + diff) < unsigned(hints->min_height))) {
		diff_ret = _gm.height - hints->min_width + min;
	}

	return diff_ret;
}

// END - PDecor interface.

//! @brief Sets _PEKWM_FRAME_ID on all children in the frame
void
Frame::setId(uint id)
{
	_id = id;

	list<PWinObj*>::iterator it(_child_list.begin());
	long atom = PekwmAtoms::instance()->getAtom(PEKWM_FRAME_ID);
	for (; it != _child_list.end(); ++it) {
		AtomUtil::setLong((*it)->getWindow(), atom, id);
	}
}

//! @brief Gets the state from the Client
void
Frame::getState(Client *cl)
{
  if (!cl)
    return;

  bool b_client_iconified = cl->isIconified ();

  if (_sticky != cl->isSticky())
    _sticky = !_sticky;
  if (_state.maximized_horz != cl->isMaximizedHorz())
    setStateMaximized(STATE_TOGGLE, true, false, false);
  if (_state.maximized_vert != cl->isMaximizedVert())
    setStateMaximized(STATE_TOGGLE, false, true, false);
  if (isShaded() != cl->isShaded())
    setShaded(STATE_TOGGLE);
  if (_layer != cl->getLayer())
    setLayer(cl->getLayer());
  if (_workspace != cl->getWorkspace())
    setWorkspace(cl->getWorkspace());

  // We need to set border and titlebar before setting fullscreen, as
  // fullscreen will unset border and titlebar if needed.
  if (hasBorder() != _client->hasBorder())
    setBorder(STATE_TOGGLE);
  if (hasTitlebar() != _client->hasTitlebar())
    setTitlebar(STATE_TOGGLE);
  if (_state.fullscreen != cl->isFullscreen())
    setStateFullscreen(STATE_TOGGLE);

  if (_iconified != b_client_iconified)
    {
      if (_iconified)
        mapWindow();
      else
        iconify();
    }

  if (_state.skip != cl->getSkip())
    setSkip(cl->getSkip());
}

//! @brief Applies the frame's state on the Client
void
Frame::applyState(Client *cl)
{
	if (!cl)
		return;

	cl->setSticky(_sticky);
	cl->setMaximizedHorz(_state.maximized_horz);
	cl->setMaximizedVert(_state.maximized_vert);
	cl->setShade(isShaded());
	cl->setWorkspace(_workspace);
	cl->setLayer(_layer);

	// fix border / titlebar state
	cl->setBorder(hasBorder());
	cl->setTitlebar(hasTitlebar());
	// make sure the window has the correct mapped state
	if (_mapped != cl->isMapped()) {
		if (_mapped == false) {
			cl->unmapWindow();
		} else {
			cl->mapWindow();
		}
	}

	cl->updateEwmhStates();
}

//! @brief
void
Frame::setSkip(uint skip)
{
	_state.skip = skip;

	if (_client != NULL) {
		_client->setSkip(skip);
	}
}

//! @brief
void
Frame::setupAPGeometry(Client *client, AutoProperty *ap)
{
	// frame geomtry overides client geometry

	// get client geometry
	if (ap->isMask(AP_CLIENT_GEOMETRY)) {
		Geometry gm(client->_gm);
		applyAPGeometry(gm, ap->client_gm, ap->client_gm_mask);

		if (ap->client_gm_mask&(XValue|YValue)) {
			moveChild(gm.x, gm.y);
		}
		if(ap->client_gm_mask&(WidthValue|HeightValue)) {
			resizeChild(gm.width, gm.height);
		}
	}

	// get frame geometry
	if (ap->isMask(AP_FRAME_GEOMETRY)) {
		applyAPGeometry(_gm, ap->frame_gm, ap->frame_gm_mask);
		if (ap->frame_gm_mask&(XValue|YValue)) {
			move(_gm.x, _gm.y);
		}
		if (ap->frame_gm_mask&(WidthValue|HeightValue)) {
			resize(_gm.width, _gm.height);
		}
	}
}

//! @brief
void
Frame::applyAPGeometry(Geometry &gm, const Geometry &ap_gm, int mask)
{
	// read size before position so negative position works
	if (mask&WidthValue)
		gm.width = ap_gm.width;
	if (mask&HeightValue)
		gm.height = ap_gm.height;

	// read position
	if (mask&(XValue|YValue)) {
		if (mask&XValue) {
			gm.x = ap_gm.x;
			if (mask&XNegative)
				gm.x += _wm->getScreen()->getWidth() - gm.width;
		}
		if (mask&YValue) {
			gm.y = ap_gm.y;
			if (mask&YNegative)
				gm.y += _wm->getScreen()->getHeight() - gm.height;
		}
	}
}

//! @brief Removes the client from the Frame and creates a new Frame for it
void
Frame::detachClient(Client *client)
{
	if (client->getParent() != this) {
		return;
	}

	if (_child_list.size() > 1) {
		removeChild(client);

		client->move(_gm.x, _gm.y + borderTop());
		Frame *frame = new Frame(_wm, client, NULL);

		client->setParent(frame);
		client->setWorkspace(Workspaces::instance()->getActive());

		setFocused(false);
	}
}

//! @brief Makes sure the frame doesn't cover any struts / the harbour.
bool
Frame::fixGeometry(void)
{
	Geometry head, before;
	PScreen::instance()->getHeadInfoWithEdge(getNearestHead(), head);

	before = _gm;

	// fix size
	if (_gm.width > head.width) {
		_gm.width = head.width;
	} if (_gm.height > head.height) {
		_gm.height = head.height;
	}

	// fix position
	if (_gm.x < head.x) {
		_gm.x = head.x;
	} else if ((_gm.x + _gm.width) > (head.x + head.width)) {
		_gm.x = head.x + head.width - _gm.width;
	}
	if (_gm.y < head.y) {
		_gm.y = head.y;
	} else if ((_gm.y + _gm.height) > (head.y + head.height)) {
		_gm.y = head.y + head.height - _gm.height;
	}

	return (_gm != before);
}

//! @brief Initiates grouping move, based on a XMotionEvent.
void
Frame::doGroupingDrag(XMotionEvent *ev, Client *client, bool behind) // FIXME: rewrite
{
	if (client == NULL)
		return;

	int o_x, o_y;
	o_x = ev ? ev->x_root : 0;
	o_y = ev ? ev->y_root : 0;

	string name("Grouping ");
	if (client->getTitle()->getVisible().size() > 0) {
		name += client->getTitle()->getVisible();
	} else {
		name += "No Name";
	}

	bool status = _scr->grabPointer(_scr->getRoot(),
																	ButtonReleaseMask|PointerMotionMask, None);
	if (status != true)
		return;

	StatusWindow *sw = StatusWindow::instance();

	sw->draw(name); // resize window and render bg
	sw->move(o_x, o_y);
	sw->mapWindowRaised();
	sw->draw(name); // redraw after map

	XEvent e;
	while (true) { // this breaks when we get an button release
		XMaskEvent(_dpy, PointerMotionMask|ButtonReleaseMask, &e);

		switch (e.type)  {
		case MotionNotify:
			// update the position
			o_x = e.xmotion.x_root;
			o_y = e.xmotion.y_root;

			sw->move(o_x, o_y);
			sw->draw(name);
			break;

		case ButtonRelease:
			sw->unmapWindow();
			_scr->ungrabPointer();

			Client *search = NULL;

			// only group if we have grouping turned on
			if (_wm->isAllowGrouping()) {
				int x, y;
				Window win;

				// find the frame we dropped the client on
				XTranslateCoordinates(_dpy, _scr->getRoot(), _scr->getRoot(),
															e.xmotion.x_root, e.xmotion.y_root,
															&x, &y, &win);

				search = _wm->findClient(win);
			}

			// if we found a client, and it's not in the current frame and
			// it has a "normal" ( make configurable? ) layer we group
			if ((search != NULL) && (search->getParent() != NULL) &&
					(search->getParent() != this) &&
					(search->getLayer() > LAYER_BELOW) &&
					(search->getLayer() < LAYER_ONTOP)) {

				// if we currently have focus and the frame exists after we remove
				// this client we need to redraw it as unfocused
				bool focus = behind ? false : (_child_list.size() > 1);

				removeChild(client);

				Frame *frame = static_cast<Frame*>(search->getParent());
				frame->addChild(client);
				if (behind == false) {
					frame->activateChild(client);
					frame->giveInputFocus();
				}

				if (focus) {
					setFocused(false);
				}

			}  else if (_child_list.size() > 1) {
				// if we have more than one client in the frame detach this one
				removeChild(client);

				client->move(e.xmotion.x_root, e.xmotion.y_root);

				Frame *frame = new Frame(_wm, client, NULL);
				client->setParent(frame);

				// make sure the client ends up on the current workspace
				client->setWorkspace(Workspaces::instance()->getActive());

				// make sure it get's focus
				setFocused(false);
				frame->giveInputFocus();
			}

			return;
		}
	}
}

//! @brief Initiates resizing of a window based on motion event
void
Frame::doResize(XMotionEvent *ev)
{
	if (!ev)
		return;

	// figure out which part of the window we are in
	bool left = false, top = false;
	if (ev->x < signed(_gm.width / 2))
		left = true;
	if (ev->y < signed(_gm.height / 2))
		top = true;

	doResize(left, true, top, true);
}

//! @brief Initiates resizing of a window based border position
void
Frame::doResize(BorderPosition pos)
{
	bool x = false, y = false;
	bool left = false, top = false, resize = true;

	switch (pos) {
	case BORDER_TOP_LEFT:
		x = y = left = top = true;
		break;
	case BORDER_TOP:
		y = top = true;
		break;
	case BORDER_TOP_RIGHT:
		x = y = top = true;
		break;
	case BORDER_LEFT:
		x = left = true;
		break;
	case BORDER_RIGHT:
		x = true;
		break;
	case BORDER_BOTTOM_LEFT:
		x = y = left = true;
		break;
	case BORDER_BOTTOM:
		y = true;
		break;
	case BORDER_BOTTOM_RIGHT:
		x = y = true;
		break;
	default:
		resize = false;
		break;
	}

	if (resize) {
		doResize(left, x, top, y);
	}
}

//! @brief Resizes the frame by handling MotionNotify events.
void
Frame::doResize(bool left, bool x, bool top, bool y)
{
	if ((_client->allowResize() == false) || isShaded()) {
		return;
	}

	if (!_scr->grabPointer(_scr->getRoot(), ButtonMotionMask|ButtonReleaseMask,
												 ScreenResources::instance()->getCursor(ScreenResources::CURSOR_RESIZE)))
		return;

	setShaded(STATE_UNSET); // make sure the frame isn't shaded

	// Initialize variables
	int start_x, new_x;
	int start_y, new_y;
	uint last_width, old_width;
	uint last_height, old_height;

	start_x = new_x = left ? _gm.x : (_gm.x + _gm.width);
	start_y = new_y = top ? _gm.y : (_gm.y + _gm.height);
	last_width = old_width = _gm.width;
	last_height = old_height = _gm.height;

	// the basepoint of our window
	_click_x = left ? (_gm.x + _gm.width) : _gm.x;
	_click_y = top ? (_gm.y + _gm.height) : _gm.y;

	int pointer_x = _gm.x, pointer_y = _gm.y;
	_scr->getMousePosition(pointer_x, pointer_y);

	char buf[128];
	getDecorInfo(buf, sizeof(buf)/sizeof(char));

	StatusWindow *sw = StatusWindow::instance();
	if (Config::instance()->isShowStatusWindow()) {
		sw->draw(buf, true);
		sw->mapWindowRaised();
		sw->draw(buf);
	}

	bool outline = (Config::instance()->getOpaqueResize() == false);

	// grab server, we don't want invert traces
	if (outline) {
		_scr->grabServer();
	}

	XEvent ev;
	bool exit = false;
	while (exit != true) {
		if (outline) {
			drawOutline(_gm);
		}
		XMaskEvent(_dpy, ButtonPressMask|ButtonReleaseMask|ButtonMotionMask, &ev);
		if (outline) {
			drawOutline(_gm); // clear
		}

		switch (ev.type) {
		case MotionNotify:
			if (x) {
				new_x = start_x - pointer_x + ev.xmotion.x;
			}
			if (y) {
				new_y = start_y - pointer_y + ev.xmotion.y;
			}

			recalcResizeDrag(new_x, new_y, left, top);

			getDecorInfo(buf, sizeof(buf)/sizeof(char));
			if (Config::instance()->isShowStatusWindow()) {
				sw->draw(buf, true);
			}

			// only updated when needed when in opaque mode
			if (outline == false) {
				if ((old_width != _gm.width) || (old_height != _gm.height)) {
					resize(_gm.width, _gm.height);
					move(_gm.x, _gm.y);
				}
				old_width = _gm.width;
				old_height = _gm.height;
			}
		break;
		case ButtonRelease:
			exit = true;
			break;
		}
	}

	if (Config::instance()->isShowStatusWindow()) {
		sw->unmapWindow();
	}

	_scr->ungrabPointer();

	// Make sure the state isn't set to maximized after we've resized.
	if (_state.maximized_horz || _state.maximized_vert) {
		_state.maximized_horz = false;
		_state.maximized_vert = false;
		_client->setMaximizedHorz(false);
		_client->setMaximizedVert(false);
		_client->updateEwmhStates();
	}

	if (outline) {
		resize(_gm.width, _gm.height);
		move(_gm.x, _gm.y);
	}

	if (outline) {
		_scr->ungrabServer(true);
	}
}

//! @brief Updates the width, height of the frame when resizing it.
void
Frame::recalcResizeDrag(int nx, int ny, bool left, bool top)
{
	uint brdr_lr = borderLeft() + borderRight();
	uint brdr_tb = borderTop() + borderBottom();

	if (left) {
		if (nx >= signed(_click_x - brdr_lr))
			nx = _click_x - brdr_lr - 1;
	} else {
		if (nx <= signed(_click_x + brdr_lr))
			nx = _click_x + brdr_lr + 1;
	}

	if (top) {
		if (ny >= signed(_click_y - getTitleHeight() - brdr_tb))
			ny = _click_y - getTitleHeight() - brdr_tb - 1;
	} else {
		if (ny <= signed(_click_y + getTitleHeight() + brdr_tb))
			ny = _click_y + getTitleHeight() + brdr_tb + 1;
	}

	uint width = left ? (_click_x - nx) : (nx - _click_x);
	uint height = top ? (_click_y - ny) : (ny - _click_y);

	if (width > _scr->getWidth())
		width = _scr->getWidth();
	if (height > _scr->getHeight())
		height = _scr->getHeight();

	width -= brdr_lr;
	height -= brdr_tb + getTitleHeight();
	_client->getIncSize(&width, &height, width, height);

	const XSizeHints *hints = _client->getXSizeHints();
	// check so we aren't overriding min or max size
	if (hints->flags & PMinSize) {
		if (signed(width) < hints->min_width)
			width = hints->min_width;
		if (signed(height) < hints->min_height)
			height = hints->min_height;
	}

	if (hints->flags & PMaxSize) {
		if (signed(width) > hints->max_width)
			width = hints->max_width;
		if (signed(height) > hints->max_height)
			height = hints->max_height;
	}

	_gm.width = width + brdr_lr;
	_gm.height = height + getTitleHeight() + brdr_tb;

	_gm.x = left ? (_click_x - _gm.width) : _click_x;
	_gm.y = top ? (_click_y - _gm.height) : _click_y;
}

//! @brief Moves the Frame to the screen edge ori ( considering struts )
void
Frame::moveToEdge(OrientationType ori)
{
	uint head_nr;
	Geometry head, real_head;

	head_nr = getNearestHead();
	_scr->getHeadInfo(head_nr, real_head);
	_scr->getHeadInfoWithEdge(head_nr, head);

	switch (ori) {
	case TOP_LEFT:
		_gm.x = head.x;
		_gm.y = head.y;
		break;
	case TOP_EDGE:
		_gm.y = head.y;
		break;
	case TOP_CENTER_EDGE:
		_gm.x = real_head.x + ((real_head.width - _gm.width) / 2);
		_gm.y = head.y;
		break;
	case TOP_RIGHT:
		_gm.x = head.x + head.width - _gm.width;
		_gm.y = head.y;
		break;
	case BOTTOM_RIGHT:
		_gm.x = head.x + head.width - _gm.width;
		_gm.y = head.y + head.height - _gm.height;
		break;
	case BOTTOM_EDGE:
		_gm.y = head.y + head.height - _gm.height;
		break;
	case BOTTOM_CENTER_EDGE:
		_gm.x = real_head.x + ((real_head.width - _gm.width) / 2);
		_gm.y = head.y + head.height - _gm.height;
		break;
	case BOTTOM_LEFT:
		_gm.x = head.x;
		_gm.y = head.y + head.height - _gm.height;
		break;
	case LEFT_EDGE:
		_gm.x = head.x;
		break;
	case LEFT_CENTER_EDGE:
		_gm.x = head.x;
		_gm.y = real_head.y + ((real_head.height - _gm.height) / 2);
		break;
	case RIGHT_EDGE:
		_gm.x = head.x + head.width - _gm.width;
		break;
	case RIGHT_CENTER_EDGE:
		_gm.x = head.x + head.width - _gm.width;
		_gm.y = real_head.y + ((real_head.height - _gm.height) / 2);
		break;
	case CENTER:
		_gm.x = real_head.x + ((real_head.width - _gm.width) / 2);
		_gm.y = real_head.y + ((real_head.height - _gm.height) / 2);
	default:
		// DO NOTHING
		break;
	}

	move(_gm.x, _gm.y);
}

//! @brief Updates all inactive childrens geometry and state
void
Frame::updateInactiveChildInfo(void)
{
	if (_client == NULL)
		return;

	list<PWinObj*>::iterator it(_child_list.begin());
	for (; it != _child_list.end(); ++it) {
		if (*it != _client) {
			applyState(static_cast<Client*>(*it));
			(*it)->resize(getChildWidth(), getChildHeight());
		}
	}
}

// STATE actions begin

//! @brief Toggles current clients max size
//! @param sa State to set
//! @param horz Include horizontal in (de)maximize
//! @param vert Include vertcical in (de)maximize
//! @param fill Limit size by other frame boundaries ( defaults to false )
void
Frame::setStateMaximized(StateAction sa, bool horz, bool vert, bool fill)
{
	// we don't want to maximize transients
	if (_client->getTransientWindow()) {
		return;
	}

	setShaded(STATE_UNSET);

	// make sure the two states are in sync if toggling
	if ((horz == vert) && (sa == STATE_TOGGLE)) {
		if (_state.maximized_horz != _state.maximized_vert) {
			horz = !_state.maximized_horz;
			vert = !_state.maximized_vert;
		}
	}

	XSizeHints *size_hint = _client->getXSizeHints(); // convenience

	Geometry head;
	_scr->getHeadInfoWithEdge(getNearestHead(), head);

	int max_x, max_r, max_y, max_b;
	max_x = head.x;
	max_r = head.width + head.x;
	max_y = head.y;
	max_b = head.height + head.y;

	if (fill) {
		getMaxBounds(max_x, max_r, max_y, max_b);

		// make sure vert and horz gets set if fill is on
		sa = STATE_SET;
	}

	if (horz && (fill || _client->allowMaximizeHorz())) {
		// maximize
		if ((sa == STATE_SET) ||
				((sa == STATE_TOGGLE) && (_state.maximized_horz == false))) {
			uint h_decor = _gm.width - getChildWidth();

			if (fill == false) {
				_old_gm.x = _gm.x;
				_old_gm.width = _gm.width;
			}

			_gm.x = max_x;
			_gm.width = max_r - max_x;

			if ((size_hint->flags&PMaxSize) &&
					(_gm.width > (size_hint->max_width + h_decor))) {
				_gm.width = size_hint->max_width + h_decor;
			}
		// demaximize
		} else if ((sa == STATE_UNSET) ||
							 ((sa == STATE_TOGGLE) && (_state.maximized_horz == true))) {
			_gm.x = _old_gm.x;
			_gm.width = _old_gm.width;
		}

		// we unset the maximized state if we use maxfill
		_state.maximized_horz = fill ? false : !_state.maximized_horz;
		_client->setMaximizedHorz(_state.maximized_horz);
	}

	if (vert && (fill || _client->allowMaximizeVert())) {
		// maximize
		if ((sa == STATE_SET) ||
				((sa == STATE_TOGGLE) && (_state.maximized_vert == false))) {
			uint v_decor = _gm.height - getChildHeight();

			if (fill == false) {
				_old_gm.y = _gm.y;
				_old_gm.height = _gm.height;
			}

			_gm.y = max_y;
			_gm.height = max_b - max_y;

			if ((size_hint->flags&PMaxSize) &&
					(_gm.height > (size_hint->max_height + v_decor))) {
				_gm.height = size_hint->max_height + v_decor;
			}
		// demaximize
		} else if ((sa == STATE_UNSET) ||
							 ((sa == STATE_TOGGLE) && (_state.maximized_vert == true))) {
			_gm.y = _old_gm.y;
			_gm.height = _old_gm.height;
		}

		// we unset the maximized state if we use maxfill
		_state.maximized_vert = fill ? false : !_state.maximized_vert;
		_client->setMaximizedVert(_state.maximized_vert);
	}

	fixGeometry(); // harbour already considered
	downSize(true, true); // keep x and keep y ( make conform to inc size )

	resize(_gm.width, _gm.height);
	move(_gm.x, _gm.y);

	_client->updateEwmhStates();
}

//! @brief
void
Frame::setStateFullscreen(StateAction sa)
{
	if (ActionUtil::needToggle(sa, _state.fullscreen) == false) {
		return;
	}

	bool lock = _client->setConfigureRequestLock(true);

	if (_state.fullscreen) {
		if ((_old_decor_state&DECOR_BORDER) != hasBorder()) {
			setBorder(STATE_TOGGLE);
		}
		if ((_old_decor_state&DECOR_TITLEBAR) != hasTitlebar()) {
			setTitlebar(STATE_TOGGLE);
		}
		_gm = _old_gm;

	} else {
		_old_gm = _gm;
		_old_decor_state = _client->getDecorState();

		setBorder(STATE_UNSET);
		setTitlebar(STATE_UNSET);

		Geometry head;
		uint nr = getNearestHead();
		_scr->getHeadInfo(nr, head);

		_gm = head;
	}

	_state.fullscreen = !_state.fullscreen;
	_client->setFullscreen(_state.fullscreen);

	move(_gm.x, _gm.y);
	resize(_gm.width, _gm.height);

	_client->setConfigureRequestLock(lock);
	_client->configureRequestSend();

	_client->updateEwmhStates();
}

//! @brief
void
Frame::setStateSticky(StateAction sa)
{
	if (ActionUtil::needToggle(sa, _sticky) == true) {
		stick();
	}
}

//! @brief
void
Frame::setStateAlwaysOnTop(StateAction sa)
{
	if (ActionUtil::needToggle(sa, _layer == LAYER_ONTOP) == false) {
		return;
	}

	_client->alwaysOnTop(_layer < LAYER_ONTOP);
	setLayer(_client->getLayer());

	raise();
}

//! @brief
void
Frame::setStateAlwaysBelow(StateAction sa)
{
	if (ActionUtil::needToggle(sa, _layer == LAYER_BELOW) == false) {
		return;
	}

	_client->alwaysBelow(_layer > LAYER_BELOW);
	setLayer(_client->getLayer());

	lower();
}

//! @brief Hides/Shows the border depending on _client
//! @param sa State to set
void
Frame::setStateDecorBorder(StateAction sa)
{
	bool border = hasBorder();

	setBorder(sa);

	// state changed, update client and atom state
	if (border != hasBorder()) {
		_client->setBorder(hasBorder());

		// update the _PEKWM_FRAME_DECOR hint
		AtomUtil::setLong(_client->getWindow(),
											PekwmAtoms::instance()->getAtom(PEKWM_FRAME_DECOR),
											_client->getDecorState());
	}
}

//! @brief Hides/Shows the titlebar depending on _client
//! @param sa State to set
void
Frame::setStateDecorTitlebar(StateAction sa)
{
	bool titlebar = hasTitlebar();

	setTitlebar(sa);

	// state changed, update client and atom state
	if (titlebar != hasTitlebar()) {
		_client->setTitlebar(hasTitlebar());

		AtomUtil::setLong(_client->getWindow(),
											PekwmAtoms::instance()->getAtom(PEKWM_FRAME_DECOR),
											_client->getDecorState());
	}
}

//! @brief
void
Frame::setStateIconified(StateAction sa)
{
	if (ActionUtil::needToggle(sa, _iconified) == false) {
		return;
	}

	if (_iconified) {
		mapWindow();
	} else {
		iconify();
	}
}

//! @brief
void
Frame::setStateTagged(StateAction sa, bool behind)
{
	if (ActionUtil::needToggle(sa, (this != _wm->getTaggedFrame())) == false) {
		return;
	}

	_wm->setTaggedFrame((this == _wm->getTaggedFrame()) ? NULL : this, !behind);
}

//! @brief
void
Frame::setStateSkip(StateAction sa, uint skip)
{
	if (ActionUtil::needToggle(sa, _state.skip&skip) == false) {
		return;
	}

 if (_state.skip&skip) {
		_state.skip &= ~skip;
	} else {
		_state.skip |= skip;
	}

	setSkip(_state.skip);
}

//! @brief Sets client title
void
Frame::setStateTitle(StateAction sa, Client *client, const std::string &title)
{
	if (sa == STATE_SET) {
		client->getTitle()->setUserSet(true);
		client->getTitle()->setVisible(title);
	} else if (sa == STATE_UNSET) {
		client->getTitle()->setUserSet(false);
		client->getXClientName();
	} else {
		client->getTitle()->setUserSet(!client->getTitle()->isUserSet());
		if (client->getTitle()->isUserSet() == false) {
			client->getXClientName();
		}
	}

	renderTitle();
}

// STATE actions end

//! @brief
void
Frame::getMaxBounds(int &max_x,int &max_r, int &max_y, int &max_b)
{
	int f_r, f_b;
	int x, y, h, w, r, b;

	f_r = getRX();
	f_b = getBY();

	list<Frame*>::iterator it = _wm->frame_begin();
	for (; it != _wm->frame_end(); ++it) {
		if ((*it)->isMapped() == false) {
			continue;
		}

		x = (*it)->getX();
		y = (*it)->getY();
		h = (*it)->getHeight();
		w = (*it)->getWidth();
		r = (*it)->getRX();
		b = (*it)->getBY();

		// update max borders when other frame border lies between
		// this border and prior max border (originally screen/head edge)
		if ((r >= max_x) && (r <= _gm.x) && !((y >= f_b) || (b <= _gm.y))) {
			max_x = r;
		}
		if ((x <= max_r) && (x >= f_r) && !((y >= f_b) || (b <= _gm.y))) {
			max_r = x;
		}
		if ((b >= max_y) && (b <= _gm.y) && !((x >= f_r) || (r <= _gm.x))) {
			max_y = b;
		}
		if ((y <= max_b) && (y >= f_b) && !((x >= f_r) || (r <= _gm.x))) {
			max_b = y;
		}
	}
}

//! @brief
void
Frame::growDirection(uint direction)
{
	Geometry head;
	_scr->getHeadInfoWithEdge(getNearestHead(), head);

	switch (direction) {
	case DIRECTION_UP:
		_gm.height = getBY() - head.y;
		_gm.y = head.y;
		break;
	case DIRECTION_DOWN:
		_gm.height = head.y + head.height - _gm.y;
		break;
	case DIRECTION_LEFT:
		_gm.width = getRX() - head.x;
		_gm.x = head.x;
		break;
	case DIRECTION_RIGHT:
		_gm.width = head.x + head.width - _gm.x;
		break;
	default:
		break;
	}

	downSize((direction != DIRECTION_LEFT), (direction != DIRECTION_UP));

	move(_gm.x, _gm.y);
	resize(_gm.width, _gm.height);
}

//! @brief Closes the frame and all clients in it
void
Frame::close(void)
{
	list<PWinObj*>::iterator it(_child_list.begin());
	for (; it != _child_list.end(); ++it) {
		static_cast<Client*>(*it)->close();
	}
}

//! @brief Reads autoprops for the active client.
//! @param type Defaults to APPLY_ON_RELOAD
void
Frame::readAutoprops(uint type)
{
	if ((type != APPLY_ON_RELOAD) && (type != APPLY_ON_WORKSPACE))
		return;

	_class_hint->title = _client->getTitle()->getReal();
	AutoProperty *data =
		_wm->getAutoProperties()->findAutoProperty(_class_hint, _workspace, type);
	_class_hint->title = "";

	if (data == NULL)
		return;

	// Set the correct group of the window
	_class_hint->group = data->group_name;

	if ((_class_hint == _client->getClassHint()) &&
			(_client->getTransientWindow() &&
			 !data->isApplyOn(APPLY_ON_TRANSIENT))) {
		return;
	}

	if (data->isMask(AP_STICKY) && (_sticky != data->sticky))
		stick();
	if (data->isMask(AP_SHADED) && (isShaded() != data->shaded))
		setShaded(STATE_UNSET);
	if (data->isMask(AP_MAXIMIZED_HORIZONTAL) &&
			(_state.maximized_horz != data->maximized_horizontal)) {
		setStateMaximized(STATE_TOGGLE, true, false, false);
	}
	if (data->isMask(AP_MAXIMIZED_VERTICAL) &&
			(_state.maximized_vert != data->maximized_vertical)) {
		setStateMaximized(STATE_TOGGLE, false, true, false);
	}
	if (data->isMask(AP_FULLSCREEN) && (_state.fullscreen != data->fullscreen)) {
		setStateFullscreen(STATE_TOGGLE);
	}

	if (data->isMask(AP_ICONIFIED) && (_iconified != data->iconified)) {
		if (_iconified) mapWindow();
		else iconify();
	}
	if (data->isMask(AP_WORKSPACE)) {
		// I do this to avoid coming in an eternal loop.
		if (type == APPLY_ON_WORKSPACE)
			_workspace = data->workspace;
		else if (_workspace != data->workspace)
			setWorkspace(data->workspace); // FIXME: FIX, false); // do not read autoprops again
	}

	if (data->isMask(AP_SHADED) && (isShaded() != data->shaded))
		setShaded(STATE_TOGGLE);
	if (data->isMask(AP_LAYER) && (data->layer <= LAYER_MENU)) {
		_client->setLayer(data->layer);
		raise(); // restack the frame
	}

	if (data->isMask(AP_FRAME_GEOMETRY|AP_CLIENT_GEOMETRY)) {
		setupAPGeometry(_client, data);

		// apply changes
		move(_gm.x, _gm.y);
		resize(_gm.width, _gm.height);
	}

	if (data->isMask(AP_BORDER) && (hasBorder() != data->border))
		setStateDecorBorder(STATE_TOGGLE);
	if (data->isMask(AP_TITLEBAR) && (hasTitlebar() != data->titlebar))
		setStateDecorTitlebar(STATE_TOGGLE);

	if (data->isMask(AP_SKIP)) {
		_client->setSkip(data->skip);
		setSkip(_client->getSkip());
	}

	if (data->isMask(AP_FOCUSABLE)) {
		_client->setFocusable(data->focusable);
	}
}

//! @brief Figure out how large the frame is in cells.
void
Frame::calcSizeInCells(uint &width, uint &height)
{
	const XSizeHints *hints = _client->getXSizeHints();

	if (hints->flags&PResizeInc) {
		width = (getChildWidth() - hints->base_width) / hints->width_inc;
		height = (getChildHeight() - hints->base_height) / hints->height_inc;
	} else {
		width = _gm.width;
		height = _gm.height;
	}
}

//! @brief Calculates position based on current gravity
void
Frame::calcGravityPosition(int gravity, int x, int y, int &g_x, int &g_y)
{
	switch (gravity) {
	case NorthEastGravity: // outside border corner
		g_x = x - _gm.x;
		g_y = y;
		break;
	case SouthWestGravity: // outside border corner
		g_x = x;
		g_y = y - _gm.y;
		break;
	case SouthEastGravity: // outside border corner
		g_x = x - _gm.x;
		g_y = y - _gm.y;
		break;

	case NorthGravity: // outside border center
		g_x = x - (_gm.x / 2);
		g_y = y;
		break;
	case SouthGravity: // outside border center
		g_x = x - (_gm.x / 2);
		g_y = y - _gm.y;
		break;
	case WestGravity: // outside border center
		g_x = x;
		g_y = y - (_gm.y / 2);
		break;
	case EastGravity: // outside border center
		g_x = x - _gm.x;
		g_y = y - (_gm.y / 2);
		break;

	case CenterGravity: // center of window
		g_x = x - (_gm.x / 2);
		g_y = y - (_gm.y / 2);
		break;
	case StaticGravity: // client top left
		g_x = x - (_gm.width - getChildWidth());
		g_y = y - (_gm.height - getChildHeight());
		break;
	case NorthWestGravity: // outside border corner
	default:
		g_x = x;
		g_y = y;
		break;
	}
}

//! @brief Makes Frame conform to Clients width and height inc
void
Frame::downSize(bool keep_x, bool keep_y)
{
	XSizeHints *size_hint = _client->getXSizeHints(); // convenience

	// conform to width_inc
	if (size_hint->flags&PResizeInc) {
		int o_r = getRX();
		int b_x = (size_hint->flags&PBaseSize)
			? size_hint->base_width
			: (size_hint->flags&PMinSize) ? size_hint->min_width : 0;

		_gm.width -= (getChildWidth() - b_x) % size_hint->width_inc;
		if (keep_x == false) {
			_gm.x = o_r - _gm.width;
		}
	}

	// conform to height_inc
	if (size_hint->flags&PResizeInc) {
		int o_b = getBY();
		int b_y = (size_hint->flags&PBaseSize)
			? size_hint->base_height
			: (size_hint->flags&PMinSize) ? size_hint->min_height : 0;

		_gm.height -= (getChildHeight() - b_y) % size_hint->height_inc;
		if (keep_y == false) {
			_gm.y = o_b - _gm.height;
		}
	}
}

// Below this Client message handling is done

//! @brief Handle XConfgiureRequestEvents
//! @todo Should we send a ConfigureRequest back to the Client if ignoring?
void
Frame::handleConfigureRequest(XConfigureRequestEvent *ev, Client *client)
{
	if (client != _client) {
		return; // only handle the active client's events
	}

	// size before position, as we rely on size when gravitating
	if (client->isCfgDeny(CFG_DENY_SIZE) == false) {
		if ((ev->value_mask&CWWidth) || (ev->value_mask&CWHeight)) {
			resizeChild(ev->width, ev->height);
#ifdef HAVE_SHAPE
			_client->setShaped(setShape(_client->isShaped()));
#endif // HAVE_SHAPE
		}
	}

	if (client->isCfgDeny(CFG_DENY_POSITION) == false) {
		if ((ev->value_mask&CWX) || (ev->value_mask&CWY)) {
			calcGravityPosition(_client->getXSizeHints()->win_gravity,
													ev->x, ev->y, _gm.x, _gm.y);
			move(_gm.x, _gm.y);
		}
	}

	// update the stacking
	if (client->isCfgDeny(CFG_DENY_STACKING) == false) {
		if (ev->value_mask&CWStackMode) {
			if (ev->value_mask&CWSibling) {
				switch(ev->detail) {
				case Above:
					Workspaces::instance()->stackAbove(this, ev->above);
					break;
				case Below:
					Workspaces::instance()->stackBelow(this, ev->above);
					break;
				case TopIf:
				case BottomIf:
					// FIXME: What does occlude mean?
					break;
				}
			} else {
				switch(ev->detail) { // FIXME: Is this broken?
				case Above:
					raise();
					break;
				case Below:
					lower();
					break;
				case TopIf:
				case BottomIf:
					// FIXME: Why does the manual say that it should care about siblings
					// even if we don't have any specified?
					break;
				}
			}
		}
	}
}

//! @brief
void
Frame::handleClientMessage(XClientMessageEvent *ev, Client *client)
{
	EwmhAtoms *ewmh = EwmhAtoms::instance(); // convenience

	StateAction sa;

	if (ev->message_type == ewmh->getAtom(STATE)) {
		if (ev->data.l[0]== NET_WM_STATE_REMOVE) {
			sa = STATE_UNSET;
		} else if (ev->data.l[0]== NET_WM_STATE_ADD) {
			sa = STATE_SET;
		} else if (ev->data.l[0]== NET_WM_STATE_TOGGLE) {
			sa = STATE_TOGGLE;
		} else {
#ifdef DEBUG
			cerr << __FILE__ << "@" << __LINE__ << ": "
					 << "None of StateAdd, StateRemove or StateToggle." << endl;
#endif // DEBUG
			return;
		}

#define IS_STATE(S) ((ev->data.l[1] == long(ewmh->getAtom(S))) || (ev->data.l[2] == long(ewmh->getAtom(S))))

		// actions that only is going to be applied on the active client
		if (client == _client) {
			// there is no modal support in pekwm yet
// 			if (IS_STATE(STATE_MODAL)) {
// 				is_modal=true;
// 			}
			if (IS_STATE(STATE_STICKY)) {
				setStateSticky(sa);
			}
			if (IS_STATE(STATE_MAXIMIZED_HORZ)
					&& !client->isCfgDeny(CFG_DENY_STATE_MAXIMIZED_HORZ)) {
				setStateMaximized(sa, true, false, false);
			}
			if (IS_STATE(STATE_MAXIMIZED_VERT)
					&& !client->isCfgDeny(CFG_DENY_STATE_MAXIMIZED_VERT)) {
				setStateMaximized(sa, false, true, false);
			}
			if (IS_STATE(STATE_SHADED)) {
				setShaded(sa);
			}
			if (IS_STATE(STATE_HIDDEN)
					&& !client->isCfgDeny(CFG_DENY_STATE_HIDDEN)) {
				setStateIconified(sa);
			}
			if (IS_STATE(STATE_FULLSCREEN)
					&& !client->isCfgDeny(CFG_DENY_STATE_FULLSCREEN)) {
				setStateFullscreen(sa);
			}
			if (IS_STATE(STATE_ABOVE)
					&& !client->isCfgDeny(CFG_DENY_STATE_ABOVE)) {
				setStateAlwaysOnTop(sa);
			}
			if (IS_STATE(STATE_BELOW)
					&& !client->isCfgDeny(CFG_DENY_STATE_BELOW)) {
				setStateAlwaysBelow(sa);
			}
		}

		if (IS_STATE(STATE_SKIP_TASKBAR)) {
			client->setStateSkipTaskbar(sa);
		}
		if (IS_STATE(STATE_SKIP_PAGER)) {
			client->setStateSkipPager(sa);
		}

		client->updateEwmhStates();

	} else if (ev->message_type == ewmh->getAtom(NET_ACTIVE_WINDOW)) {
		if (!client->isCfgDeny(CFG_DENY_ACTIVE_WINDOW)) {
      // Active child if it's not the active child
			if (client != _client)
				activateChild(client);
      // If we aren't mapped we check if we make sure we're on the right
      // workspace and then map the window.
      if (!_mapped) {
        if (_workspace != Workspaces::instance()->getActive()) {
          Workspaces::instance()->setWorkspace(_workspace, false);
        }
        mapWindow();
      }
			giveInputFocus();
		}
	} else if (ev->message_type == ewmh->getAtom(NET_CLOSE_WINDOW)) {
		client->close();
	} else if (ev->message_type == ewmh->getAtom(NET_WM_DESKTOP)) {
		if (client == _client)
			setWorkspace(ev->data.l[0]);
	} else if (ev->message_type == IcccmAtoms::instance()->getAtom(WM_CHANGE_STATE) &&
						 (ev->format == 32) && (ev->data.l[0] == IconicState)) {
		if (client == _client)
			iconify();
	}
#undef IS_STATE
}


//! @brief
void
Frame::handlePropertyChange(XPropertyEvent *ev, Client *client)
{
	EwmhAtoms *ewmh = EwmhAtoms::instance(); // convenience

	if (ev->atom == ewmh->getAtom(NET_WM_DESKTOP)) {
		if (client == _client) {
			long workspace;

			if (AtomUtil::getLong(client->getWindow(),
														ewmh->getAtom(NET_WM_DESKTOP), workspace)) {
				if (workspace != signed(_workspace))
					setWorkspace(workspace);
			}

		}
	} else if (ev->atom == ewmh->getAtom(NET_WM_STRUT)) {
		client->getStrutHint();
	} else if (ev->atom == ewmh->getAtom(NET_WM_NAME)) {
		// FIXME: UTF8 support so we can handle the hint
		client->getXClientName();
		renderTitle();
	} else if (ev->atom == XA_WM_NAME) {
//	if (!m_has_extended_net_name)
		client->getXClientName();
		renderTitle();
	} else if (ev->atom == XA_WM_NORMAL_HINTS) {
		client->getWMNormalHints();
	} else if (ev->atom == XA_WM_TRANSIENT_FOR) {
		client->getTransientForHint();
	}
}
