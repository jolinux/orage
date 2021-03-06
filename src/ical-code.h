/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2005 Mickael Graf (korbinus at xfce.org)
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

#ifndef __ICAL_CODE_H__
#define __ICAL_CODE_H__

#define XFICAL_UID_LEN 200

typedef enum
{
    XFICAL_FREQ_NONE = 0
   ,XFICAL_FREQ_DAILY
   ,XFICAL_FREQ_WEEKLY
   ,XFICAL_FREQ_MONTHLY
   ,XFICAL_FREQ_YEARLY
   ,XFICAL_FREQ_HOURLY
} xfical_freq;

typedef enum
{
    XFICAL_TYPE_EVENT = 0
   ,XFICAL_TYPE_TODO
   ,XFICAL_TYPE_JOURNAL
} xfical_type;

typedef struct _xfical_exception
{
    gchar  time[17]; /* Orage uses no timezones here */
    gchar  type[7]; /* EXDATE, RDATE */
} xfical_exception;

typedef struct _xfical_appt
{
    xfical_type type;
    /* note that version 4.5.9 changed uid format.
     * new format starts with 3 char source id (plus separator '.'), 
     * which tells the file where the id is found:
     * "O00." = Orage file (normal file)
     * "A00." = Archive file
     * "F10." = Foreign file number 10
     * "H08." = Holiday file number 08
     */
    gchar *uid; 
    gchar *title;
    gchar *location;

    gboolean allDay;
    gboolean readonly;

        /* time format must be:
         * yyyymmdd[Thhmiss[Z]] = %04d%02d%02dT%02d%02d%02d
         * T means it has also time part
         * Z means it is in UTC format
         */
    gchar  starttime[17];
    gchar *start_tz_loc;
    gboolean use_due_time;  /* VTODO has due date or not */
    gchar  endtime[17];
    gchar *end_tz_loc;
    gboolean use_duration;
    gint   duration;
    gboolean completed;
    gchar  completedtime[17];
    gchar *completed_tz_loc;

    gint availability;
    gint priority;
    gchar *categories;
    gchar *note;

        /* alarm */
    gint alarmtime;
    gboolean alarm_before; /* TRUE = before FALSE = after */
        /* TRUE = related to start FALSE= related to end */
    gboolean alarm_related_start; 
    gboolean alarm_persistent;  /* do this alarm even if orage has been down */

    gboolean sound_alarm;
    gchar *sound;
    gboolean soundrepeat;
    gint soundrepeat_cnt;
    gint soundrepeat_len;

    gboolean display_alarm_orage;
    gboolean display_alarm_notify;
        /* used only with libnotify. -1 = no timeout 0 = use default timeout */
    gint display_notify_timeout;  

    /*
    gboolean email_alarm;
    gchar *email_attendees;
    */

    gboolean procedure_alarm;
    gchar *procedure_cmd;
    gchar *procedure_params;

        /* for repeating events cur times show current repeating event.
         * normal times are always the real (=first) start and end times
         */
    gchar  starttimecur[17];
    gchar  endtimecur[17];
    xfical_freq freq;
    gint   recur_limit; /* 0 = no limit  1 = count  2 = until */
    gint   recur_count;
    gchar  recur_until[17];
    gboolean recur_byday[7]; /* 0=Mo, 1=Tu, 2=We, 3=Th, 4=Fr, 5=Sa, 6=Su */
    gint   recur_byday_cnt[7]; /* monthly/early: 1=first -1=last 2=second... */
    gint   interval;    /* 1=every day/week..., 2=every second day/week,... */
    gboolean recur_todo_base_start; /* TRUE=start time, FALSE=completed time */
    GList  *recur_exceptions; /* EXDATE and RDATE list xfical_exception */
} xfical_appt;

gboolean xfical_set_local_timezone(gboolean testing);

gboolean xfical_file_open(gboolean foreign);
void xfical_file_close(gboolean foreign);
void xfical_file_close_force(void);

xfical_appt *xfical_appt_alloc();
char *xfical_appt_add(char *ical_file_id, xfical_appt *appt);
xfical_appt *xfical_appt_get(char *ical_id);
void xfical_appt_free(xfical_appt *appt);
gboolean xfical_appt_mod(char *ical_id, xfical_appt *appt);
gboolean xfical_appt_del(char *ical_id);

xfical_appt *xfical_appt_get_next_on_day(char *a_day, gboolean first, gint days
        , xfical_type type,  gchar *file_type);
xfical_appt *xfical_appt_get_next_with_string(char *str, gboolean first
        , gchar *file_type);
void xfical_get_each_app_within_time(char *a_day, int days
        , xfical_type type, gchar *file_type , GList **data);

void xfical_mark_calendar(GtkCalendar *gtkcal);
void xfical_mark_calendar_recur(GtkCalendar *gtkcal, xfical_appt *appt);

void xfical_alarm_build_list(gboolean first_list_today);

int xfical_compare_times(xfical_appt *appt);
#ifdef HAVE_ARCHIVE
gboolean xfical_archive_open(void);
void xfical_archive_close(void);
gboolean xfical_archive(void);
gboolean xfical_unarchive(void);
gboolean xfical_unarchive_uid(char *uid);
#endif

gboolean xfical_import_file(char *file_name);
gboolean xfical_export_file(char *file_name, int type, char *uids);

gboolean xfical_file_check(gchar *file_name);

#endif /* !__ICAL_CODE_H__ */
