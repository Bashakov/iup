/** \file
 * \brief iupflatscrollbar utilities
 *
 * See Copyright Notice in "iup.h"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "iup.h"
#include "iupcbs.h"
#include "iupkey.h"

#include "iup_object.h"
#include "iup_register.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_stdcontrols.h"
#include "iup_layout.h"
#include "iup_drv.h"
#include "iup_drvdraw.h"
#include "iup_childtree.h"
#include "iup_flatscrollbar.h"


static Ihandle* iFlatScrollBarGetVertical(Ihandle *ih)
{
  return ih->firstchild;
}

static Ihandle* iFlatScrollBarGetHorizontal(Ihandle *ih)
{
  return ih->firstchild->brother;
}

static void iFlatScrollBarRedrawVertical(Ihandle* ih)
{
  Ihandle* sb_vert = iFlatScrollBarGetVertical(ih);
  IupUpdate(sb_vert);
}

static void iFlatScrollBarRedrawHorizontal(Ihandle* ih)
{
  Ihandle* sb_horiz = iFlatScrollBarGetHorizontal(ih);
  IupUpdate(sb_horiz);
}

static void iFlatScrollBarNormalizePos(int *pos, int max, int d)
{
  if (*pos < 0) *pos = 0;
  if (*pos > max - d) *pos = max - d;
}

static int iFlatScrollBarGetLineY(Ihandle* ih, int dy)
{
  int liney = dy / 10;
  char* liney_str = iupAttribGet(ih, "LINEY");
  if (liney_str) iupStrToInt(liney_str, &liney);
  return liney;
}

static int iFlatScrollBarGetLineX(Ihandle* ih, int dx)
{
  int linex = dx / 10;
  char* linex_str = iupAttribGet(ih, "LINEX");
  if (linex_str) iupStrToInt(linex_str, &linex);
  return linex;
}

static void iFlatScrollBarNotify(Ihandle *ih, int handler)
{
  if (handler < 0)
  {
    IFn cb = IupGetCallback(ih, "FLATSCROLL_CB");
    if (cb) cb(ih);
  }
  else
  {
    IFniff cb = (IFniff)IupGetCallback(ih, "SCROLL_CB");
    if (cb)
    {
      int sb_size = iupAttribGetInt(ih, "SCROLLBARSIZE");
      int posx = iupAttribGetInt(ih, "POSX");
      int posy = iupAttribGetInt(ih, "POSY");

      int sb2cb_op[10] = {
        IUP_SBRIGHT, IUP_SBPGRIGHT, IUP_SBDRAGH, IUP_SBPGLEFT, IUP_SBLEFT,
        IUP_SBDN, IUP_SBPGDN, IUP_SBDRAGV, IUP_SBPGUP, IUP_SBUP };

      if (iupAttribGetInt(ih, "DX") > iupAttribGetInt(ih, "XMAX") - sb_size)
        posx = 0;
      if (iupAttribGetInt(ih, "DY") > iupAttribGetInt(ih, "YMAX") - sb_size)
        posy = 0;

      cb(ih, sb2cb_op[handler], (float)posx, (float)posy);
    }
  }
}

void iupFlatScrollBarSetPos(Ihandle *ih, int posx, int posy)
{
  iFlatScrollBarNormalizePos(&posx, iupAttribGetInt(ih, "XMAX"), iupAttribGetInt(ih, "DX"));
  iFlatScrollBarNormalizePos(&posy, iupAttribGetInt(ih, "YMAX"), iupAttribGetInt(ih, "DY"));

  iupAttribSetInt(ih, "POSX", posx);
  iupAttribSetInt(ih, "POSY", posy);

  iFlatScrollBarRedrawVertical(ih);
  iFlatScrollBarRedrawHorizontal(ih);

  iFlatScrollBarNotify(ih, -1);
}

/*************************************************************************/


enum {
  SB_NONE,
  SB_INC_X, SB_PAGEINC_X, SB_DRAG_X, SB_PAGEDEC_X, SB_DEC_X,
  SB_INC_Y, SB_PAGEINC_Y, SB_DRAG_Y, SB_PAGEDEC_Y, SB_DEC_Y
};


static char* iFlatScrollBarMakeImageName(Ihandle* ih, const char* baseattrib, const char* state)
{
  char attrib[1024];
  strcpy(attrib, baseattrib);
  strcat(attrib, state);
  return iupAttribGet(ih, attrib);
}

static const char* iFlatScrollBarGetImageName(Ihandle* ih, const char* baseattrib, int pressed, int highlighted, int active, int *make_inactive)
{
  const char* new_imagename = NULL;

  *make_inactive = 0;

  if (active)
  {
    if (pressed)
      new_imagename = iFlatScrollBarMakeImageName(ih, baseattrib, "PRESS");
    else
    {
      if (highlighted)
        new_imagename = iFlatScrollBarMakeImageName(ih, baseattrib, "HIGHLIGHT");
    }
  }
  else
  {
    new_imagename = iFlatScrollBarMakeImageName(ih, baseattrib, "INACTIVE");
    if (!new_imagename)
      *make_inactive = 1;
  }

  if (new_imagename)
    return new_imagename;
  else
    return baseattrib;
}

static void iFlatScrollBarDrawVertical(Ihandle* sb_ih, IdrawCanvas* dc, int active, const char* fgcolor, const char* bgcolor, int pressed,
                                       int highlight, int ymax, int dy, int sb_size, int has_horiz_scroll)
{
  int height = sb_ih->currentheight;
  int pos, d, range;
  int posy = iupAttribGetInt(sb_ih->parent, "POSY");
  int show_arrows = iupAttribGetInt(sb_ih->parent, "SHOWARROWS");
  int arrow_size = sb_size;

  const char *fgcolor_inc = fgcolor,
    *fgcolor_dec = fgcolor,
    *fgcolor_drag = fgcolor;

  if (!show_arrows)
    arrow_size = 0;

  if (pressed)
  {
    char* presscolor = iupAttribGetStr(sb_ih->parent, "PRESSCOLOR");
    if (presscolor)
    {
      if (pressed == SB_INC_Y)
        fgcolor_inc = presscolor;
      else if (pressed == SB_DEC_Y)
        fgcolor_dec = presscolor;
      else if (pressed == SB_DRAG_Y)
        fgcolor_drag = presscolor;
    }
  }
  else if (highlight)
  {
    char* hlcolor = iupAttribGetStr(sb_ih->parent, "HIGHCOLOR");
    if (hlcolor)
    {
      if (highlight == SB_INC_Y)
        fgcolor_inc = hlcolor;
      else if (highlight == SB_DEC_Y)
        fgcolor_dec = hlcolor;
      else if (highlight == SB_DRAG_Y)
        fgcolor_drag = hlcolor;
    }
  }

  if (has_horiz_scroll)
    height -= sb_size;

  range = height - 1 - 2 * arrow_size;

  d = (dy * range) / ymax;
  pos = (posy * range) / ymax;
  pos += arrow_size;

  /* draw arrows */
  if (show_arrows)
  {
    int arrow_images = iupAttribGetInt(sb_ih->parent, "ARROWIMAGES");
    if (arrow_images)
    {
      int make_inactive;
      const char* image;

      image = iFlatScrollBarGetImageName(sb_ih->parent, "IMAGETOP", pressed, highlight, active, &make_inactive);
      iupdrvDrawImage(dc, image, make_inactive, 0, 0);

      image = iFlatScrollBarGetImageName(sb_ih->parent, "IMAGEBOTTOM", pressed, highlight, active, &make_inactive);
      iupdrvDrawImage(dc, image, make_inactive, height - 1 - arrow_size, 0);
    }
    else
    {
      iupFlatDrawArrow(dc, 2, 2, arrow_size - 5, fgcolor_dec, bgcolor, active, IUPDRAW_ARROW_TOP);
      iupFlatDrawArrow(dc, 2, height - 1 - arrow_size + 3, arrow_size - 5, fgcolor_inc, bgcolor, active, IUPDRAW_ARROW_BOTTOM);
    }
  }

  /* draw handler */
  iupFlatDrawBox(dc, 2, sb_size - 1 - 2, pos, pos + d, fgcolor_drag, bgcolor, active);
}

static void iFlatScrollBarDrawHorizontal(Ihandle* sb_ih, IdrawCanvas* dc, int active, const char* fgcolor, const char* bgcolor, int pressed,
                                         int highlight, int xmax, int dx, int sb_size, int has_vert_scroll)
{
  int width = sb_ih->currentwidth;
  int pos, d, range;
  int posx = iupAttribGetInt(sb_ih->parent, "POSX");
  int show_arrows = iupAttribGetInt(sb_ih->parent, "SHOWARROWS");
  int arrow_size = sb_size;

  const char *fgcolor_inc = fgcolor,
    *fgcolor_dec = fgcolor,
    *fgcolor_drag = fgcolor;

  if (!show_arrows)
    arrow_size = 0;

  if (pressed)
  {
    char* presscolor = iupAttribGetStr(sb_ih->parent, "PRESSCOLOR");
    if (presscolor)
    {
      if (pressed == SB_INC_X)
        fgcolor_inc = presscolor;
      else if (pressed == SB_DEC_X)
        fgcolor_dec = presscolor;
      else if (pressed == SB_DRAG_X)
        fgcolor_drag = presscolor;
    }
  }
  else if (highlight)
  {
    char* hlcolor = iupAttribGetStr(sb_ih->parent, "HIGHCOLOR");
    if (hlcolor)
    {
      if (highlight == SB_INC_X)
        fgcolor_inc = hlcolor;
      else if (highlight == SB_DEC_X)
        fgcolor_dec = hlcolor;
      else if (highlight == SB_DRAG_X)
        fgcolor_drag = hlcolor;
    }
  }

  if (has_vert_scroll)
    width -= sb_size;

  range = width - 1 - 2 * arrow_size;

  d = (dx * range) / xmax;
  pos = (posx * range) / xmax;
  pos += arrow_size;

  /* draw arrows */
  if (show_arrows)
  {
    int arrow_images = iupAttribGetInt(sb_ih->parent, "ARROWIMAGES");
    if (arrow_images)
    {
      int make_inactive;
      const char* image;
      
      image = iFlatScrollBarGetImageName(sb_ih->parent, "IMAGELEFT", pressed, highlight, active, &make_inactive);
      iupdrvDrawImage(dc, image, make_inactive, 0, 0);

      image = iFlatScrollBarGetImageName(sb_ih->parent, "IMAGERIGHT", pressed, highlight, active, &make_inactive);
      iupdrvDrawImage(dc, image, make_inactive, width - 1 - arrow_size, 0);
    }
    else
    {
      iupFlatDrawArrow(dc, 2, 2, arrow_size - 5, fgcolor_dec, bgcolor, active, IUPDRAW_ARROW_LEFT);
      iupFlatDrawArrow(dc, width - 1 - arrow_size + 3, 2, arrow_size - 5, fgcolor_inc, bgcolor, active, IUPDRAW_ARROW_RIGHT);
    }
  }

  /* draw handler */
  iupFlatDrawBox(dc, pos, pos + d, 2, sb_size - 1 - 2, fgcolor_drag, bgcolor, active);
}

static int iFlatScrollBarAction_CB(Ihandle* sb_ih)
{
  int sb_size = iupAttribGetInt(sb_ih->parent, "SCROLLBARSIZE");
  int xmax = iupAttribGetInt(sb_ih->parent, "XMAX");
  int ymax = iupAttribGetInt(sb_ih->parent, "YMAX");
  int dx = iupAttribGetInt(sb_ih->parent, "DX");
  int dy = iupAttribGetInt(sb_ih->parent, "DY");
  char* fgcolor = iupAttribGetStr(sb_ih->parent, "FORECOLOR");
  char* bgcolor = iupAttribGetStr(sb_ih->parent, "BACKCOLOR");
  int highlight = iupAttribGetInt(sb_ih, "_IUP_HIGHLIGHT_HANDLER");
  int pressed = iupAttribGetInt(sb_ih, "_IUP_PRESSED_HANDLER");
  int active = IupGetInt(sb_ih->parent, "ACTIVE");
  int is_vert_scrollbar = 0;
  int has_vert_scroll = 0;
  int has_horiz_scroll = 0;

  IdrawCanvas* dc = iupdrvDrawCreateCanvas(sb_ih);

  if (!bgcolor)
    bgcolor = iupBaseNativeParentGetBgColorAttrib(sb_ih);

  if (sb_ih->currentwidth == sb_size)
    is_vert_scrollbar = 1;

  if (xmax > dx)  /* has horizontal scrollbar */
    has_horiz_scroll = 1;

  if (ymax > dy)  /* has vertical scrollbar */
    has_vert_scroll = 1;

  /* draw background */
  iupFlatDrawBox(dc, 0, sb_ih->currentwidth - 1, 0, sb_ih->currentheight - 1, bgcolor, NULL, 1);

  if (is_vert_scrollbar)
    iFlatScrollBarDrawVertical(sb_ih, dc, active, fgcolor, bgcolor, pressed, highlight, ymax, dy, sb_size, has_horiz_scroll);
  else
    iFlatScrollBarDrawHorizontal(sb_ih, dc, active, fgcolor, bgcolor, pressed, highlight, xmax, dx, sb_size, has_vert_scroll);

  iupdrvDrawFlush(dc);

  iupdrvDrawKillCanvas(dc);

  return IUP_DEFAULT;
}

static int iFlatScrollBarGetHandler(Ihandle* sb_ih, int x, int y)
{
  int sb_size = iupAttribGetInt(sb_ih->parent, "SCROLLBARSIZE");
  int xmax = iupAttribGetInt(sb_ih->parent, "XMAX");
  int ymax = iupAttribGetInt(sb_ih->parent, "YMAX");
  int dy = iupAttribGetInt(sb_ih->parent, "DY");
  int dx = iupAttribGetInt(sb_ih->parent, "DX");
  int show_arrows = iupAttribGetInt(sb_ih->parent, "SHOWARROWS");
  int arrow_size = sb_size;
  int is_vert_scrollbar = 0;
  int range, d, pos;

  if (sb_ih->currentwidth == sb_size)
    is_vert_scrollbar = 1;

  if (!show_arrows)
    arrow_size = 0;

  if (is_vert_scrollbar)
  {
    int posy = iupAttribGetInt(sb_ih->parent, "POSY");

    int height = sb_ih->currentheight;
    if (xmax > dx)  /* has horizontal scrollbar */
      height -= sb_size;

    range = height - 1 - 2 * arrow_size;
    d = (dy * range) / ymax;
    pos = (posy * range) / ymax;
    pos += arrow_size;

    if (y < arrow_size)
      return SB_DEC_Y;
    else if (y < pos)
      return SB_PAGEDEC_Y;
    else if (y < pos + d)
      return SB_DRAG_Y;
    else if (y < height - arrow_size)
      return SB_PAGEINC_Y;
    else if (y < height && show_arrows)
      return SB_INC_Y;
  }
  else
  {
    int posx = iupAttribGetInt(sb_ih->parent, "POSX");

    int width = sb_ih->currentwidth;
    if (ymax > dy)  /* has vertical scrollbar */
      width -= sb_size;

    range = width - 1 - 2 * arrow_size;
    d = (dx * range) / xmax;
    pos = (posx * range) / xmax;
    pos += arrow_size;

    if (x < arrow_size)
      return SB_DEC_X;
    else if (x < pos)
      return SB_PAGEDEC_X;
    else if (x < pos + d)
      return SB_DRAG_X;
    else if (x < width - arrow_size)
      return SB_PAGEINC_X;
    else if (x < width && show_arrows)
      return SB_INC_X;
  }

  return SB_NONE;
}

static void iFlatScrollBarPressX(Ihandle* sb_ih, int handler)
{
  int xmax = iupAttribGetInt(sb_ih->parent, "XMAX");
  int dx = iupAttribGetInt(sb_ih->parent, "DX");
  int posx = iupAttribGetInt(sb_ih->parent, "POSX");

  if (handler == SB_INC_X)
  {
    int linex = iFlatScrollBarGetLineX(sb_ih->parent, dx);
    posx += linex;
  }
  else if (handler == SB_PAGEINC_X)
    posx += dx;
  if (handler == SB_DEC_X)
  {
    int linex = iFlatScrollBarGetLineX(sb_ih->parent, dx);
    posx -= linex;
  }
  else if (handler == SB_PAGEDEC_X)
    posx -= dx;

  iFlatScrollBarNormalizePos(&posx, xmax, dx);

  iupAttribSetInt(sb_ih->parent, "POSX", posx);
}

static void iFlatScrollBarPressY(Ihandle* sb_ih, int handler)
{
  int ymax = iupAttribGetInt(sb_ih->parent, "YMAX");
  int dy = iupAttribGetInt(sb_ih->parent, "DY");
  int posy = iupAttribGetInt(sb_ih->parent, "POSY");

  if (handler == SB_INC_Y)
  {
    int liney = iFlatScrollBarGetLineY(sb_ih->parent, dy);
    posy += liney;
  }
  else if (handler == SB_PAGEINC_Y)
    posy += dy;
  if (handler == SB_DEC_Y)
  {
    int liney = iFlatScrollBarGetLineY(sb_ih->parent, dy);
    posy -= liney;
  }
  else if (handler == SB_PAGEDEC_Y)
    posy -= dy;

  iFlatScrollBarNormalizePos(&posy, ymax, dy);

  iupAttribSetInt(sb_ih->parent, "POSY", posy);
}

static int iFlatScrollBarMoveX(Ihandle* sb_ih, int diff, int start_posx)
{
  int xmax = iupAttribGetInt(sb_ih->parent, "XMAX");
  int ymax = iupAttribGetInt(sb_ih->parent, "YMAX");
  int dx = iupAttribGetInt(sb_ih->parent, "DX");
  int dy = iupAttribGetInt(sb_ih->parent, "DY");
  int sb_size = iupAttribGetInt(sb_ih->parent, "SCROLLBARSIZE");
  int range, pos, posx;
  int width = sb_ih->currentwidth;
  int show_arrows = iupAttribGetInt(sb_ih->parent, "SHOWARROWS");
  int arrow_size = sb_size;

  if (!show_arrows)
    arrow_size = 0;

  if (ymax > dy)  /* has vertical scrollbar */
    width -= sb_size;

  range = width - 1 - 2 * arrow_size;

  pos = (start_posx * range) / xmax;
  pos += arrow_size;

  pos += diff;

  pos -= arrow_size;
  posx = (pos * xmax) / range;

  iFlatScrollBarNormalizePos(&posx, xmax, dx);

  if (posx != start_posx)
  {
    iupAttribSetInt(sb_ih->parent, "POSX", posx);
    return 1;
  }

  return 0;
}

static int iFlatScrollBarMoveY(Ihandle* sb_ih, int diff, int start_posy)
{
  int xmax = iupAttribGetInt(sb_ih->parent, "XMAX");
  int ymax = iupAttribGetInt(sb_ih->parent, "YMAX");
  int dx = iupAttribGetInt(sb_ih->parent, "DX");
  int dy = iupAttribGetInt(sb_ih->parent, "DY");
  int sb_size = iupAttribGetInt(sb_ih->parent, "SCROLLBARSIZE");
  int range, pos, posy;
  int height = sb_ih->currentheight;
  int show_arrows = iupAttribGetInt(sb_ih->parent, "SHOWARROWS");
  int arrow_size = sb_size;

  if (!show_arrows)
    arrow_size = 0;

  if (xmax > dx)  /* has horizontal scrollbar */
    height -= sb_size;

  range = height - 1 - 2 * arrow_size;

  pos = (start_posy * range) / ymax;
  pos += arrow_size;

  pos += diff;

  pos -= arrow_size;
  posy = (pos * ymax) / range;

  iFlatScrollBarNormalizePos(&posy, ymax, dy);

  if (posy != start_posy)
  {
    iupAttribSetInt(sb_ih->parent, "POSY", posy);
    return 1;
  }

  return 0;
}

static int iFlatScrollBarButton_CB(Ihandle* sb_ih, int button, int pressed, int x, int y)
{
  if (button != IUP_BUTTON1)
    return IUP_DEFAULT;

  if (pressed)
  {
    int handler = iFlatScrollBarGetHandler(sb_ih, x, y);
    iupAttribSetInt(sb_ih, "_IUP_PRESSED_HANDLER", handler);

    if (handler == SB_DRAG_X || handler == SB_DRAG_Y)
    {
      iupAttribSetInt(sb_ih, "_IUP_START_X", x);
      iupAttribSetInt(sb_ih, "_IUP_START_Y", y);

      if (handler == SB_DRAG_X)
        iupAttribSetStr(sb_ih, "_IUP_START_POSX", iupAttribGet(sb_ih->parent, "POSX"));
      else
        iupAttribSetStr(sb_ih, "_IUP_START_POSY", iupAttribGet(sb_ih->parent, "POSY"));
    }

    if (handler != SB_NONE)
    {
      IupUpdate(sb_ih);
      return IUP_DEFAULT;
    }
  }
  else
  {
    int press_handler = iupAttribGetInt(sb_ih, "_IUP_PRESSED_HANDLER");
    int handler = iFlatScrollBarGetHandler(sb_ih, x, y);

    if (handler != SB_NONE && handler != SB_DRAG_X && handler != SB_DRAG_Y &&
        handler == press_handler)
    {
      if (handler == SB_INC_X || handler == SB_PAGEINC_X ||
          handler == SB_DEC_X || handler == SB_PAGEDEC_X)
        iFlatScrollBarPressX(sb_ih, handler);

      if (handler == SB_INC_Y || handler == SB_PAGEINC_Y ||
          handler == SB_DEC_Y || handler == SB_PAGEDEC_Y)
        iFlatScrollBarPressY(sb_ih, handler);

      iFlatScrollBarNotify(sb_ih->parent, handler);
    }
    iupAttribSet(sb_ih, "_IUP_PRESSED_HANDLER", NULL);

    if (handler != SB_NONE)
    {
      IupUpdate(sb_ih);
      return IUP_DEFAULT;
    }
  }

  return IUP_DEFAULT;
}

static int iFlatScrollBarMotion_CB(Ihandle *sb_ih, int x, int y)
{
  int redraw = 0;
  int handler = iFlatScrollBarGetHandler(sb_ih, x, y);

  /* special highlight processing for scrollbar area */
  int old_handler = iupAttribGetInt(sb_ih, "_IUP_HIGHLIGHT_HANDLER");
  if (old_handler != handler)
  {
    redraw = 1;
    iupAttribSetInt(sb_ih, "_IUP_HIGHLIGHT_HANDLER", handler);
  }

  handler = iupAttribGetInt(sb_ih, "_IUP_PRESSED_HANDLER");
  if (handler == SB_DRAG_X)
  {
    int start_x = iupAttribGetInt(sb_ih, "_IUP_START_X");
    int start_posx = iupAttribGetInt(sb_ih, "_IUP_START_POSX");

    if (x != start_x && iFlatScrollBarMoveX(sb_ih, x - start_x, start_posx))
    {
      iFlatScrollBarNotify(sb_ih->parent, handler);
      redraw = 1;
    }
  }
  else if (handler == SB_DRAG_Y)
  {
    int start_y = iupAttribGetInt(sb_ih, "_IUP_START_Y");
    int start_posy = iupAttribGetInt(sb_ih, "_IUP_START_POSY");

    if (y != start_y && iFlatScrollBarMoveY(sb_ih, y - start_y, start_posy))
    {
      iFlatScrollBarNotify(sb_ih->parent, handler);
      redraw = 1;
    }
  }

  if (redraw)
    IupUpdate(sb_ih);

  return IUP_DEFAULT;
}

static int iFlatScrollBarLeaveWindow_CB(Ihandle* sb_ih)
{
  int handler = iupAttribGetInt(sb_ih, "_IUP_HIGHLIGHT_HANDLER");
  if (handler != SB_NONE)
  {
    iupAttribSet(sb_ih, "_IUP_HIGHLIGHT_HANDLER", NULL);
    IupUpdate(sb_ih);
  }
  return IUP_DEFAULT;
}

void iupFlatScrollBarWheelUpdate(Ihandle* ih, float delta)
{
  int posy = iupAttribGetInt(ih, "POSY");
  int dy = iupAttribGetInt(ih, "DY");
  int liney = iFlatScrollBarGetLineY(ih, dy);
  posy -= (int)(delta * liney);
  iFlatScrollBarNormalizePos(&posy, iupAttribGetInt(ih, "YMAX"), dy);
  iupAttribSetInt(ih, "POSY", posy);
  iFlatScrollBarRedrawVertical(ih);
  iFlatScrollBarNotify(ih, delta>0 ? SB_DEC_Y: SB_INC_Y);
}

static int iFlatScrollBarWheel_CB(Ihandle* sb_ih, float delta)
{
  iupFlatScrollBarWheelUpdate(sb_ih->parent, delta);
  return IUP_DEFAULT;
}




/*****************************************************************************/

static int iFlatScrollBarSetDXAttrib(Ihandle* ih, const char *value)
{
  int dx;
  if (iupStrToInt(value, &dx))
  {
    Ihandle* sb_horiz = iFlatScrollBarGetHorizontal(ih);
    int xmax = iupAttribGetInt(ih, "XMAX");

    if (dx >= xmax)
    {
      IupSetAttribute(sb_horiz, "VISIBLE", "NO");
      iupAttribSet(ih, "POSX", "0");
    }
    else
    {
      int posx = iupAttribGetInt(ih, "POSX");
      if (posx > xmax - dx)
        iupAttribSetInt(ih, "POSX", xmax - dx);

      IupSetAttribute(sb_horiz, "VISIBLE", "Yes");

      iFlatScrollBarRedrawHorizontal(ih);  /* force a redraw if it is already visible */

      iFlatScrollBarNotify(ih, -1);
    }

    iFlatScrollBarRedrawVertical(ih);  /* force a redraw of the other scrollbar */
  }

  return 1;
}

static int iFlatScrollBarSetDYAttrib(Ihandle* ih, const char *value)
{
  int dy;
  if (iupStrToInt(value, &dy))
  {
    Ihandle* sb_vert = iFlatScrollBarGetVertical(ih);
    int ymax = iupAttribGetInt(ih, "YMAX");

    if (dy >= ymax)
    {
      IupSetAttribute(sb_vert, "VISIBLE", "NO");
      iupAttribSet(ih, "POSY", "0");
    }
    else
    {
      int posy = iupAttribGetInt(ih, "POSY");
      if (posy > ymax - dy)
        iupAttribSetInt(ih, "POSY", ymax - dy);

      IupSetAttribute(sb_vert, "VISIBLE", "Yes");

      iFlatScrollBarRedrawVertical(ih);  /* force a redraw if it is already visible */

      iFlatScrollBarNotify(ih, -1);
    }

    iFlatScrollBarRedrawHorizontal(ih);  /* force a redraw of the other scrollbar */
  }

  return 1;
}

static int iFlatScrollBarSetPosXAttrib(Ihandle* ih, const char *value)
{
  int xmax, dx;
  int posx;

  if (!iupStrToInt(value, &posx))
    return 0;

  xmax = iupAttribGetInt(ih, "XMAX");
  dx = iupAttribGetInt(ih, "DX");

  iFlatScrollBarNormalizePos(&posx, xmax, dx);

  iupAttribSetInt(ih, "POSX", posx);

  iFlatScrollBarRedrawHorizontal(ih);

  iFlatScrollBarNotify(ih, -1);

  return 0;
}

static int iFlatScrollBarSetPosYAttrib(Ihandle* ih, const char *value)
{
  int ymax, dy;
  int posy;

  if (!iupStrToInt(value, &posy))
    return 0;

  ymax = iupAttribGetInt(ih, "YMAY");
  dy = iupAttribGetInt(ih, "DY");

  iFlatScrollBarNormalizePos(&posy, ymax, dy);

  iupAttribSetInt(ih, "POSY", posy);

  iFlatScrollBarRedrawVertical(ih);

  iFlatScrollBarNotify(ih, -1);

  return 0;
}


/*******************************************************************************************************/


void iupFlatScrollBarSetChildrenCurrentSize(Ihandle* ih, int shrink)
{
  Ihandle* sb_vert = iFlatScrollBarGetVertical(ih);
  Ihandle* sb_horiz = iFlatScrollBarGetHorizontal(ih);
  int sb_size = iupAttribGetInt(ih, "SCROLLBARSIZE");

  iupBaseSetCurrentSize(sb_vert, sb_size, ih->currentheight, shrink);
  iupBaseSetCurrentSize(sb_horiz, ih->currentwidth, sb_size, shrink);
}

void iupFlatScrollBarSetChildrenPosition(Ihandle* ih)
{
  Ihandle* sb_vert = iFlatScrollBarGetVertical(ih);
  Ihandle* sb_horiz = iFlatScrollBarGetHorizontal(ih);
  int sb_size = iupAttribGetInt(ih, "SCROLLBARSIZE");

  iupBaseSetPosition(sb_vert, ih->currentwidth - sb_size, 0);
  iupBaseSetPosition(sb_horiz, 0, ih->currentheight - sb_size);

  if (IupGetInt(sb_vert, "VISIBLE"))
    IupSetAttribute(sb_vert, "ZORDER", "TOP");
  if (IupGetInt(sb_horiz, "VISIBLE"))
    IupSetAttribute(sb_horiz, "ZORDER", "TOP");
}


/******************************************************************************/


void iupFlatScrollBarCreate(Ihandle* ih)
{
  Ihandle* sb_vert, *sb_horiz;

  sb_vert = IupCanvas(NULL);
  IupSetAttribute(sb_vert, "BORDER", "NO");
  IupSetAttribute(sb_vert, "ZORDER", "TOP");
  iupChildTreeAppend(ih, sb_vert);  /* sb_vert will always be the firstchild */
  sb_vert->flags |= IUP_INTERNAL;
  IupSetCallback(sb_vert, "ACTION", (Icallback)iFlatScrollBarAction_CB);
  IupSetCallback(sb_vert, "BUTTON_CB", (Icallback)iFlatScrollBarButton_CB);
  IupSetCallback(sb_vert, "MOTION_CB", (Icallback)iFlatScrollBarMotion_CB);
  IupSetCallback(sb_vert, "LEAVEWINDOW_CB", (Icallback)iFlatScrollBarLeaveWindow_CB);
  IupSetCallback(sb_vert, "WHEEL_CB", (Icallback)iFlatScrollBarWheel_CB);
  IupSetAttribute(sb_vert, "CANFOCUS", "NO");

  sb_horiz = IupCanvas(NULL);
  IupSetAttribute(sb_horiz, "BORDER", "NO");
  IupSetAttribute(sb_horiz, "ZORDER", "TOP");
  iupChildTreeAppend(ih, sb_horiz);  /* sb_vert will always be the firstchild->brother */
  sb_horiz->flags |= IUP_INTERNAL;
  IupSetCallback(sb_horiz, "ACTION", (Icallback)iFlatScrollBarAction_CB);
  IupSetCallback(sb_horiz, "BUTTON_CB", (Icallback)iFlatScrollBarButton_CB);
  IupSetCallback(sb_horiz, "MOTION_CB", (Icallback)iFlatScrollBarMotion_CB);
  IupSetCallback(sb_horiz, "LEAVEWINDOW_CB", (Icallback)iFlatScrollBarLeaveWindow_CB);
  IupSetCallback(sb_horiz, "WHEEL_CB", (Icallback)iFlatScrollBarWheel_CB);
  IupSetAttribute(sb_horiz, "CANFOCUS", "NO");
}

void iupFlatScrollBarRegister(Iclass* ic)
{
  iupClassRegisterCallback(ic, "FLATSCROLL_CB", "");

  //  iupClassRegisterAttribute(ic, "SCROLLBAR", iFlatScrollBarGetScrollbarAttrib, NULL, NULL, NULL, IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);
  iupClassRegisterReplaceAttribFlags(ic, "SCROLLBAR", IUPAF_READONLY | IUPAF_NO_INHERIT);  /* will be always NO, but it will behave as always Yes */

  iupClassRegisterAttribute(ic, "YAUTOHIDE", NULL, NULL, IUPAF_SAMEASSYSTEM, "YES", IUPAF_READONLY | IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);  /* will be always Yes */
  iupClassRegisterAttribute(ic, "XAUTOHIDE", NULL, NULL, IUPAF_SAMEASSYSTEM, "YES", IUPAF_READONLY | IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);  /* will be always Yes */

  iupClassRegisterAttribute(ic, "DX", NULL, iFlatScrollBarSetDXAttrib, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "DY", NULL, iFlatScrollBarSetDYAttrib, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "POSX", NULL, iFlatScrollBarSetPosXAttrib, "0", NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "POSY", NULL, iFlatScrollBarSetPosYAttrib, "0", NULL, IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "SCROLLBARSIZE", NULL, NULL, IUPAF_SAMEASSYSTEM, "15", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "HIGHCOLOR", NULL, NULL, IUPAF_SAMEASSYSTEM, "132 132 132", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "FORECOLOR", NULL, NULL, IUPAF_SAMEASSYSTEM, "220 220 220", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "PRESSCOLOR", NULL, NULL, IUPAF_SAMEASSYSTEM, "96 96 96", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "BACKCOLOR", NULL, NULL, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "SHOWARROWS", NULL, NULL, IUPAF_SAMEASSYSTEM, "Yes", IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "ARROWIMAGES", NULL, NULL, NULL, NULL, IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGELEFT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGELEFTPRESS", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGELEFTHIGHLIGHT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGELEFTINACTIVE", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGERIGHT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGERIGHTPRESS", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGERIGHTHIGHLIGHT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGERIGHTINACTIVE", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGETOP", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGETOPPRESS", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGETOPHIGHLIGHT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGETOPINACTIVE", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGEBOTTOM", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBOTTOMPRESS", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBOTTOMHIGHLIGHT", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBOTTOMINACTIVE", NULL, NULL, NULL, NULL, IUPAF_IHANDLENAME | IUPAF_NO_DEFAULTVALUE | IUPAF_NO_INHERIT);
}
