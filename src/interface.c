/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2003-2005 Mickael Graf (korbinus at xfce.org)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "orage-i18n.h"
#include "functions.h"
#include "mainbox.h"
#include "interface.h"
#include "ical-code.h"
#include "parameters.h"


enum {
    DRAG_TARGET_URILIST = 0
   ,DRAG_TARGET_STRING
};
static const GtkTargetEntry file_drag_targets[] =
{
    { "text/uri-list", 0, DRAG_TARGET_URILIST }
};
static int file_drag_target_count = 1;
static const GtkTargetEntry uid_drag_targets[] =
{
    { "STRING", 0, DRAG_TARGET_STRING }
};
static int uid_drag_target_count = 1;

/* This prevents access from cmd line if the window is open.
 * We must make sure foreign files are not deleted from command
 * line (using dbus) while they are being updated using the window 
 */
static gboolean interface_lock = FALSE;


static void refresh_foreign_files(intf_win *intf_w, gboolean first);

gboolean orage_external_update_check(gpointer user_data)
{
#undef P_N
#define P_N "orage_external_update_check: "
    struct stat s;
    gint i;
    gboolean external_changes_present = FALSE;

    /* check main Orage file */
    if (g_stat(g_par.orage_file, &s) < 0) {
        orage_message(150, P_N "stat of %s failed: %d (%s)",
                g_par.orage_file, errno, strerror(errno));
    }
    else {
        if (s.st_mtime > g_par.latest_file_change) {
            g_par.latest_file_change = s.st_mtime;
            orage_message(10, _("Found external update on file %s.")
                    , g_par.orage_file);  
            external_changes_present = TRUE;
        }
    }

    /* check also foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        if (g_stat(g_par.foreign_data[i].file, &s) < 0) {
            orage_message(150, P_N "stat of %s failed: %d (%s)",
                    g_par.foreign_data[i].file, errno, strerror(errno));
        }
        else {
            if (s.st_mtime > g_par.foreign_data[i].latest_file_change) {
                g_par.foreign_data[i].latest_file_change = s.st_mtime;
                orage_message(10, _("Found external update on file %s.")
                        , g_par.foreign_data[i].file);
                external_changes_present = TRUE;
            }
        }
    }
    
    if (external_changes_present) {
        orage_message(80, _("Refreshing alarms and calendar due to external file update."));
        xfical_file_close_force();
        xfical_alarm_build_list(FALSE);
        orage_mark_appointments();
    }

    return(TRUE); /* keep running */
}

static void orage_file_entry_changed(GtkWidget *dialog, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(intf_w->orage_file_entry));
    if (strcmp(g_par.orage_file, s) == 0) {
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
    }
    else {
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, TRUE);
    }
}

static void orage_file_save_button_clicked(GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *s;
    gboolean ok = TRUE;

    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(intf_w->orage_file_entry)));
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_rename_rb))) {
        if (!g_file_test(s, G_FILE_TEST_EXISTS)) {
            g_warning("New file %s does not exist. Rename not done", s);
            ok = FALSE;
        }
        if (!xfical_file_check(s)) {
            g_warning("New file %s is not valid ical calendar file. Rename not done", s);
            ok = FALSE;
        }
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_copy_rb))) {
        ok = orage_copy_file(g_par.orage_file, s);
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_move_rb))) {
        if (g_rename(g_par.orage_file, s)) { /* failed */
            g_warning("rename failed. trying manual copy");
            ok = orage_copy_file(g_par.orage_file, s);
            if (ok) { /* this is move. so let's remove the orig */
               if (g_remove(g_par.orage_file))
                   g_warning("file remove failed %s", g_par.orage_file);
            }
        }
    }
    else {
        g_warning("illegal file save toggle button status");
        ok = FALSE;
    }

    /* finally orage internal file name change */
    if (ok) {
        if (g_par.orage_file)
            g_free(g_par.orage_file);
        g_par.orage_file = s;
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
        write_parameters(); /* store file name */
        xfical_file_close_force(); /* close it so that we open new file */
    }
    else {
        g_free(s);
    }
}

#ifdef HAVE_ARCHIVE
static void archive_file_entry_changed(GtkWidget *dialog, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(intf_w->archive_file_entry));
    if (strcmp(g_par.archive_file, s) == 0) { /* same file */
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
    }
    else {
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, TRUE);
    }
}

static void archive_file_save_button_clicked(GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *s;
    gboolean ok = TRUE;

    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(intf_w->archive_file_entry)));
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_rename_rb))) {
        if (!g_file_test(s, G_FILE_TEST_EXISTS)) {
            g_warning("New file %s does not exist. Rename not done", s);
            ok = FALSE;
        }
        if (!xfical_file_check(s)) {
            g_warning("New file %s is not valid ical calendar file. Rename not done", s);
            ok = FALSE;
        }
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_copy_rb))) {
        ok = orage_copy_file(g_par.archive_file, s);
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_move_rb))) {
        if (g_rename(g_par.archive_file, s)) { /* failed */
            g_warning("rename failed. trying manual copy");
            ok = orage_copy_file(g_par.archive_file, s);
            if (ok) { /* this is move. so let's remove the orig */
               if (g_remove(g_par.archive_file))
                   g_warning("file remove failed %s", g_par.archive_file);
            }
        }
    }
    else {
        g_warning("illegal file save toggle button status");
        ok = FALSE;
    }

    /* finally archive internal file name change */
    if (ok) {
        if (g_par.archive_file)
            g_free(g_par.archive_file);
        g_par.archive_file = s;
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
        write_parameters(); /* store file name */
    }
    else {
        g_free(s);
    }
}
#endif

static GtkWidget *orage_file_chooser(GtkWidget *parent_window
        , gboolean save, gchar *cur_file, gchar *cur_folder, gchar *def_name)
{
    GtkFileChooser *f_chooser;
    GtkFileFilter *filter;

    /* Create file chooser */
    f_chooser = GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(
            _("Select a file..."), GTK_WINDOW(parent_window)
            , save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN
            , GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
            , GTK_STOCK_OK, GTK_RESPONSE_ACCEPT
            , NULL));
    /* Add filters */
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Calendar files"));
    gtk_file_filter_add_pattern(filter, "*.ics");
    gtk_file_filter_add_pattern(filter, "*.vcs");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(f_chooser), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(f_chooser, filter);

    if (ORAGE_STR_EXISTS(cur_folder)) 
        gtk_file_chooser_add_shortcut_folder(f_chooser, cur_folder, NULL);

    gtk_file_chooser_set_show_hidden(f_chooser, TRUE);

    if (ORAGE_STR_EXISTS(cur_file)) {
        if (!gtk_file_chooser_set_filename(f_chooser, cur_file)) {
            if (ORAGE_STR_EXISTS(cur_folder)) 
                gtk_file_chooser_set_current_folder(f_chooser, cur_folder);
            if (ORAGE_STR_EXISTS(def_name)) 
                gtk_file_chooser_set_current_name(f_chooser, def_name);
        }
    }
    else {
        if (ORAGE_STR_EXISTS(cur_folder)) 
            gtk_file_chooser_set_current_folder(f_chooser, cur_folder);
        if (ORAGE_STR_EXISTS(def_name)) 
            gtk_file_chooser_set_current_name(f_chooser, def_name);
    }
    return(GTK_WIDGET(f_chooser));
}

static void orage_file_open_button_clicked(GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *rcfile, *rcdir;
    gchar *s;

    rcdir = g_path_get_dirname((const gchar *)g_par.orage_file);
    rcfile = g_path_get_basename((const gchar *)g_par.orage_file);
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , g_par.orage_file, rcdir, rcfile);
    g_free(rcdir);
    g_free(rcfile);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        s = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (s) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->orage_file_entry), s);
            gtk_widget_grab_focus(intf_w->orage_file_entry);
            gtk_editable_set_position(
                    GTK_EDITABLE(intf_w->orage_file_entry), -1);
            g_free(s);
        }
    }
    gtk_widget_destroy(f_chooser);
}

#ifdef HAVE_ARCHIVE
static void archive_file_open_button_clicked(GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *rcfile, *rcdir;
    gchar *s;

    rcdir = g_path_get_dirname((const gchar *)g_par.archive_file);
    rcfile = g_path_get_basename((const gchar *)g_par.archive_file);
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , g_par.archive_file, rcdir, rcfile);
    g_free(rcdir);
    g_free(rcfile);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        s = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (s) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->archive_file_entry), s);
            gtk_widget_grab_focus(intf_w->archive_file_entry);
            gtk_editable_set_position(
                    GTK_EDITABLE(intf_w->archive_file_entry), -1);
            g_free(s);
        }
    }
    gtk_widget_destroy(f_chooser);
}
#endif

static void exp_open_button_clicked(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL, *file_name=NULL;
    gchar *cal_file;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
        file_name = g_path_get_basename(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , entry_filename, file_path, file_name);
    g_free(file_path);
    g_free(file_name);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->iea_exp_entry), cal_file);
            gtk_widget_grab_focus(intf_w->iea_exp_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->iea_exp_entry), -1);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

static void imp_open_button_clicked(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL;
    gchar *cal_file;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_imp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, FALSE
            , entry_filename, file_path, NULL);
    g_free(file_path);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->iea_imp_entry), cal_file);
            gtk_widget_grab_focus(intf_w->iea_imp_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->iea_imp_entry), -1);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

#ifdef HAVE_ARCHIVE
static void on_archive_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    xfical_archive();
}

static void on_unarchive_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    xfical_unarchive();
}
#endif

gboolean orage_import_file(gchar *entry_filename) 
{
    if (xfical_import_file(entry_filename)) {
        orage_mark_appointments();
        xfical_alarm_build_list(FALSE);
        return(TRUE);
    }
    else
        return(FALSE);
}

gboolean orage_export_file(gchar *entry_filename, gint type, gchar *uids) 
{
    return(xfical_export_file(entry_filename, type, uids));
}

static void imp_save_button_clicked(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *entry_filename, *filename, *filename_end;
    gboolean more_files;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_imp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        more_files = TRUE;
        for (filename = entry_filename; more_files; ) {
            filename_end = g_strstr_len((const gchar *)filename
                    , strlen(filename), ",");
            if (filename_end != NULL)
                *filename_end = 0; /* filename ends here */
            /* FIXME: proper messages to screen */
            if (orage_import_file(filename))
                orage_message(40, "Import done %s", filename);
            else
                g_warning("import failed file=%s\n", filename);
            if (filename_end != NULL) { /* we have more files */
                filename = filename_end+1; /* next file starts here */
                more_files = TRUE;
            }
            else {
                more_files = FALSE;
            }
        }
    }
    else
        g_warning("save_button_clicked: filename MISSING");
    g_free(entry_filename);
}

static void exp_save_button_clicked(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *entry_filename, *entry_uids;
    gint app_count = 0; /* 0 = all, 1 = one */

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_entry));
    entry_uids = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_id_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        /* FIXME: proper messages to screen */
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(intf_w->iea_exp_add_all_rb))) {
            app_count = 0;
        }
        else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(intf_w->iea_exp_add_id_rb))) {
            app_count = 1;
        }
        else {
            g_warning("UNKNOWN select appointment\n");
        }

        if (orage_export_file(entry_filename, app_count, entry_uids))
            orage_message(40, "Export done %s", entry_filename);
        else
            g_warning("export failed file=%s\n", entry_filename);
    }
    else
        g_warning("save_button_clicked: filename MISSING");
    g_free(entry_filename);
    g_free(entry_uids);
}

static void for_open_button_clicked(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL;
    gchar *cal_file, *cal_name;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->for_new_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, FALSE
            , entry_filename, file_path, NULL);
    g_free(file_path);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->for_new_entry), cal_file);
            cal_name = g_path_get_basename(cal_file);
            gtk_entry_set_text(GTK_ENTRY(intf_w->for_new_name_entry), cal_name);
            gtk_widget_grab_focus(intf_w->for_new_name_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->for_new_name_entry)
                    , -1);
            g_free(cal_name);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

static void orage_foreign_file_remove_line(gint del_line)
{
    int i;

    g_free(g_par.foreign_data[del_line].file);
    g_free(g_par.foreign_data[del_line].name);
    g_par.foreign_count--;
    for (i = del_line; i < g_par.foreign_count; i++) {
        g_par.foreign_data[i].file = g_par.foreign_data[i+1].file;
        g_par.foreign_data[i].read_only = g_par.foreign_data[i+1].read_only;
        g_par.foreign_data[i].latest_file_change = g_par.foreign_data[i+1].latest_file_change;
        g_par.foreign_data[i].name = g_par.foreign_data[i+1].name;
    }
    g_par.foreign_data[i].file = NULL;
    g_par.foreign_data[i].name = NULL;

    write_parameters();
    orage_mark_appointments();
    xfical_alarm_build_list(FALSE);
}

gboolean orage_foreign_file_remove(gchar *filename)
{
    int i = 0;
    gboolean found = FALSE;

    if (interface_lock) {
        g_warning("Exchange window active, can't remove files from cmd line\n");
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(filename)) {
        g_warning("File is empty. Not removed.");
        return(FALSE);
    }
    for (i = 0; i < g_par.foreign_count && !found; i++) {
        if (strcmp(g_par.foreign_data[i].file, filename) == 0 ||
            strcmp(g_par.foreign_data[i].name, filename) == 0) {
            found = TRUE;
        }
    }
    if (!found) {
        g_warning("File not found. Not removed.");
        return(FALSE);
    }

    orage_foreign_file_remove_line(i);
    return(TRUE);
}

static void for_remove_button_clicked(GtkButton *button, gpointer user_data)
{
#undef P_N
#define P_N "for_remove_button_clicked: "
    gint del_line = GPOINTER_TO_INT(user_data);

    orage_message(90, P_N "Removing foreign file %s (%s).", g_par.foreign_data[del_line].name, g_par.foreign_data[del_line].file);
    orage_foreign_file_remove_line(del_line);
    orage_message(90, P_N "Foreign file removed and Orage alarms refreshed.");
}

static void for_remove_button_clicked2(GtkButton *button, gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    refresh_foreign_files(intf_w, FALSE);
}

static void refresh_foreign_files(intf_win *intf_w, gboolean first)
{
    GtkWidget *label, *hbox, *vbox, *button;
    gchar num[3];
    gint i;

    if (!first)
        gtk_widget_destroy(intf_w->for_cur_frame);
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->for_cur_frame = orage_create_framebox_with_content(
            _("Current foreign files"), vbox);
    gtk_box_pack_start(GTK_BOX(intf_w->for_tab_main_vbox)
            , intf_w->for_cur_frame, FALSE, FALSE, 5);

    intf_w->for_cur_table = orage_table_new(10, 5);
    if (g_par.foreign_count) {
        for (i = 0; i < g_par.foreign_count; i++) {
            hbox = gtk_hbox_new(FALSE, 0);
            if (g_par.foreign_data[i].name) {
                label = gtk_label_new(g_par.foreign_data[i].name);
                gtk_widget_set_tooltip_text(label, g_par.foreign_data[i].file);
            }
            else
                label = gtk_label_new(g_par.foreign_data[i].file);
            gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
            gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);
            if (g_par.foreign_data[i].read_only)
                label = gtk_label_new(_("READ ONLY"));
            else
                label = gtk_label_new(_("READ WRITE"));
            gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
            button = gtk_button_new_from_stock("gtk-remove");
            gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
            g_sprintf(num, "%02d", i+1);
            label = gtk_label_new(num);
            orage_table_add_row(intf_w->for_cur_table
                    , label, hbox
                    , i, (GTK_EXPAND | GTK_FILL), (0));

            g_signal_connect((gpointer)button, "clicked"
                    , G_CALLBACK(for_remove_button_clicked),GINT_TO_POINTER(i));
            g_signal_connect_after((gpointer)button, "clicked"
                    , G_CALLBACK(for_remove_button_clicked2), intf_w);
        }
        gtk_box_pack_start(GTK_BOX(vbox), intf_w->for_cur_table
                , FALSE, FALSE, 5);
    }
    else {
        label = gtk_label_new(_("***** No foreign files *****"));
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
    }
    gtk_widget_show_all(intf_w->for_cur_frame);
}

static gboolean orage_foreign_file_add_internal(gchar *filename
        , gchar *name, gboolean read_only, GtkWidget *main_window)
{
#undef P_N
#define P_N "orage_foreign_file_add_internal: "
    gint i = 0;
    gint result;
    char *add_failed = _("Foreign file add failed");

    if (g_par.foreign_count > 9) {
        orage_message(150, P_N "Orage can only handle 10 foreign files. Limit reached. New file not added.");
        if (main_window)
            result = orage_error_dialog(GTK_WINDOW(main_window)
                    , add_failed
                    , _("Orage can only handle 10 foreign files. Limit reached."));
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(filename)) {
        orage_message(150, P_N "File is empty. New file not added.");
        if (main_window)
            result = orage_error_dialog(GTK_WINDOW(main_window)
                    , add_failed
                    , _("Filename is empty."));
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(name)) {
        orage_message(150, P_N "Name is empty. New file not added.");
        if (main_window)
            result = orage_error_dialog(GTK_WINDOW(main_window)
                    , add_failed
                    , _("Name is empty."));
        return(FALSE);
    }
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        orage_message(150, P_N "New file %s does not exist. New file not added.", filename);
        if (main_window)
            result = orage_error_dialog(GTK_WINDOW(main_window)
                    , add_failed
                    , _("File does not exist."));
        return(FALSE);
    }
    for (i = 0; i < g_par.foreign_count; i++) {
        if (strcmp(g_par.foreign_data[i].file, filename) == 0) {
            orage_message(150, P_N "Foreign file already exists. New file not added");
            if (main_window)
                result = orage_error_dialog(GTK_WINDOW(main_window)
                        , add_failed
                        , _("Same filename already exists in Orage."));
                return(FALSE);
        }
        if (strcmp(g_par.foreign_data[i].name, name) == 0) {
            orage_message(150, P_N "Foreign file name already exists. New file not added");
            if (main_window)
                result = orage_error_dialog(GTK_WINDOW(main_window)
                        , add_failed
                        , _("Same name already exists in Orage."));
            return(FALSE);
        }
    }

    g_par.foreign_data[g_par.foreign_count].file = g_strdup(filename);
    g_par.foreign_data[g_par.foreign_count].name = g_strdup(name);
    g_par.foreign_data[g_par.foreign_count].read_only = read_only;
    g_par.foreign_data[g_par.foreign_count].latest_file_change = (time_t)0;
    g_par.foreign_count++;

    write_parameters();
    orage_mark_appointments();
    xfical_alarm_build_list(FALSE);
    return(TRUE);
}

/* this is used from command line */
gboolean orage_foreign_file_add(gchar *filename, gboolean read_only
        , gchar *name)
{
    if (interface_lock) {
        g_warning("Exchange window active, can't add files from cmd line\n");
        return(FALSE);
    }
    return(orage_foreign_file_add_internal(filename, name, read_only, NULL));
}

static void for_add_button_clicked(GtkButton *button, gpointer user_data)
{
#undef P_N
#define P_N "for_add_button_clicked: "
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *entry_filename;
    const gchar *entry_name;
    gboolean read_only;

    entry_filename = gtk_entry_get_text((GtkEntry *)intf_w->for_new_entry);
    entry_name = gtk_entry_get_text((GtkEntry *)intf_w->for_new_name_entry);
    read_only = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->for_new_read_only));
    if (orage_foreign_file_add_internal((char *)entry_filename
                , (char *)entry_name, read_only, intf_w->main_window)) {
        refresh_foreign_files(intf_w, FALSE);
        orage_message(80, P_N "New foreign file %s (%s) added.", entry_name
                , entry_filename);
    }
}

static void close_intf_w(gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_destroy(intf_w->main_window);
    g_free(intf_w);
    interface_lock = FALSE;
}

static void close_button_clicked(GtkButton *button, gpointer user_data)
{
    close_intf_w(user_data);
}

static void filemenu_close_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    close_intf_w(user_data);
}

static gboolean on_Window_delete_event(GtkWidget *w, GdkEvent *e
        , gpointer user_data)
{
    close_intf_w(user_data);
    return(FALSE);
}

static void create_menu(intf_win *intf_w)
{
    /* Menu bar */
    intf_w->menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(intf_w->main_vbox), intf_w->menubar
            , FALSE, FALSE, 0);

    /* File menu */
    intf_w->filemenu = orage_menu_new(_("_File"), intf_w->menubar);

    intf_w->filemenu_close = 
            orage_image_menu_item_new_from_stock("gtk-close" 
                    , intf_w->filemenu, intf_w->accelgroup);

    g_signal_connect((gpointer)intf_w->filemenu_close, "activate"
            , G_CALLBACK(filemenu_close_activated), intf_w);
}

static void create_toolbar(intf_win *intf_w)
{
    gint i = 0;

    /* Toolbar */
    intf_w->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(intf_w->main_vbox), intf_w->toolbar
            , FALSE, FALSE, 0);

    /* Buttons */
    intf_w->close_button = orage_toolbar_append_button(intf_w->toolbar
            , "gtk-close", _("Close"), i++);

    g_signal_connect((gpointer)intf_w->close_button, "clicked"
            , G_CALLBACK(close_button_clicked), intf_w);
}

static void handle_file_drag_data(GtkWidget *widget, GdkDragContext *context
        , GtkSelectionData *data, guint d_time, gboolean imp)
{
    gchar **file_list;
    gchar *file;
    gint i, pos;
    GError *error = NULL;

    if (data->length < 0) {
        g_warning("File drag failed");
        gtk_drag_finish(context, FALSE, FALSE, d_time);
        return;
    }
    file_list = g_uri_list_extract_uris((gchar *)data->data);
    for (i = 0; file_list[i] != NULL; i++) {
        if ((file = g_filename_from_uri(file_list[i], NULL, &error)) == NULL) {
            g_warning("Dragging g_filename_from_uri %s failed %s"
                    , file_list[i], error->message);
            g_error_free(error);
            return;
        }
        if (i == 0) { /* first file (often the only file */
            gtk_entry_set_text(GTK_ENTRY(widget), file);
            gtk_editable_set_position(GTK_EDITABLE(widget), -1);
            pos = gtk_editable_get_position(GTK_EDITABLE(widget));
        }
        else {
            if (imp) { /* quite ok to have more files */
                gtk_editable_insert_text(GTK_EDITABLE(widget), ",", 1, &pos);
                gtk_editable_insert_text(GTK_EDITABLE(widget), file
                        , strlen(file), &pos);
            }
            else { /* export to only 1 file, ignoring the rest */
                g_warning("Exporting only to one file, ignoring drag file %d (%s)\n"
                        , i+1, file_list[i]);
            }
        }
    }
    gtk_drag_finish(context, TRUE, FALSE, d_time);
}

static void imp_file_drag_data_received(GtkWidget *widget
        , GdkDragContext *context, gint x, gint y, GtkSelectionData *data
        , guint info, guint d_time)
{
    handle_file_drag_data(widget, context, data, d_time, TRUE);
}

static void exp_file_drag_data_received(GtkWidget *widget
        , GdkDragContext *context, gint x, gint y, GtkSelectionData *data
        , guint info, guint d_time)
{
    handle_file_drag_data(widget, context, data, d_time, FALSE);
}

static void uid_drag_data_received(GtkWidget *widget, GdkDragContext *context
        , gint x, gint y, GtkSelectionData *data, guint info, guint d_time)
{
    if (data->length < 0) {
        g_warning("UID drag failed");
        gtk_drag_finish(context, FALSE, FALSE, d_time);
        return;
    }
    gtk_drag_finish(context, TRUE, FALSE, d_time);
}

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context
        , gint x, gint y, guint d_time)
{
    gtk_drag_get_data(widget, context
            , GDK_POINTER_TO_ATOM(context->targets->data), d_time);

    return(TRUE);
}

static void drag_and_drop_init(intf_win *intf_w)
{
    gtk_drag_dest_set(intf_w->iea_imp_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , file_drag_targets, file_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_imp_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_imp_entry, "drag_data_received"
            , G_CALLBACK(imp_file_drag_data_received), NULL);

    gtk_drag_dest_set(intf_w->iea_exp_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , file_drag_targets, file_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_exp_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_exp_entry, "drag_data_received"
            , G_CALLBACK(exp_file_drag_data_received), NULL);

    gtk_drag_dest_set(intf_w->iea_exp_id_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , uid_drag_targets, uid_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_exp_id_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_exp_id_entry, "drag_data_received"
            , G_CALLBACK(uid_drag_data_received), NULL);
}

static void exp_add_all_rb_clicked(GtkWidget *button, gpointer *user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, FALSE);
}

static void exp_add_id_rb_clicked(GtkWidget *button, gpointer *user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, TRUE);
}

static void create_import_export_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox, *m_vbox;
    gchar *file;
    gchar *str;

    m_vbox = gtk_vbox_new(FALSE, 0);
    intf_w->iea_notebook_page = orage_create_framebox_with_content(
            NULL, m_vbox);
    intf_w->iea_tab_label = gtk_label_new(_("Import/export"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->iea_notebook_page, intf_w->iea_tab_label);

    /***** import *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->iea_imp_frame = orage_create_framebox_with_content(
            _("Import"), vbox);
    gtk_box_pack_start(GTK_BOX(m_vbox)
            , intf_w->iea_imp_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Read from file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->iea_imp_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_imp_entry, TRUE, TRUE, 0);
    intf_w->iea_imp_open_button = gtk_button_new_from_stock("gtk-open");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_imp_open_button
            , FALSE, FALSE, 5);
    intf_w->iea_imp_save_button = gtk_button_new_from_stock("gtk-save");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_imp_save_button
            , FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->iea_imp_open_button, "clicked"
            , G_CALLBACK(imp_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->iea_imp_save_button, "clicked"
            , G_CALLBACK(imp_save_button_clicked), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_imp_entry 
            , _("Separate filenames with comma(,).\n NOTE: comma is not valid character in filenames for Orage."));

    /***** export *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->iea_exp_frame = orage_create_framebox_with_content(
            _("Export"), vbox);
    gtk_box_pack_start(GTK_BOX(m_vbox)
            , intf_w->iea_exp_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Write to file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->iea_exp_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_entry, TRUE, TRUE, 0);
    file = g_build_filename(g_get_home_dir(), "orage_export.ics", NULL);
    gtk_entry_set_text(GTK_ENTRY(intf_w->iea_exp_entry), file);
    g_free(file);
    intf_w->iea_exp_open_button = gtk_button_new_from_stock("gtk-open");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_open_button
            , FALSE, FALSE, 5);
    intf_w->iea_exp_save_button = gtk_button_new_from_stock("gtk-save");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_save_button
            , FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->iea_exp_open_button, "clicked"
            , G_CALLBACK(exp_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->iea_exp_save_button, "clicked"
            , G_CALLBACK(exp_save_button_clicked), intf_w);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Select"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->iea_exp_add_all_rb = gtk_radio_button_new_with_label(NULL
            , _("All appointments"));
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_add_all_rb
            , FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->iea_exp_add_all_rb, "clicked"
            , G_CALLBACK(exp_add_all_rb_clicked), intf_w);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Select"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->iea_exp_add_id_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(intf_w->iea_exp_add_all_rb)
            , _("Named appointments: "));
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_add_id_rb
            , FALSE, FALSE, 0);
    intf_w->iea_exp_id_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_exp_id_entry, TRUE, TRUE, 0);
    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->iea_exp_add_id_rb, "clicked"
            , G_CALLBACK(exp_add_id_rb_clicked), intf_w);
#ifdef HAVE_ARCHIVE
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_all_rb
            , _("Note that only main file appointments are read.\nArchived and Foreign events are not exported."));
#else
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_all_rb
            , _("Note that only main file appointments are read.\nForeign events are not exported."));
#endif
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_id_rb
            , _("You can easily drag these from event-list window."));
    gtk_widget_set_tooltip_text(intf_w->iea_exp_id_entry
            , _("Orage appointment UIDs separated by commas."));

#ifdef HAVE_ARCHIVE
    /***** archive *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->iea_arc_frame = orage_create_framebox_with_content(
            _("Archive"), vbox);
    gtk_box_pack_start(GTK_BOX(m_vbox)
            , intf_w->iea_arc_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    intf_w->iea_arc_button1 = gtk_button_new_from_stock("gtk-execute");
    gtk_box_pack_start(GTK_BOX(hbox),  intf_w->iea_arc_button1
            , FALSE, FALSE, 5);
    str = g_strdup_printf(_("Archive now (threshold: %d months)")
            , g_par.archive_limit);
    label = gtk_label_new(str);
    g_free(str);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->iea_arc_button1, "clicked"
            , G_CALLBACK(on_archive_button_clicked_cb), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_arc_button1
            , _("You can change archive threshold in parameters"));

    hbox = gtk_hbox_new(FALSE, 0);
    intf_w->iea_arc_button2 = gtk_button_new_from_stock("gtk-execute");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->iea_arc_button2
            , FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    label = gtk_label_new(_("Revert archive now"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    g_signal_connect((gpointer)intf_w->iea_arc_button2, "clicked"
            , G_CALLBACK(on_unarchive_button_clicked_cb), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_arc_button2
            , _("Return all archived events into main orage file and remove arch file.\nThis is useful for example when doing export and moving orage\nappointments to another system."));
#endif
}

static void create_orage_file_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox, *m_vbox;

    m_vbox = gtk_vbox_new(FALSE, 0);
    /* FIXME: this could be simpler than framebox */
    intf_w->fil_notebook_page = orage_create_framebox_with_content(NULL, m_vbox);
    intf_w->fil_tab_label = gtk_label_new(_("Orage files"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->fil_notebook_page, intf_w->fil_tab_label);

    /***** main file *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->orage_file_frame = orage_create_framebox_with_content(
            _("Orage main calendar file"), vbox);
    gtk_box_pack_start(GTK_BOX(m_vbox)
            , intf_w->orage_file_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Current file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    label = gtk_label_new((const gchar *)g_par.orage_file);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("New file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->orage_file_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(intf_w->orage_file_entry)
            , (const gchar *)g_par.orage_file);
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->orage_file_entry, TRUE, TRUE, 0);
    intf_w->orage_file_open_button = gtk_button_new_from_stock("gtk-open");
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->orage_file_open_button, FALSE, FALSE, 5);
    intf_w->orage_file_save_button = gtk_button_new_from_stock("gtk-save");
    gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->orage_file_save_button, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Action options:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->orage_file_rename_rb = 
            gtk_radio_button_new_with_label(NULL, _("Rename"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->orage_file_rename_rb, FALSE, FALSE, 5);
    gtk_widget_set_tooltip_text(intf_w->orage_file_rename_rb
            , _("Orage internal file rename only.\nDoes not touch external filesystem at all.\nNew file must exist."));

    intf_w->orage_file_copy_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(intf_w->orage_file_rename_rb), _("Copy"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->orage_file_copy_rb, FALSE, FALSE, 5);
    gtk_widget_set_tooltip_text(intf_w->orage_file_copy_rb
            , _("Current file is copied and stays unmodified in the old place."));

    intf_w->orage_file_move_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(intf_w->orage_file_rename_rb), _("Move"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->orage_file_move_rb, FALSE, FALSE, 5);
    gtk_widget_set_tooltip_text(intf_w->orage_file_move_rb
            , _("Current file is moved and vanishes from the old place."));
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect(G_OBJECT(intf_w->orage_file_open_button), "clicked"
            , G_CALLBACK(orage_file_open_button_clicked), intf_w);
    g_signal_connect(G_OBJECT(intf_w->orage_file_entry), "changed"
            , G_CALLBACK(orage_file_entry_changed), intf_w);
    g_signal_connect(G_OBJECT(intf_w->orage_file_save_button), "clicked"
            , G_CALLBACK(orage_file_save_button_clicked), intf_w);

#ifdef HAVE_ARCHIVE
    /***** archive file *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->archive_file_frame = orage_create_framebox_with_content(
            _("Archive file"), vbox);
    gtk_box_pack_start(GTK_BOX(m_vbox)
            , intf_w->archive_file_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Current file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    label = gtk_label_new((const gchar *)g_par.archive_file);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("New file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->archive_file_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(intf_w->archive_file_entry)
            , (const gchar *)g_par.archive_file);
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_entry, TRUE, TRUE, 0);
    intf_w->archive_file_open_button = gtk_button_new_from_stock("gtk-open");
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_open_button, FALSE, FALSE, 5);
    intf_w->archive_file_save_button = gtk_button_new_from_stock("gtk-save");
    gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_save_button, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Action options:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->archive_file_rename_rb = 
            gtk_radio_button_new_with_label(NULL, _("Rename"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_rename_rb, FALSE, FALSE, 5);
    intf_w->archive_file_copy_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                GTK_RADIO_BUTTON(intf_w->archive_file_rename_rb), _("Copy"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_copy_rb, FALSE, FALSE, 5);
    intf_w->archive_file_move_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                GTK_RADIO_BUTTON(intf_w->archive_file_rename_rb), _("Move"));
    gtk_box_pack_start(GTK_BOX(hbox)
            , intf_w->archive_file_move_rb, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect(G_OBJECT(intf_w->archive_file_open_button), "clicked"
            , G_CALLBACK(archive_file_open_button_clicked), intf_w);
    g_signal_connect(G_OBJECT(intf_w->archive_file_entry), "changed"
            , G_CALLBACK(archive_file_entry_changed), intf_w);
    g_signal_connect(G_OBJECT(intf_w->archive_file_save_button), "clicked"
            , G_CALLBACK(archive_file_save_button_clicked), intf_w);
#endif
}

static void create_foreign_file_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox;

    intf_w->for_tab_main_vbox = gtk_vbox_new(FALSE, 0);
    /* FIXME: this could be simpler than framebox */
    intf_w->for_notebook_page = orage_create_framebox_with_content(NULL
            , intf_w->for_tab_main_vbox);
    intf_w->for_tab_label = gtk_label_new(_("Foreign files"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->for_notebook_page, intf_w->for_tab_label);

    /***** Add new file *****/
    vbox = gtk_vbox_new(FALSE, 0);
    intf_w->for_new_frame = orage_create_framebox_with_content(
            _("Add new foreign file"), vbox);
    gtk_box_pack_start(GTK_BOX(intf_w->for_tab_main_vbox)
            , intf_w->for_new_frame, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Foreign file:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->for_new_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->for_new_entry, TRUE, TRUE, 0);
    intf_w->for_new_open_button = gtk_button_new_from_stock("gtk-open");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->for_new_open_button
            , FALSE, FALSE, 5);
    intf_w->for_new_save_button = gtk_button_new_from_stock("gtk-add");
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->for_new_save_button
            , FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    g_signal_connect((gpointer)intf_w->for_new_open_button, "clicked"
            , G_CALLBACK(for_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->for_new_save_button, "clicked"
            , G_CALLBACK(for_add_button_clicked), intf_w);

    hbox = gtk_hbox_new(FALSE, 0);
    /*
    label = gtk_label_new(_("Options:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    */
    label = gtk_label_new(_("Visible name:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    intf_w->for_new_name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->for_new_name_entry
            , TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    intf_w->for_new_read_only = gtk_check_button_new_with_label(_("Read only"));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(intf_w->for_new_read_only), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), intf_w->for_new_read_only
            , FALSE, FALSE, 5);

    gtk_widget_set_tooltip_text(intf_w->for_new_read_only
            , _("Set this if you want to make sure that this file is never modified by Orage.\nNote that modifying foreign files may make them incompatible with the original tool, where they came from!"));
    gtk_widget_set_tooltip_text(intf_w->for_new_name_entry
            , _("This internal name is displayed to user instead of file name."));

    /***** Current files *****/
    refresh_foreign_files(intf_w, TRUE);
}

void orage_external_interface(CalWin *xfcal)
{
    intf_win *intf_w = g_new(intf_win, 1);

    interface_lock = TRUE;
     /* main window creation and base elements */
    intf_w->main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(intf_w->main_window)
            , _("Exchange data - Orage"));
    gtk_window_set_default_size(GTK_WINDOW(intf_w->main_window), 300, 200);

    intf_w->accelgroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(intf_w->main_window)
            , intf_w->accelgroup);

    intf_w->main_vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(intf_w->main_window), intf_w->main_vbox);

    create_menu(intf_w);
    create_toolbar(intf_w);

     /* create tabs */
    intf_w->notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(intf_w->main_vbox), intf_w->notebook);
    gtk_container_set_border_width(GTK_CONTAINER(intf_w->notebook), 5);

    create_import_export_tab(intf_w);
    create_orage_file_tab(intf_w);
    create_foreign_file_tab(intf_w);

    g_signal_connect((gpointer)intf_w->main_window, "delete_event",
            G_CALLBACK(on_Window_delete_event), intf_w);

    gtk_widget_show_all(intf_w->main_window);
    drag_and_drop_init(intf_w);
}
