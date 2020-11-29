/***********************************************************************
 Freeciv - Copyright (C) 2001 - R. Falke, M. Kaufman
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* SDL2 */
#ifdef SDL2_PLAIN_INCLUDE
#include <SDL.h>
#else  /* SDL2_PLAIN_INCLUDE */
#include <SDL2/SDL.h>
#endif /* SDL2_PLAIN_INCLUDE */

/* utility */
#include "fcintl.h"

/* common */
#include "game.h"

/* client */
#include "client_main.h" /* can_client_issue_orders() */

/* gui-sdl2 */
#include "citydlg.h"
#include "cityrep.h"
#include "cma_fec.h"
#include "colors.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "sprite.h"
#include "themespec.h"
#include "widget.h"

#include "cma_fe.h"

struct hmove {
  struct widget *pscroll_bar;
  int min, max, base;
};

static struct cma_dialog {
  struct city *pcity;
  struct small_dialog *dlg;
  struct advanced_dialog *pAdv;
  struct cm_parameter edited_cm_parm;
} *pCma = NULL;

enum specialist_type {
  SP_ELVIS, SP_SCIENTIST, SP_TAXMAN, SP_LAST
};

static void set_cma_hscrollbars(void);

/* =================================================================== */

/**********************************************************************//**
  User interacted with cma dialog.
**************************************************************************/
static int cma_dlg_callback(struct widget *pwindow)
{
  return -1;
}

/**********************************************************************//**
  User interacted with cma dialog close button.
**************************************************************************/
static int exit_cma_dialog_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popdown_city_cma_dialog();
    flush_dirty();
  }

  return -1;
}

/**********************************************************************//**
  User released mouse button while in scrollbar.
**************************************************************************/
static Uint16 scroll_mouse_button_up(SDL_MouseButtonEvent *button_event,
                                     void *pData)
{
  return (Uint16)ID_SCROLLBAR;
}

/**********************************************************************//**
  User moved mouse while holding scrollbar.
**************************************************************************/
static Uint16 scroll_mouse_motion_handler(SDL_MouseMotionEvent *pMotionEvent,
                                          void *pData)
{
  struct hmove *pMotion = (struct hmove *)pData;
  char cbuf[4];

  pMotionEvent->x -= pMotion->pscroll_bar->dst->dest_rect.x;

  if (pMotion && pMotionEvent->xrel
      && (pMotionEvent->x >= pMotion->min) && (pMotionEvent->x <= pMotion->max)) {
    /* draw bcgd */
    widget_undraw(pMotion->pscroll_bar);
    widget_mark_dirty(pMotion->pscroll_bar);

    if ((pMotion->pscroll_bar->size.x + pMotionEvent->xrel) >
        (pMotion->max - pMotion->pscroll_bar->size.w)) {
      pMotion->pscroll_bar->size.x = pMotion->max - pMotion->pscroll_bar->size.w;
    } else {
      if ((pMotion->pscroll_bar->size.x + pMotionEvent->xrel) < pMotion->min) {
	pMotion->pscroll_bar->size.x = pMotion->min;
      } else {
	pMotion->pscroll_bar->size.x += pMotionEvent->xrel;
      }
    }

    *(int *)pMotion->pscroll_bar->data.ptr =
      pMotion->base + (pMotion->pscroll_bar->size.x - pMotion->min);

    fc_snprintf(cbuf, sizeof(cbuf), "%d", *(int *)pMotion->pscroll_bar->data.ptr);
    copy_chars_to_utf8_str(pMotion->pscroll_bar->next->string_utf8, cbuf);

    /* redraw label */
    widget_redraw(pMotion->pscroll_bar->next);
    widget_mark_dirty(pMotion->pscroll_bar->next);

    /* redraw scroolbar */
    if (get_wflags(pMotion->pscroll_bar) & WF_RESTORE_BACKGROUND) {
      refresh_widget_background(pMotion->pscroll_bar);
    }
    widget_redraw(pMotion->pscroll_bar);
    widget_mark_dirty(pMotion->pscroll_bar);

    flush_dirty();
  }

  return ID_ERROR;
}

/**********************************************************************//**
  User interacted with minimal horizontal cma scrollbar
**************************************************************************/
static int min_horiz_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct hmove pMotion;

    pMotion.pscroll_bar = pwidget;
    pMotion.min = pwidget->next->size.x + pwidget->next->size.w + 5;
    pMotion.max = pMotion.min + 70;
    pMotion.base = -20;

    MOVE_STEP_X = 2;
    MOVE_STEP_Y = 0;
    /* Filter mouse motion events */
    SDL_SetEventFilter(FilterMouseMotionEvents, NULL);
    gui_event_loop((void *)(&pMotion), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                   scroll_mouse_button_up, scroll_mouse_motion_handler);
    /* Turn off Filter mouse motion events */
    SDL_SetEventFilter(NULL, NULL);
    MOVE_STEP_X = DEFAULT_MOVE_STEP;
    MOVE_STEP_Y = DEFAULT_MOVE_STEP;

    selected_widget = pwidget;
    set_wstate(pwidget, FC_WS_SELECTED);
    /* save the change */
    cmafec_set_fe_parameter(pCma->pcity, &pCma->edited_cm_parm);
    /* refreshes the cma */
    if (cma_is_city_under_agent(pCma->pcity, NULL)) {
      cma_release_city(pCma->pcity);
      cma_put_city_under_agent(pCma->pcity, &pCma->edited_cm_parm);
    }
    update_city_cma_dialog();
  }

  return -1;
}

/**********************************************************************//**
  User interacted with factor horizontal cma scrollbar
**************************************************************************/
static int factor_horiz_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct hmove pMotion;

    pMotion.pscroll_bar = pwidget;
    pMotion.min = pwidget->next->size.x + pwidget->next->size.w + 5;
    pMotion.max = pMotion.min + 54;
    pMotion.base = 1;

    MOVE_STEP_X = 2;
    MOVE_STEP_Y = 0;
    /* Filter mouse motion events */
    SDL_SetEventFilter(FilterMouseMotionEvents, NULL);
    gui_event_loop((void *)(&pMotion), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                   scroll_mouse_button_up, scroll_mouse_motion_handler);
    /* Turn off Filter mouse motion events */
    SDL_SetEventFilter(NULL, NULL);
    MOVE_STEP_X = DEFAULT_MOVE_STEP;
    MOVE_STEP_Y = DEFAULT_MOVE_STEP;

    selected_widget = pwidget;
    set_wstate(pwidget, FC_WS_SELECTED);
    /* save the change */
    cmafec_set_fe_parameter(pCma->pcity, &pCma->edited_cm_parm);
    /* refreshes the cma */
    if (cma_is_city_under_agent(pCma->pcity, NULL)) {
      cma_release_city(pCma->pcity);
      cma_put_city_under_agent(pCma->pcity, &pCma->edited_cm_parm);
    }
    update_city_cma_dialog();
  }

  return -1;
}

/**********************************************************************//**
  User interacted with cma celebrating -toggle.
**************************************************************************/
static int toggle_cma_celebrating_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    pCma->edited_cm_parm.require_happy ^= TRUE;
    /* save the change */
    cmafec_set_fe_parameter(pCma->pcity, &pCma->edited_cm_parm);
    update_city_cma_dialog();
  }

  return -1;
}

/* ============================================================= */

/**********************************************************************//**
  User interacted with widget that result in cma window getting saved.
**************************************************************************/
static int save_cma_window_callback(struct widget *pwindow)
{
  return -1;
}

/**********************************************************************//**
  User interacted with "yes" button from save cma dialog.
**************************************************************************/
static int ok_save_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    if (pwidget && pCma && pCma->pAdv) {
      struct widget *pedit = (struct widget *)pwidget->data.ptr;

      if (pedit->string_utf8->text != NULL) {
        cmafec_preset_add(pedit->string_utf8->text, &pCma->edited_cm_parm);
      } else {
        cmafec_preset_add(_("new preset"), &pCma->edited_cm_parm);
      }

      del_group_of_widgets_from_gui_list(pCma->pAdv->begin_widget_list,
                                         pCma->pAdv->end_widget_list);
      FC_FREE(pCma->pAdv);

      update_city_cma_dialog();
    }
  }

  return -1;
}

/**********************************************************************//**
  Cancel : SAVE, LOAD, DELETE Dialogs
**************************************************************************/
static int cancel_SLD_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    if (pCma && pCma->pAdv) {
      popdown_window_group_dialog(pCma->pAdv->begin_widget_list,
                                  pCma->pAdv->end_widget_list);
      FC_FREE(pCma->pAdv->scroll);
      FC_FREE(pCma->pAdv);
      flush_dirty();
    }
  }

  return -1;
}

/**********************************************************************//**
  User interacted with cma setting saving button.
**************************************************************************/
static int save_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct widget *buf, *pwindow;
    utf8_str *pstr;
    SDL_Surface *text;
    SDL_Rect dst;
    SDL_Rect area;

    if (pCma->pAdv) {
      return 1;
    }

    pCma->pAdv = fc_calloc(1, sizeof(struct advanced_dialog));

    pstr = create_utf8_from_char(_("Name new preset"), adj_font(12));
    pstr->style |= TTF_STYLE_BOLD;

    pwindow = create_window_skeleton(NULL, pstr, 0);

    pwindow->action = save_cma_window_callback;
    set_wstate(pwindow, FC_WS_NORMAL);
    pCma->pAdv->end_widget_list = pwindow;

    add_to_gui_list(ID_WINDOW, pwindow);

    area = pwindow->area;
    area.h = MAX(area.h, 1);

    /* ============================================================= */
    /* label */
    pstr = create_utf8_from_char(_("What should we name the preset?"), adj_font(10));
    pstr->style |= (TTF_STYLE_BOLD|SF_CENTER);
    pstr->fgcol = *get_theme_color(COLOR_THEME_CMA_TEXT);

    text = create_text_surf_from_utf8(pstr);
    FREEUTF8STR(pstr);
    area.w = MAX(area.w, text->w);
    area.h += text->h + adj_size(5);
    /* ============================================================= */

    buf = create_edit(NULL, pwindow->dst,
                       create_utf8_from_char(_("new preset"), adj_font(12)), adj_size(100),
                       (WF_RESTORE_BACKGROUND|WF_FREE_STRING));
    set_wstate(buf, FC_WS_NORMAL);
    area.h += buf->size.h;
    area.w = MAX(area.w, buf->size.w);

    add_to_gui_list(ID_EDIT, buf);
    /* ============================================================= */

    buf = create_themeicon_button_from_chars(current_theme->OK_Icon, pwindow->dst,
                                              _("Yes"), adj_font(12), 0);

    buf->action = ok_save_cma_callback;
    set_wstate(buf, FC_WS_NORMAL);
    buf->key = SDLK_RETURN;
    add_to_gui_list(ID_BUTTON, buf);
    buf->data.ptr = (void *)buf->next;

    buf = create_themeicon_button_from_chars(current_theme->CANCEL_Icon,
                                              pwindow->dst, _("No"),
                                              adj_font(12), 0);
    buf->action = cancel_SLD_cma_callback;
    set_wstate(buf, FC_WS_NORMAL);
    buf->key = SDLK_ESCAPE;

    add_to_gui_list(ID_BUTTON, buf);

    area.h += buf->size.h;
    buf->size.w = MAX(buf->next->size.w, buf->size.w);
    buf->next->size.w = buf->size.w;
    area.w = MAX(area.w, 2 * buf->size.w + adj_size(20));

    pCma->pAdv->begin_widget_list = buf;

    /* setup window size and start position */
    area.w += adj_size(20);
    area.h += adj_size(15);

    resize_window(pwindow, NULL, get_theme_color(COLOR_THEME_BACKGROUND),
                  (pwindow->size.w - pwindow->area.w) + area.w,
                  (pwindow->size.h - pwindow->area.h) + area.h);

    area = pwindow->area;

    widget_set_position(pwindow,
                        pwidget->size.x - pwindow->size.w / 2,
                        pwidget->size.y - pwindow->size.h);

    /* setup rest of widgets */
    /* label */
    dst.x = area.x + (area.w - text->w) / 2;
    dst.y = area.y + 1;
    alphablit(text, NULL, pwindow->theme, &dst, 255);
    dst.y += text->h + adj_size(5);
    FREESURFACE(text);

    /* edit */
    buf = pwindow->prev;
    buf->size.w = area.w - adj_size(10);
    buf->size.x = area.x + adj_size(5);
    buf->size.y = dst.y;
    dst.y += buf->size.h + adj_size(5);

    /* yes */
    buf = buf->prev;
    buf->size.x = area.x + (area.w - (2 * buf->size.w + adj_size(20))) / 2;
    buf->size.y = dst.y;

    /* no */
    buf = buf->prev;
    buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(20);
    buf->size.y = buf->next->size.y;

    /* ================================================== */
    /* redraw */
    redraw_group(pCma->pAdv->begin_widget_list, pwindow, 0);
    widget_mark_dirty(pwindow);
    flush_dirty();
  }

  return -1;
}

/* ================================================== */

/**********************************************************************//**
  User interacted with some preset cma button.
**************************************************************************/
static int LD_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    bool load = pwidget->data.ptr != NULL;
    int index = MAX_ID - pwidget->ID;

    popdown_window_group_dialog(pCma->pAdv->begin_widget_list,
                                pCma->pAdv->end_widget_list);
    FC_FREE(pCma->pAdv->scroll);
    FC_FREE(pCma->pAdv);

    if (load) {
      cm_copy_parameter(&pCma->edited_cm_parm, cmafec_preset_get_parameter(index));
      set_cma_hscrollbars();
      /* save the change */
      cmafec_set_fe_parameter(pCma->pcity, &pCma->edited_cm_parm);
      /* stop the cma */
      if (cma_is_city_under_agent(pCma->pcity, NULL)) {
        cma_release_city(pCma->pcity);
      }
    } else {
      cmafec_preset_remove(index);
    }

    update_city_cma_dialog();
  }

  return -1;
}

/**********************************************************************//**
  User clicked either load or delete preset widget.
**************************************************************************/
static void popup_load_del_presets_dialog(bool load, struct widget *button)
{
  int hh, count, i;
  struct widget *buf, *pwindow;
  utf8_str *pstr;
  SDL_Rect area;

  if (pCma->pAdv) {
    return;
  }

  count = cmafec_preset_num();

  if (count == 1) {
    if (load) {
      cm_copy_parameter(&pCma->edited_cm_parm, cmafec_preset_get_parameter(0));
      set_cma_hscrollbars();
      /* save the change */
      cmafec_set_fe_parameter(pCma->pcity, &pCma->edited_cm_parm);
      /* stop the cma */
      if (cma_is_city_under_agent(pCma->pcity, NULL)) {
        cma_release_city(pCma->pcity);
      }
    } else {
      cmafec_preset_remove(0);
    }
    update_city_cma_dialog();
    return;
  }

  pCma->pAdv = fc_calloc(1, sizeof(struct advanced_dialog));

  pstr = create_utf8_from_char(_("Presets"), adj_font(12));
  pstr->style |= TTF_STYLE_BOLD;

  pwindow = create_window_skeleton(NULL, pstr, 0);

  pwindow->action = save_cma_window_callback;
  set_wstate(pwindow, FC_WS_NORMAL);
  pCma->pAdv->end_widget_list = pwindow;

  add_to_gui_list(ID_WINDOW, pwindow);

  area = pwindow->area;

  /* ---------- */
  /* create exit button */
  buf = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                          WF_WIDGET_HAS_INFO_LABEL | WF_RESTORE_BACKGROUND);
  buf->info_label = create_utf8_from_char(_("Close Dialog (Esc)"),
                                           adj_font(12));
  buf->action = cancel_SLD_cma_callback;
  set_wstate(buf, FC_WS_NORMAL);
  buf->key = SDLK_ESCAPE;
  area.w += (buf->size.w + adj_size(10));

  add_to_gui_list(ID_BUTTON, buf);
  /* ---------- */

  for (i = 0; i < count; i++) {
    pstr = create_utf8_from_char(cmafec_preset_get_descr(i), adj_font(10));
    pstr->style |= TTF_STYLE_BOLD;
    buf = create_iconlabel(NULL, pwindow->dst, pstr,
    	     (WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));
    buf->string_utf8->bgcol = (SDL_Color) {0, 0, 0, 0};
    buf->action = LD_cma_callback;

    area.w = MAX(area.w, buf->size.w);
    area.h += buf->size.h;
    set_wstate(buf , FC_WS_NORMAL);

    if (load) {
      buf->data.ptr = (void *)1;
    } else {
      buf->data.ptr = NULL;
    }

    add_to_gui_list(MAX_ID - i, buf);
    
    if (i > 10) {
      set_wflag(buf, WF_HIDDEN);
    }
  }
  pCma->pAdv->begin_widget_list = buf;
  pCma->pAdv->begin_active_widget_list = pCma->pAdv->begin_widget_list;
  pCma->pAdv->end_active_widget_list = pwindow->prev->prev;
  pCma->pAdv->active_widget_list = pCma->pAdv->end_active_widget_list;

  area.w += adj_size(2);
  area.h += 1;

  if (count > 11) {
    create_vertical_scrollbar(pCma->pAdv, 1, 11, FALSE, TRUE);

    /* ------- window ------- */
    area.h = 11 * pwindow->prev->prev->size.h + adj_size(2)
      + 2 * pCma->pAdv->scroll->up_left_button->size.h;
    pCma->pAdv->scroll->up_left_button->size.w = area.w;
    pCma->pAdv->scroll->down_right_button->size.w = area.w;
  }

  /* ----------------------------------- */

  resize_window(pwindow, NULL, NULL,
                (pwindow->size.w - pwindow->area.w) + area.w,
                (pwindow->size.h - pwindow->area.h) + area.h);

  area = pwindow->area;

  widget_set_position(pwindow,
                      button->size.x - (pwindow->size.w / 2),
                      button->size.y - pwindow->size.h);

  /* exit button */
  buf = pwindow->prev;
  buf->size.x = area.x + area.w - buf->size.w - 1;
  buf->size.y = pwindow->size.y + adj_size(2);

  buf = buf->prev;
  hh = (pCma->pAdv->scroll ? pCma->pAdv->scroll->up_left_button->size.h + 1 : 0);
  setup_vertical_widgets_position(1, area.x + 1,
                                  area.y + 1 + hh, area.w - 1, 0,
                                  pCma->pAdv->begin_active_widget_list, buf);

  if (pCma->pAdv->scroll) {
    pCma->pAdv->scroll->up_left_button->size.x = area.x;
    pCma->pAdv->scroll->up_left_button->size.y = area.y;
    pCma->pAdv->scroll->down_right_button->size.x = area.x;
    pCma->pAdv->scroll->down_right_button->size.y =
      area.y + area.h - pCma->pAdv->scroll->down_right_button->size.h;
  }

  /* ==================================================== */
  /* redraw */
  redraw_group(pCma->pAdv->begin_widget_list, pwindow, 0);

  widget_flush(pwindow);
}

/**********************************************************************//**
  User interacted with load cma settings -widget
**************************************************************************/
static int load_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_load_del_presets_dialog(TRUE, pwidget);
  }

  return -1;
}

/**********************************************************************//**
  User interacted with delete cma settings -widget
**************************************************************************/
static int del_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_load_del_presets_dialog(FALSE, pwidget);
  }

  return -1;
}

/* ================================================== */

/**********************************************************************//**
  Changes the workers of the city to the cma parameters and puts the
  city under agent control
**************************************************************************/
static int run_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    cma_put_city_under_agent(pCma->pcity, &pCma->edited_cm_parm);
    update_city_cma_dialog();
  }

  return -1;
}

/**********************************************************************//**
  Changes the workers of the city to the cma parameters
**************************************************************************/
static int run_cma_once_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct cm_result *result;

    update_city_cma_dialog();
    /* fill in result label */
    result = cm_result_new(pCma->pcity);
    cm_query_result(pCma->pcity, &pCma->edited_cm_parm, result, FALSE);
    cma_apply_result(pCma->pcity, result);
    cm_result_destroy(result);
  }

  return -1;
}

/**********************************************************************//**
  User interacted with release city from cma -widget
**************************************************************************/
static int stop_cma_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    cma_release_city(pCma->pcity);
    update_city_cma_dialog();
  }

  return -1;
}

/* ===================================================================== */

/**********************************************************************//**
  Setup horizontal cma scrollbars
**************************************************************************/
static void set_cma_hscrollbars(void)
{
  struct widget *pbuf;
  char cbuf[4];

  if (!pCma) {
    return;
  }

  /* exit button */
  pbuf = pCma->dlg->end_widget_list->prev;
  output_type_iterate(i) {
    /* min label */
    pbuf = pbuf->prev;
    fc_snprintf(cbuf, sizeof(cbuf), "%d", *(int *)pbuf->prev->data.ptr);
    copy_chars_to_utf8_str(pbuf->string_utf8, cbuf);

    /* min scrollbar */
    pbuf = pbuf->prev;
    pbuf->size.x = pbuf->next->size.x
    	+ pbuf->next->size.w + adj_size(5) + adj_size(20) + *(int *)pbuf->data.ptr;

    /* factor label */
    pbuf = pbuf->prev;
    fc_snprintf(cbuf, sizeof(cbuf), "%d", *(int *)pbuf->prev->data.ptr);
    copy_chars_to_utf8_str(pbuf->string_utf8, cbuf);

    /* factor scrollbar*/
    pbuf = pbuf->prev;
    pbuf->size.x = pbuf->next->size.x
      + pbuf->next->size.w + adj_size(5) + *(int *)pbuf->data.ptr - 1;
  } output_type_iterate_end;

  /* happy factor label */
  pbuf = pbuf->prev;
  fc_snprintf(cbuf, sizeof(cbuf), "%d", *(int *)pbuf->prev->data.ptr);
  copy_chars_to_utf8_str(pbuf->string_utf8, cbuf);

  /* happy factor scrollbar */
  pbuf = pbuf->prev;
  pbuf->size.x = pbuf->next->size.x
    + pbuf->next->size.w + adj_size(5) + *(int *)pbuf->data.ptr - 1;
}

/**********************************************************************//**
  Update cma dialog
**************************************************************************/
void update_city_cma_dialog(void)
{
  SDL_Color bg_color = {255, 255, 255, 136};
  int count, step, i;
  struct widget *buf = pCma->dlg->end_widget_list; /* pwindow */
  SDL_Surface *text;
  utf8_str *pstr;
  SDL_Rect dst;
  bool cma_presets_exist = cmafec_preset_num() > 0;
  bool client_under_control = can_client_issue_orders();
  bool controlled = cma_is_city_under_agent(pCma->pcity, NULL);
  struct cm_result *result = cm_result_new(pCma->pcity);

  /* redraw window background and exit button */
  redraw_group(buf->prev, buf, 0);

  /* fill in result label */
  cm_result_from_main_map(result, pCma->pcity);

  if (result->found_a_valid) {
    /* redraw Citizens */
    count = city_size_get(pCma->pcity);

    text = get_tax_surface(O_LUXURY);
    step = (buf->size.w - adj_size(20)) / text->w;
    if (count > step) {
      step = (buf->size.w - adj_size(20) - text->w) / (count - 1);
    } else {
      step = text->w;
    }

    dst.y = buf->area.y + adj_size(4);
    dst.x = buf->area.x + adj_size(7);

    for (i = 0;
         i < count - (result->specialists[SP_ELVIS]
                      + result->specialists[SP_SCIENTIST]
                      + result->specialists[SP_TAXMAN]); i++) {
      text = adj_surf(get_citizen_surface(CITIZEN_CONTENT, i));
      alphablit(text, NULL, buf->dst->surface, &dst, 255);
      dst.x += step;
    }

    text = get_tax_surface(O_LUXURY);
    for (i = 0; i < result->specialists[SP_ELVIS]; i++) {
      alphablit(text, NULL, buf->dst->surface, &dst, 255);
      dst.x += step;
    }

    text = get_tax_surface(O_GOLD);
    for (i = 0; i < result->specialists[SP_TAXMAN]; i++) {
      alphablit(text, NULL, buf->dst->surface, &dst, 255);
      dst.x += step;
    }

    text = get_tax_surface(O_SCIENCE);
    for (i = 0; i < result->specialists[SP_SCIENTIST]; i++) {
      alphablit(text, NULL, buf->dst->surface, &dst, 255);
      dst.x += step;
    }
  }

  /* create result text surface */
  pstr = create_utf8_from_char(cmafec_get_result_descr(pCma->pcity, result,
                                                       &pCma->edited_cm_parm),
                               adj_font(12));

  text = create_text_surf_from_utf8(pstr);
  FREEUTF8STR(pstr);

  /* fill result text background */  
  dst.x = buf->area.x + adj_size(7);
  dst.y = buf->area.y + adj_size(186);
  dst.w = text->w + adj_size(10);
  dst.h = text->h + adj_size(10);
  fill_rect_alpha(buf->dst->surface, &dst, &bg_color);

  create_frame(buf->dst->surface,
               dst.x, dst.y, dst.x + dst.w - 1, dst.y + dst.h - 1,
               get_theme_color(COLOR_THEME_CMA_FRAME));

  dst.x += adj_size(5);
  dst.y += adj_size(5);
  alphablit(text, NULL, buf->dst->surface, &dst, 255);
  FREESURFACE(text);

  /* happy factor scrollbar */
  buf = pCma->dlg->begin_widget_list->next->next->next->next->next->next->next;
  if (client_under_control && get_checkbox_state(buf->prev)) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* save as ... */
  buf = buf->prev->prev;
  if (client_under_control) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* load */
  buf = buf->prev;
  if (cma_presets_exist && client_under_control) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* del */
  buf = buf->prev;
  if (cma_presets_exist && client_under_control) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* Run */
  buf = buf->prev;
  if (client_under_control && result->found_a_valid && !controlled) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* Run once */
  buf = buf->prev;
  if (client_under_control && result->found_a_valid && !controlled) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* stop */
  buf = buf->prev;
  if (client_under_control && controlled) {
    set_wstate(buf, FC_WS_NORMAL);
  } else {
    set_wstate(buf, FC_WS_DISABLED);
  }

  /* redraw rest widgets */
  redraw_group(pCma->dlg->begin_widget_list,
               pCma->dlg->end_widget_list->prev->prev, 0);

  widget_flush(pCma->dlg->end_widget_list);

  cm_result_destroy(result);
}

/**********************************************************************//**
  Open cma dialog for city.
**************************************************************************/
void popup_city_cma_dialog(struct city *pcity)
{
  SDL_Color bg_color = {255, 255, 255, 136};

  struct widget *pwindow, *buf;
  SDL_Surface *logo, *text[O_LAST + 1], *pMinimal, *pFactor;
  SDL_Surface *pcity_map;
  utf8_str *pstr;
  char cBuf[128];
  int w, text_w, x, cs;
  SDL_Rect dst, area;

  if (pCma) {
    return;
  }

  pCma = fc_calloc(1, sizeof(struct cma_dialog));
  pCma->pcity = pcity;
  pCma->dlg = fc_calloc(1, sizeof(struct small_dialog));
  pCma->pAdv = NULL;
  pcity_map = get_scaled_city_map(pcity);

  cmafec_get_fe_parameter(pcity, &pCma->edited_cm_parm);

  /* --------------------------- */

  fc_snprintf(cBuf, sizeof(cBuf),
              _("City of %s (Population %s citizens) : %s"),
              city_name_get(pcity),
              population_to_text(city_population(pcity)),
              _("Citizen Governor"));

  pstr = create_utf8_from_char(cBuf, adj_font(12));
  pstr->style |= TTF_STYLE_BOLD;

  pwindow = create_window_skeleton(NULL, pstr, 0);

  pwindow->action = cma_dlg_callback;
  set_wstate(pwindow, FC_WS_NORMAL);
  add_to_gui_list(ID_WINDOW, pwindow);
  pCma->dlg->end_widget_list = pwindow;

  area = pwindow->area;

  /* ---------- */
  /* create exit button */
  buf = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                          WF_WIDGET_HAS_INFO_LABEL | WF_RESTORE_BACKGROUND);
  buf->info_label = create_utf8_from_char(_("Close Dialog (Esc)"),
                                           adj_font(12));
  buf->action = exit_cma_dialog_callback;
  set_wstate(buf, FC_WS_NORMAL);
  buf->key = SDLK_ESCAPE;
  area.w += (buf->size.w + adj_size(10));

  add_to_gui_list(ID_BUTTON, buf);

  pstr = create_utf8_str(NULL, 0, adj_font(12));
  text_w = 0;

  copy_chars_to_utf8_str(pstr, _("Minimal Surplus"));
  pMinimal = create_text_surf_from_utf8(pstr);

  copy_chars_to_utf8_str(pstr, _("Factor"));
  pFactor = create_text_surf_from_utf8(pstr);

  /* ---------- */
  output_type_iterate(i) {
    copy_chars_to_utf8_str(pstr, get_output_name(i));
    text[i] = create_text_surf_from_utf8(pstr);
    text_w = MAX(text_w, text[i]->w);

    /* minimal label */
    buf = create_iconlabel(NULL, pwindow->dst,
                            create_utf8_from_char("999", adj_font(10)),
                            (WF_FREE_STRING | WF_RESTORE_BACKGROUND));

    add_to_gui_list(ID_LABEL, buf);

    /* minimal scrollbar */
    buf = create_horizontal(current_theme->Horiz, pwindow->dst, adj_size(30),
                             (WF_RESTORE_BACKGROUND));

    buf->action = min_horiz_cma_callback;
    buf->data.ptr = &pCma->edited_cm_parm.minimal_surplus[i];

    set_wstate(buf, FC_WS_NORMAL);

    add_to_gui_list(ID_SCROLLBAR, buf);

    /* factor label */
    buf = create_iconlabel(NULL, pwindow->dst,
                            create_utf8_from_char("999", adj_font(10)),
                            (WF_FREE_STRING | WF_RESTORE_BACKGROUND));

    add_to_gui_list(ID_LABEL, buf);

    /* factor scrollbar */
    buf = create_horizontal(current_theme->Horiz, pwindow->dst, adj_size(30),
                             (WF_RESTORE_BACKGROUND));

    buf->action = factor_horiz_cma_callback;
    buf->data.ptr = &pCma->edited_cm_parm.factor[i];

    set_wstate(buf, FC_WS_NORMAL);

    add_to_gui_list(ID_SCROLLBAR, buf);
  } output_type_iterate_end;

  copy_chars_to_utf8_str(pstr, _("Celebrate"));
  text[O_LAST] = create_text_surf_from_utf8(pstr);
  FREEUTF8STR(pstr);

  /* happy factor label */
  buf = create_iconlabel(NULL, pwindow->dst,
                          create_utf8_from_char("999", adj_font(10)),
                          (WF_FREE_STRING | WF_RESTORE_BACKGROUND));

  add_to_gui_list(ID_LABEL, buf);

  /* happy factor scrollbar */
  buf = create_horizontal(current_theme->Horiz, pwindow->dst, adj_size(30),
                           (WF_RESTORE_BACKGROUND));

  buf->action = factor_horiz_cma_callback;
  buf->data.ptr = &pCma->edited_cm_parm.happy_factor;

  set_wstate(buf, FC_WS_NORMAL);

  add_to_gui_list(ID_SCROLLBAR, buf);

  /* celebrating */
  buf = create_checkbox(pwindow->dst,
                         pCma->edited_cm_parm.require_happy, WF_RESTORE_BACKGROUND);

  set_wstate(buf, FC_WS_NORMAL);
  buf->action = toggle_cma_celebrating_callback;
  add_to_gui_list(ID_CHECKBOX, buf);

  /* save as ... */
  buf = create_themeicon(current_theme->SAVE_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND |WF_WIDGET_HAS_INFO_LABEL);
  buf->action = save_cma_callback;
  buf->info_label = create_utf8_from_char(_("Save settings as..."),
                                           adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* load settings */
  buf = create_themeicon(current_theme->LOAD_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL);
  buf->action = load_cma_callback;
  buf->info_label = create_utf8_from_char(_("Load settings"),
                                           adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* del settings */
  buf = create_themeicon(current_theme->DELETE_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL);
  buf->action = del_cma_callback;
  buf->info_label = create_utf8_from_char(_("Delete settings"),
                                           adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* run cma */
  buf = create_themeicon(current_theme->QPROD_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL);
  buf->action = run_cma_callback;
  buf->info_label = create_utf8_from_char(_("Control city"), adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* run cma onece */
  buf = create_themeicon(current_theme->FindCity_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL);
  buf->action = run_cma_once_callback;
  buf->info_label = create_utf8_from_char(_("Apply once"), adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* del settings */
  buf = create_themeicon(current_theme->Support_Icon, pwindow->dst,
                          WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL);
  buf->action = stop_cma_callback;
  buf->info_label = create_utf8_from_char(_("Release city"), adj_font(10));

  add_to_gui_list(ID_ICON, buf);

  /* -------------------------------- */
  pCma->dlg->begin_widget_list = buf;

#ifdef SMALL_SCREEN
  area.w = MAX(pcity_map->w + adj_size(220) + text_w + adj_size(10) +
               (pwindow->prev->prev->size.w + adj_size(5 + 70 + 5) +
                pwindow->prev->prev->size.w + adj_size(5 + 55 + 15)), w);
  area.h = adj_size(390) - (pwindow->size.w - pwindow->area.w);
#else  /* SMALL_SCREEN */
  area.w = MAX(pcity_map->w + adj_size(150) + text_w + adj_size(10) +
               (pwindow->prev->prev->size.w + adj_size(5 + 70 + 5) +
                pwindow->prev->prev->size.w + adj_size(5 + 55 + 15)), area.w);
  area.h = adj_size(360) - (pwindow->size.w - pwindow->area.w);
#endif /* SMALL_SCREEN */

  logo = theme_get_background(theme, BACKGROUND_CITYGOVDLG);
  if (resize_window(pwindow, logo, NULL,
                    (pwindow->size.w - pwindow->area.w) + area.w,
                    (pwindow->size.w - pwindow->area.w) + area.h)) {
    FREESURFACE(logo);
  }

#if 0
  logo = SDL_DisplayFormat(pwindow->theme);
  FREESURFACE(pwindow->theme);
  pwindow->theme = logo;
#endif

  area = pwindow->area;

  widget_set_position(pwindow,
                      (main_window_width() - pwindow->size.w) / 2,
                      (main_window_height() - pwindow->size.h) / 2);

  /* exit button */
  buf = pwindow->prev;
  buf->size.x = area.x + area.w - buf->size.w - 1;
  buf->size.y = pwindow->size.y + adj_size(2);

  /* ---------- */
  dst.x = pcity_map->w + adj_size(80) +
    (pwindow->size.w - (pcity_map->w + adj_size(40)) -
     (text_w + adj_size(10) + pwindow->prev->prev->size.w + adj_size(5 + 70 + 5) +
      pwindow->prev->prev->size.w + adj_size(5 + 55))) / 2;

#ifdef SMALL_SCREEN
  dst.x += 22;
#endif

  dst.y =  adj_size(75);

  x = area.x = dst.x - adj_size(10);
  area.y = dst.y - adj_size(20);
  w = area.w = adj_size(10) + text_w + adj_size(10) + pwindow->prev->prev->size.w + adj_size(5 + 70 + 5)
    + pwindow->prev->prev->size.w + adj_size(5 + 55 + 10);
  area.h = (O_LAST + 1) * (text[0]->h + adj_size(6)) + adj_size(20);
  fill_rect_alpha(pwindow->theme, &area, &bg_color);

  create_frame(pwindow->theme,
               area.x, area.y, area.w - 1, area.h - 1,
               get_theme_color(COLOR_THEME_CMA_FRAME));

  area.x = dst.x + text_w + adj_size(10);
  alphablit(pMinimal, NULL, pwindow->theme, &area, 255);
  area.x += pMinimal->w + adj_size(10);
  FREESURFACE(pMinimal);

  alphablit(pFactor, NULL, pwindow->theme, &area, 255);
  FREESURFACE(pFactor);

  area.x = pwindow->area.x + adj_size(22);
  area.y = pwindow->area.y + adj_size(31);
  alphablit(pcity_map, NULL, pwindow->theme, &area, 255);
  FREESURFACE(pcity_map);

  output_type_iterate(i) {
    /* min label */
    buf = buf->prev;
    buf->size.x = pwindow->size.x + dst.x + text_w + adj_size(10);
    buf->size.y = pwindow->size.y + dst.y + (text[i]->h - buf->size.h) / 2;

    /* min sb */
    buf = buf->prev;
    buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(5);
    buf->size.y = pwindow->size.y + dst.y + (text[i]->h - buf->size.h) / 2;

    area.x = buf->size.x - pwindow->size.x - adj_size(2);
    area.y = buf->size.y - pwindow->size.y;
    area.w = adj_size(74);
    area.h = buf->size.h;
    fill_rect_alpha(pwindow->theme, &area, &bg_color);

    create_frame(pwindow->theme,
                 area.x, area.y, area.w - 1, area.h - 1,
                 get_theme_color(COLOR_THEME_CMA_FRAME));

    /* factor label */
    buf = buf->prev;
    buf->size.x = buf->next->size.x + adj_size(75);
    buf->size.y = pwindow->size.y + dst.y + (text[i]->h - buf->size.h) / 2;

    /* factor sb */
    buf = buf->prev;
    buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(5);
    buf->size.y = pwindow->size.y + dst.y + (text[i]->h - buf->size.h) / 2;

    area.x = buf->size.x - pwindow->size.x - adj_size(2);
    area.y = buf->size.y - pwindow->size.y;
    area.w = adj_size(58);
    area.h = buf->size.h;
    fill_rect_alpha(pwindow->theme, &area, &bg_color);

    create_frame(pwindow->theme,
                 area.x, area.y, area.w - 1, area.h - 1,
                 get_theme_color(COLOR_THEME_CMA_FRAME));

    alphablit(text[i], NULL, pwindow->theme, &dst, 255);
    dst.y += text[i]->h + adj_size(6);
    FREESURFACE(text[i]);
  } output_type_iterate_end;

  /* happy factor label */
  buf = buf->prev;
  buf->size.x = buf->next->next->size.x;
  buf->size.y = pwindow->size.y + dst.y + (text[O_LAST]->h - buf->size.h) / 2;

  /* happy factor sb */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(5);
  buf->size.y = pwindow->size.y + dst.y + (text[O_LAST]->h - buf->size.h) / 2;

  area.x = buf->size.x - pwindow->size.x - adj_size(2);
  area.y = buf->size.y - pwindow->size.y;
  area.w = adj_size(58);
  area.h = buf->size.h;
  fill_rect_alpha(pwindow->theme, &area, &bg_color);

  create_frame(pwindow->theme,
               area.x, area.y, area.w - 1, area.h - 1,
               get_theme_color(COLOR_THEME_CMA_FRAME));

  /* celebrate cbox */
  buf = buf->prev;
  buf->size.x = pwindow->size.x + dst.x + adj_size(10);
  buf->size.y = pwindow->size.y + dst.y;

  /* celebrate static text */
  dst.x += (adj_size(10) + buf->size.w + adj_size(5));
  dst.y += (buf->size.h - text[O_LAST]->h) / 2;
  alphablit(text[O_LAST], NULL, pwindow->theme, &dst, 255);
  FREESURFACE(text[O_LAST]);
  /* ------------------------ */

  /* save as */
  buf = buf->prev;
  buf->size.x = pwindow->size.x + x + (w - (buf->size.w + adj_size(6)) * 6) / 2;
  buf->size.y = pwindow->size.y + pwindow->size.h - buf->size.h * 2 - adj_size(10);

  area.x = x;
  area.y = buf->size.y - pwindow->size.y - adj_size(5);
  area.w = w;
  area.h = buf->size.h + adj_size(10);
  fill_rect_alpha(pwindow->theme, &area, &bg_color);

  create_frame(pwindow->theme,
               area.x, area.y, area.w - 1, area.h - 1,
               get_theme_color(COLOR_THEME_CMA_FRAME));

  /* load */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(4);
  buf->size.y = buf->next->size.y;

  /* del */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(4);
  buf->size.y = buf->next->size.y;

  /* run */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(4);
  buf->size.y = buf->next->size.y;

  /* run one time */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(4);
  buf->size.y = buf->next->size.y;

  /* del */
  buf = buf->prev;
  buf->size.x = buf->next->size.x + buf->next->size.w + adj_size(4);
  buf->size.y = buf->next->size.y;

  /* ------------------------ */
  /* check if Citizen Icons style was loaded */
  cs = style_of_city(pCma->pcity);

  if (cs != pIcons->style) {
    reload_citizens_icons(cs);
  }

  set_cma_hscrollbars();
  update_city_cma_dialog();
}

/**********************************************************************//**
  Close cma dialog
**************************************************************************/
void popdown_city_cma_dialog(void)
{
  if (pCma) {
    popdown_window_group_dialog(pCma->dlg->begin_widget_list,
                                pCma->dlg->end_widget_list);
    FC_FREE(pCma->dlg);
    if (pCma->pAdv) {
      del_group_of_widgets_from_gui_list(pCma->pAdv->begin_widget_list,
                                         pCma->pAdv->end_widget_list);
      FC_FREE(pCma->pAdv->scroll);
      FC_FREE(pCma->pAdv);
    }
    if (city_dialog_is_open(pCma->pcity)) {
      /* enable city dlg */
      enable_city_dlg_widgets();
      refresh_city_dialog(pCma->pcity);
    }

    city_report_dialog_update();
    FC_FREE(pCma);
  }
}
