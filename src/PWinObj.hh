//
// PWinObj.hh for pekwm
// Copyright (C)  2003-2005 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"

#ifndef _WINDOW_OBJECT_HH_
#define _WINDOW_OBJECT_HH_

#include "pekwm.hh"
#include "Action.hh"

#include <list>
#include <algorithm>

//! @brief X11 Window wrapper class.
class PWinObj
{
public:
	//! @brief PWinObj inherited types.
	enum Type {
		WO_FRAME = (1<<1), //!< Frame type.
		WO_CLIENT = (1<<2), //!< Client type.
		WO_MENU = (1<<3), //!< PMenu type.
		WO_DOCKAPP = (1<<4), //!< DockApp type.
		WO_SCREEN_EDGE = (1<<5), //!< ScreenEdge type.
		WO_SCREEN_ROOT = (1<<6), //!< PWinObj type for root Window.
		WO_CMD_DIALOG = (1<<7), //!< CmdDialog type.
		WO_STATUS = (1<<8), //!< StatusWindow type.
		WO_NO_TYPE = 0 //!< No type.
	};

	PWinObj(Display *dpy);
	virtual ~PWinObj(void);

	//! @brief Returns the focused PWinObj.
	static inline PWinObj *getFocusedPWinObj(void) { return _focused_wo; }
	//! @brief Returns the PWinObj representing the root Window.
	static inline PWinObj *getRootPWinObj(void) {	return _root_wo; }
	//! @brief Sets the focused PWinObj.
	static inline void setFocusedPWinObj(PWinObj *wo) {	_focused_wo = wo; }
	//! @brief Sets the PWinObj representing the root Window.
	static inline void setRootPWinObj(PWinObj *wo) { _root_wo = wo; }

	//! @brief Searches for the PWinObj matching Window win.
	//! @param win Window to match PWinObjs against.
	//! @return PWinObj pointer on match, else NULL.
	static inline PWinObj *findPWinObj(Window win) {
		std::list<PWinObj*>::iterator it = _wo_list.begin();
		for (; it != _wo_list.end(); ++it) {
			if (*(*it) == win) {
				return *it;
			}
		}
		return NULL;
	}
	//! @brief Searches in PWinObj list if PWinObj wo exists.
	//! @param wo PWinObj to search for.
	//! @return true if found, else false.
	static inline bool windowObjectExists(PWinObj *wo) {
		std::list<PWinObj*>::iterator it =
			find(_wo_list.begin(), _wo_list.end(), wo);
		if (it != _wo_list.end())
			return true;
		return false;
	}

	//! @brief Return Window this PWinObj represents.
	inline Window getWindow(void) const { return _window; }
	//! @brief Sets Window this PWinObj represents.
	inline void setWindow(Window window) { _window = window; }
	//! @brief Returns parent PWinObj.
	inline PWinObj *getParent(void) const { return _parent; }
	//! @brief Sets parent PWinObj.
	inline void setParent(PWinObj *wo) { _parent = wo; }
	//! @brief Returns type of PWinObj.
	inline Type getType(void) const { return _type; }

	//! @brief Returns x coordinate of PWinObj.
	inline int getX(void) const { return _gm.x; }
	//! @brief Returns y coordinate of PWinObj.
	inline int getY(void) const { return _gm.y; }
	//! @brief Returns right edge x coordinate of PWinObj.
	inline int getRX(void) const { return _gm.x + _gm.width; }
	//! @brief Returns bottom edge y coordinate of PWinObj.
	inline int getBY(void) const { return _gm.y + _gm.height; }

	//! @brief Returns width of PWinObj.
	inline uint getWidth(void) const { return _gm.width; }
	//! @brief Returns height of PWinObj:
	inline uint getHeight(void) const { return _gm.height; }

	//! @brief Returns workspace PWinObj is on.
	inline uint getWorkspace(void) const { return _workspace; }
	//! @brief Returns layer PWinObj is in.
	inline uint getLayer(void) const { return _layer; }

	//! @brief Returns mapped state of PWinObj.
	inline bool isMapped(void) const { return _mapped; }
	//! @brief Returns iconofied state of PWinObj.
	inline bool isIconified(void) const { return _iconified; }
	//! @brief Returns hidden state of PWinObj.
	inline bool isHidden(void) const { return _hidden; }
	//! @brief Returns focused state of PWinObj.
	inline bool isFocused(void) const { return _focused; }
	//! @brief Returns sticky state of PWinObj.
	inline bool isSticky(void) const { return _sticky; }

	//! @brief Returns Focusable state of PWinObj.
	inline bool isFocusable(void) const { return _focusable; }

	// interface
	virtual void mapWindow(void);
	virtual void mapWindowRaised(void);
	virtual void unmapWindow(void);
	virtual void iconify(void);
	virtual void stick(void);

	virtual void move(int x, int y, bool do_virtual = true);
	virtual void moveVirtual(int x, int y);
	virtual void resize(uint width, uint height);
	//! @brief Raises PWinObj without respect of layer.
	virtual void raise(void) { XRaiseWindow(_dpy, _window); }
	//! @brief Lowers PWinObj without respect of layer.
	virtual void lower(void) { XLowerWindow(_dpy, _window); }

	virtual void setWorkspace(uint workspace);
	virtual void setLayer(uint layer);
	virtual void setFocused(bool focused);
	virtual void setSticky(bool sticky);
	virtual void setHidden(bool hidden);

	virtual bool giveInputFocus(void);
	virtual void reparent(PWinObj *parent, int x, int y);

	// event interface

	//! @brief Handles button press events, always return NULL.
	virtual ActionEvent *handleButtonPress(XButtonEvent *ev) { return NULL; }
	//! @brief Handles button release events, always return NULL.
	virtual ActionEvent *handleButtonRelease(XButtonEvent *ev) { return NULL; }
	//! @brief Handles key press events, always return NULL.
	virtual ActionEvent *handleKeyPress(XKeyEvent *ev) { return NULL; }
	//! @brief Handles key release vents, always return NULL.
	virtual ActionEvent *handleKeyRelease(XKeyEvent *ev) { return NULL; }
	//! @brief Handles motion events, always return NULL.
	virtual ActionEvent *handleMotionEvent(XMotionEvent *ev) { return NULL; } 
	//! @brief Handles enter events, always return NULL.
	virtual ActionEvent *handleEnterEvent(XCrossingEvent *ev) { return NULL; }
	//! @brief Handles leave events, always return NULL.
	virtual ActionEvent *handleLeaveEvent(XCrossingEvent *ev) { return NULL; }
	//! @brief Handles expose events, always return NULL.
	virtual ActionEvent *handleExposeEvent(XExposeEvent *ev) { return NULL; }

	//! @brief Handles handle map request events, always return NULL.
	virtual ActionEvent *handleMapRequest(XMapRequestEvent *ev) { return NULL; }
	//! @brief Handles handle unmap events, always return NULL.
	virtual ActionEvent *handleUnmapEvent(XUnmapEvent *ev) { return NULL; }

	// operators

	//! @brief Operator matching against Window PWinObj represents..
	virtual bool operator == (const Window &window) {
		return (_window == window);
	}
	//! @brief Operator matching against Window PWinObj represents.
	virtual bool operator != (const Window &window) {
		return (_window != window);
	}

	// other window commands

	//! @brief Clears Window causing a redraw.
	inline void clear(void) { XClearWindow(_dpy, _window); }
	//! @brief Sets Window background colour.
	inline void setBackground(long pixel) {
		XSetWindowBackground(_dpy, _window, pixel);
	}
	//! @brief Sets Window background pixmap.
	inline void setBackgroundPixmap(Pixmap pm) {
		XSetWindowBackgroundPixmap(_dpy, _window, pm);
	}

protected:
	Display *_dpy; //!< Display PWinObj is on.
	Window _window; //!< Window PWinObj represents.
	PWinObj *_parent; //!< Parent PWinObj.

	Type _type; //!< Type of PWinObj.

	Geometry _gm; //!< Geometry of PWinObj.
	int _v_x; //!< Virtual x position used with Viewport.
	int _v_y; //!< Virtual y position used with Viewport.
	uint _workspace; //!< Workspace PWinObj is on.
	uint _layer; //!< Layer PWinObj is in.
	bool _mapped; //!< Mapped state of PWinObj.
	bool _iconified; //!< Iconified state of PWinObj.
	bool _hidden; //!< Hidden state of PWinObj.
	bool _focused; //!< Focused state of PWinObj.
	bool _sticky; //!< Sticky state of PWinObj.
	bool _focusable; //!< Focusable state of PWinObj.

	static PWinObj *_root_wo; //!< Static root PWinObj pointer.
	static PWinObj *_focused_wo; //!< Static focused PWinObj pointer.
	static std::list<PWinObj*> _wo_list; //!< List of PWinObjs.
};

#endif // _WINDOW_OBJECT_HH_
