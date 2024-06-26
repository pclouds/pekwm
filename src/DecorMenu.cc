//
// FrameListMenu.cc for pekwm
// Copyright (C) 2002-2004 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"
#ifdef MENUS

#include "PWinObj.hh"
#include "PDecor.hh"
#include "PMenu.hh"
#include "WORefMenu.hh"
#include "DecorMenu.hh"

#include "Workspaces.hh"
#include "ActionHandler.hh"
#include "Theme.hh"

#include <map>

#ifdef DEBUG
#include <iostream>
using std::cerr;
using std::endl;
#endif // DEBUG
using std::string;
using std::map;

//! @brief Constructor for DecorMenu.
DecorMenu::DecorMenu(PScreen *scr, Theme *theme, ActionHandler *act) :
WORefMenu(scr, theme, "Decor Menu"),
_act(act)
{
	_menu_type = DECORMENU_TYPE;
}

//! @brief Destructor for DecorMenu
DecorMenu::~DecorMenu(void)
{
}

//! @brief Handles button1 release
void
DecorMenu::handleItemExec(PMenu::Item *item)
{
	if (item == NULL) {
		return;
	}

	ActionPerformed ap(_wo_ref, item->getAE());
	_act->handleAction(ap);
}

//! @brief Rebuilds the menu.
void
DecorMenu::reload(void)
{
	// clear the menu before loading
	removeAll();

	// setup dummy action
	Action action;
	ActionEvent ae;

	action.setAction(ACTION_SET);
	action.setParamI(0, ACTION_STATE_DECOR);
	ae.action_list.push_back(action);

	map<string, Theme::PDecorData*>::const_iterator it(_theme->decor_begin());
	for (; it != _theme->decor_end(); ++it) {
		ae.action_list.back().setParamS(it->first);
		insert(it->first, ae, NULL);
	}

	buildMenu(); // rebuild the menu
}

#endif // MENUS
