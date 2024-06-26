//
// Frame.hh for pekwm
// Copyright (C) 2003-2004 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"

#ifndef _FRAME_HH_
#define _FRAME_HH_

#include "pekwm.hh"
#include "Action.hh"

class PScreen;
class PWinObj;
class PDecor;
class Strut;
class Theme;
class WindowManager;
class ClassHint;
class AutoProperty;

class Client;

class PDecor::Button;

#include <string>
#include <list>
#include <vector>

class Frame : public PDecor
{
public:
	Frame(WindowManager *wm, Client *client, AutoProperty *ap);
	virtual ~Frame(void);

	// START - PWinObj interface.
	virtual void iconify(void);
	virtual void stick(void);

	virtual ActionEvent *handleMotionEvent(XMotionEvent *ev);
	virtual ActionEvent *handleEnterEvent(XCrossingEvent *ev);
	virtual ActionEvent *handleLeaveEvent(XCrossingEvent *ev);

	virtual ActionEvent *handleMapRequest(XMapRequestEvent *ev);
	virtual ActionEvent *handleUnmapEvent(XUnmapEvent *ev);
	// END - PWinObj interface.

#ifdef HAVE_SHAPE
	virtual void handleShapeEvent(XAnyEvent *ev);
#endif // HAVE_SHAPE

	// START - PDecor interface.
	virtual void addChild(PWinObj *child);
	virtual void removeChild(PWinObj *child, bool do_delete = true);
	virtual void activateChild(PWinObj *child);

	virtual void updatedChildOrder(void);
	virtual void updatedActiveChild(void);

	virtual void getDecorInfo(char *buf, uint size);

	virtual void setShaded(StateAction sa);
	// END - PDecor interface.

	inline uint getId(void) const { return _id; }
	void setId(uint id);

	void detachClient(Client *client);

	inline const ClassHint* getClassHint(void) const { return _class_hint; }

	inline bool isSkip(uint skip) const { return (_state.skip&skip); }

	void growDirection(uint direction);
	void moveToEdge(OrientationType ori);

	void updateInactiveChildInfo(void);

	// state actions
	void setStateMaximized(StateAction sa, bool horz, bool vert, bool fill);
	void setStateFullscreen(StateAction sa);
	void setStateSticky(StateAction sa);
	void setStateAlwaysOnTop(StateAction sa);
	void setStateAlwaysBelow(StateAction sa);
	void setStateDecorBorder(StateAction sa);
	void setStateDecorTitlebar(StateAction sa);
	void setStateIconified(StateAction sa);
	void setStateTagged(StateAction sa, bool behind);
	void setStateSkip(StateAction sa, uint skip);
	void setStateTitle(StateAction sa, Client *client, const std::string &title);

	void close(void);

	void readAutoprops(uint type = APPLY_ON_RELOAD);

	void doResize(XMotionEvent *ev); // redirects to doResize(bool...
	void doResize(BorderPosition pos); // redirect to doResize(bool...
	void doResize(bool left, bool x, bool top, bool y);
	void doGroupingDrag(XMotionEvent *ev, Client *client, bool behind);

	bool fixGeometry(void);

	// client message handling
	void handleConfigureRequest(XConfigureRequestEvent *ev, Client *client);
	void handleClientMessage(XClientMessageEvent *ev, Client *client);
	void handlePropertyChange(XPropertyEvent *ev, Client *client);

protected:
	// BEGIN - PDecor interface
	virtual int resizeHorzStep(int diff) const;
	virtual int resizeVertStep(int diff) const;
	// END - PDecor interface

private:
	void recalcResizeDrag(int nx, int ny, bool left, bool top);
	void getMaxBounds(int &max_x,int &max_r, int &max_y, int &max_b);
	void calcSizeInCells(uint &width, uint &height);
	void calcGravityPosition(int gravity, int x, int y, int &g_x, int &g_y);
	void downSize(bool keep_x, bool keep_y);

	void getState(Client *cl);
	void applyState(Client *cl);

	void setSkip(uint skip);

	void setupAPGeometry(Client *client, AutoProperty *ap);
	void applyAPGeometry(Geometry &gm, const Geometry &ap_gm, int mask);

	void setActiveTitle(void);

private:
	WindowManager *_wm;
	PScreen *_scr;

	uint _id; // unique id of the frame

	Client *_client; // to skip all the casts from PWinObj
	ClassHint *_class_hint;

	// frame information used when maximizing / going fullscreen
	Geometry _old_gm; // FIXME: move to PDecor?
	uint _old_decor_state; // FIXME: move to PDecor?

	// state switches specific for the frame
	class State { // FIXME: move to PDecor?
	public:
		State(void) : maximized_vert(false), maximized_horz(false),
									fullscreen(false), skip(0) { }
		bool maximized_vert, maximized_horz, fullscreen;
		uint skip;
	} _state;

	// EWMH
	static const int NET_WM_STATE_REMOVE = 0; // remove/unset property
	static const int NET_WM_STATE_ADD = 1; // add/set property
	static const int NET_WM_STATE_TOGGLE = 2; // toggle property
};

#endif // _FRAME_HH_
