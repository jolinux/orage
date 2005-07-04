/* xfcalendar
 *
 * Copyright (C) 2003-2005 Mickael Graf (korbinus@xfce.org)
 * Parts of the code below are copyright (C) 2003 Benedikt Meurer <benny@xfce.org>
 *                                       (C) 2005 Juha Kautto <kautto.juha at kolumbus.fi>
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.  You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/netk-trayicon.h>
#include <libxfce4mcs/mcs-client.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "mainbox.h"
#include "event-list.h"
#include "tray_icon.h"
#include "xfce_trayicon.h"
#include "reminder.h"
#include "appointment.h"
#include "ical-code.h"

#define CHANNEL  "xfcalendar"
#define RCDIR    "xfce4" G_DIR_SEPARATOR_S "xfcalendar"

/* session client handler */
static SessionClient	*session_client = NULL;

/* main window */
CalWin *xfcal;

/* MCS client */
McsClient *client = NULL;

/* tray icon */
XfceTrayIcon *trayIcon = NULL;

/* main calendar window position */
gint pos_x = 0, pos_y = 0; 
/* latest event-list window size */
gint event_win_size_x = 400, event_win_size_y = 200;

/* List of active alarms */
GList *alarm_list=NULL;

static void
raise_window()
{
  GdkScreen *screen;

    screen = xfce_gdk_display_locate_monitor_with_pointer(NULL, NULL);
    gtk_window_set_screen(GTK_WINDOW(xfcal->mWindow)
                , screen ? screen : gdk_screen_get_default ());
    if (pos_x || pos_y)
        gtk_window_move(GTK_WINDOW(xfcal->mWindow), pos_x, pos_y);
    gtk_window_stick(GTK_WINDOW(xfcal->mWindow));
    gtk_widget_show(xfcal->mWindow);
}

void apply_settings()
{
  gchar *fpath;
  FILE *fp;
                                                                                
  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
                                                                                
  /* Save settings here */
  /* I know, it's bad(tm) */
  fpath = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                RCDIR G_DIR_SEPARATOR_S "xfcalendarrc", FALSE);
  if ((fp = fopen(fpath, "w")) == NULL){
    g_warning("Unable to open RC file.");
  }else {
    fprintf(fp, "[Main Window Position]\n");
    gtk_window_get_position(GTK_WINDOW(xfcal->mWindow), &pos_x, &pos_y);
    fprintf(fp, "X=%i, Y=%i\n", pos_x, pos_y);
    fprintf(fp, "[Event Window Size]\n");
    fprintf(fp, "X=%i, Y=%i\n", event_win_size_x, event_win_size_y);
    fclose(fp);
  }
  g_free(fpath);
}

static gboolean
client_message_received(GtkWidget * widget, GdkEventClient * event,
    gpointer user_data)
{
    TRACE("client message received");

    if (event->message_type ==
        gdk_atom_intern("_XFCE_CALENDAR_RAISE", FALSE)) {
        DBG("RAISING...\n");
        raise_window();
        return TRUE;
    }
    else if (event->message_type ==
        gdk_atom_intern ("_XFCE_CALENDAR_TOGGLE_HERE", FALSE)) {
        DBG("TOGGLE\n");
        if (GTK_WIDGET_VISIBLE(xfcal->mWindow)) {
            apply_settings();
            gtk_window_get_position(GTK_WINDOW(xfcal->mWindow), &pos_x, &pos_y);
            gtk_widget_hide(xfcal->mWindow);
            return TRUE;
        }
        else {
            raise_window();
            return TRUE;
        }
    }

    return FALSE;
}

int keep_tidy(void){
    /* we could move old appointment to other file to keep the active
       calendar file smaller and faster */
    return TRUE;
}

void 
notify_cb(const char *name, const char *channel_name
        , McsAction action, McsSetting * setting, void *data)
{
    gboolean normalmode
           , showtaskbar, showpager, showsystray
           , showstart, hidestart, ministart
           ;

    if (g_ascii_strcasecmp(CHANNEL, channel_name)) {
        g_message(_("This should not happen"));
        return;
    }

    switch (action) {
        case MCS_ACTION_NEW:
            if (!strcmp(name, "XFCalendar/ShowStart")) {
                showstart = (setting->data.v_int == 1) ? TRUE : FALSE;
                hidestart = (setting->data.v_int == 0) ? TRUE : FALSE;
                ministart = (setting->data.v_int == 2) ? TRUE : FALSE;
                if (showstart)
                    gtk_widget_show_all(xfcal->mWindow);
                else if (ministart) {
                    gtk_window_iconify(GTK_WINDOW(xfcal->mWindow));
                    gtk_widget_show_all(xfcal->mWindow);
                }
            }
         /* note that break is missing, we want to do also CHANGED actions */
        case MCS_ACTION_CHANGED:
            if (setting->type == MCS_TYPE_INT) {
                if (!strcmp(name, "XFCalendar/NormalMode")) {
                    normalmode = setting->data.v_int ? TRUE : FALSE;
                    gtk_window_set_decorated(GTK_WINDOW(xfcal->mWindow)
                            , normalmode);
                    if (!normalmode)
                        gtk_widget_hide(xfcal->mMenubar);
                    else
                        gtk_widget_show(xfcal->mMenubar);
                }
                else if (!strcmp(name, "XFCalendar/TaskBar")) {
                    showtaskbar = setting->data.v_int ? TRUE : FALSE;
           /* Reminder: if we want to show the calendar in the taskbar
            * (i.e. showtaskbar is TRUE) then gtk_window_set_skip_taskbar_hint
            * must get a FALSE value, and if we don't want to be seen in
            * the taskbar, then the function must eat a TRUE.
            */
            gtk_window_set_skip_taskbar_hint(GTK_WINDOW(xfcal->mWindow)
                            , !showtaskbar);
                }
                else if(!strcmp(name, "XFCalendar/Pager")) {
                    showpager = setting->data.v_int ? TRUE : FALSE;
           /* Reminder: if we want to show the calendar in the pager
            * (i.e. showpager is TRUE) then gtk_window_set_skip_pager_hint
            * must get a FALSE value, and if we don't want to be seen in
            * the pager, then the function must eat a TRUE.
            */
                    gtk_window_set_skip_pager_hint(GTK_WINDOW(xfcal->mWindow)
                            , !showpager);
                }
                else if(!strcmp(name, "XFCalendar/Systray")) {
                    showsystray = setting->data.v_int ? TRUE : FALSE;
                    if (showsystray)
                        xfce_tray_icon_connect(trayIcon);
                    else
                        xfce_tray_icon_disconnect(trayIcon);
                }
            }
            break;
        case MCS_ACTION_DELETED:
        default:
            break;
    }
}

GdkFilterReturn
client_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
    if(mcs_client_process_event(client, (XEvent *) xevent))
        return GDK_FILTER_REMOVE;
    else
        return GDK_FILTER_CONTINUE;
}

void
watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin;

    gdkwin = gdk_window_lookup(window);

    if(is_start)
    {
        if(!gdkwin)
        {
            gdkwin = gdk_window_foreign_new(window);
        }
        else
        {
            g_object_ref(gdkwin);
        }
        gdk_window_add_filter(gdkwin, client_event_filter, cb_data);
    }
    else
    {
        g_assert(gdkwin);
        gdk_window_remove_filter(gdkwin, client_event_filter, cb_data);
        g_object_unref(gdkwin);
    }
}

/*
 * SaveYourself callback
 *
 * This is called when the session manager requests the client to save its
 * state.
 */
/* ARGUSED */
void
save_yourself_cb(gpointer data, int save_style, gboolean shutdown,
                 int interact_style, gboolean fast)
{
  apply_settings();
}

/*
 * Die callback
 *
 * This is called when the session manager requests the client to go down.
 */

void
die_cb(gpointer data)
{
  gtk_main_quit();
}

static void
ensure_basedir_spec (void)
{
  char *newdir, *olddir;
  GError *error = NULL;
  GDir *gdir;

  newdir = xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
                                        RCDIR, FALSE);

  /* if new directory exist, assume old config has been copied */
  if (g_file_test (newdir, G_FILE_TEST_IS_DIR)) {
    g_free (newdir);
    return;
  }

  if (!xfce_mkdirhier (newdir, 0700, &error)) {
    g_critical("Cannot create directory %s: %s", newdir, error->message);
    g_error_free (error);
    g_free (newdir);
    exit (EXIT_FAILURE);
  }

  olddir = xfce_get_userfile ("xfcalendar", NULL);

  if ((gdir = g_dir_open (olddir, 0, NULL)) != NULL) {
    const char *name;
    
    while ((name = g_dir_read_name (gdir)) != NULL) {
      FILE *r, *w;
      char *path;

      path = g_build_filename (olddir, name, NULL);
      r = fopen (path, "r");
      g_free (path);

      path = g_build_filename (newdir, name, NULL);
      w = fopen (path, "w");
      g_free (path);

      if (r && w) {
        char c;

        while ((c = getc(r)) != EOF)
          putc (c, w);
      }

      if (r)
        fclose (r);
      if (w)
        fclose (w);
    }
  }

  g_free (newdir);
  g_free (olddir);
}

int
main(int argc, char *argv[])
{
  GtkWidget *hidden;
  Window xwindow;
  GdkAtom atom;
  Display *dpy;
  int scr;

  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  gtk_init(&argc, &argv);

  dpy = GDK_DISPLAY();
  scr = DefaultScreen(dpy);

  xfcal = g_new(CalWin, 1);
/* Build the main window */
  xfcal->mWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hidden = gtk_invisible_new();
  gtk_widget_show(hidden);
  g_signal_connect (hidden, "client-event",
        G_CALLBACK (client_message_received), NULL);

  atom = gdk_atom_intern("_XFCE_CALENDAR_RUNNING", FALSE);

  /*
   * Check if xfcalendar is already running on the display
   */
  if ((xwindow = XGetSelectionOwner(GDK_DISPLAY(),
        gdk_x11_atom_to_xatom(atom))) != None) {
    XClientMessageEvent xev;

    memset(&xev, 0, sizeof(xev));

    xev.type = ClientMessage;
    xev.window = xwindow;
    xev.message_type = XInternAtom(GDK_DISPLAY(), "_XFCE_CALENDAR_RAISE", FALSE);
    xev.format = 32;

    XSendEvent(GDK_DISPLAY(), xwindow, False, NoEventMask,
	       (XEvent *)&xev);
    XSync(GDK_DISPLAY(), False);

    return(EXIT_SUCCESS);
  }
  if (!gdk_selection_owner_set(hidden->window, atom,
       gdk_x11_get_server_time(hidden->window),
       FALSE)) {
    g_warning("Unable acquire ownership of selection");
  }

  /* 
   * try to connect to the session manager
   */
  session_client = client_session_new(argc, argv, NULL,
      SESSION_RESTART_IF_RUNNING, 50);
  session_client->save_yourself = save_yourself_cb;
  session_client->die = die_cb;
  (void)session_init(session_client);

  /*
   * Now it's serious, the application is running, so we create the RC
   * directory and check for config files in old location.
   */
  ensure_basedir_spec();

  /*
   * Create the Xfcalendar.
   */
  create_mainWin();

  /*
   * Create the tray icon and its popup menu
   */
  trayIcon = create_TrayIcon(xfcal);

  client = mcs_client_new(dpy, scr, notify_cb, watch_cb, xfcal->mWindow);
  if(client) {
      mcs_client_add_channel(client, CHANNEL);
  }
  else {
      g_warning(_("Cannot create MCS client channel"));
  }

/* start alarm monitoring timeout */
  g_timeout_add_full(0,
             5000,
             (GtkFunction) xfcalendar_alarm_clock,
             (gpointer) xfcal,
             NULL);
                                                                                
  gtk_main();

  xfical_keep_tidy();

  return(EXIT_SUCCESS);
}

