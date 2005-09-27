/* tray_icon.c
 *
 * (C) 2004 Micka�l Graf
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/netk-trayicon.h>
#include <libxfce4mcs/mcs-client.h>

#include "event-list.h"
#include "appointment.h"
#include "functions.h"
#include "mainbox.h"
#include "xfce_trayicon.h"
#include "about-xfcalendar.h"

#define CHANNEL  "orage"

void
on_Today_activate                      (GtkMenuItem *menuitem,
                                        gpointer user_data)
{
    struct tm *t;
    time_t tt;
    CalWin *xfcal = (CalWin *)user_data;
    eventlist_win *el;

    tt=time(NULL);
    t=localtime(&tt);
    xfcalendar_select_date (GTK_CALENDAR (xfcal->mCalendar), t->tm_year+1900, t->tm_mon, t->tm_mday);
    el = create_eventlist_win();
    manage_eventlist_win(GTK_CALENDAR(xfcal->mCalendar), el);
}

void 
on_preferences_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  mcs_client_show (GDK_DISPLAY (), DefaultScreen (GDK_DISPLAY ()),
		   CHANNEL);
}

void
on_new_appointment_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    appt_win *app;
    struct tm *t;
    time_t tt;
    char cur_date[9];

    tt=time(NULL);
    t=localtime(&tt);
    g_snprintf(cur_date, 9, "%04d%02d%02d", t->tm_year+1900
               , t->tm_mon+1, t->tm_mday);
    app = create_appt_win("NEW", cur_date, NULL);  
}

void
on_about_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  create_wAbout((GtkWidget *)menuitem, user_data);
}

void 
toggle_visible_cb ()
{

  xfcalendar_toggle_visible ();

}

XfceTrayIcon*
create_TrayIcon (CalWin *xfcal)
{
  XfceTrayIcon *trayIcon = NULL;
  GtkWidget *menuItem;
  GtkWidget *trayMenu;
  GdkPixbuf *pixbuf;

  /*
   * Create the tray icon popup menu
   */
  trayMenu = gtk_menu_new();
  menuItem = gtk_image_menu_item_new_with_mnemonic(_("Today"));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem), gtk_image_new_from_stock(GTK_STOCK_HOME, GTK_ICON_SIZE_MENU));
  g_signal_connect(menuItem, "activate", G_CALLBACK(on_Today_activate),
		   xfcal);
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show_all(menuItem);
  menuItem = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);

  menuItem = gtk_menu_item_new_with_label(_("New appointment"));
  g_signal_connect(menuItem, "activate", G_CALLBACK(on_new_appointment_activate),
		   NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);
  menuItem = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);
  
  menuItem = gtk_menu_item_new_with_label(_("Preferences"));
  g_signal_connect(menuItem, "activate", G_CALLBACK(on_preferences_activate),
		   NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);
  menuItem = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);

  menuItem = gtk_menu_item_new_with_label(_("About Orage"));
  g_signal_connect(menuItem, "activate", G_CALLBACK(on_about_activate),
		   xfcal);
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);
  menuItem = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);
  menuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
  g_signal_connect(menuItem, "activate", G_CALLBACK(gtk_main_quit), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
  gtk_widget_show(menuItem);

  /*
   * Create the tray icon
   */
  pixbuf = xfce_themed_icon_load ("xfcalendar", 16);
  trayIcon = xfce_tray_icon_new_with_menu_from_pixbuf(trayMenu, pixbuf);
  g_object_unref(pixbuf);

  g_signal_connect_swapped(G_OBJECT(trayIcon), "clicked",
			   G_CALLBACK(toggle_visible_cb), xfcal);
  return trayIcon;
}
