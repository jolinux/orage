/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2006 Mickael Graf (korbinus at xfce.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the 
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#ifndef __TRAY_ICON_H__
#define __TRAY_ICON_H__

GdkPixbuf *orage_create_icon(gboolean static_icon, gint size);

GtkStatusIcon *create_TrayIcon(GdkPixbuf *orage_logo);

void refresh_TrayIcon(void);

#endif /* !__TRAY_ICON_H__ */
