//
// menu.hh for Blackbox - an X11 Window manager
// Copyright (c) 1997, 1998 by Brad Hughes, bhughes@arn.net
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// (See the included file COPYING / GPL-2.0)
//

#ifndef _blackbox_menu_hh
#define _blackbox_menu_hh

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif

#include "debug.hh"
#include "llist.hh"

// forward declarations
class BlackboxMenu;
class BlackboxMenuItem;
class BlackboxSession;

// Note:  this is a base menu class... it is inherited and expanded upon for
// sessions, windows, and workspaces
class BlackboxMenu {
private:
  llist<BlackboxMenuItem> *menuitems;
  BlackboxSession *session;
  Debugger *debug;

  Bool moving, show_title, visible, sub, movable;
  Display *display;
  GC titleGC, itemGC, pitemGC;
  XContext itemContext;
  
  int which_sub;

  struct menu {
    char *label;
    int x, y, x_move, y_move;
    unsigned width, height,
      title_w, title_h,
      button_w, button_h, item_h;
    unsigned long pframe, ptitle, pitem, ppushed;
    Pixmap item_pixmap, pushed_pixmap;
    Window frame, title;
  } menu;


protected:
  virtual void itemPressed(int, int) = 0;
  virtual void itemReleased(int, int) = 0;
  virtual void titlePressed(int) = 0;
  virtual void titleReleased(int) = 0;

  void showMenu(void);
  void hideMenu(void);
  void raiseMenu(void);
  void showTitle(void);
  void hideTitle(void);
  void moveMenu(int, int);
  void updateMenu(void);

  int insert(char *);
  int insert(char **);
  int insert(char *, int, char * = 0);
  int insert(char *, BlackboxMenu *);
  int remove(char *);
  int remove(int);
  BlackboxMenuItem *at(int);
  BlackboxSession *Session(void);

  void drawSubmenu(int);
  Window createItemWindow(void);


public:
  BlackboxMenu(BlackboxSession *);
  virtual ~BlackboxMenu(void);

  Window windowID(void);
  unsigned int Width(void) { return menu.width; }
  unsigned int Height(void) { return menu.height; }
  unsigned int titleHeight(void) { return menu.title_h; }
  int X(void) { return menu.x; }
  int Y(void) { return menu.y; }
  int menuVisible(void) { return visible; }
  void setMenuLabel(char *);
  const char *label(void) const { return menu.label; }
  int count(void) { return menuitems->count(); }
  Bool hasSubmenu(int);
  void setMovable(Bool);

  void buttonPressEvent(XButtonEvent *);
  void buttonReleaseEvent(XButtonEvent *);
  void motionNotifyEvent(XMotionEvent *);
  void exposeEvent(XExposeEvent *);
};


class BlackboxMenuItem {
public:
  BlackboxMenuItem(Window w, char **u)
    { window = w; ulabel = u; label = 0; exec = 0; sub_menu = 0;
      function = 0; }
  
  BlackboxMenuItem(Window w, char *l, int f)
    { window = w; ulabel = 0; label = l; exec = 0; sub_menu = 0;
      function = f; }
  
  BlackboxMenuItem(Window w, char *l, char *e)
    { window = w; ulabel = 0; label = l; exec = e; sub_menu = 0;
      function = 0; }

  BlackboxMenuItem(Window w, char *l)
    { window = w; ulabel = 0; label = l; exec = 0; sub_menu = 0;
      function = 0; }

  BlackboxMenuItem(Window w, char *l, BlackboxMenu *m)
    { window = w; ulabel = 0; label = l; sub_menu = m; exec = 0;
      function = 0; }

  ~BlackboxMenuItem(void)
    { }

  char *Exec(void) { return exec; }
  char *Label(void) { return label; }
  char **ULabel(void) { return ulabel; }
  int Function(void) { return function; }
  BlackboxMenu *Submenu(void) { return sub_menu; }

protected:


private:
  Window window;

  BlackboxMenu *sub_menu;

  char **ulabel, *label, *exec;
  int function;

  friend BlackboxMenu;
};


#endif
