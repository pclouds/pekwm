//
// ActionHandler.cc for pekwm
// Copyright (C) 2002-2005 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"
#include "ActionHandler.hh"

#include "PWinObj.hh"
#include "PDecor.hh"
#include "PMenu.hh"
#include "PScreen.hh"
#include "Frame.hh"
#include "Client.hh"
#include "Config.hh"
#include "CmdDialog.hh"
#include "Workspaces.hh"
#include "Viewport.hh"
#include "WindowManager.hh"
#include "Util.hh"
#include "RegexString.hh"

#ifdef HARBOUR
#include "Harbour.hh"
#endif // HARBOUR

#ifdef MENUS
#include "PDecor.hh"
#include "PMenu.hh"
#include "WORefMenu.hh"
#include "ActionMenu.hh"
#include "FrameListMenu.hh"
#include "DecorMenu.hh"
#ifdef HARBOUR
#include "HarbourMenu.hh"
#endif // HARBOUR
#endif // MENUS

#include <memory>

extern "C" {
#include <X11/keysym.h>
}

#ifdef DEBUG
#include <iostream>
using std::cerr;
using std::endl;
#endif // DEBUG
using std::auto_ptr;
using std::string;
using std::list;
using std::find;

ActionHandler *ActionHandler::_instance = NULL;

//! @brief ActionHandler constructor
ActionHandler::ActionHandler(WindowManager *wm) :
_wm(wm)
{
#ifdef DEBUG
	if (_instance != NULL) {
		cerr << __FILE__ << "@" << __LINE__ << ": "
				 << "ActionHandler(" << this << ")::ActionHandler(" << wm << ")" << endl
				 << " *** _instance allready set: " << _instance << endl;
	}
#endif // DEBUG
	_instance = this;
}

//! @brief ActionHandler destructor
ActionHandler::~ActionHandler(void)
{
	_instance = NULL;
}

//! @brief Executes an ActionPerformed event.
void
ActionHandler::handleAction(const ActionPerformed &ap)
{
	Client *client = NULL;
	Frame *frame = NULL;
#ifdef MENUS
	PMenu *menu = NULL;
#endif // MENUS
	PDecor *decor = NULL;
	bool matched = false;

	// determine what type if any of the window object that is focused
	if (ap.wo != NULL) {
		if (ap.wo->getType() == PWinObj::WO_CLIENT) {
			client = static_cast<Client*>(ap.wo);
			frame = static_cast<Frame*>(client->getParent());
			decor = static_cast<PDecor*>(frame);
		} else if (ap.wo->getType() == PWinObj::WO_FRAME) {
			frame = static_cast<Frame*>(ap.wo);
			client = static_cast<Client*>(frame->getActiveChild());
			decor = static_cast<PDecor*>(ap.wo);
#ifdef MENUS
		} else if (ap.wo->getType() == PWinObj::WO_MENU) {
			menu = static_cast<PMenu*>(ap.wo);
			decor = static_cast<PDecor*>(ap.wo);
#endif // MENUS
		} else {
			decor = dynamic_cast<PDecor*>(ap.wo);
		}
	}

	// go through the list of actions and execute them
	list<Action>::const_iterator it = ap.ae.action_list.begin();
	for (; it != ap.ae.action_list.end(); ++it, matched = false) {
		// actions valid for all PWinObjs
		if ((matched == false) && (ap.wo != NULL)) {
			matched = true;
			switch (it->getAction()) {
			case ACTION_FOCUS:
				ap.wo->giveInputFocus();
				break;
			case ACTION_UNFOCUS:
				PWinObj::getRootPWinObj()->giveInputFocus();
				break;
			case ACTION_FOCUS_DIRECTIONAL:
				actionFocusDirectional(ap.wo, DirectionType(it->getParamI(0)), it->getParamI(1));
				break;
			default:
				matched = false;
				break;
			};
		}

		// actions valid for Clients and Frames
		if ((matched == false) && (frame != NULL)) {
			matched = true;
			switch (it->getAction()) {
			case ACTION_GROUPING_DRAG:
				if (ap.type == MotionNotify)
					frame->doGroupingDrag(ap.event.motion, client, it->getParamI(0));
				break;
			case ACTION_ACTIVATE_CLIENT:
				if ((ap.type == ButtonPress) || (ap.type == ButtonRelease))
					frame->activateChild(frame->getChildFromPos(ap.event.button->x));
				break;
			case ACTION_MAXFILL:
				frame->setStateMaximized(STATE_SET,
																 it->getParamI(0), it->getParamI(1), true);
				break;
			case ACTION_GROW_DIRECTION:
				frame->growDirection(it->getParamI(0));
				break;
			case ACTION_RESIZE:
				if (ap.type == MotionNotify)
					frame->doResize(ap.event.motion);
				else
					frame->doResize(BorderPosition(it->getParamI(0)));
				break;
			case ACTION_MOVE_RESIZE:
				frame->doKeyboardMoveResize();
				break;
			case ACTION_CLOSE:
				client->close();
				break;
			case ACTION_CLOSE_FRAME:
				frame->close();
				break;
			case ACTION_KILL:
				client->kill();
				break;
			case ACTION_RAISE:
				if (it->getParamI(0) == true) {
					_wm->familyRaiseLower(client, true);
				} else {
					frame->raise();
				}
				break;
			case ACTION_LOWER:
				if (it->getParamI(0) == true) {
					_wm->familyRaiseLower(client, false);
				} else {
					frame->lower();
				}
				break;
			case ACTION_ACTIVATE_OR_RAISE:
				if ((ap.type == ButtonPress) || (ap.type == ButtonRelease)) {
					if (ap.event.button->window == frame->getTitleWindow()) {
						frame->activateChild(frame->getChildFromPos(ap.event.button->x));
					}
				}

				if (frame->isFocused()) {
					frame->raise();
				} else {
					ap.wo->giveInputFocus();
				}
				break;
			case ACTION_MOVE_TO_EDGE:
				frame->moveToEdge(OrientationType(it->getParamI(0)));
				break;
			case ACTION_ACTIVATE_CLIENT_REL:
				frame->activateChildRel(it->getParamI(0));
				break;
			case ACTION_MOVE_CLIENT_REL:
				frame->moveChildRel(it->getParamI(0));
				break;
			case ACTION_ACTIVATE_CLIENT_NUM:
				frame->activateChildNum(it->getParamI(0));
				break;
			case ACTION_SEND_TO_WORKSPACE:
				switch (it->getParamI(0)) {
				case WORKSPACE_LEFT:
				case WORKSPACE_PREV:
					if (Workspaces::instance()->getActive() > 0) {
						frame->setWorkspace(Workspaces::instance()->getActive() - 1);
					} else if (static_cast<uint>(it->getParamI(0)) == WORKSPACE_PREV) {
						frame->setWorkspace(Workspaces::instance()->size() - 1);
					}
			break;
				case WORKSPACE_NEXT:
				case WORKSPACE_RIGHT:
					if ((Workspaces::instance()->getActive() + 1) <
							Workspaces::instance()->size()) {
						frame->setWorkspace(Workspaces::instance()->getActive() + 1);
					} else if (static_cast<uint>(it->getParamI(0)) == WORKSPACE_NEXT) {
						frame->setWorkspace(0);
					}
					break;
				default:
					frame->setWorkspace(it->getParamI(0));
					break;
				}
				break;
			case ACTION_DETACH:
				frame->detachClient(client);
				break;
			case ACTION_ATTACH_MARKED:
				_wm->attachMarked(frame);
				break;
			case ACTION_ATTACH_CLIENT_IN_NEXT_FRAME:
				_wm->attachInNextPrevFrame(client, false, true);
				break;
			case ACTION_ATTACH_CLIENT_IN_PREV_FRAME:
				_wm->attachInNextPrevFrame(client, false, false);
				break;
			case ACTION_ATTACH_FRAME_IN_NEXT_FRAME:
				_wm->attachInNextPrevFrame(client, true, true);
				break;
			case ACTION_ATTACH_FRAME_IN_PREV_FRAME:
				_wm->attachInNextPrevFrame(client, true, false);
				break;
			default:
				matched = false;
				break;
			}
		}

		// Actions valid for Menus
#ifdef MENUS
		if ((matched == false) && (menu != NULL)) {
			matched = true;
			switch (it->getAction()) {
			// menu navigation
			case ACTION_MENU_NEXT:
				menu->selectNextItem();
				break;
			case ACTION_MENU_PREV:
				menu->selectPrevItem();
				break;
			case ACTION_MENU_SELECT:
				menu->exec(menu->getItemCurr());

				// special case: execItem can cause an reload to be issued, if that's
				// the case it causes the list (ae) to change and therefore
				// it can't be used anymore
				return;

				break;
			case ACTION_MENU_ENTER_SUBMENU:
				if ((menu->getItemCurr() != NULL) &&
						(menu->getItemCurr()->getWORef() != NULL) &&
						(menu->getItemCurr()->getWORef()->getType() == PWinObj::WO_MENU)) {
					menu->mapSubmenu(static_cast<PMenu*>(menu->getItemCurr()->getWORef()), true);
				}
				break;
			case ACTION_MENU_LEAVE_SUBMENU:
				menu->gotoParentMenu();
				break;
			case ACTION_CLOSE:
				menu->unmapAll();
				_wm->findWOAndFocus(NULL);
				break;
			default:
				matched = false;
				break;
			}
		}
#endif // MENUS
		// actions valid for pdecor
		if ((matched == false) && (decor != NULL)) {
			matched = true;
			switch (it->getAction()) {
			case ACTION_MOVE:
				decor->doMove((ap.type == MotionNotify) ? ap.event.motion : NULL);
				break;
			case ACTION_CLOSE:
				decor->unmapWindow();
				break;
			case ACTION_WARP_TO_WORKSPACE:
				actionWarpToWorkspace(decor, it->getParamI(0));
				break;
			case ACTION_WARP_TO_VIEWPORT:
				actionWarpToViewport(decor, it->getParamI(0));
				break;
			default:
				matched = false;
				break;
			}
		}

		// Actions valid from everywhere
		if (matched == false) {
			matched = true;
			switch (it->getAction()) {
			case ACTION_SET:
			case ACTION_UNSET:
			case ACTION_TOGGLE:
				handleStateAction(*it, ap.wo, client, frame);
				break;
			case ACTION_NEXT_FRAME:
				actionFocusToggle(ap.ae.sym, it->getParamI(0), 1, false);
				break;
			case ACTION_NEXT_FRAME_MRU:
				actionFocusToggle(ap.ae.sym, it->getParamI(0), 1, true);
				break;
			case ACTION_PREV_FRAME:
				actionFocusToggle(ap.ae.sym, it->getParamI(0), -1, false);
				break;
			case ACTION_PREV_FRAME_MRU:
				actionFocusToggle(ap.ae.sym, it->getParamI(0), -1, true);
				break;
			case ACTION_GOTO_WORKSPACE:
				// if the event was caused by a motion event ( dragging frame to the
				// edge ) or enter event ( moving the pointer to the edge ) we'll want
				// to warp the pointer
				Workspaces::instance()->gotoWorkspace(it->getParamI(0),
																							((ap.type == MotionNotify) ||
																							 (ap.type == EnterNotify)));
				break;
			case ACTION_VIEWPORT_MOVE_XY:
				Workspaces::instance()->getActiveViewport()->move(it->getParamI(0), it->getParamI(1));
				break;
			case ACTION_VIEWPORT_MOVE_DRAG:
				if (ap.type == MotionNotify) {
					Workspaces::instance()->getActiveViewport()->moveDrag(ap.event.motion->x_root, ap.event.motion->y_root);
				}
				break;
			case ACTION_VIEWPORT_MOVE_DIRECTION:
				if (ap.type == KeyPress) {
					Workspaces::instance()->getActiveViewport()->moveDirection(DirectionType(it->getParamI(0)), false);
				} else {
					Workspaces::instance()->getActiveViewport()->moveDirection(DirectionType(it->getParamI(0)), true);
				}
				break;
			case ACTION_VIEWPORT_SCROLL:
				Workspaces::instance()->getActiveViewport()->scroll(it->getParamI(0), it->getParamI(1));
				break;
			case ACTION_VIEWPORT_GOTO:
				Workspaces::instance()->getActiveViewport()->gotoColRow((unsigned) it->getParamI(0), (unsigned) it->getParamI(1));
				break;

			case ACTION_FIND_CLIENT:
				actionFindClient(it->getParamS());
				break;

			case ACTION_EXEC:
				if (it->getParamS().size())
					Util::forkExec(it->getParamS());
				break;
#ifdef MENUS
			case ACTION_SHOW_MENU:
				actionShowMenu(MenuType(it->getParamI(0)), it->getParamI(1),
											 ap.type, client ? client : ap.wo);
				break;
			case ACTION_HIDE_ALL_MENUS:
				_wm->hideAllMenus();
				_wm->findWOAndFocus(NULL);
				break;
#endif // MENUS
			case ACTION_RELOAD:
				_wm->reload();

				// special case: reload causes the list (ae) to change and therefore
				// it can't be used anymore
				return;

				break;
			case ACTION_RESTART:
				_wm->restart();
				break;
			case ACTION_RESTART_OTHER:
				if (it->getParamS().size())
					_wm->restart(it->getParamS());
				break;
			case ACTION_EXIT:
				_wm->shutdown();
				break;
			case ACTION_SHOW_CMD_DIALOG:
				if (_wm->getCmdDialog()->isMapped()) {
					_wm->getCmdDialog()->unmapWindow();
				} else {
					_wm->getCmdDialog()->setWORef(client ? client : ap.wo);
					_wm->getCmdDialog()->mapCenteredOnWORef();
					_wm->getCmdDialog()->giveInputFocus();
				}
				break;
			default:
				matched = false;
				break;
			}
		}
	}
}

//! @brief Handles state actions
void
ActionHandler::handleStateAction(const Action &action, PWinObj *wo,
																 Client *client, Frame *frame)
{
	StateAction sa = static_cast<StateAction>(action.getAction()); // convenience

	bool matched = false;

	// check for frame actions
	if ((matched == false) && (frame != NULL)) {
		matched = true;
		switch (action.getParamI(0)) {
		case ACTION_STATE_MAXIMIZED:
			frame->setStateMaximized(sa, action.getParamI(1),
															 action.getParamI(2), false);
			break;
		case ACTION_STATE_FULLSCREEN:
			frame->setStateFullscreen(sa);
			break;
		case ACTION_STATE_SHADED:
			frame->setShaded(sa);
			break;
		case ACTION_STATE_STICKY:
			frame->setStateSticky(sa);
			break;
		case ACTION_STATE_ALWAYS_ONTOP:
			frame->setStateAlwaysOnTop(sa);
			break;
		case ACTION_STATE_ALWAYS_BELOW:
			frame->setStateAlwaysBelow(sa);
			break;
		case ACTION_STATE_DECOR_BORDER:
			frame->setStateDecorBorder(sa);
			break;
		case ACTION_STATE_DECOR_TITLEBAR:
			frame->setStateDecorTitlebar(sa);
			break;
		case ACTION_STATE_ICONIFIED:
			frame->setStateIconified(sa);
			break;
		case ACTION_STATE_TAGGED:
			frame->setStateTagged(sa, action.getParamI(1));
			break;
		case ACTION_STATE_MARKED:
			client->setStateMarked(sa);
			break;
		case ACTION_STATE_SKIP:
			frame->setStateSkip(sa, action.getParamI(1));
			break;
		case ACTION_STATE_CFG_DENY:
			client->setStateCfgDeny(sa, action.getParamI(1));
			break;
		case ACTION_STATE_DECOR:
			frame->setDecorOverride(sa, action.getParamS());
			break;
		case ACTION_STATE_TITLE:
			frame->setStateTitle(sa, client, action.getParamS());
			break;
		default:
			matched = false;
			break;
		}
	}

	// check for menu actions
	if ((matched == false) && (wo != NULL) &&
			(wo->getType() == PWinObj::WO_MENU)) {
		matched = true;
		switch (action.getParamI(0)) {
		case ACTION_STATE_STICKY:
			wo->stick();
			break;
		default:
			matched = false;
			break;
		}
	}

	if (matched == false) {
		matched = true;
		switch (action.getParamI(0)) {
		case ACTION_STATE_GLOBAL_GROUPING:
			_wm->setStateGlobalGrouping(sa);
			break;
		default:
			matched = false;
			break;
		}
	}
}

//! @brief Checks if motion threshold is within bounds.
bool
ActionHandler::checkAEThreshold(int x, int y, int x_t, int y_t, uint t)
{
	if (((x > x_t) ? (x > (x_t + signed(t))) : (x < (x_t - signed(t)))) ||
			((y > y_t) ? (y > (y_t + signed(t))) : (y < (y_t - signed(t))))) {
		return true;
	}
	return false;
}

//! @brief Searches the actions list for an matching event
ActionEvent*
ActionHandler::findMouseAction(uint button, uint state, MouseEventType type,
															 std::list<ActionEvent> *actions)
{
	if (actions == NULL)
		return NULL;

	state &= ~PScreen::instance()->getNumLock() &
		~PScreen::instance()->getScrollLock() & ~LockMask;
	state &= ~Button1Mask & ~Button2Mask & ~Button3Mask
		& ~Button4Mask & ~Button5Mask;

	list<ActionEvent>::iterator it(actions->begin());
	for (; it != actions->end(); ++it) {
		if ((it->type == unsigned(type)) &&
				((it->mod == MOD_ANY) || (it->mod == state)) &&
				((it->sym == BUTTON_ANY) || (it->sym == button))) {
			return &*it;
		}
	}

	return NULL;
}

//! @brief Searches for a client matching titles and makes it visible
void
ActionHandler::actionFindClient(const std::string &title)
{
	if (title.size() == 0) {
		return;
	}

	Client *client = findClientFromTitle(title);
	if (client == NULL) {
		return;
	}
	Frame *frame = static_cast<Frame*>(client->getParent());

	// make sure it's visible
	if (frame->isMapped() == false) {
		if ((frame->isSticky() == false) &&
		    (frame->getWorkspace() != Workspaces::instance()->getActive())) {
			Workspaces::instance()->setWorkspace(frame->getWorkspace(), false);
		}

		frame->mapWindow();
	}
	if (Workspaces::instance()->getActiveViewport()->isInside(frame) == false) {
		Workspaces::instance()->getActiveViewport()->moveToWO(frame);
	}

	frame->activateChild(client);
	frame->raise();
	frame->giveInputFocus();
}

//! @brief
void
ActionHandler::actionWarpToWorkspace(PDecor *decor, uint direction)
{
	// actually did move
	if (Workspaces::instance()->gotoWorkspace(DirectionType(direction), true) == true) {
		int x, y;
		PScreen::instance()->getMousePosition(x, y);

		decor->move(decor->getClickX() + x - decor->getPointerX(),
								decor->getClickY() + y - decor->getPointerY());
		decor->setWorkspace(Workspaces::instance()->getActive());
	}
}

//! @brief
void
ActionHandler::actionWarpToViewport(PDecor *decor, uint direction)
{
	Viewport *vp = Workspaces::instance()->getActiveViewport();

	// actually did move
	if (vp->moveDirection(DirectionType(direction)) == true) {
		int x, y;
		PScreen::instance()->getMousePosition(x, y);

		decor->move(decor->getClickX() + x - decor->getPointerX(),
								decor->getClickY() + y - decor->getPointerY());
	}
}

//! @brief Tries to find the next/prev frame relative to the focused client
void
ActionHandler::actionFocusToggle(uint button, uint raise, int off, bool mru)
{
	PMenu *p_menu;

	if (mru) {
		p_menu = createMRUMenu();
	} else {
		p_menu = createNextPrevMenu();
	}

	auto_ptr<PMenu> menu(p_menu);

	// no clients in the list
	if (menu->size() == 0) {
		return;
	}

	// unable to grab keyboard
	if (PScreen::instance()->grabKeyboard(PScreen::instance()->getRoot()) == false) {
		return;
	}

	// find the focused window object
	PWinObj *fo_wo = NULL;
	if (PWinObj::getFocusedPWinObj() != NULL) {
		if (PWinObj::getFocusedPWinObj()->getType() == PWinObj::WO_CLIENT) {
			fo_wo = PWinObj::getFocusedPWinObj()->getParent();

			list<PMenu::Item*>::iterator it(menu->m_begin());
			for (; it != menu->m_end(); ++it) {
				if ((*it)->getWORef() == fo_wo) {
					menu->selectItem(it);
					break;
				}
			}
			fo_wo->setFocused(false);

		} else {
#ifdef DEBUG
		cerr << __FILE__ << "@" << __LINE__ << ": "
				 << "ActionHandler(" << this << ")::actionFocusToggle("
				 << button << "," << raise << "," << off << "," << mru << ")" << endl
				 << " *** focused PWinObj != WO_FRAME" << endl;
#endif // DEBUG
		}
	}

	if (Config::instance()->getShowFrameList()) {
		menu->buildMenu();

		Geometry head;
		PScreen::instance()->getHeadInfo(PScreen::instance()->getCurrHead(), head);
		menu->move(head.x + ((head.width - menu->getWidth()) / 2),
							 head.y + ((head.height - menu->getHeight()) / 2));
		menu->setFocused(true);
		menu->mapWindowRaised();
	}

	menu->selectItemRel(off);
	fo_wo = menu->getItemCurr()->getWORef();

	XEvent ev;
	bool cycling = true;
	while (cycling) {
		if (fo_wo != NULL) {
			fo_wo->setFocused(true);
			if (Raise(raise) == ALWAYS_RAISE)	{
				fo_wo->raise();
			}
		}

		XMaskEvent(PScreen::instance()->getDpy(), KeyPressMask|KeyReleaseMask, &ev);
		if (ev.type == KeyPress) {
			if (ev.xkey.keycode == button) {
				if (fo_wo != NULL) {
					fo_wo->setFocused(false);
				}

				menu->selectItemRel(off);
				fo_wo = menu->getItemCurr()->getWORef();
			} else {
				XPutBackEvent (PScreen::instance()->getDpy(), &ev);
				cycling = false;
			}
		} else if (ev.type == KeyRelease) {
			if (IsModifierKey(XKeycodeToKeysym(PScreen::instance()->getDpy(),
												ev.xkey.keycode, 0))) {
				cycling = false;
			}
		} else {
			XPutBackEvent(PScreen::instance()->getDpy(), &ev);
		}
	}

	PScreen::instance()->ungrabKeyboard();

	if (fo_wo != NULL) {
		fo_wo->giveInputFocus();
	}
	if (Raise(raise) == END_RAISE) {
		fo_wo->raise();
	}
}


//! @brief Finds a window in direction and gives it focus
void
ActionHandler::actionFocusDirectional(PWinObj *wo, DirectionType dir, bool raise)
{
	PWinObj *wo_focus =
		Workspaces::instance()->findDirectional(wo, dir, SKIP_FOCUS_TOGGLE);

	if (wo_focus != NULL) {
		if (raise == true) {
			wo_focus->raise();
		}
		wo_focus->giveInputFocus();
	}
}


#ifdef MENUS
//! @brief Toggles visibility of menu of type m_type
void
ActionHandler::actionShowMenu(MenuType m_type, bool stick,
															uint e_type, PWinObj *wo_ref)
{
	PMenu *menu = _wm->getMenu(m_type);
	if (menu == NULL) {
		return;
	}

	if (menu->isMapped()) {
		menu->unmapAll();

	} else {

		// if it's a WORefMenu, set referencing client
		WORefMenu *wo_ref_menu = dynamic_cast<WORefMenu*>(menu);
		if ((wo_ref_menu != NULL) &&
		    (wo_ref_menu->getMenuType() != ROOTMENU_TYPE)) {
			wo_ref_menu->setWORef(wo_ref);
		}

		// mapping can fail because of empty menu, like iconmenu, so we check
		menu->mapUnderMouse();
		if (menu->isMapped()) {
			menu->giveInputFocus();
			if (stick) {
				menu->setSticky(true);
			}

			// if we opened the menu with the keyboard, select item 0
			if (e_type == KeyPress) {
				menu->selectItemNum(0);
			}
		}
	}
}
#endif // MENUS

//! @brief Creates a menu containing a list of Frames currently visible
PMenu*
ActionHandler::createNextPrevMenu(void)
{
	ActionEvent ae; // empty ae, used when inserting
	Viewport *vp = Workspaces::instance()->getActiveViewport();
	PMenu *menu =
		new PMenu(PScreen::instance()->getDpy(), _wm->getTheme(), "Windows");

	list<Frame*>::iterator f_it(_wm->frame_begin());
	for (; f_it != _wm->frame_end(); ++f_it) {
		// if it's not hidden it's either sticky or on this workspace
		if ((*f_it)->isMapped() && vp->isInside(*f_it) && (*f_it)->isFocusable() &&
				((*f_it)->isSkip(SKIP_FOCUS_TOGGLE) == false)) {
			menu->insert(static_cast<Client*>((*f_it)->getActiveChild())->getTitle()->getVisible(), ae, *f_it);
		}
	}

	return menu;
}

//! @brief Creates a menu containing a list of Frames currently visible ( MRU order )
PMenu*
ActionHandler::createMRUMenu(void)
{
	ActionEvent ae; // empty ae, used when inserting
	Viewport *vp = Workspaces::instance()->getActiveViewport();
	PMenu *menu =
		new PMenu(PScreen::instance()->getDpy(), _wm->getTheme(), "MRU Windows");

	Frame *fr;
	list<PWinObj*>::reverse_iterator f_it = _wm->mru_rbegin();
	for (; f_it != _wm->mru_rend(); ++f_it) {
		fr = static_cast<Frame*>(*f_it);
		// if it's not hidden it's either sticky or on this workspace
		if (fr->isMapped() && vp->isInside(fr) && fr->isFocusable() &&
				(fr->isSkip(SKIP_FOCUS_TOGGLE) == false)) {
			menu->insert(static_cast<Client*>(fr->getActiveChild())->getTitle()->getVisible(), ae, fr);
		}
	}

	return menu;
}

//! @brief Searches the client list for a client with a title matching title
Client*
ActionHandler::findClientFromTitle(const std::string &or_title)
{
  RegexString o_rs;

  if (o_rs.parse_match (or_title))
    {
      list<Client*>::iterator it (_wm->client_begin ());
      for (; it != _wm->client_end (); ++it)
        {
          if (o_rs == (*it)->getTitle()->getReal())
            return (*it);
        }
    }

  return NULL;
}
