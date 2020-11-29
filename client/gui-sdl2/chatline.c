/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

/***********************************************************************
                          chatline.c  -  description
                             -------------------
    begin                : Sun Jun 30 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
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
#include "log.h"

/* common */
#include "game.h"
#include "packets.h"

/* client */
#include "client_main.h"
#include "clinet.h"
#include "connectdlg_common.h"

/* gui-sdl2 */
#include "colors.h"
#include "dialogs.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "messagewin.h"
#include "pages.h"
#include "themespec.h"
#include "unistring.h"
#include "utf8string.h"
#include "widget.h"

#include "chatline.h"

#define PTSIZE_LOG_FONT adj_font(10)

struct CONNLIST {
  struct advanced_dialog *users_dlg;
  struct advanced_dialog *chat_dlg;
  struct widget *begin_widget_list;
  struct widget *end_widget_list;
  struct widget *start_button;
  struct widget *pSelectNationButton;
  struct widget *pLoadGameButton;
  struct widget *pConfigure;
  struct widget *pBackButton;
  struct widget *pedit;
  int text_width;
  int active;
} *conn_dlg = NULL;

static void popup_conn_list_dialog(void);
static void add_to_chat_list(char *msg, size_t n_alloc);

/**************************************************************************
                                  LOAD GAME
**************************************************************************/

struct advanced_dialog *pLoadDialog;

/**********************************************************************//**
  User event to load game dialog window.
**************************************************************************/
static int move_load_game_dlg_callback(struct widget *pwindow)
{
  if (PRESSED_EVENT(main_data.event)) {
    move_window_group(pLoadDialog->begin_widget_list, pwindow);
  }

  return -1;
}

/**********************************************************************//**
  Close load game dialog
**************************************************************************/
void popdown_load_game_dialog(void)
{
  if (pLoadDialog) {
    popdown_window_group_dialog(pLoadDialog->begin_widget_list, pLoadDialog->end_widget_list);
    FC_FREE(pLoadDialog->scroll);
    FC_FREE(pLoadDialog);

    /* enable buttons */
    set_wstate(conn_dlg->pBackButton, FC_WS_NORMAL);
    widget_redraw(conn_dlg->pBackButton);
    widget_mark_dirty(conn_dlg->pBackButton);
    set_wstate(conn_dlg->pLoadGameButton, FC_WS_NORMAL);
    widget_redraw(conn_dlg->pLoadGameButton);
    widget_mark_dirty(conn_dlg->pLoadGameButton);
    set_wstate(conn_dlg->start_button, FC_WS_NORMAL);
    widget_redraw(conn_dlg->start_button);
    widget_mark_dirty(conn_dlg->start_button);

    flush_dirty();
  }
}

/**********************************************************************//**
  User clicked load game dialog close-button.
**************************************************************************/
static int exit_load_dlg_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    if (get_client_page() == PAGE_LOAD) {
      set_client_page(PAGE_START);
    } else {
      popdown_load_game_dialog();
    }
  }

  return -1;
}

/**********************************************************************//**
  User selected file to load.
**************************************************************************/
static int load_selected_game_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    char *filename = (char*)pwidget->data.ptr;

    if (is_server_running()) {
      send_chat_printf("/load %s", filename);

      if (get_client_page() == PAGE_LOAD) {
        set_client_page(PAGE_START);
      } else if (get_client_page() == PAGE_START) {
        popdown_load_game_dialog();
      }
    } else {
      set_client_page(PAGE_MAIN);
    }
  }

  return -1;
}

/**********************************************************************//**
  Open load game dialog
**************************************************************************/
static void popup_load_game_dialog(void)
{
  struct widget *pwindow;
  struct widget *pCloseButton;
  struct widget *pFilenameLabel = NULL;
  struct widget *pFirstLabel = NULL;
  struct widget *pLastLabel = NULL;
  struct widget *pNextLabel = NULL;
  utf8_str *title, *filename;
  SDL_Rect area;
  struct fileinfo_list *files;
  int count = 0;
  int scrollbar_width = 0;
  int max_label_width = 0;

  if (pLoadDialog) {
    return;
  }

  /* disable buttons */
  set_wstate(conn_dlg->pBackButton, FC_WS_DISABLED);
  widget_redraw(conn_dlg->pBackButton);
  widget_mark_dirty(conn_dlg->pBackButton);
  set_wstate(conn_dlg->pLoadGameButton, FC_WS_DISABLED);
  widget_redraw(conn_dlg->pLoadGameButton);
  widget_mark_dirty(conn_dlg->pLoadGameButton);
  set_wstate(conn_dlg->pSelectNationButton, FC_WS_DISABLED);
  widget_redraw(conn_dlg->pSelectNationButton);
  widget_mark_dirty(conn_dlg->pSelectNationButton);
  set_wstate(conn_dlg->start_button, FC_WS_DISABLED);
  widget_redraw(conn_dlg->start_button);
  widget_mark_dirty(conn_dlg->start_button);

  /* create dialog */
  pLoadDialog = fc_calloc(1, sizeof(struct advanced_dialog));

  title = create_utf8_from_char(_("Choose Saved Game to Load"), adj_font(12));
  title->style |= TTF_STYLE_BOLD;

  pwindow = create_window_skeleton(NULL, title, 0);
  pwindow->action = move_load_game_dlg_callback; 
  set_wstate(pwindow, FC_WS_NORMAL);

  add_to_gui_list(ID_WINDOW, pwindow);

  pLoadDialog->end_widget_list = pwindow;

  area = pwindow->area;

  /* close button */
  pCloseButton = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                                  WF_WIDGET_HAS_INFO_LABEL
                                  | WF_RESTORE_BACKGROUND);
  pCloseButton->info_label = create_utf8_from_char(_("Close Dialog (Esc)"),
                                                   adj_font(12));
  pCloseButton->action = exit_load_dlg_callback;
  set_wstate(pCloseButton, FC_WS_NORMAL);
  pCloseButton->key = SDLK_ESCAPE;

  add_to_gui_list(ID_BUTTON, pCloseButton);

  area.w += pCloseButton->size.w;

  pLoadDialog->begin_widget_list = pCloseButton;

  /* create scrollbar */
  scrollbar_width = create_vertical_scrollbar(pLoadDialog, 1, 20, TRUE, TRUE);
  hide_scrollbar(pLoadDialog->scroll);

  /* search for user saved games. */
  files = fileinfolist_infix(get_save_dirs(), ".sav", FALSE);
  fileinfo_list_iterate(files, pfile) {
    count++;

    filename = create_utf8_from_char(pfile->name, adj_font(13));
    filename->style |= SF_CENTER;
    pFilenameLabel = create_iconlabel(NULL, pwindow->dst, filename,
      (WF_FREE_DATA | WF_SELECT_WITHOUT_BAR | WF_RESTORE_BACKGROUND));

    /* store filename */
    pFilenameLabel->data.ptr = fc_calloc(1, strlen(pfile->fullname) + 1);
    fc_strlcpy((char*)pFilenameLabel->data.ptr, pfile->fullname, strlen(pfile->fullname) + 1);

    pFilenameLabel->action = load_selected_game_callback;

    set_wstate(pFilenameLabel, FC_WS_NORMAL);

    /* FIXME: this was supposed to be add_widget_to_vertical_scroll_widget_list(), but
     * add_widget_to_vertical_scroll_widget_list() needs the scrollbar area to be defined
     * for updating the scrollbar position, but the area is not known yet (depends on
     * maximum label width) */
    add_to_gui_list(ID_LABEL, pFilenameLabel);

    if (count == 1) {
      pFirstLabel = pFilenameLabel;
    }

    max_label_width = MAX(max_label_width, pFilenameLabel->size.w);
  } fileinfo_list_iterate_end;
  fileinfo_list_destroy(files);

  pLastLabel = pFilenameLabel;

  area.w = MAX(area.w, max_label_width + scrollbar_width + 1);

  if (count > 0) {
    area.h = (pLoadDialog->scroll->active * pFilenameLabel->size.h) + adj_size(5);
  }

  resize_window(pwindow, theme_get_background(theme, BACKGROUND_LOADGAMEDLG),
                NULL,
                (pwindow->size.w - pwindow->area.w) + area.w,
                (pwindow->size.h - pwindow->area.h) + area.h);

  area = pwindow->area;

  setup_vertical_scrollbar_area(pLoadDialog->scroll,
                                area.x + area.w - 1,
                                area.y + 1,
                                area.h - adj_size(2), TRUE);

  /* add filename labels to list */
  pFilenameLabel = pFirstLabel;
  while (pFilenameLabel) {
    pFilenameLabel->size.w = area.w - scrollbar_width - 3;

    pNextLabel = pFilenameLabel->prev;

    del_widget_pointer_from_gui_list(pFilenameLabel);
    if (pFilenameLabel == pFirstLabel) {
      add_widget_to_vertical_scroll_widget_list(pLoadDialog,
          pFilenameLabel, pCloseButton,
          FALSE,
          area.x + 1,
          area.y + adj_size(2));
    } else {
      add_widget_to_vertical_scroll_widget_list(pLoadDialog,
          pFilenameLabel,
          pLoadDialog->begin_active_widget_list,
          FALSE,
          area.x + 1,
          area.y + adj_size(2));
    }

    if (pFilenameLabel == pLastLabel) {
      break;
    }

    pFilenameLabel = pNextLabel;
  }

  widget_set_position(pwindow,
                      (main_window_width() - pwindow->size.w) / 2,
                      (main_window_height() - pwindow->size.h) / 2);

  widget_set_position(pCloseButton,
                      area.x + area.w - pCloseButton->size.w - 1,
                      pwindow->size.y + adj_size(2));

  /* FIXME: the scrollbar already got a background saved in
   * add_widget_to_vertical_scroll_widget_list(), but the window
   * is not drawn yet, so this saved background is wrong.
   * Deleting it here as a workaround. */
  FREESURFACE(pLoadDialog->scroll->pscroll_bar->gfx);

  redraw_group(pLoadDialog->begin_widget_list, pwindow, 1);
  flush_dirty();
}

/**********************************************************************//**
  Sent msg/command from input dlg to server
**************************************************************************/
static int inputline_return_callback(struct widget *pwidget)
{
  if (main_data.event.type == SDL_KEYDOWN
      && (main_data.event.key.keysym.sym == SDLK_RETURN
          || main_data.event.key.keysym.sym == SDLK_KP_ENTER)) {

    if (pwidget->string_utf8->text == NULL) {
      return -1;
    }

    if (pwidget->string_utf8->text[0] != '\0') {
      send_chat(pwidget->string_utf8->text);

      output_window_append(ftc_any, pwidget->string_utf8->text);
    }
  }

  return -1;
}

/**********************************************************************//**
  This function is main chat/command client input.
**************************************************************************/
void popup_input_line(void)
{
  struct widget *input_edit;

  input_edit = create_edit_from_chars(NULL, main_data.gui, "", adj_font(12),
                                       adj_size(400), 0);

  input_edit->size.x = (main_window_width() - input_edit->size.w) / 2;
  input_edit->size.y = (main_window_height() - input_edit->size.h) / 2;

  if (edit(input_edit) != ED_ESC) {
    inputline_return_callback(input_edit);
  }

  widget_undraw(input_edit);
  widget_mark_dirty(input_edit);
  FREEWIDGET(input_edit);

  flush_dirty();
}

/**********************************************************************//**
  Appends the string to the chat output window.  The string should be
  inserted on its own line, although it will have no newline.
**************************************************************************/
void real_output_window_append(const char *astring,
                               const struct text_tag_list *tags,
                               int conn_id)
{
  /* Currently this is a wrapper to the message subsystem. */
  if (conn_dlg) {
    size_t n = strlen(astring);
    char *buffer = fc_strdup(astring);

    add_to_chat_list(buffer, n);
  } else {
    meswin_add(astring, tags, NULL, E_CHAT_MSG,
               game.info.turn, game.info.phase);
  }
}

/**********************************************************************//**
  Get the text of the output window, and call write_chatline_content() to
  log it.
**************************************************************************/
void log_output_window(void)
{
  /* TODO */
}

/**********************************************************************//**
  Clear all text from the output window.
**************************************************************************/
void clear_output_window(void)
{
  /* TODO */
}

/* ====================================================================== */

/**********************************************************************//**
  User did something to connection list dialog.
**************************************************************************/
static int conn_dlg_callback(struct widget *pwindow)
{
  return -1;
}

/**********************************************************************//**
  User selected to get back from connection list dialog.
**************************************************************************/
static int disconnect_conn_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popdown_conn_list_dialog();
    flush_dirty();
    disconnect_from_server();
  }

  return -1;
}

/**********************************************************************//**
  Handle chat messages when connection dialog open.
**************************************************************************/
static void add_to_chat_list(char *msg, size_t n_alloc)
{
  utf8_str *pstr;
  struct widget *buf, *pwindow = conn_dlg->end_widget_list;

  fc_assert_ret(msg != NULL);
  fc_assert_ret(n_alloc != 0);

  pstr = create_utf8_str(msg, n_alloc, adj_font(12));

  if (convert_utf8_str_to_const_surface_width(pstr, conn_dlg->text_width - adj_size(5))) {
    utf8_str *pstr2;
    int count = 0;
    char **utf8_texts = create_new_line_utf8strs(pstr->text);

    while (utf8_texts[count] != NULL) {
      pstr2 = create_utf8_str(utf8_texts[count],
                              strlen(utf8_texts[count]) + 1, adj_font(12));
      pstr2->bgcol = (SDL_Color) {0, 0, 0, 0};
      buf = create_themelabel2(NULL, pwindow->dst,
                                pstr2, conn_dlg->text_width, 0,
                                (WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));

      buf->size.w = conn_dlg->text_width;
      add_widget_to_vertical_scroll_widget_list(conn_dlg->chat_dlg, buf,
                        conn_dlg->chat_dlg->begin_active_widget_list, FALSE,
                        pwindow->size.x + adj_size(10 + 60 + 10),
                        pwindow->size.y + adj_size(14));
      count++;
    }
    redraw_group(conn_dlg->chat_dlg->begin_widget_list,
                 conn_dlg->chat_dlg->end_widget_list, TRUE);
    FREEUTF8STR(pstr);
  } else {
    pstr->bgcol = (SDL_Color) {0, 0, 0, 0};
    buf = create_themelabel2(NULL, pwindow->dst,
                              pstr, conn_dlg->text_width, 0,
                              (WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));

    buf->size.w = conn_dlg->text_width;

    if (add_widget_to_vertical_scroll_widget_list(conn_dlg->chat_dlg, buf,
                        conn_dlg->chat_dlg->begin_active_widget_list, FALSE,
                        pwindow->size.x + adj_size(10 + 60 + 10),
                        pwindow->size.y + adj_size(14))) {
      redraw_group(conn_dlg->chat_dlg->begin_widget_list,
                   conn_dlg->chat_dlg->end_widget_list, TRUE);
    } else {
      widget_redraw(buf);
      widget_mark_dirty(buf);
    }
  }

  flush_dirty();
}

/**********************************************************************//**
  User interacted with connection dialog input field.
**************************************************************************/
static int input_edit_conn_callback(struct widget *pwidget)
{
  if (main_data.event.type == SDL_KEYDOWN
      && (main_data.event.key.keysym.sym == SDLK_RETURN
          || main_data.event.key.keysym.sym == SDLK_KP_ENTER)) {
    if (pwidget->string_utf8->text != NULL) {
      if (pwidget->string_utf8->text[0] != '\0') {
        send_chat(pwidget->string_utf8->text);
      }

      free(pwidget->string_utf8->text);
      pwidget->string_utf8->text = fc_malloc(1);
      pwidget->string_utf8->n_alloc = 0;
    }
  }

  return -1;
}

/**********************************************************************//**
   User interacted with Start Game button.
**************************************************************************/
static int start_game_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    send_chat("/start");
  }

  return -1;
}

/**********************************************************************//**
  User interacted with Select Nation button.
**************************************************************************/
static int select_nation_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_races_dialog(client.conn.playing);
  }

  return -1;
}

/* not implemented yet */
#if 0
/**********************************************************************//**
  User interacted with Server Settings button.
**************************************************************************/
static int server_config_callback(struct widget *pwidget)
{
  return -1;
}
#endif

/**********************************************************************//**
  User interacted with Load Game button.
**************************************************************************/
static int load_game_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    /* set_wstate(conn_dlg->pLoadGameButton, FC_WS_NORMAL);
     * widget_redraw(conn_dlg->pLoadGameButton);        
     * flush_dirty(); */
    popup_load_game_dialog();
  }

  return -1;
}

/**********************************************************************//**
  Update the connected users list at pregame state.
**************************************************************************/
void real_conn_list_dialog_update(void *unused)
{
  if (C_S_PREPARING == client_state()) {
    if (conn_dlg) {
      struct widget *buf = NULL, *pwindow = conn_dlg->end_widget_list;
      utf8_str *pstr = create_utf8_str(NULL, 0, adj_font(12));
      bool create;

      pstr->bgcol = (SDL_Color) {0, 0, 0, 0};

      if (conn_dlg->users_dlg) {
        del_group(conn_dlg->users_dlg->begin_active_widget_list,
                  conn_dlg->users_dlg->end_active_widget_list);
        conn_dlg->users_dlg->active_widget_list = NULL;
        conn_dlg->users_dlg->begin_widget_list =
          conn_dlg->users_dlg->scroll->pscroll_bar;
        conn_dlg->users_dlg->scroll->count = 0;
      } else {
        conn_dlg->users_dlg = fc_calloc(1, sizeof(struct advanced_dialog));
        conn_dlg->users_dlg->end_widget_list = conn_dlg->begin_widget_list;
        conn_dlg->users_dlg->begin_widget_list = conn_dlg->begin_widget_list;

        create_vertical_scrollbar(conn_dlg->users_dlg, 1,
                                  conn_dlg->active, TRUE, TRUE);
        conn_dlg->users_dlg->end_widget_list =
          conn_dlg->users_dlg->end_widget_list->prev;
        setup_vertical_scrollbar_area(conn_dlg->users_dlg->scroll,
          pwindow->size.x + pwindow->size.w - adj_size(30),
          pwindow->size.y + adj_size(14), pwindow->size.h - adj_size(44) - adj_size(40), FALSE);
      }

      hide_scrollbar(conn_dlg->users_dlg->scroll);
      create = TRUE;
      conn_list_iterate(game.est_connections, pconn) {
        copy_chars_to_utf8_str(pstr, pconn->username);

        buf = create_themelabel2(NULL, pwindow->dst, pstr, adj_size(100), 0,
                (WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));
        clear_wflag(buf, WF_FREE_STRING);

        buf->ID = ID_LABEL;

        /* add to widget list */
        if (create) {
          add_widget_to_vertical_scroll_widget_list(conn_dlg->users_dlg,
            buf, conn_dlg->users_dlg->begin_widget_list, FALSE,
            pwindow->area.x + pwindow->area.w - adj_size(130),
            pwindow->size.y + adj_size(14));
          create = FALSE;
        } else {
	  add_widget_to_vertical_scroll_widget_list(conn_dlg->users_dlg,
                buf, conn_dlg->users_dlg->begin_active_widget_list, FALSE,
                pwindow->area.x + pwindow->area.w - adj_size(130),
                pwindow->size.y + adj_size(14));
        }
      } conn_list_iterate_end;

      conn_dlg->begin_widget_list = conn_dlg->users_dlg->begin_widget_list;
      FREEUTF8STR(pstr);

/* FIXME: implement the server settings dialog and then reactivate this part */
#if 0
      if (ALLOW_CTRL == client.conn.access_level
          || ALLOW_HACK == client.conn.access_level) {
        set_wstate(conn_dlg->pConfigure, FC_WS_NORMAL);
      } else {
        set_wstate(conn_dlg->pConfigure, FC_WS_DISABLED);
      }
#endif

      /* redraw */
      redraw_group(conn_dlg->begin_widget_list, conn_dlg->end_widget_list, 0);

      widget_flush(conn_dlg->end_widget_list);
    } else {
      popup_conn_list_dialog();
    }

    /* PAGE_LOAD -> the server was started from the main page to load a game */
    if (get_client_page() == PAGE_LOAD) {
      popup_load_game_dialog();
    }
  } else {
    if (popdown_conn_list_dialog()) {
      flush_dirty();
    }
  }
}

/**********************************************************************//**
  Open connection list dialog
**************************************************************************/
static void popup_conn_list_dialog(void)
{
  SDL_Color window_bg_color = {255, 255, 255, 96};

  struct widget *pwindow = NULL, *buf = NULL, *pLabel = NULL;
  struct widget* pBackButton = NULL;
  struct widget *start_game_button = NULL;
  struct widget *pSelectNationButton = NULL;
/*  struct widget *pServerSettingsButton = NULL;*/
  utf8_str *pstr = NULL;
  int n;
  SDL_Rect area;
  SDL_Surface *surf;

  if (conn_dlg || !client.conn.established) {
    return;
  }

  meswin_dialog_popdown();

  conn_dlg = fc_calloc(1, sizeof(struct CONNLIST));

  pwindow = create_window_skeleton(NULL, NULL, 0);
  pwindow->action = conn_dlg_callback;
  set_wstate(pwindow, FC_WS_NORMAL);
  clear_wflag(pwindow, WF_DRAW_FRAME_AROUND_WIDGET);

  conn_dlg->end_widget_list = pwindow;
  add_to_gui_list(ID_WINDOW, pwindow);

  widget_set_position(pwindow, 0, 0);

  /* create window background */
  surf = theme_get_background(theme, BACKGROUND_CONNLISTDLG);
  if (resize_window(pwindow, surf, NULL, main_window_width(), main_window_height())) {
    FREESURFACE(surf);
  }

  conn_dlg->text_width = pwindow->size.w - adj_size(130) - adj_size(20) - adj_size(20);

  /* chat area background */
  area.x = adj_size(10);
  area.y = adj_size(14);
  area.w = conn_dlg->text_width + adj_size(20);
  area.h = pwindow->size.h - adj_size(44) - adj_size(40);
  fill_rect_alpha(pwindow->theme, &area, &window_bg_color);

  create_frame(pwindow->theme,
               area.x - 1, area.y - 1, area.w + 1, area.h + 1,
               get_theme_color(COLOR_THEME_CONNLISTDLG_FRAME));

  /* user list background */
  area.x = pwindow->size.w - adj_size(130);
  area.y = adj_size(14);
  area.w = adj_size(120);
  area.h = pwindow->size.h - adj_size(44) - adj_size(40);
  fill_rect_alpha(pwindow->theme, &area, &window_bg_color);

  create_frame(pwindow->theme,
               area.x - 1, area.y - 1, area.w + 1, area.h + 1,
               get_theme_color(COLOR_THEME_CONNLISTDLG_FRAME));

  draw_frame(pwindow->theme, 0, 0, pwindow->theme->w, pwindow->theme->h);

  /* -------------------------------- */

  /* chat area */

  conn_dlg->chat_dlg = fc_calloc(1, sizeof(struct advanced_dialog));

  n = conn_list_size(game.est_connections);

  {
    char cbuf[256];

    fc_snprintf(cbuf, sizeof(cbuf), _("Total users logged in : %d"), n);
    pstr = create_utf8_from_char(cbuf, adj_font(12));
  }

  pstr->bgcol = (SDL_Color) {0, 0, 0, 0};

  pLabel = create_themelabel2(NULL, pwindow->dst,
                              pstr, conn_dlg->text_width, 0,
                              (WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));

  widget_set_position(pLabel, adj_size(10), adj_size(14));

  add_to_gui_list(ID_LABEL, pLabel);

  conn_dlg->chat_dlg->begin_widget_list = pLabel;
  conn_dlg->chat_dlg->end_widget_list = pLabel;
  conn_dlg->chat_dlg->begin_active_widget_list = conn_dlg->chat_dlg->begin_widget_list;
  conn_dlg->chat_dlg->end_active_widget_list = conn_dlg->chat_dlg->end_widget_list;

  n = (pwindow->size.h - adj_size(44) - adj_size(40)) / pLabel->size.h;
  conn_dlg->active = n;

  create_vertical_scrollbar(conn_dlg->chat_dlg, 1,
                            conn_dlg->active, TRUE, TRUE);

  setup_vertical_scrollbar_area(conn_dlg->chat_dlg->scroll,
                adj_size(10) + conn_dlg->text_width + 1,
                adj_size(14), pwindow->size.h - adj_size(44) - adj_size(40), FALSE);
  hide_scrollbar(conn_dlg->chat_dlg->scroll);

  /* -------------------------------- */

  /* input field */

  buf = create_edit_from_chars(NULL, pwindow->dst, "",
                                adj_font(12), pwindow->size.w - adj_size(10) - adj_size(10),
                                (WF_RESTORE_BACKGROUND|WF_EDIT_LOOP));

  buf->size.x = adj_size(10);
  buf->size.y = pwindow->size.h - adj_size(40) - adj_size(5) - buf->size.h;
  buf->action = input_edit_conn_callback;
  set_wstate(buf, FC_WS_NORMAL);
  conn_dlg->pedit = buf;
  add_to_gui_list(ID_EDIT, buf);

  /* buttons */

  buf = create_themeicon_button_from_chars(current_theme->BACK_Icon, pwindow->dst,
                                            _("Back"), adj_font(12), 0);
  buf->size.x = adj_size(10);
  buf->size.y = pwindow->size.h - adj_size(10) - buf->size.h;
  conn_dlg->pBackButton = buf;
  buf->action = disconnect_conn_callback;
  set_wstate(buf, FC_WS_NORMAL);
  buf->key = SDLK_ESCAPE;
  add_to_gui_list(ID_BUTTON, buf);
  pBackButton = buf;

  buf = create_themeicon_button_from_chars(current_theme->OK_Icon, pwindow->dst,
                                            _("Start"), adj_font(12), 0);
  buf->size.x = pwindow->size.w - adj_size(10) - buf->size.w;
  buf->size.y = pBackButton->size.y;
  conn_dlg->start_button = buf;
  buf->action = start_game_callback;
  set_wstate(buf, FC_WS_NORMAL);
  add_to_gui_list(ID_BUTTON, buf);
  start_game_button = buf;

  buf = create_themeicon_button_from_chars(NULL, pwindow->dst,
                                            _("Pick Nation"), adj_font(12), 0);
  buf->size.h = start_game_button->size.h;
  buf->size.x = start_game_button->size.x - adj_size(10) - buf->size.w;
  buf->size.y = start_game_button->size.y;
  conn_dlg->pSelectNationButton = buf;
  buf->action = select_nation_callback;
  set_wstate(buf, FC_WS_NORMAL);
  add_to_gui_list(ID_BUTTON, buf);
  pSelectNationButton = buf;

  buf = create_themeicon_button_from_chars(NULL, pwindow->dst,
                                            _("Load Game"), adj_font(12), 0);
  buf->size.h = pSelectNationButton->size.h;
  buf->size.x = pSelectNationButton->size.x - adj_size(10) - buf->size.w;
  buf->size.y = pSelectNationButton->size.y;
  conn_dlg->pLoadGameButton = buf;
  buf->action = load_game_callback;
  set_wstate(buf, FC_WS_NORMAL);
  add_to_gui_list(ID_BUTTON, buf);

  /* not implemented yet */
#if 0
  buf = create_themeicon_button_from_chars(NULL, pwindow->dst,
                                            _("Server Settings"),
                                            adj_font(12), 0);
  buf->size.h = pSelectNationButton->size.h;
  buf->size.x = pSelectNationButton->size.x - adj_size(10) - buf->size.w;
  buf->size.y = pSelectNationButton->size.y;
  conn_dlg->pConfigure = buf;
  buf->action = server_config_callback;
  set_wstate(buf, FC_WS_DISABLED);
  add_to_gui_list(ID_BUTTON, buf);
  pServerSettingsButton = buf;
#endif

  /* not implemented yet */
#if 0
  buf = create_themeicon_button_from_chars(NULL, pwindow->dst->surface,
                                            "?", adj_font(12), 0);
  buf->size.y = pwindow->size.y + pwindow->size.h - (buf->size.h + 7);
  buf->size.x = pwindow->size.x + pwindow->size.w - (buf->size.w + 10) - 5;

  buf->action = client_config_callback;
  set_wstate(buf, FC_WS_NORMAL);
  add_to_gui_list(ID_BUTTON, buf);
#endif

  conn_dlg->begin_widget_list = buf;
  /* ------------------------------------------------------------ */

  conn_list_dialog_update();
}

/**********************************************************************//**
  Close connection list dialog.
**************************************************************************/
bool popdown_conn_list_dialog(void)
{
  if (conn_dlg) {
    if (get_wstate(conn_dlg->pedit) == FC_WS_PRESSED) {
      force_exit_from_event_loop();
    }

    popdown_window_group_dialog(conn_dlg->begin_widget_list,
                                conn_dlg->end_widget_list);
    if (conn_dlg->users_dlg) {
      FC_FREE(conn_dlg->users_dlg->scroll);
      FC_FREE(conn_dlg->users_dlg);
    }

    if (conn_dlg->chat_dlg) {
      FC_FREE(conn_dlg->chat_dlg->scroll);
      FC_FREE(conn_dlg->chat_dlg);
    }

    FC_FREE(conn_dlg);

    return TRUE;
  }

  return FALSE;
}

/**********************************************************************//**
  Got version message from metaserver thread.
**************************************************************************/
void version_message(const char *vertext)
{
  output_window_append(ftc_client, vertext);
}
