//
// Window.cc for Blackbox - an X11 Window manager
// Copyright (c) 1997, 1998 by Brad Hughes, bhughes@tcac.net
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Windowmenu.hh"
#include "Workspace.hh"

#include "blackbox.hh"
#include "Icon.hh"
#include "Window.hh"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>


// *************************************************************************
// Window class code
// *************************************************************************

BlackboxWindow::BlackboxWindow(Blackbox *ctrl, Window window) {
  // set control members for operation
  focusable = True;
  moving = resizing = shaded = maximized = visible = iconic = transient =
    focused = stuck = False;

  window_number = -1;
  
  blackbox = ctrl;
  display = ctrl->control();
  image_ctrl = blackbox->imageControl();
  client.window = window;

  frame.window = frame.title = frame.handle = frame.border =
    frame.close_button = frame.iconify_button = frame.maximize_button =
    frame.fbutton = frame.ubutton = frame.pbutton = frame.resize_handle = None;

  decorations.titlebar = decorations.border = decorations.handle =
    decorations.iconify = decorations.maximize = decorations.menu = True;
  functions.resize = functions.move = functions.iconify = functions.maximize =
    True;
  functions.close = decorations.close = False;
  
  client.wm_hint_flags = client.normal_hint_flags = 0;
  client.transient_for = client.transient = 0;
  client.title = 0;
  client.title_len = 0;
  icon = 0;
  client.mwm_hint = 0;
  windowmenu = 0;
  frame.resize_label_w = 0;

  blackbox->syncGrabServer();
  if (! validateClient()) return;

  // fetch client size and placement
  XWindowAttributes wattrib;
  if (XGetWindowAttributes(display, client.window, &wattrib))  { 
    if (! wattrib.override_redirect) { 
      client.x = wattrib.x;
      client.y = wattrib.y;
      client.width = wattrib.width;
      client.height = wattrib.height;
      
      blackbox->saveWindowSearch(client.window, this);
      
      // get size, aspect, minimum/maximum size and other hints set by the
      // client
      getWMHints();
      getWMNormalHints();
      
      // determine if this is a transient window
      Window win;
      if (XGetTransientForHint(display, client.window, &win))
	if (win && (win != client.window))
	  if ((client.transient_for = blackbox->searchWindow(win))
	      != NULL) {
	    client.transient_for->client.transient = this;	  
	    stuck = client.transient_for->stuck;
	    transient = True;
	  } else if (win == client.window_group) {
	    if ((client.transient_for = blackbox->searchGroup(win, this))
		!= NULL) {
	      client.transient_for->client.transient = this;
	      stuck = client.transient_for->stuck;
	      transient = True;
	    }
	  }
      
      // edit the window decorations for the transient window
      if (! transient) {
	if ((client.normal_hint_flags & PMinSize) &&
	    (client.normal_hint_flags & PMaxSize))
	  if (client.max_width == client.min_width &&
	      client.max_height == client.min_height)
	    decorations.maximize = decorations.handle =
	      functions.resize = False;
      } else {
	decorations.border = decorations.handle = decorations.maximize =
	  functions.resize = False;
      }
      
      // check for mwm hints and change the window decorations if necessary
      int format;
      Atom atom_return;
      unsigned long num, len;
      
      XGetWindowProperty(display, client.window, blackbox->MwmHintsAtom(),
			 0, PropMwmHintsElements, False,
			 blackbox->MwmHintsAtom(), &atom_return, &format,
			 &num, &len, (unsigned char **) &client.mwm_hint);
      
      if (client.mwm_hint)
	if (num == PropMwmHintsElements) {
	  if (client.mwm_hint->flags & MwmHintsDecorations)
	    if (client.mwm_hint->decorations & MwmDecorAll)
	      decorations.titlebar = decorations.handle = decorations.border =
		decorations.iconify = decorations.maximize =
		decorations.close = decorations.menu = True;
	    else {
	      decorations.titlebar = decorations.handle = decorations.border =
		decorations.iconify = decorations.maximize =
		decorations.close = decorations.menu = False;
	      
	      if (client.mwm_hint->decorations & MwmDecorBorder)
		decorations.border = True;
	      if (client.mwm_hint->decorations & MwmDecorHandle)
		decorations.handle = True;
	      if (client.mwm_hint->decorations & MwmDecorTitle)
		decorations.titlebar = True;
	      if (client.mwm_hint->decorations & MwmDecorMenu)
		decorations.menu = True;
	      if (client.mwm_hint->decorations & MwmDecorIconify)
		decorations.iconify = True;
	      if (client.mwm_hint->decorations & MwmDecorMaximize)
		decorations.maximize = True;
	    }
	  
	  if (client.mwm_hint->flags & MwmHintsFunctions)
	    if (client.mwm_hint->functions & MwmFuncAll)
	      functions.resize = functions.move = functions.iconify =
		functions.maximize = functions.close = True;
	    else {
	      functions.resize = functions.move = functions.iconify =
		functions.maximize = functions.close = False;
	      
	      if (client.mwm_hint->functions & MwmFuncResize)
		functions.resize = True;
	      if (client.mwm_hint->functions & MwmFuncMove)
		functions.move = True;
	      if (client.mwm_hint->functions & MwmFuncIconify)
		functions.iconify = True;
	      if (client.mwm_hint->functions & MwmFuncMaximize)
		functions.maximize = True;
	      if (client.mwm_hint->functions & MwmFuncClose)
	    functions.close = True;
	    }
	}
      
      frame.bevel_w = blackbox->bevelWidth();
      frame.border_w = ((decorations.border) ? client.width +
			(frame.bevel_w * 2) : 0);
      frame.border_h = ((decorations.border) ? client.height +
			(frame.bevel_w * 2) : 0);
      frame.title_h = blackbox->titleFont()->ascent +
	blackbox->titleFont()->descent + (frame.bevel_w * 2);
      frame.button_h = frame.title_h - 6;
      frame.button_w = frame.button_h;
      frame.rh_w = ((decorations.handle) ? blackbox->handleWidth() : 0);
      frame.rh_h = ((decorations.handle) ? frame.button_h : 0);
      frame.handle_w = ((decorations.handle) ? blackbox->handleWidth() : 0);
      frame.handle_h = ((decorations.handle) ? 
			((decorations.border) ? frame.border_h :
			 client.height) -
			frame.rh_h - 1 : 0);
      frame.title_w = ((decorations.border) ? frame.border_w : client.width) +
	((decorations.handle) ? frame.handle_w + 1: 0);
      frame.y_border = ((decorations.titlebar) ? frame.title_h + 1 : 0);
      frame.x_handle = ((decorations.border) ? frame.border_w + 1 :
			client.width + 1);
      frame.width = frame.title_w;
      frame.height = ((decorations.border) ? frame.border_h : client.height) +
	frame.y_border;
      
      static unsigned int cx = 32, cy = 32;
      Bool cascade = True;
      
      if (blackbox->Startup()) {
	frame.x = client.x - ((decorations.border) ? (frame.bevel_w + 1) : 1);
	frame.y = client.y - frame.title_h -
	  ((decorations.border) ? (frame.bevel_w + 2) : 1);

	if (frame.x >= 0 && frame.y >= 0)
	  cascade = False;
      } else if (transient ||
		 client.normal_hint_flags & (PPosition|USPosition)) {
	switch (client.win_gravity) {
	case StaticGravity:
	  frame.x = client.x - ((decorations.border) ?
				(frame.bevel_w + 1) : 1);
	  frame.y = client.y - frame.title_h -
	    ((decorations.border) ? (frame.bevel_w + 2) : 1);
	  break;
	  
	case NorthGravity:
	  frame.x = client.x - ((decorations.border) ?
				(frame.bevel_w + 1) : 1);
	  frame.y = client.y;
	  break;
	  
	case NorthEastGravity:
	  frame.x = (client.x + client.width) - frame.width;
	  frame.y = client.y;
	  break;
	  
	case WestGravity:
	  frame.x = client.x;
	  frame.y = client.y - frame.title_h -
	    ((decorations.border) ? (frame.bevel_w + 2) : 1);
	  break;
	  
	case EastGravity:
	  frame.x = client.x + client.width - frame.width;
	  frame.y = client.y - frame.title_h -
	    ((decorations.border) ? (frame.bevel_w + 2) : 1);
	  break;
	  
	case SouthWestGravity:
	  frame.x = client.x;
	  frame.y = client.y + client.height - frame.height;
	  break;
	  
	case SouthGravity:
	  frame.x = client.x - ((decorations.border) ?
				(frame.bevel_w + 1) : 1);
	  frame.y = client.y + client.height - frame.height;
	  break;
	  
	case SouthEastGravity:
	  frame.x = client.x + client.width - frame.width;
	  frame.y = client.y + client.height - frame.height;
	  break;
	  
	case CenterGravity:
	  frame.x = client.x + (client.width / 2);
	  frame.y = client.y + (client.height / 2);
	  break;
	  
	case NorthWestGravity:
	case ForgetGravity:
	default:
	  frame.x = client.x;
	  frame.y = client.y;
	  break;
	}
	
	if (frame.x >= 0 && frame.y >= 0)
	  cascade = False;
      }
      
      if (cascade) {
	if (cx > (blackbox->xRes() / 2)) { cx = 32; cy = 32; }
	if (cy > (blackbox->yRes() / 2)) { cx = 32; cy = 32; }
	
	frame.x = cx;
	frame.y = cy;
	
	cy += frame.title_h;
	cx += frame.title_h;
      }
      
      if (frame.x + frame.width > blackbox->xRes())
	frame.x = (blackbox->xRes() - frame.width) / 2;
      if (frame.y + frame.height > blackbox->yRes())
	frame.y = (blackbox->yRes() - frame.height) / 2;
      
      frame.window = createToplevelWindow(frame.x, frame.y, frame.width,
					  frame.height, 1);
      blackbox->saveWindowSearch(frame.window, this);
      
      if (decorations.titlebar) {
	frame.title = createChildWindow(frame.window, 0, 0, frame.title_w,
					frame.title_h, 0l);
	blackbox->saveWindowSearch(frame.title, this);
      }
      
      if (decorations.border) {
	frame.border = createChildWindow(frame.window, 0, frame.y_border,
					 frame.border_w, frame.border_h, 0l);
	blackbox->saveWindowSearch(frame.border, this);
      }
      
      if (decorations.handle) {
	frame.handle = createChildWindow(frame.window, frame.x_handle,
					 frame.y_border, frame.handle_w,
					 frame.handle_h, 0l);
	blackbox->saveWindowSearch(frame.handle, this);
	
	frame.resize_handle =
	  createChildWindow(frame.window, frame.x_handle, frame.y_border +
			    frame.handle_h + 1, frame.rh_w, frame.rh_h, 0l);
	blackbox->saveWindowSearch(frame.resize_handle, this);
      }
      
      getWMProtocols();
      associateClientWindow();
      positionButtons();
      
      XGrabKey(display, XKeysymToKeycode(display, XK_Tab), Mod1Mask,
	       frame.window, True, GrabModeAsync, GrabModeAsync);
      XGrabKey(display, XKeysymToKeycode(display, XK_Left), ControlMask,
	       frame.window, True, GrabModeAsync, GrabModeAsync);
      XGrabKey(display, XKeysymToKeycode(display, XK_Right), ControlMask,
	       frame.window, True, GrabModeAsync, GrabModeAsync);
      
      XLowerWindow(display, client.window);
      if (decorations.titlebar) XMapSubwindows(display, frame.title);
      if (decorations.border) XMapSubwindows(display, frame.border);
      XMapSubwindows(display, frame.window);
      
      if (decorations.menu)
	windowmenu = new Windowmenu(this, blackbox);
      
      createDecorations();
      if (stuck)
	blackbox->toolbar()->workspace(0)->addWindow(this);
      else
	blackbox->toolbar()->currentWorkspace()->addWindow(this);
      
      setFocusFlag(False);
      
      // this is a nasty little hack to make sure that the window knows where
      // it has been placed
      int dx = frame.x, dy = frame.y;
      frame.x = frame.y = 0;
      configureWindow(dx, dy, frame.width, frame.height); 
    }
  }
  
  blackbox->ungrabServer();
}  


BlackboxWindow::~BlackboxWindow(void) {
  blackbox->syncGrabServer();
  
  if (moving) XUngrabPointer(display, CurrentTime);
  else if (resizing) {
    XUngrabPointer(display, CurrentTime);
    delete [] resizeLabel;
  }
  
  blackbox->toolbar()->workspace(workspace_number)->
    removeWindow(this);
  
  if (windowmenu)
    delete windowmenu;
  if (icon)
    delete icon;

  if (client.title)
    if (strcmp(client.title, "Unnamed")) {
      XFree(client.title);
      client.title = 0;
    }
  
  if (client.mwm_hint) {
    XFree(client.mwm_hint);
    client.mwm_hint = 0;
  }
  
  if (client.window_group)
    blackbox->removeGroupSearch(client.window_group);
  
  if (transient && client.transient_for)
    client.transient_for->client.transient = 0;
  
  if (frame.close_button != None) {
    blackbox->removeWindowSearch(frame.close_button);
    XDestroyWindow(display, frame.close_button);
  }
  
  if (frame.iconify_button != None) {
    blackbox->removeWindowSearch(frame.iconify_button);
    XDestroyWindow(display, frame.iconify_button);
  }
  
  if (frame.maximize_button != None) {
    blackbox->removeWindowSearch(frame.maximize_button);
    XDestroyWindow(display, frame.maximize_button);
  }
  
  if (frame.title != None) {
    if (frame.ftitle) {
      image_ctrl->removeImage(frame.ftitle);
      frame.ftitle = None;
    }
    
    if (frame.utitle) {
      image_ctrl->removeImage(frame.utitle);
      frame.utitle = None;
    }
    
    blackbox->removeWindowSearch(frame.title);
    XDestroyWindow(display, frame.title);
  }
  
  if (frame.handle != None) {
    if (frame.fhandle) {
      image_ctrl->removeImage(frame.fhandle);
      frame.fhandle = None;
    }
    
    if (frame.uhandle) {
      image_ctrl->removeImage(frame.uhandle);
      frame.uhandle = None;
    }

    if (frame.rfhandle) {
      image_ctrl->removeImage(frame.rfhandle);
      frame.rfhandle = None;
    }

    if (frame.ruhandle) {
      image_ctrl->removeImage(frame.ruhandle);
      frame.ruhandle = None;
    }

    blackbox->removeWindowSearch(frame.handle);
    blackbox->removeWindowSearch(frame.resize_handle);
    XDestroyWindow(display, frame.resize_handle);
    XDestroyWindow(display, frame.handle);
  }
  
  if (frame.border != None) {
    if (frame.frame) {
      image_ctrl->removeImage(frame.frame);
      frame.frame = None;
    }
    
    blackbox->removeWindowSearch(frame.border);
    XDestroyWindow(display, frame.border);
  }
  
  if (frame.fbutton != None) {
    image_ctrl->removeImage(frame.fbutton);
    frame.fbutton = None;
  }

  if (frame.ubutton != None) {
    image_ctrl->removeImage(frame.ubutton);
    frame.ubutton = None;
  }

  if (frame.pbutton != None) {
    image_ctrl->removeImage(frame.pbutton);
    frame.pbutton = None;
  }

  blackbox->removeWindowSearch(client.window);
  
  blackbox->removeWindowSearch(frame.window);  
  XDestroyWindow(display, frame.window);
  
  blackbox->ungrabServer();
}


// *************************************************************************
// Window decoration code
// *************************************************************************


Window BlackboxWindow::createToplevelWindow(int x, int y, unsigned int width,
					    unsigned int height,
					    unsigned int borderwidth)
{
  XSetWindowAttributes attrib_create;
  unsigned long create_mask = CWBackPixmap|CWBackPixel|CWBorderPixel|
    CWOverrideRedirect |CWCursor|CWEventMask; 
  
  attrib_create.background_pixmap = None;
  attrib_create.background_pixel = attrib_create.border_pixel =
    blackbox->borderColor().pixel;
  attrib_create.override_redirect = True;
  attrib_create.cursor = blackbox->sessionCursor();
  attrib_create.event_mask = ButtonPressMask | EnterWindowMask;
  
  return (XCreateWindow(display, blackbox->Root(), x, y, width, height,
			borderwidth, blackbox->Depth(), InputOutput,
			blackbox->visual(), create_mask,
			&attrib_create));
}


Window BlackboxWindow::createChildWindow(Window parent, int x, int y,
					 unsigned int width,
					 unsigned int height,
					 unsigned int borderwidth)
{
  XSetWindowAttributes attrib_create;
  unsigned long create_mask = CWBackPixmap|CWBackPixel|CWBorderPixel|CWCursor|
    CWEventMask;
  
  attrib_create.background_pixmap = None;
  attrib_create.background_pixel = attrib_create.border_pixel = 
    blackbox->borderColor().pixel;
  attrib_create.cursor = blackbox->sessionCursor();
  attrib_create.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
    ButtonReleaseMask | ExposureMask | EnterWindowMask | LeaveWindowMask |
    ButtonMotionMask;
  
  return (XCreateWindow(display, parent, x, y, width, height, borderwidth,
			blackbox->Depth(), InputOutput, blackbox->visual(),
			create_mask, &attrib_create));
}


void BlackboxWindow::associateClientWindow(void) {
  XSetWindowBorderWidth(display, client.window, 0);
  if (! XFetchName(display, client.window, &client.title))
    client.title = "Unnamed";
  client.title_len = strlen(client.title);
  client.title_text_w = XTextWidth(blackbox->titleFont(), client.title,
				   client.title_len);

  XChangeSaveSet(display, client.window, SetModeInsert);
  XSetWindowAttributes attrib_set;
  if (decorations.border) {
    XSelectInput(display, frame.border, NoEventMask);
    XReparentWindow(display, client.window, frame.border, frame.bevel_w,
		    frame.bevel_w);  
    XSelectInput(display, frame.border, ButtonPressMask | ButtonReleaseMask |
		 ButtonMotionMask | EnterWindowMask | LeaveWindowMask |
		 SubstructureRedirectMask);
  } else {
    XSelectInput(display, frame.window, NoEventMask);
    XReparentWindow(display, client.window, frame.window, 0, frame.y_border);
    XSelectInput(display, frame.window, ButtonPressMask | EnterWindowMask |
		 SubstructureRedirectMask);
  }
  
  XFlush(display);

  attrib_set.event_mask = PropertyChangeMask | FocusChangeMask |
    StructureNotifyMask;
  attrib_set.do_not_propagate_mask = ButtonPressMask | ButtonReleaseMask |
    ButtonMotionMask;
  attrib_set.save_under = False;

  XChangeWindowAttributes(display, client.window, CWEventMask|CWDontPropagate|
                          CWSaveUnder, &attrib_set);


#ifdef SHAPE
  if (blackbox->shapeExtensions()) {
    XShapeSelectInput(display, client.window, ShapeNotifyMask);
    
    int foo;
    unsigned int ufoo;

    XShapeQueryExtents(display, client.window, &frame.shaped, &foo, &foo,
		       &ufoo, &ufoo, &foo, &foo, &foo, &ufoo, &ufoo);
    
    if (frame.shaped) {
      XShapeCombineShape(display, frame.window, ShapeBounding,
			 ((decorations.border) ? frame.bevel_w : 0),
			 frame.y_border +
			 ((decorations.border) ? frame.bevel_w : 0),
			 client.window, ShapeBounding, ShapeSet);
      
      int num = 1;
      XRectangle xrect[2];
      xrect[0].x = xrect[0].y = 0;
      xrect[0].width = frame.width;
      xrect[0].height = frame.y_border;
      
      if (decorations.handle) {
	xrect[1].x = frame.x_handle;
	xrect[1].y = frame.y_border;
	xrect[1].width = frame.handle_w;
	xrect[1].height = frame.handle_h + frame.rh_h + 1;
	num++;
      }
      
      XShapeCombineRectangles(display, frame.window, ShapeBounding, 0, 0,
			      xrect, num, ShapeUnion, Unsorted);
    }
  }
#endif
  
  if (functions.iconify) createIconifyButton();
  if (functions.maximize) createMaximizeButton();
  
  if (frame.ubutton && frame.close_button)
    XSetWindowBackgroundPixmap(display, frame.close_button, frame.ubutton);
  if (frame.ubutton && frame.maximize_button)
    XSetWindowBackgroundPixmap(display, frame.maximize_button, frame.ubutton);
  if (frame.ubutton && frame.iconify_button)
    XSetWindowBackgroundPixmap(display, frame.iconify_button, frame.ubutton);
}


void BlackboxWindow::createDecorations(void) {
  if (decorations.titlebar) {
    frame.ftitle =
      image_ctrl->renderImage(frame.title_w, frame.title_h,
			      blackbox->wResource()->decoration.ftexture,
			      blackbox->wResource()->decoration.fcolor,
			      blackbox->wResource()->decoration.fcolorTo);
    frame.utitle =
      image_ctrl->renderImage(frame.title_w, frame.title_h,
			      blackbox->wResource()->decoration.utexture,
			      blackbox->wResource()->decoration.ucolor,
			      blackbox->wResource()->decoration.ucolorTo);
  }

  if (decorations.border) {
    frame.frame =
      image_ctrl->renderImage(frame.border_w, frame.border_h,
			      blackbox->wResource()->frame.texture |
			      BImage_NoDitherSolid,
			      blackbox->wResource()->frame.color,
                              blackbox->wResource()->frame.color);
    XSetWindowBackgroundPixmap(display, frame.border, frame.frame);
    XClearWindow(display, frame.border);
  }

  if (decorations.handle) {
    frame.fhandle =
      image_ctrl->renderImage(frame.handle_w, frame.handle_h,
			      blackbox->wResource()->decoration.ftexture,
			      blackbox->wResource()->decoration.fcolor,
			      blackbox->wResource()->decoration.fcolorTo);
    frame.uhandle =
      image_ctrl->renderImage(frame.handle_w, frame.handle_h,
			      blackbox->wResource()->decoration.utexture,
			      blackbox->wResource()->decoration.ucolor,
			      blackbox->wResource()->decoration.ucolorTo);
    
    frame.rfhandle =
      image_ctrl->renderImage(frame.rh_w, frame.rh_h,
			      blackbox->wResource()->button.ftexture,
			      blackbox->wResource()->button.fcolor,
			      blackbox->wResource()->button.fcolorTo);
    frame.ruhandle =
      image_ctrl->renderImage(frame.rh_w, frame.rh_h,
			      blackbox->wResource()->button.utexture,
			      blackbox->wResource()->button.ucolor,
			      blackbox->wResource()->button.ucolorTo);
  }
  
  frame.fbutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			    blackbox->wResource()->button.ftexture,
			    blackbox->wResource()->button.fcolor,
			    blackbox->wResource()->button.fcolorTo);
  frame.ubutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			    blackbox->wResource()->button.utexture,
			    blackbox->wResource()->button.ucolor,
			    blackbox->wResource()->button.ucolorTo);
  frame.pbutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			  blackbox->wResource()->button.ptexture,
			  blackbox->wResource()->button.pressed,
			  blackbox->wResource()->button.pressedTo);
}


void BlackboxWindow::positionButtons(void) {
  if ((frame.title_w > ((frame.button_w + 4) * 6)) &&
      (client.title_text_w + 4 +
       (((frame.button_w + 4) * 3) + 2) < frame.title_w)) {
    if (decorations.iconify && frame.iconify_button != None) {
      XMoveResizeWindow(display, frame.iconify_button, 2, 2, frame.button_w,
			frame.button_h);
      XMapWindow(display, frame.iconify_button);
      XClearWindow(display, frame.iconify_button);
    }
    
    int bx = frame.title_w - (frame.button_w + 4);

    if (decorations.close && frame.close_button != None) {
      XMoveResizeWindow(display, frame.close_button, bx, 2, frame.button_w,
			frame.button_h);
      bx -= (frame.button_w + 4);
      XMapWindow(display, frame.close_button);
      XClearWindow(display, frame.close_button);
    }

    if (decorations.maximize && frame.maximize_button != None) {
      XMoveResizeWindow(display, frame.maximize_button, bx, 2, frame.button_w,
			frame.button_h);
      XMapWindow(display, frame.maximize_button);
      XClearWindow(display, frame.maximize_button);
    }
  } else {
    if (frame.iconify_button) XUnmapWindow(display, frame.iconify_button);
    if (frame.maximize_button) XUnmapWindow(display, frame.maximize_button);
    if (frame.close_button) XUnmapWindow(display, frame.close_button);
  }
}


void BlackboxWindow::createCloseButton(void) {
  if (decorations.close && frame.title != None &&
      frame.close_button == None) {
    frame.close_button =
      createChildWindow(frame.title, 0, 0, frame.button_w, frame.button_h, 1);
    if (frame.ubutton != None)
      XSetWindowBackgroundPixmap(display, frame.close_button, frame.ubutton);
    blackbox->saveWindowSearch(frame.close_button, this);
  }
}


void BlackboxWindow::createIconifyButton(void) {
  if (decorations.iconify && frame.title != None) {
    frame.iconify_button =
      createChildWindow(frame.title, 0, 0, frame.button_w, frame.button_h, 1);
    blackbox->saveWindowSearch(frame.iconify_button, this);
  }
}


void BlackboxWindow::createMaximizeButton(void) {
  if (decorations.maximize && frame.title != None) {
    frame.maximize_button =
      createChildWindow(frame.title, 0, 0, frame.button_w, frame.button_h, 1);
    blackbox->saveWindowSearch(frame.maximize_button, this);
  }
}


void BlackboxWindow::Reconfigure(void) {
  frame.bevel_w = blackbox->bevelWidth();
  frame.border_w = ((decorations.border) ? client.width +
		    (frame.bevel_w * 2) : 0);
  frame.border_h = ((decorations.border) ? client.height +
		    (frame.bevel_w * 2) : 0);
  frame.title_h = ((decorations.titlebar) ? (blackbox->titleFont()->ascent +
					     blackbox->titleFont()->descent +
					     (frame.bevel_w * 2)) : 0);
  frame.button_h = frame.title_h - 6;
  frame.button_w = frame.button_h;
  frame.rh_w = ((decorations.handle) ? blackbox->handleWidth() : 0);
  frame.rh_h = ((decorations.handle) ? frame.button_h : 0);
  frame.handle_w = ((decorations.handle) ? blackbox->handleWidth() : 0);
  frame.handle_h = ((decorations.handle) ?
		    frame.border_h - frame.rh_h - 1 : 0);
  frame.title_w = ((decorations.border) ? frame.border_w +
		   ((decorations.handle) ? frame.handle_w + 1: 0) :
		   client.width);
  frame.y_border = ((decorations.titlebar) ? frame.title_h + 1 : 0);
  frame.x_handle = ((decorations.border) ? frame.border_w + 1 :
		    client.width + 1);
  frame.width = frame.title_w;
  frame.height = ((decorations.border) ? frame.border_h : client.height) +
    frame.y_border;
  
  client.x = frame.x + ((decorations.border) ? (frame.bevel_w + 1) : 1);
  client.y = frame.y + frame.y_border +
    ((decorations.border) ? frame.bevel_w : 0);

  if (client.title)
    client.title_text_w = XTextWidth(blackbox->titleFont(), client.title,
                                     client.title_len);

  XResizeWindow(display, frame.window, frame.width,
                ((shaded) ? frame.title_h : frame.height));

  if (decorations.titlebar)
    XResizeWindow(display, frame.title, frame.title_w, frame.title_h);
  
  if (decorations.border) {
    XMoveResizeWindow(display, frame.border, 0, frame.title_h + 1,
		      frame.border_w, frame.border_h);  
    XMoveResizeWindow(display, client.window, frame.bevel_w, frame.bevel_w,
		      client.width, client.height);
  } else
    XMoveResizeWindow(display, client.window, 0, frame.title_h + 1,
		      client.width, client.height);
  
  if (decorations.handle) {
    XMoveResizeWindow(display, frame.handle, frame.border_w + 1,
		      frame.title_h + 1,
		      frame.handle_w, frame.handle_h);
    
    XMoveResizeWindow(display, frame.resize_handle, frame.border_w + 1,
		      frame.handle_h + frame.title_h + 2,
		      frame.rh_w, frame.rh_h);
  }

#ifdef SHAPE
  if (blackbox->shapeExtensions()) {
    if (frame.shaped) {
      XShapeCombineShape(display, frame.window, ShapeBounding,
			 ((decorations.border) ? frame.bevel_w : 0),
			 frame.title_h +
			 ((decorations.border) ? (frame.bevel_w + 1) : 1),
			 client.window, ShapeBounding, ShapeSet);
      
      int num = 1;
      XRectangle xrect[2];
      xrect[0].x = xrect[0].y = 0;
      xrect[0].width = frame.title_w;
      xrect[0].height = frame.title_h + 1;
      
      if (decorations.handle) {
	xrect[1].x = frame.border_w + 1;
	xrect[1].y = frame.title_h + 1;
	xrect[1].width = frame.handle_w;
	xrect[1].height = frame.border_h;
	num++;
      }
      
      XShapeCombineRectangles(display, frame.window, ShapeBounding, 0, 0,
			      xrect, num, ShapeUnion, Unsorted);
    }
  }
#endif

  Pixmap tmp = frame.fbutton;
  
  frame.fbutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			    blackbox->wResource()->button.ftexture,
			    blackbox->wResource()->button.fcolor,
			    blackbox->wResource()->button.fcolorTo);
  if (tmp) image_ctrl->removeImage(tmp);
  
  tmp = frame.ubutton;
  frame.ubutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			    blackbox->wResource()->button.utexture,
			    blackbox->wResource()->button.ucolor,
			    blackbox->wResource()->button.ucolorTo);
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.pbutton;
  frame.pbutton =
    image_ctrl->renderImage(frame.button_w, frame.button_h,
			  blackbox->wResource()->button.ptexture,
			  blackbox->wResource()->button.pressed,
			  blackbox->wResource()->button.pressedTo);
  if (tmp) image_ctrl->removeImage(tmp);
  
  if (frame.iconify_button) XSetWindowBorder(display, frame.iconify_button,
					     blackbox->borderColor().pixel);
  if (frame.maximize_button) XSetWindowBorder(display, frame.maximize_button,
					      blackbox->borderColor().pixel);
  if (frame.close_button) XSetWindowBorder(display, frame.close_button,
					   blackbox->borderColor().pixel);
  
  positionButtons();
  
  if (decorations.titlebar) {
    tmp = frame.ftitle;
    frame.ftitle =
      image_ctrl->renderImage(frame.title_w, frame.title_h,
			      blackbox->wResource()->decoration.ftexture,
			      blackbox->wResource()->decoration.fcolor,
			      blackbox->wResource()->decoration.fcolorTo);
    if (tmp) image_ctrl->removeImage(tmp);

    tmp = frame.utitle;
    frame.utitle =
      image_ctrl->renderImage(frame.title_w, frame.title_h,
			      blackbox->wResource()->decoration.utexture,
			      blackbox->wResource()->decoration.ucolor,
			      blackbox->wResource()->decoration.ucolorTo);
    if (tmp) image_ctrl->removeImage(tmp);
  }

  if (decorations.border) {
    tmp = frame.frame;
    frame.frame =
      image_ctrl->renderImage(frame.border_w, frame.border_h,
			      blackbox->wResource()->frame.texture |
			      BImage_NoDitherSolid,
			      blackbox->wResource()->frame.color,
                              blackbox->wResource()->frame.color);
    if (tmp) image_ctrl->removeImage(tmp);
    XSetWindowBackgroundPixmap(display, frame.border, frame.frame);
    XClearWindow(display, frame.border);
  }

  if (decorations.handle) {
    tmp = frame.fhandle;
    frame.fhandle =
      image_ctrl->renderImage(frame.handle_w, frame.handle_h,
			      blackbox->wResource()->decoration.ftexture,
			      blackbox->wResource()->decoration.fcolor,
			      blackbox->wResource()->decoration.fcolorTo);
    if (tmp) image_ctrl->removeImage(tmp);
    
    tmp = frame.uhandle;
    frame.uhandle =
      image_ctrl->renderImage(frame.handle_w, frame.handle_h,
			      blackbox->wResource()->decoration.utexture,
			      blackbox->wResource()->decoration.ucolor,
			      blackbox->wResource()->decoration.ucolorTo);
    if (tmp) image_ctrl->removeImage(tmp);
    
    tmp = frame.rfhandle;
    frame.rfhandle =
      image_ctrl->renderImage(frame.rh_w, frame.rh_h,
			      blackbox->wResource()->button.ftexture,
			      blackbox->wResource()->button.fcolor,
			      blackbox->wResource()->button.fcolorTo);
    if (tmp) image_ctrl->removeImage(tmp);
    
    tmp = frame.ruhandle;
    frame.ruhandle =
      image_ctrl->renderImage(frame.rh_w, frame.rh_h,
			      blackbox->wResource()->button.utexture,
			      blackbox->wResource()->button.ucolor,
			      blackbox->wResource()->button.ucolorTo);
    if (tmp) image_ctrl->removeImage(tmp);
  }
  
  XSetWindowBorder(display, frame.window, blackbox->borderColor().pixel);
  XSetWindowBackground(display, frame.window, blackbox->borderColor().pixel);
  XClearWindow(display, frame.window);
  setFocusFlag(focused);
  if (decorations.titlebar) drawTitleWin();
  drawAllButtons();

  configureWindow(frame.x, frame.y, frame.width, frame.height);
  
  if (windowmenu) {
    windowmenu->Move(windowmenu->X(), frame.y + frame.title_h);
    windowmenu->Reconfigure();
  }
}


// *************************************************************************
// Window protocol and ICCCM code
// *************************************************************************

Bool BlackboxWindow::getWMProtocols(void) {
  Atom *proto;
  int num_return = 0;

  blackbox->syncGrabServer();
  if (! validateClient()) return False;
  
  if (XGetWMProtocols(display, client.window, &proto, &num_return)) {
    for (int i = 0; i < num_return; ++i) {
      if (proto[i] == blackbox->DeleteAtom()) {
	functions.close = decorations.close = True;
	createCloseButton();
	positionButtons();
	if (windowmenu) windowmenu->setClosable();
      } else if (proto[i] ==  blackbox->FocusAtom()) {
	focusable = True;
      } else if (proto[i] ==  blackbox->StateAtom()) {
	unsigned long *state = 0;
	fetchWMState(state);
	
	if (state) {
	  switch (*state) {
	  case WithdrawnState:
	    withdrawWindow();
	    setFocusFlag(False);
	    break;
	    
	  case IconicState:
	    iconifyWindow();
	    break;
	    
	  case NormalState:
	  default:
	    deiconifyWindow();
	    setFocusFlag(False);
	    break;
	  }
	}
      } else  if (proto[i] ==  blackbox->ColormapAtom()) {
      }
    }
    
    XFree(proto);
  }
  
  blackbox->ungrabServer();
  return True;
}


Bool BlackboxWindow::getWMHints(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return False;
  
  XWMHints *wmhint = XGetWMHints(display, client.window);
  if (! wmhint) {
    visible = True;
    iconic = False;
    focus_mode = F_Passive;
    client.window_group = None;
    client.initial_state = NormalState;
  } else {
    client.wm_hint_flags = wmhint->flags;
    if (wmhint->flags & InputHint) {
      if (wmhint->input == True) {
	if (focusable)
	  focus_mode = F_LocallyActive;
	else
	  focus_mode = F_Passive;
      } else {
	if (focusable)
	  focus_mode = F_GloballyActive;
	else
	  focus_mode = F_NoInput;
      }
    } else
      focus_mode = F_Passive;
    
    if (wmhint->flags & StateHint)
      client.initial_state = wmhint->initial_state;
    else
      client.initial_state = NormalState;
    
    if (wmhint->flags & WindowGroupHint) {
      if (! client.window_group) {
	client.window_group = wmhint->window_group;
	blackbox->saveGroupSearch(client.window_group, this);
      }
    } else
      client.window_group = None;
  }
  
  blackbox->ungrabServer();
  return True;
}


Bool BlackboxWindow::getWMNormalHints(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return False;
  
  long icccm_mask;
  XSizeHints sizehint;
  if (! XGetWMNormalHints(display, client.window, &sizehint, &icccm_mask)) {
    client.min_width = client.min_height =
      client.base_width = client.base_height =
      client.width_inc = client.height_inc = 1;
    client.max_width = blackbox->xRes();
    client.max_height = blackbox->yRes();
    client.min_aspect_x = client.min_aspect_y =
      client.max_aspect_x = client.max_aspect_y = 1;
  } else {
    client.normal_hint_flags = sizehint.flags;
    if (sizehint.flags & PMinSize) {
      client.min_width = sizehint.min_width;
      client.min_height = sizehint.min_height;
    } else
      client.min_width = client.min_height = 1;
    
    if (sizehint.flags & PMaxSize) {
      client.max_width = sizehint.max_width;
      client.max_height = sizehint.max_height;
    } else {
      client.max_width = blackbox->xRes();
      client.max_height = blackbox->yRes();
    }
    
    if (sizehint.flags & PResizeInc) {
      client.width_inc = sizehint.width_inc;
      client.height_inc = sizehint.height_inc;
    } else
      client.width_inc = client.height_inc = 1;
    
    if (sizehint.flags & PAspect) {
      client.min_aspect_x = sizehint.min_aspect.x;
      client.min_aspect_y = sizehint.min_aspect.y;
      client.max_aspect_x = sizehint.max_aspect.x;
      client.max_aspect_y = sizehint.max_aspect.y;
    } else
      client.min_aspect_x = client.min_aspect_y =
	client.max_aspect_x = client.max_aspect_y = 1;    
    
    if (sizehint.flags & PBaseSize) {
      client.base_width = sizehint.base_width;
      client.base_height = sizehint.base_height;
    } else
      client.base_width = client.base_height = 0;
    
    if (sizehint.flags & PWinGravity)
      client.win_gravity = sizehint.win_gravity;
    else
      client.win_gravity =
	((icccm_mask == (USPosition | USSize | PPosition |
			 PSize | PMinSize | PMaxSize | PResizeInc | PAspect))
	 ? NorthWestGravity : StaticGravity);
  }

  blackbox->ungrabServer();
  return True;
}


// *************************************************************************
// Window utility function code
// *************************************************************************

void BlackboxWindow::configureWindow(int dx, int dy, unsigned int dw,
				     unsigned int dh) {
  //
  // configure our client and decoration windows according to the frame size
  // changes passed as the arguments
  //

  Bool resize, send_event;
  if (dw > (client.max_width +
	    ((decorations.border) ? (frame.bevel_w * 2) : 0) +
	    ((decorations.handle) ? frame.handle_w + 1 : 0)))
    dw = client.max_width +
      ((decorations.border) ? (frame.bevel_w * 2) : 0) +
      ((decorations.handle) ? frame.handle_w + 1 : 0);
  if (dh > (client.max_height + frame.y_border +
	    ((decorations.border) ?  (frame.bevel_w * 2) : 0)))
    dh = client.max_height + frame.y_border +
      ((decorations.border) ? (frame.bevel_w * 2) : 0);
  
  if ((((signed) frame.width) + dx) < 0) dx = 0;
  if ((((signed) frame.height) + dy) < 0) dy = 0;
  
  resize = ((dw != frame.width) || (dh != frame.height));
  send_event = ((dx != frame.x || dy != frame.y) && (! resize));
  
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  if (resize) {
    frame.x = dx;
    frame.y = dy;
    frame.width = dw;
    frame.height = dh;
    
    frame.border_w = ((decorations.border) ? frame.width -
		      ((decorations.handle) ? (frame.handle_w + 1) : 0) : 0);
    frame.border_h = ((decorations.border) ? frame.height -
		      frame.y_border : 0);
    frame.title_w = frame.width;
    frame.handle_h = ((decorations.handle) ?
		      dh - frame.y_border - frame.rh_h - 1 : 0);
    client.x = dx + ((decorations.border) ? frame.bevel_w : 0);
    client.y = dy + frame.y_border +
      ((decorations.border) ? frame.bevel_w : 0);
    client.width = ((decorations.border) ? frame.border_w -
		    (frame.bevel_w * 2) : frame.width -
		    ((decorations.handle) ? frame.handle_w + 1 : 0));
    client.height = ((decorations.border) ?
		     frame.border_h - (frame.bevel_w * 2) :
		     frame.height - frame.y_border);
    frame.x_handle = ((decorations.border) ? frame.border_w + 1 :
		      client.width + 1);

    XWindowChanges xwc;
    xwc.x = dx;
    xwc.y = dy;
    xwc.width = dw;
    xwc.height = ((shaded) ? frame.title_h : dh);
    XConfigureWindow(display, frame.window, CWX|CWY|CWWidth|CWHeight, &xwc);

    if (decorations.titlebar)
      XResizeWindow(display, frame.title, frame.title_w, frame.title_h);

    positionButtons();
    
    if (decorations.handle) {
      XMoveResizeWindow(display, frame.handle, frame.x_handle,
			frame.y_border, frame.handle_w, frame.handle_h);
      
      XMoveWindow(display, frame.resize_handle, frame.x_handle,
		  frame.y_border + frame.handle_h + 1);
    }
    
    if (decorations.border) {
      XResizeWindow(display, frame.border, frame.border_w, frame.border_h);
      XMoveResizeWindow(display, client.window, frame.bevel_w,
			frame.bevel_w, client.width, client.height);
    } else
      XMoveResizeWindow(display, client.window, 0, frame.y_border,
			client.width, client.height);
    
    Pixmap tmp;
    
    if (decorations.titlebar) {
      tmp = frame.ftitle;
      frame.ftitle =
	image_ctrl->renderImage(frame.title_w, frame.title_h,
				blackbox->wResource()->decoration.ftexture,
				blackbox->wResource()->decoration.fcolor,
				blackbox->wResource()->decoration.fcolorTo);
      if (tmp) image_ctrl->removeImage(tmp);
      
      tmp = frame.utitle;
      frame.utitle =
	image_ctrl->renderImage(frame.title_w, frame.title_h,
				blackbox->wResource()->decoration.utexture,
				blackbox->wResource()->decoration.ucolor,
				blackbox->wResource()->decoration.ucolorTo);
      if (tmp) image_ctrl->removeImage(tmp);
    }
    
    if (decorations.handle) {
      tmp = frame.fhandle;
      frame.fhandle =
	image_ctrl->renderImage(frame.handle_w, frame.handle_h,
				blackbox->wResource()->decoration.ftexture,
				blackbox->wResource()->decoration.fcolor,
				blackbox->wResource()->decoration.fcolorTo);
      if (tmp) image_ctrl->removeImage(tmp);
      
      tmp = frame.uhandle;
      frame.uhandle =
	image_ctrl->renderImage(frame.handle_w, frame.handle_h,
				blackbox->wResource()->decoration.utexture,
				blackbox->wResource()->decoration.ucolor,
				blackbox->wResource()->decoration.ucolorTo);
      if (tmp) image_ctrl->removeImage(tmp);
    }
    
    setFocusFlag(focused);
    
    if (decorations.border) {
      tmp = frame.frame;
      frame.frame =
	image_ctrl->renderImage(frame.border_w, frame.border_h,
			        blackbox->wResource()->frame.texture |
				BImage_NoDitherSolid,
			        blackbox->wResource()->frame.color,
			        blackbox->wResource()->frame.color);
      if (tmp) image_ctrl->removeImage(tmp);
      XSetWindowBackgroundPixmap(display, frame.border, frame.frame);
      XClearWindow(display, frame.border);
    }
    
    if (decorations.titlebar) drawTitleWin();
    drawAllButtons();
  } else {
    frame.x = dx;
    frame.y = dy;
    
    client.x = dx + ((decorations.border) ? (frame.bevel_w + 1) : 1);
    client.y = dy + frame.y_border +
      ((decorations.border) ? frame.bevel_w : 0);
    
    XWindowChanges xwc;
    xwc.x = dx;
    xwc.y = dy;
    XConfigureWindow(display, frame.window, CWX|CWY, &xwc);
  }
  
  if (send_event) {
    XEvent event;
    event.type = ConfigureNotify;
    
    event.xconfigure.display = display;
    event.xconfigure.event = client.window;
    event.xconfigure.window = client.window;
    event.xconfigure.x = client.x;
    event.xconfigure.y = client.y;
    event.xconfigure.width = client.width;
    event.xconfigure.height = client.height;
    event.xconfigure.border_width = 0;
    event.xconfigure.above = frame.window;
    event.xconfigure.override_redirect = False;
    
    XSendEvent(display, client.window, False, StructureNotifyMask, &event);
  }

  blackbox->ungrabServer();
}


Bool BlackboxWindow::setInputFocus(void) {
  if (((signed) (frame.x + frame.width)) < 0) {
    if (((signed) (frame.y + frame.y_border)) < 0)
      configureWindow(0, 0, frame.width, frame.height);
    else if (frame.y > (signed) blackbox->yRes())
      configureWindow(0, blackbox->yRes() - frame.height, frame.width,
		      frame.height);
    else
      configureWindow(0, frame.y, frame.width, frame.height);
  } else if (frame.x > (signed) blackbox->xRes()) {
    if (((signed) (frame.y + frame.y_border)) < 0)
      configureWindow(blackbox->xRes() - frame.width, 0, frame.width,
		      frame.height);
    else if (frame.y > (signed) blackbox->yRes())
      configureWindow(blackbox->xRes() - frame.width,
		      blackbox->yRes() - frame.height, frame.width,
		      frame.height);
    else
      configureWindow(blackbox->xRes() - frame.width, frame.y,
		      frame.width, frame.height);
  }
  
  
  blackbox->syncGrabServer();
  if (! validateClient()) return False;

  Bool ret = False;

  switch (focus_mode) {
  case F_NoInput:
  case F_GloballyActive:
    if (decorations.titlebar) drawTitleWin();
    break;
    
  case F_LocallyActive:
  case F_Passive:
    if (client.transient)
      ret = client.transient->setInputFocus();
    else
      if (! focused) {
	if (XSetInputFocus(display, client.window, RevertToPointerRoot,
			   CurrentTime))
	  ret = True;
      } else
	ret = True;
    
    break;
  }
  
  blackbox->ungrabServer();
  
  return ret;
}


void BlackboxWindow::iconifyWindow(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  if (windowmenu) windowmenu->Hide();
  
  XUnmapWindow(display, frame.window);
  visible = False;
  iconic = True;
  focused = False;    
  
  if (transient) {
    if (! client.transient_for->iconic)
      client.transient_for->iconifyWindow();
  } else
    icon = new BlackboxIcon(blackbox, this);
  
  if (client.transient)
    if (! client.transient->iconic)
      client.transient->iconifyWindow();
  
  unsigned long state[3];
  state[0] = (unsigned long) IconicState;
  state[1] = (unsigned long) None;
  state[2] = (unsigned long) workspace_number;
  XChangeProperty(display, client.window, blackbox->StateAtom(),
		  blackbox->StateAtom(), 32, PropModeReplace,
		  (unsigned char *) state, 3);

  blackbox->ungrabServer();
}


void BlackboxWindow::deiconifyWindow(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  blackbox->reassociateWindow(this);
  
  XMapWindow(display, frame.window);
    
  XMapSubwindows(display, frame.window);
  visible = True;
  iconic = False;
  
  if (client.transient) client.transient->deiconifyWindow();
    
  unsigned long state[3];
  state[0] = (unsigned long) NormalState;
  state[1] = (unsigned long) None;
  state[2] = (unsigned long) workspace_number;
  XChangeProperty(display, client.window, blackbox->StateAtom(),
		  blackbox->StateAtom(), 32, PropModeReplace,
		  (unsigned char *) state, 3);
  
  if (icon) {
    delete icon;
    removeIcon();
  }

  blackbox->ungrabServer();
}


void BlackboxWindow::closeWindow(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  XEvent ce;
  ce.xclient.type = ClientMessage;
  ce.xclient.message_type = blackbox->ProtocolsAtom();
  ce.xclient.display = display;
  ce.xclient.window = client.window;
  ce.xclient.format = 32;
  ce.xclient.data.l[0] = blackbox->DeleteAtom();
  ce.xclient.data.l[1] = CurrentTime;
  ce.xclient.data.l[2] = 0l;
  ce.xclient.data.l[3] = 0l;
  ce.xclient.data.l[4] = 0l;
  XSendEvent(display, client.window, False, NoEventMask, &ce);

  blackbox->ungrabServer();
}


void BlackboxWindow::withdrawWindow(void) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  focused = False;
  visible = False;
  
  XUnmapWindow(display, frame.window);
  if (windowmenu) windowmenu->Hide();

  unsigned long state[3];
  state[0] = (unsigned long) NormalState;
  state[1] = (unsigned long) None;
  state[2] = (unsigned long) workspace_number;
  XChangeProperty(display, client.window, blackbox->StateAtom(),
		  blackbox->StateAtom(), 32, PropModeReplace,
		  (unsigned char *) state, 3);

  blackbox->ungrabServer();
}


int BlackboxWindow::setWindowNumber(int n) {
  window_number = n;
  return window_number;
}


int BlackboxWindow::setWorkspace(int n) {
  workspace_number = n;
  return workspace_number;
}


void BlackboxWindow::maximizeWindow(void) {
  if (! maximized) {
    int dx, dy;
    unsigned int dw, dh;

    frame.x_maximize = frame.x;
    frame.y_maximize = frame.y;
    frame.w_maximize = frame.width;
    frame.h_maximize = frame.height;
    
    dw = blackbox->xRes() - ((decorations.handle) ? frame.handle_w + 1: 0) -
      client.base_width - ((decorations.border) ? (frame.bevel_w * 2) : 0);
    dh = blackbox->yRes() - blackbox->toolbar()->Height() -
      frame.y_border - client.base_height -
      ((decorations.border) ? (frame.bevel_w * 2) : 0);

    if (dw < client.min_width) dw = client.min_width;
    if (dh < client.min_height) dh = client.min_height;
    if (dw > client.max_width) dw = client.max_width;
    if (dh > client.max_height) dh = client.max_height;
    
    dw /= client.width_inc;
    dh /= client.height_inc;
    
    dw = (dw * client.width_inc) + client.base_width;
    dh = (dh * client.height_inc) + client.base_height;

    dw += ((decorations.handle) ? frame.handle_w + 1 : 0) +
      ((decorations.border) ? (frame.bevel_w * 2) : 0);
    dh += frame.y_border + ((decorations.border) ? (frame.bevel_w * 2) : 0);

    dx = ((blackbox->xRes() - dw) / 2) - 1;
    dy = ((blackbox->yRes() - blackbox->toolbar()->Height())
	  - dh) / 2;
    
    maximized = True;
    shaded = False;
    configureWindow(dx, dy, dw, dh);
    blackbox->toolbar()->workspace(workspace_number)->
      raiseWindow(this);
  } else {
    configureWindow(frame.x_maximize, frame.y_maximize, frame.w_maximize,
                    frame.h_maximize);
    maximized = False;
  }
}


void BlackboxWindow::shadeWindow(void) {
  if (decorations.titlebar)
    if (shaded) {
      XResizeWindow(display, frame.window, frame.width, frame.height);
      shaded = False;
    } else {
      XResizeWindow(display, frame.window, frame.width, frame.title_h);
    shaded = True;
    }
}


void BlackboxWindow::setFocusFlag(Bool focus) {
  focused = focus;

  if (decorations.titlebar) {
    XSetWindowBackgroundPixmap(display, frame.title,
			       (focused) ? frame.ftitle : frame.utitle);
    XClearWindow(display, frame.title);
  }

  if (decorations.handle) {
    XSetWindowBackgroundPixmap(display, frame.handle,
			       (focused) ? frame.fhandle : frame.uhandle);
    XSetWindowBackgroundPixmap(display, frame.resize_handle,
			       (focused) ? frame.rfhandle : frame.ruhandle);
    XClearWindow(display, frame.handle);
    XClearWindow(display, frame.resize_handle);
  }
  
  if (decorations.titlebar) drawTitleWin();
  drawAllButtons();
}


void BlackboxWindow::installColormap(Bool install) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;
  
  int i = 0, ncmap = 0;
  Colormap *cmaps = XListInstalledColormaps(display, client.window, &ncmap);  
  XWindowAttributes wattrib;
  if (cmaps) {
    if (XGetWindowAttributes(display, client.window, &wattrib)) {
      if (install) {
	// install the window's colormap
	for (i = 0; i < ncmap; i++)
	  if (*(cmaps + i) == wattrib.colormap)
	    // this window is using an installed color map... do not install
	    install = False;
	
	// otherwise, install the window's colormap
	if (install)
	  XInstallColormap(display, wattrib.colormap);
      } else {
	// uninstall the window's colormap
	for (i = 0; i < ncmap; i++)
	  if (*(cmaps + i) == wattrib.colormap)
	    // we found the colormap to uninstall
	    XUninstallColormap(display, wattrib.colormap);
      }
    }
    
    XFree(cmaps);
  }
  
  blackbox->ungrabServer();
}


// *************************************************************************
// utility function to fetch the _XA_WM_STATE property of the specified
// window - this follows twm's GetWMState() function closely
// *************************************************************************

Bool BlackboxWindow::fetchWMState(unsigned long *state_return,
				  unsigned long *workspace_return) {
  blackbox->syncGrabServer();
  if (! validateClient()) return False;

  *state_return = 0;
  if (workspace_return)
    *workspace_return = (unsigned long) workspace_number;
  
  Atom atom_return;
  Bool ret = False;
  int foo;
  unsigned long *state, ulfoo, nitems;

  if ((XGetWindowProperty(display, client.window, blackbox->StateAtom(),
			  0l, 3l, False, blackbox->StateAtom(), &atom_return,
			  &foo, &nitems, &ulfoo,
			  (unsigned char **) &state) != Success) ||
      (! state) || (! state_return)) {
    blackbox->ungrabServer();
    return False;
  }
  
  if (nitems >= 1) {
    *state_return = (unsigned long) state[0];

    if (nitems == 3 && workspace_return)
      *workspace_return = (unsigned long) state[2];
    
    ret = True;
  }
  
  XFree((void *) state);
  blackbox->ungrabServer();

  return ret;
}


// *************************************************************************
// Window drawing code
// *************************************************************************

void BlackboxWindow::drawTitleWin(void) {  
  switch (blackbox->Justification()) {
  case Blackbox::B_LeftJustify: {
    int dx = frame.button_w + 6, dlen = -1;
    
    if (frame.title_w > ((frame.button_w + 4) * 6)) {
      if (client.title_text_w + 4 +
	  (((frame.button_w + 4) * 3) + 2) > frame.title_w) {
	dx = 4;
	dlen = 0;
      }
    } else if (client.title_text_w + 8 > frame.title_w) {
      dx = 4;
      dlen = 0;
    }
    
    if (dlen != -1) {
      unsigned int l;
      for (dlen = client.title_len; dlen >= 0; dlen--) {
	l = XTextWidth(blackbox->titleFont(), client.title, dlen) + 8;
	if (l < frame.title_w)
	  break;
      }
    } else
      dlen = client.title_len;
    
    XDrawString(display, frame.title,
		((focused) ? blackbox->WindowFocusGC() :
		 blackbox->WindowUnfocusGC()), dx,
		blackbox->titleFont()->ascent + frame.bevel_w, client.title,
		dlen);
    break; }
  
  case Blackbox::B_RightJustify: {
    int dx = frame.title_w - (client.title_text_w +
			      ((frame.button_w + 4) * 2) + 4),
      dlen = -1;
    
    if (frame.title_w > ((frame.button_w + 4) * 6)) {
      if (client.title_text_w + 4 +
	  (((frame.button_w + 4) * 3) + 2) > frame.title_w) {
	dx = 4;
	dlen = 0;
      }
    } else if (client.title_text_w + 8 > frame.title_w) {
      dx = 4;
      dlen = 0;
    }
    
    if (dlen != -1) {
      unsigned int l;
      for (dlen = client.title_len; dlen >= 0; dlen--) {
	l = XTextWidth(blackbox->titleFont(), client.title, dlen) + 8;
	if (l < frame.title_w)
	  break;
      }
    } else
      dlen = client.title_len;
    
    XDrawString(display, frame.title,
		((focused) ? blackbox->WindowFocusGC() :
		 blackbox->WindowUnfocusGC()),dx,
		blackbox->titleFont()->ascent + frame.bevel_w, client.title,
		dlen);
    break;  }
  
  case Blackbox::B_CenterJustify: {
    int dx = (frame.title_w  - (client.title_text_w + frame.button_w + 6)) / 2,
      dlen = -1;
    
    if (frame.title_w > ((frame.button_w + 4) * 6)) {
      if (client.title_text_w + 4 +
	  (((frame.button_w + 4) * 3) + 2) > frame.title_w) {
	dx = 4;
	dlen = 0;
      }
    } else if (client.title_text_w + 8 > frame.title_w) {
      dx = 4;
      dlen = 0;
    }
    
    if (dlen != -1) {
      unsigned int l;
      for (dlen = client.title_len; dlen >= 0; dlen--) {
	l = XTextWidth(blackbox->titleFont(), client.title, dlen) + 8;
	if (l < frame.title_w)
	  break;
      }
    } else
      dlen = client.title_len;
    
    XDrawString(display, frame.title,
		((focused) ? blackbox->WindowFocusGC() :
		 blackbox->WindowUnfocusGC()),dx,
		blackbox->titleFont()->ascent + frame.bevel_w, client.title,
		dlen);
    break; }
  }
}


void BlackboxWindow::drawAllButtons(void) {
  if (frame.iconify_button) drawIconifyButton(False);
  if (frame.maximize_button) drawMaximizeButton(False);
  if (frame.close_button) drawCloseButton(False);
}


void BlackboxWindow::drawIconifyButton(Bool pressed) {
  if (! pressed) {
    XSetWindowBackgroundPixmap(display, frame.iconify_button,
			       ((focused) ? frame.fbutton : frame.ubutton));
    XClearWindow(display, frame.iconify_button);
  } else if (frame.pbutton) {
    XSetWindowBackgroundPixmap(display, frame.iconify_button, frame.pbutton);
    XClearWindow(display, frame.iconify_button);
  }

  XDrawRectangle(display, frame.iconify_button,
		 ((focused) ? blackbox->WindowFocusGC() :
		  blackbox->WindowUnfocusGC()),
		 2, frame.button_h - 5, frame.button_w - 5, 2);
}


void BlackboxWindow::drawMaximizeButton(Bool pressed) {
  if (! pressed) {
    XSetWindowBackgroundPixmap(display, frame.maximize_button,
			       ((focused) ? frame.fbutton : frame.ubutton));
    XClearWindow(display, frame.maximize_button);
  } else if (frame.pbutton) {
    XSetWindowBackgroundPixmap(display, frame.maximize_button, frame.pbutton);
    XClearWindow(display, frame.maximize_button);
  }
  
  XDrawRectangle(display, frame.maximize_button,
		 ((focused) ? blackbox->WindowFocusGC() :
		  blackbox->WindowUnfocusGC()),
		 2, 2, frame.button_w - 5, frame.button_h - 5);
  XDrawLine(display, frame.maximize_button,
	    ((focused) ? blackbox->WindowFocusGC() :
	     blackbox->WindowUnfocusGC()),
	    2, 3, frame.button_w - 3, 3);
}


void BlackboxWindow::drawCloseButton(Bool pressed) {
  if (! pressed) {
    XSetWindowBackgroundPixmap(display, frame.close_button,
			       ((focused) ? frame.fbutton : frame.ubutton));
    XClearWindow(display, frame.close_button);
  } else if (frame.pbutton) {
    XSetWindowBackgroundPixmap(display, frame.close_button, frame.pbutton);
    XClearWindow(display, frame.close_button);
  }

  XDrawLine(display, frame.close_button,
	    ((focused) ? blackbox->WindowFocusGC() :
	     blackbox->WindowUnfocusGC()), 2, 2,
            frame.button_w - 3, frame.button_h - 3);
  XDrawLine(display, frame.close_button,
	    ((focused) ? blackbox->WindowFocusGC() :
	     blackbox->WindowUnfocusGC()), 2, frame.button_h - 3,
            frame.button_w - 3, 2);
}


// *************************************************************************
// Window event code
// *************************************************************************

void BlackboxWindow::mapRequestEvent(XMapRequestEvent *re) {
  if (re->window == client.window) {
    if ((! iconic) && (client.wm_hint_flags & StateHint)) {
      blackbox->syncGrabServer();
      if (! validateClient()) return;

      unsigned long state, workspace_new = workspace_number;
      if (! (fetchWMState(&state, &workspace_new) && blackbox->Startup() &&
	     (state == NormalState || state == IconicState)))
	state = client.initial_state;
      
      if (((int) workspace_new != blackbox->toolbar()->currentWorkspaceID()) &&
	  ((int) workspace_new < blackbox->toolbar()->count()) &&
	  ( ! (stuck && workspace_new == 0))) {
	blackbox->toolbar()->workspace(workspace_number)->removeWindow(this);
	blackbox->toolbar()->workspace(workspace_new)->addWindow(this);
	
	state = WithdrawnState;
      }
      
      switch (state) {
      case IconicState:
	iconifyWindow();
	break;
	
      case WithdrawnState:
	withdrawWindow();
	break;
	
      case NormalState:
      case InactiveState:
      case ZoomState:
      default:
	positionButtons();
	
	XMapWindow(display, client.window);
	XMapSubwindows(display, frame.window);
	XMapWindow(display, frame.window);	
	setFocusFlag(False);
	
	visible = True;
	iconic = False;
	
	unsigned long new_state[3];
	new_state[0] = (unsigned long) NormalState;
	new_state[1] = (unsigned long) None;
	new_state[2] = (unsigned long) workspace_number;
	
	XChangeProperty(display, client.window, blackbox->StateAtom(),
			blackbox->StateAtom(), 32, PropModeReplace,
			(unsigned char *) new_state, 3);
	break;
      }
      
      blackbox->ungrabServer();
    } else      
      deiconifyWindow();
  }
}


void BlackboxWindow::mapNotifyEvent(XMapEvent *ne) {
  if ((ne->window == client.window) && (! ne->override_redirect) &&
      (! iconic) && (visible)) {
    blackbox->syncGrabServer();
    if (! validateClient()) return;
    
    positionButtons();
    
    unsigned long state[3];
    state[0] = (unsigned long) NormalState;
    state[1] = (unsigned long) None;
    state[2] = (unsigned long) workspace_number;
    
    XChangeProperty(display, client.window, blackbox->StateAtom(),
		    blackbox->StateAtom(), 32, PropModeReplace,
		    (unsigned char *) state, 3);
    
    XMapSubwindows(display, frame.window);
    XMapWindow(display, frame.window);
    
    if (! transient)
      setFocusFlag(False);
    else
      setInputFocus();
    
    visible = True;
    iconic = False;
    
    blackbox->ungrabServer();
  }
}


void BlackboxWindow::unmapNotifyEvent(XUnmapEvent *ue) {
  if (ue->window == client.window) {
    if (! iconic && ! visible) return;

    blackbox->syncGrabServer();
    if (! validateClient()) return;

    unsigned long state[3];
    state[0] = (unsigned long) WithdrawnState;
    state[1] = (unsigned long) None;
    state[2] = (unsigned long) workspace_number;
    
    visible = False;
    iconic = False;
    XUnmapWindow(display, frame.window);
    
    XChangeProperty(display, client.window, blackbox->StateAtom(),
		    blackbox->StateAtom(), 32, PropModeReplace,
		    (unsigned char *) state, 3);
    
    XEvent dummy;
    if (! XCheckTypedWindowEvent(display, client.window, ReparentNotify,
				 &dummy))
      XReparentWindow(display, client.window, blackbox->Root(), client.x,
		      client.y);
    
    XChangeSaveSet(display, client.window, SetModeDelete);
    XSelectInput(display, client.window, NoEventMask);
    
    XSync(display, False);
    blackbox->ungrabServer();
    
    delete this;
  }
}


void BlackboxWindow::destroyNotifyEvent(XDestroyWindowEvent *de) {
  if (de->window == client.window) {
    XUnmapWindow(display, frame.window);
    delete this;
  }
}


void BlackboxWindow::propertyNotifyEvent(Atom atom) {
  blackbox->syncGrabServer();
  if (! validateClient()) return;

  switch(atom) {
  case XA_WM_CLASS:
  case XA_WM_CLIENT_MACHINE:
  case XA_WM_COMMAND:   
  case XA_WM_TRANSIENT_FOR:
    break;
      
  case XA_WM_HINTS:
    getWMHints();
    break;
    
  case XA_WM_ICON_NAME:
    if (icon) icon->rereadLabel();
    break;
      
  case XA_WM_NAME:
    if (client.title)
      if (strcmp(client.title, "Unnamed")) {
	XFree(client.title);
	client.title = 0;
      }
    
    if (! XFetchName(display, client.window, &client.title))
      client.title = "Unnamed";
    client.title_len = strlen(client.title);
    client.title_text_w = XTextWidth(blackbox->titleFont(), client.title,
				     client.title_len);

    XClearWindow(display, frame.title);
    if (decorations.titlebar) drawTitleWin();
    blackbox->toolbar()->workspace(workspace_number)->Update();
    break;
      
  case XA_WM_NORMAL_HINTS:
    getWMNormalHints();
    break;
    
  default:
    if (atom == blackbox->ProtocolsAtom())
      getWMProtocols();
  }
  
  blackbox->ungrabServer();
}


void BlackboxWindow::exposeEvent(XExposeEvent *ee) {
  if (frame.title == ee->window) {
    if (decorations.titlebar)
      drawTitleWin();
  } else if (frame.close_button == ee->window) {
    drawCloseButton(False);
  } else if (frame.maximize_button == ee->window) {
    drawMaximizeButton(False);
  } else if (frame.iconify_button == ee->window) {
    drawIconifyButton(False);
  }
}


void BlackboxWindow::configureRequestEvent(XConfigureRequestEvent *cr) {  
  if (cr->window == client.window) {
    int cx, cy;
    unsigned int cw, ch;
    
    if (cr->value_mask & CWX)
      cx = cr->x - 1;
    else
      cx = frame.x;

    if (cr->value_mask & CWY)
      cy = cr->y - frame.y_border - 1;
    else
      cy = frame.y;

    if (cr->value_mask & CWWidth)
      cw = cr->width + ((decorations.border) ? (frame.bevel_w * 2) : 0) +
	((decorations.handle) ? frame.handle_w + 1 : 0);
    else
      cw = frame.width;

    if (cr->value_mask & CWHeight)
      ch = cr->height + frame.y_border +
	((decorations.border) ? (frame.bevel_w * 2) : 0);
    else
      ch = frame.height;
    
    configureWindow(cx, cy, cw, ch);
  }
}


void BlackboxWindow::buttonPressEvent(XButtonEvent *be) {
  if (be->button == 1) {
    if (! focused)
      setInputFocus();

    if (frame.title == be->window || frame.handle == be->window ||
	frame.resize_handle == be->window || frame.border == be->window)
      blackbox->toolbar()->workspace(workspace_number)->
	raiseWindow(this);
    else if (frame.iconify_button == be->window)
      drawIconifyButton(True);
    else if (frame.maximize_button == be->window)
      drawMaximizeButton(True);
    else if (frame.close_button == be->window)
      drawCloseButton(True);
  } else if (be->button == 2) {
    if (frame.title == be->window || frame.handle == be->window ||
        frame.resize_handle == be->window || frame.border == be->window) {
      blackbox->toolbar()->workspace(workspace_number)->
	lowerWindow(this);
    }
  } else if (be->button == 3 && (frame.title == be->window ||
				 frame.border == be->window ||
				 frame.handle == be->window)) {
    int mx = 0, my = 0;
    
    if (frame.title == be->window) {
      mx = be->x_root - (windowmenu->Width() / 2);
      my = frame.y + frame.title_h;
    } else if (frame.border == be->window) {
      mx = be->x_root - (windowmenu->Width() / 2);

      if (be->y <= (signed) frame.bevel_w)
	my = frame.y + frame.y_border;
      else
	my = be->y_root - (windowmenu->Height() / 2);
    } else if (frame.handle == be->window) {
      mx = frame.x + frame.width - windowmenu->Width();
      my = be->y_root - (windowmenu->Height() / 2);
    }
    
    if (mx > (signed) (frame.x + frame.width - (frame.handle_w +
						windowmenu->Width())))
      mx = frame.x + frame.width - (frame.handle_w + windowmenu->Width());
    if (mx < frame.x)
      mx = frame.x;
    
    if (my > (signed) (frame.y + frame.height - windowmenu->Height()))
      my = frame.y + frame.height - windowmenu->Height();
    if (my < (signed) (frame.y + ((decorations.titlebar) ? frame.title_h :
			     	  frame.y_border)))
      my = frame.y +
	((decorations.titlebar) ? frame.title_h : frame.y_border);
    
    if (windowmenu)
      if (! windowmenu->Visible()) {
	windowmenu->Move(mx, my);
	windowmenu->Show();
      } else
	windowmenu->Hide();
  }
}


void BlackboxWindow::buttonReleaseEvent(XButtonEvent *re) {
  if (re->button == 1) {
    if (re->window == frame.title || re->window == frame.handle ||
	re->window == frame.border) {
      if (moving) {
        if (! blackbox->opaqueMove()) {
	  if (shaded) {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.title_h);
	  } else {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.height);
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move +
			   ((decorations.border) ? frame.bevel_w : 1),
			   frame.y_move + frame.y_border +
			   ((decorations.border) ? frame.bevel_w : 1),
			   client.width, client.height);
	  }
	  
	  configureWindow(frame.x_move, frame.y_move, frame.width,
			  frame.height);
	  blackbox->ungrabServer();
	} else
	  configureWindow(frame.x, frame.y, frame.width, frame.height);
	
	moving = False;
	XUngrabPointer(display, CurrentTime);
      } else if ((re->state & ControlMask))
	shadeWindow();
    } else if (resizing) {
      int dx, dy;
      
      XDrawString(display, blackbox->Root(), blackbox->OpGC(),
		  frame.x + frame.x_resize - frame.resize_label_w,
		  frame.y + frame.y_resize - (frame.bevel_w * 2),
		  resizeLabel, strlen(resizeLabel));
      XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
		     frame.x, frame.y, frame.x_resize, frame.y_resize);

      // calculate the size of the client window and conform it to the
      // size specified by the size hints of the client window...
      dx = frame.x_resize - frame.handle_w - client.base_width -
	((decorations.border) ? (frame.bevel_w * 2) : 0) - 1;
      dy = frame.y_resize - frame.y_border - client.base_height -
	((decorations.border) ? (frame.bevel_w * 2) : 0);
      
      if (dx < (signed) client.min_width) dx = client.min_width;
      if (dy < (signed) client.min_height) dy = client.min_height;
      if ((unsigned) dx > client.max_width) dx = client.max_width;
      if ((unsigned) dy > client.max_height) dy = client.max_height;
      
      dx /= client.width_inc;
      dy /= client.height_inc;
      
      dx = (dx * client.width_inc) + client.base_width;
      dy = (dy * client.height_inc) + client.base_height;
      
      frame.x_resize = dx + frame.handle_w +
	((decorations.border) ? (frame.bevel_w * 2) : 0) + 1;
      frame.y_resize = dy + frame.y_border +
	((decorations.border) ? (frame.bevel_w * 2) : 0);
      frame.resize_label_w = 0;
      
      delete [] resizeLabel;
      
      resizing = False;
      XUngrabPointer(display, CurrentTime);
      
      configureWindow(frame.x, frame.y, frame.x_resize, frame.y_resize);
      blackbox->ungrabServer();
    } else if (re->window == frame.iconify_button) {
      if ((re->x >= 0) && ((unsigned) re->x <= frame.button_w) &&
	  (re->y >= 0) && ((unsigned) re->y <= frame.button_h)) {
	iconifyWindow();
      }	else
	drawIconifyButton(False);
    } else if (re->window == frame.maximize_button) {
      if ((re->x >= 0) && ((unsigned) re->x <= frame.button_w) &&
	  (re->y >= 0) && ((unsigned) re->y <= frame.button_h)) {
	maximizeWindow();
      } else
	drawMaximizeButton(False);
    } else if (re->window == frame.close_button) {
      if ((re->x >= 0) && ((unsigned) re->x <= frame.button_w) &&
	  (re->y >= 0) && ((unsigned) re->y <= frame.button_h)) {
	closeWindow();
      } else
	drawCloseButton(False);
    }
  }
}


void BlackboxWindow::motionNotifyEvent(XMotionEvent *me) {
  if (frame.title == me->window || frame.handle == me->window ||
      frame.border == me->window) {
    if ((me->state & Button1Mask) && functions.move) {
      if (! moving) {
	if (windowmenu)
	  if (windowmenu->Visible())
	    windowmenu->Hide();
	
	if (XGrabPointer(display, me->window, False, PointerMotionMask|
			 ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
			 None, blackbox->moveCursor(), CurrentTime)
	    == GrabSuccess) {
	  moving = True;
	  frame.x_grab = me->x;
	  frame.y_grab = me->y;
	  
	  if (! blackbox->opaqueMove()) {
            blackbox->syncGrabServer();
	    
	    frame.x_move = frame.x;
	    frame.y_move = frame.y;
	    
	    if (shaded) {
	      XDrawRectangle(display, blackbox->Root(),
			     blackbox->OpGC(), frame.x_move,
			     frame.y_move, frame.width, frame.title_h);
	    } else {
	      XDrawRectangle(display, blackbox->Root(),
			     blackbox->OpGC(), frame.x_move,
			     frame.y_move, frame.width, frame.height);
	      XDrawRectangle(display, blackbox->Root(),
			     blackbox->OpGC(), frame.x_move +
			     ((decorations.border) ? frame.bevel_w : 1),
			     frame.y_move + frame.y_border +
			     ((decorations.border) ? frame.bevel_w : 1),
			     client.width, client.height);
	    }
	  }
	} else
	  moving = False;
      } else if (moving) {
	int dx, dy;
	if (frame.handle == me->window) {
	  dx = me->x_root - frame.x_grab + frame.handle_w - frame.width;
	  dy = me->y_root - frame.y_grab - frame.y_border;
	} else if (frame.border == me->window) {
	  dx = me->x_root - frame.x_grab;
	  dy = me->y_root - frame.y_grab - frame.y_border;
	} else {
	  dx = me->x_root - frame.x_grab;
	  dy = me->y_root - frame.y_grab;
	}

	if (! blackbox->opaqueMove()) {
	  if (shaded) {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.title_h);
	  } else if (! transient) {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.height);
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move +
			   ((decorations.border) ? frame.bevel_w : 1),
			   frame.y_move + frame.y_border +
			   ((decorations.border) ? frame.bevel_w : 1),
			   client.width, client.height);
	  }
	  
	  frame.x_move = dx;
	  frame.y_move = dy;

	  if (shaded) {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.title_h);
	  } else if (! transient) {
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move, frame.y_move, frame.width,
			   frame.height);
	    XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			   frame.x_move +
			   ((decorations.border) ? frame.bevel_w : 1),
			   frame.y_move + frame.y_border +
			   ((decorations.border) ? frame.bevel_w : 1),
			   client.width, client.height);
	  }
	} else
	  configureWindow(dx, dy, frame.width, frame.height);
      }
    }
  } else if (me->window == frame.resize_handle) {
    if ((me->state & Button1Mask) && functions.resize) {
      if (! resizing) {
	if (XGrabPointer(display, frame.resize_handle, False,
			 PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
			 GrabModeAsync, None, None, CurrentTime)
	    == GrabSuccess) {
	  int dx, dy;
	  resizing = True;
	  
          blackbox->syncGrabServer();

	  frame.x_resize = frame.width;
	  frame.y_resize = frame.height;
	  
	  // calculate the size of the client window and conform it to the
	  // size specified by the size hints of the client window...
	  dx = frame.x_resize - frame.handle_w - client.base_width -
	    ((decorations.border) ? (frame.bevel_w * 2) : 0) - 1;
	  dy = frame.y_resize - frame.y_border - client.base_height -
	    ((decorations.border) ? (frame.bevel_w * 2) : 0);
	  
	  if (dx < (signed) client.min_width) dx = client.min_width;
	  if (dy < (signed) client.min_height) dy = client.min_height;
	  if ((unsigned) dx > client.max_width) dx = client.max_width;
	  if ((unsigned) dy > client.max_height) dy = client.max_height;
	  
	  dx /= client.width_inc;
	  dy /= client.height_inc;
	  
	  resizeLabel = new char[strlen("00000 x 00000")];
	  sprintf(resizeLabel, "%d x %d", dx, dy);
	  
	  dx = (dx * client.width_inc) + client.base_width;
	  dy = (dy * client.height_inc) + client.base_height;
	  
	  frame.x_resize = dx + frame.handle_w +
	    ((decorations.border) ? (frame.bevel_w * 2) : 0) + 1;
	  frame.y_resize = dy + frame.y_border +
	    ((decorations.border) ? (frame.bevel_w * 2) : 0);
	  
	  frame.resize_label_w =
	    XTextWidth(blackbox->titleFont(), resizeLabel,
		       strlen(resizeLabel)) + (frame.bevel_w * 2);
	  
	  XDrawString(display, blackbox->Root(), blackbox->OpGC(),
		      frame.x + frame.x_resize - frame.resize_label_w,
		      frame.y + frame.y_resize - (frame.bevel_w * 2),
		      resizeLabel, strlen(resizeLabel));
	  XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
			 frame.x, frame.y, frame.x_resize, frame.y_resize);
	} else
	  resizing = False;
      } else if (resizing) {
	int dx, dy;

	XDrawString(display, blackbox->Root(), blackbox->OpGC(),
		    frame.x + frame.x_resize - frame.resize_label_w,
		    frame.y + frame.y_resize - (frame.bevel_w * 2),
		    resizeLabel, strlen(resizeLabel));
	XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
		       frame.x, frame.y, frame.x_resize, frame.y_resize);

	frame.x_resize = me->x_root - frame.x;
	if (frame.x_resize < 1) frame.x_resize = 1;
	frame.y_resize = me->y_root - frame.y;
	if (frame.y_resize < 1) frame.y_resize = 1;

	// calculate the size of the client window and conform it to the
	// size specified by the size hints of the client window...
	dx = frame.x_resize - frame.handle_w - client.base_width -
	  ((decorations.border) ? (frame.bevel_w * 2) : 0) - 1;
	dy = frame.y_resize - client.base_height - frame.y_border -
	  ((decorations.border) ? (frame.bevel_w * 2) : 0);

	if (dx < (signed) client.min_width) dx = client.min_width;
	if (dy < (signed) client.min_height) dy = client.min_height;
	if ((unsigned) dx > client.max_width) dx = client.max_width;
	if ((unsigned) dy > client.max_height) dy = client.max_height;
	
	dx /= client.width_inc;
	dy /= client.height_inc;	
	sprintf(resizeLabel, "%d x %d", dx, dy);
	
	dx = (dx * client.width_inc) + client.base_width;
	dy = (dy * client.height_inc) + client.base_height;

	frame.x_resize = dx + frame.handle_w +
	  ((decorations.border) ? (frame.bevel_w * 2) : 0) + 1;
	frame.y_resize = dy + frame.y_border +
	  ((decorations.border) ? (frame.bevel_w * 2) : 0);
	
	frame.resize_label_w =
	  XTextWidth(blackbox->titleFont(), resizeLabel,
		     strlen(resizeLabel)) + (frame.bevel_w * 2);
	
	XDrawString(display, blackbox->Root(), blackbox->OpGC(),
		    frame.x + frame.x_resize - frame.resize_label_w,
		    frame.y + frame.y_resize - (frame.bevel_w * 2),
		    resizeLabel, strlen(resizeLabel));
	XDrawRectangle(display, blackbox->Root(), blackbox->OpGC(),
		       frame.x, frame.y, frame.x_resize, frame.y_resize);
      }
    }
  }
}


#ifdef SHAPE
void BlackboxWindow::shapeEvent(XShapeEvent *) {
  if (blackbox->shapeExtensions()) {
    if (frame.shaped) {
      blackbox->syncGrabServer();
      if (! validateClient()) return;
      XShapeCombineShape(display, frame.window, ShapeBounding,
			 ((decorations.border) ? frame.bevel_w : 0),
			 frame.title_h +
			 ((decorations.border) ? (frame.bevel_w + 1) : 1),
			 client.window, ShapeBounding, ShapeSet);
      
      int num = 1;
      XRectangle xrect[2];
      xrect[0].x = xrect[0].y = 0;
      xrect[0].width = frame.title_w;
      xrect[0].height = frame.title_h + 1;
      
      if (decorations.handle) {
	xrect[1].x = frame.border_w + 1;
	xrect[1].y = frame.title_h + 1;
	xrect[1].width = frame.handle_w;
	xrect[1].height = frame.border_h;
	num++;
      }
      
      XShapeCombineRectangles(display, frame.window, ShapeBounding, 0, 0,
			      xrect, num, ShapeUnion, Unsorted);
      blackbox->ungrabServer();
    }
  }
}
#endif


Bool BlackboxWindow::validateClient(void) {
  XEvent e;
  if (XCheckTypedWindowEvent(display, client.window, DestroyNotify, &e)) {
    blackbox->ungrabServer();

    delete this;
    return False;
  }

  return True;
}