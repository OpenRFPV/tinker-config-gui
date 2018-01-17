/* NOTE tinker-config nonint functions obey sh return codes - 0 is in general success / yes / selected, 1 is failed / no / not selected */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <libintl.h>

/* Command strings */
#define EXPAND_FS       "sudo tinker-config nonint do_expand_rootfs"
#define GET_HOSTNAME    "sudo tinker-config nonint get_hostname"
#define SET_HOSTNAME    "sudo tinker-config nonint do_change_hostname %s"
#define GET_BOOT_CLI    "sudo tinker-config nonint get_boot_cli"
#define GET_AUTOLOGIN   "sudo tinker-config nonint get_autologin"
#define SET_BOOT_CLI    "sudo tinker-config nonint do_boot_behaviour B1"
#define SET_BOOT_CLIA   "sudo tinker-config nonint do_boot_behaviour B2"
#define SET_BOOT_GUI    "sudo tinker-config nonint do_boot_behaviour B3"
#define SET_BOOT_GUIA   "sudo tinker-config nonint do_boot_behaviour B4"
#define GET_BOOT_WAIT   "sudo tinker-config nonint get_boot_wait"
#define SET_BOOT_WAIT   "sudo tinker-config nonint do_boot_wait %d"
#define GET_SPLASH      "sudo tinker-config nonint get_boot_splash"
#define SET_SPLASH      "sudo tinker-config nonint do_boot_splash %d"
#define GET_SSH         "sudo tinker-config nonint get_ssh"
#define SET_SSH         "sudo tinker-config nonint do_ssh %d"
#define GET_VNC         "sudo tinker-config nonint get_vnc"
#define SET_VNC         "sudo tinker-config nonint do_vnc %d"
#define GET_SPI         "sudo tinker-config nonint get_spi %d"
#define SET_SPI         "sudo tinker-config nonint do_single_spi %d %d"
#define GET_I2C         "sudo tinker-config nonint get_i2c %d"
#define SET_I2C         "sudo tinker-config nonint do_single_i2c %d %d"
#define GET_UART        "sudo tinker-config nonint get_uart %d"
#define SET_UART        "sudo tinker-config nonint do_single_uart %d %d"
#define GET_SERIAL      "sudo tinker-config nonint get_serial"
#define GET_SERIALHW    "sudo tinker-config nonint get_serial_hw"
#define SET_SERIAL      "sudo tinker-config nonint do_serial %d"
#define GET_1WIRE       "sudo tinker-config nonint get_onewire"
#define SET_1WIRE       "sudo tinker-config nonint do_onewire %d"
#define GET_RGPIO       "sudo tinker-config nonint get_rgpio"
#define SET_RGPIO       "sudo tinker-config nonint do_rgpio %d"
#define CHECK_RGPIO     "sudo tinker-config nonint check_rgpio"
#define SET_CUS_RES     "sudo tinker-config nonint do_cus_resolution %d %d %d"
#define GET_WIFI_CTRY   "sudo tinker-config nonint get_wifi_country"
#define SET_WIFI_CTRY   "sudo tinker-config nonint do_wifi_country %s"
#define CHANGE_PASSWD   "(echo \"%s\" ; echo \"%s\") | sudo passwd linaro"
#define CHANGE_VNC_PASSWD   "sudo tinker-config nonint set_vnc_passwd %s"
#define VNC_PASSWD_EXIST    "[ -e $HOME/.vnc/passwd ]" //0 = exist, 1 = not exist

#define PACKAGE_DATA_DIR "/usr/share/tc_gui/"

/* Controls */

static GObject *expandfs_btn, *passwd_btn, *res_btn, *cus_res_btn;
static GObject *vncpasswd_btn, *spi_btn, *i2c_btn, *uart_btn;
static GObject *locale_btn, *timezone_btn, *keyboard_btn, *wifi_btn;
static GObject *boot_desktop_rb, *boot_cli_rb;
static GObject *overscan_on_rb, *overscan_off_rb, *ssh_on_rb, *ssh_off_rb, *vnc_on_rb, *vnc_off_rb;
static GObject *serial_on_rb, *serial_off_rb, *onewire_on_rb, *onewire_off_rb, *rgpio_on_rb, *rgpio_off_rb;
static GObject *autologin_cb, *netwait_cb, *splash_on_rb, *splash_off_rb;
static GObject *memsplit_sb, *hostname_tb;
static GObject *cusresentry2_tb, *cusresentry3_tb, *cusresentry4_tb, *cusresok_btn;
static GObject *pwentry1_tb, *pwentry2_tb, *pwentry3_tb, *pwok_btn;
static GObject *vncpwentry1_tb, *vncpwentry2_tb, *vncpwentry3_tb, *vncpwok_btn;
static GObject *rtname_tb, *rtemail_tb, *rtok_btn;
static GObject *tzarea_cb, *tzloc_cb, *wccountry_cb, *resolution_cb;
static GObject *loclang_cb, *loccount_cb, *locchar_cb;
static GObject *language_ls, *country_ls;
static GtkWidget *main_dlg, *msg_dlg;

/* Initial values */

static char orig_hostname[128];
static char username[128];
static int orig_boot, orig_overscan, orig_camera, orig_ssh, orig_serial, orig_splash;
static int orig_clock, orig_gpumem, orig_autolog, orig_netwait, orig_onewire, orig_rgpio, orig_vnc;

/* Reboot flag set after locale change */

static int needs_reboot;

/* Number of items in comboboxes */

static int loc_count, country_count, char_count;

/* Global locale accessed from multiple threads */

static char glocale[64];

/* Helpers */

static int get_status (char *cmd)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];
    int res;

    if (fp == NULL) return 0;
    if (fgets (buf, sizeof (buf) - 1, fp) != NULL)
    {
        sscanf (buf, "%d", &res);
        pclose (fp);
        return res;
    }
    pclose (fp);
    return 0;
}

static void get_string (char *cmd, char *name)
{
    FILE *fp = popen (cmd, "r");
    char buf[64];

    name[0] = 0;
    if (fp == NULL) return;
    if (fgets (buf, sizeof (buf) - 1, fp) != NULL)
    {
        sscanf (buf, "%s", name);
    }
    pclose (fp);
}

static void get_quoted_param (char *path, char *fname, char *toseek, char *result)
{
    char buffer[256], *linebuf = NULL, *cptr, *dptr;
    int len = 0;

    sprintf (buffer, "%s/%s", path, fname);
    FILE *fp = fopen (buffer, "rb");

    while (getline (&linebuf, &len, fp) > 0)
    {
        // skip whitespace at line start
        cptr = linebuf;
        while (*cptr == ' ' || *cptr == '\t') cptr++;

        // compare against string to find
        if (!strncmp (cptr, toseek, strlen (toseek)))
        {
            // find string in quotes
            strtok (cptr, "\"");
            dptr = strtok (NULL, "\"\n\r");

            // copy to dest
            if (dptr) strcpy (result, dptr);
            else result[0] = 0;

            // done
            free (linebuf);
            fclose (fp);
            return;
        }
    }

    // end of file with no match
    result[0] = 0;
    free (linebuf);
    fclose (fp);
}

static void get_language (char *instr, char *lang) 
{
    char *cptr = lang;
    int count = 0;
    
    while (count < 4 && instr[count] >= 'a' && instr[count] <= 'z')
    {
        *cptr++ = instr[count++];
    }
    if (count < 2 || count > 3 || (instr[count] != '_' && instr[count] != 0  && instr[count] != '.')) *lang = 0;
    else *cptr = 0;
}

static void get_country (char *instr, char *ctry)
{
    char *cptr = ctry;
    int count = 0;

    while (instr[count] != '_' && instr[count] != 0 && count < 5) count++;

    if (count == 5 || instr[count] == 0) *ctry = 0;
    else
    {
        count++;
        while (instr[count] && instr[count] != '.') *cptr++ = instr[count++];
        *cptr = 0;
    }
}

/* Password setting */

static void set_passwd (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb))) && strcmp (gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), 
		gtk_entry_get_text (GTK_ENTRY(pwentry3_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), TRUE);
}

static void on_change_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128];
    int res;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "passwddialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    pwentry2_tb = gtk_builder_get_object (builder, "pwentry2");
    pwentry3_tb = gtk_builder_get_object (builder, "pwentry3");
    gtk_entry_set_visibility (GTK_ENTRY (pwentry2_tb), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (pwentry3_tb), FALSE);
    g_signal_connect (pwentry2_tb, "changed", G_CALLBACK (set_passwd), NULL);
    g_signal_connect (pwentry3_tb, "changed", G_CALLBACK (set_passwd), NULL);
    pwok_btn = gtk_builder_get_object (builder, "passwdok");
    gtk_widget_set_sensitive (GTK_WIDGET (pwok_btn), FALSE);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        sprintf (buffer, CHANGE_PASSWD, gtk_entry_get_text (GTK_ENTRY (pwentry2_tb)), gtk_entry_get_text (GTK_ENTRY (pwentry3_tb)));
        res = system (buffer);
        gtk_widget_destroy (dlg);
		if (res)
			dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwbaddialog");
		else
			dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwokdialog");
		gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
    }
    else gtk_widget_destroy (dlg);
    g_object_unref (builder);
}

/* VNC Password setting */

static void set_vnc_passwd (GtkEntry *entry, gpointer ptr)
{
    if (strlen (gtk_entry_get_text (GTK_ENTRY (vncpwentry2_tb))) && strcmp (gtk_entry_get_text (GTK_ENTRY (vncpwentry2_tb)),
		gtk_entry_get_text (GTK_ENTRY(vncpwentry3_tb))))
        gtk_widget_set_sensitive (GTK_WIDGET (vncpwok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (vncpwok_btn), TRUE);
}

static void on_change_vnc_passwd (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128];
    int res;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "vncpasswddialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    vncpwentry2_tb = gtk_builder_get_object (builder, "vncpwentry2");
    vncpwentry3_tb = gtk_builder_get_object (builder, "vncpwentry3");
    gtk_entry_set_visibility (GTK_ENTRY (vncpwentry2_tb), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (vncpwentry3_tb), FALSE);
    g_signal_connect (vncpwentry2_tb, "changed", G_CALLBACK (set_vnc_passwd), NULL);
    g_signal_connect (vncpwentry3_tb, "changed", G_CALLBACK (set_vnc_passwd), NULL);
    vncpwok_btn = gtk_builder_get_object (builder, "vncpasswdok");
    gtk_widget_set_sensitive (GTK_WIDGET (vncpwok_btn), FALSE);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        sprintf (buffer, CHANGE_VNC_PASSWD, gtk_entry_get_text (GTK_ENTRY (vncpwentry3_tb)));
        res = system (buffer);
        gtk_widget_destroy (dlg);
		if (res)
			dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwbaddialog");
		else
			dlg = (GtkWidget *) gtk_builder_get_object (builder, "pwokdialog");
		gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
    }
    else gtk_widget_destroy (dlg);
    g_object_unref (builder);
}

/* SPI setting */

static void on_set_spi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GObject *spi0_on_rb, *spi0_off_rb, *spi2_on_rb, *spi2_off_rb;
    int orig_spi0, orig_spi2;
    char buffer[128];

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "spidialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    spi0_on_rb = gtk_builder_get_object (builder, "rb_spi0_on");
    spi0_off_rb = gtk_builder_get_object (builder, "rb_spi0_off");
    sprintf (buffer, GET_SPI, 0);
    if (orig_spi0 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi0_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi0_on_rb), TRUE);

    spi2_on_rb = gtk_builder_get_object (builder, "rb_spi2_on");
    spi2_off_rb = gtk_builder_get_object (builder, "rb_spi2_off");
    sprintf (buffer, GET_SPI, 2);
    if (orig_spi2 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi2_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spi2_on_rb), TRUE);

    g_object_unref (builder);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (orig_spi0 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi0_off_rb)))
        {
            sprintf (buffer, SET_SPI, 0, (1 - orig_spi0));
            system (buffer);
            needs_reboot = 1;
        }

        if (orig_spi2 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spi2_off_rb)))
        {
            sprintf (buffer, SET_SPI, 2, (1 - orig_spi2));
            system (buffer);
            needs_reboot = 1;
        }
    }
    gtk_widget_destroy (dlg);
}

/* I2C setting */

static void on_set_i2c (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GObject *i2c1_on_rb, *i2c1_off_rb, *i2c4_on_rb, *i2c4_off_rb;
    int orig_i2c1, orig_i2c4;
    char buffer[128];

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "i2cdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    i2c1_on_rb = gtk_builder_get_object (builder, "rb_i2c1_on");
    i2c1_off_rb = gtk_builder_get_object (builder, "rb_i2c1_off");
    sprintf (buffer, GET_I2C, 1);
    if (orig_i2c1 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c1_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c1_on_rb), TRUE);

    i2c4_on_rb = gtk_builder_get_object (builder, "rb_i2c4_on");
    i2c4_off_rb = gtk_builder_get_object (builder, "rb_i2c4_off");
    sprintf (buffer, GET_I2C, 4);
    if (orig_i2c4 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c4_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (i2c4_on_rb), TRUE);

    g_object_unref (builder);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (orig_i2c1 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c1_off_rb)))
        {
            sprintf (buffer, SET_I2C, 1, (1 - orig_i2c1));
            system (buffer);
            needs_reboot = 1;
        }

        if (orig_i2c4 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (i2c4_off_rb)))
        {
            sprintf (buffer, SET_I2C, 4, (1 - orig_i2c4));
            system (buffer);
            needs_reboot = 1;
        }
    }
    gtk_widget_destroy (dlg);
}

/* UART setting */

static void on_set_uart (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    GObject *uart1_on_rb, *uart1_off_rb, *uart2_on_rb, *uart2_off_rb, *uart3_on_rb, *uart3_off_rb, *uart4_on_rb, *uart4_off_rb;
    int orig_uart1, orig_uart2, orig_uart3, orig_uart4;
    char buffer[128];

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "uartdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    uart1_on_rb = gtk_builder_get_object (builder, "rb_uart1_on");
    uart1_off_rb = gtk_builder_get_object (builder, "rb_uart1_off");
    sprintf (buffer, GET_UART, 1);
    if (orig_uart1 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart1_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart1_on_rb), TRUE);

    uart2_on_rb = gtk_builder_get_object (builder, "rb_uart2_on");
    uart2_off_rb = gtk_builder_get_object (builder, "rb_uart2_off");
    sprintf (buffer, GET_UART, 2);
    if (orig_uart2 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart2_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart2_on_rb), TRUE);

    uart3_on_rb = gtk_builder_get_object (builder, "rb_uart3_on");
    uart3_off_rb = gtk_builder_get_object (builder, "rb_uart3_off");
    sprintf (buffer, GET_UART, 3);
    if (orig_uart3 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart3_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart3_on_rb), TRUE);

    uart4_on_rb = gtk_builder_get_object (builder, "rb_uart4_on");
    uart4_off_rb = gtk_builder_get_object (builder, "rb_uart4_off");
    sprintf (buffer, GET_UART, 4);
    if (orig_uart4 = get_status (buffer)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart4_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uart4_on_rb), TRUE);

    g_object_unref (builder);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (orig_uart1 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uart1_off_rb)))
        {
            sprintf (buffer, SET_UART, 1, (1 - orig_uart1));
            system (buffer);
            needs_reboot = 1;
        }

        if (orig_uart2 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uart2_off_rb)))
        {
            sprintf (buffer, SET_UART, 2, (1 - orig_uart2));
            system (buffer);
            needs_reboot = 1;
        }

        if (orig_uart3 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uart3_off_rb)))
        {
            sprintf (buffer, SET_UART, 3, (1 - orig_uart3));
            system (buffer);
            needs_reboot = 1;
        }

        if (orig_uart4 != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uart4_off_rb)))
        {
            sprintf (buffer, SET_UART, 4, (1 - orig_uart4));
            system (buffer);
            needs_reboot = 1;
        }
    }
    gtk_widget_destroy (dlg);
}

/* Locale setting */

static void country_changed (GtkComboBox *cb, gpointer ptr)
{
    char buffer[1024], cb_lang[64], cb_ctry[64], *cb_ext, init_char[32], *cptr;
    FILE *fp;

    // clear the combo box
    while (char_count--) gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (locchar_cb), 0);
    char_count = 0;

    // if an initial setting is supplied at ptr...
    if (ptr)
    {
        // find the line in SUPPORTED that exactly matches the supplied country string
        sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", ptr);
        fp = popen (buffer, "r");
        if (fp == NULL) return;
        while (fgets (buffer, sizeof (buffer) - 1, fp))
        {
            // copy the current character code into cur_char
            strtok (buffer, " ");
            cptr = strtok (NULL, " \n\r");
            strcpy (init_char, cptr);
        }
        pclose (fp);
    }
    else init_char[0] = 0;

    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
    if (cptr)
    {
        strcpy (cb_lang, cptr);
        strtok (cb_lang, " ");
    }
    else cb_lang[0] = 0;

    // read the country from the combo box and split off code and extension
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loccount_cb));
    if (cptr)
    {
        strcpy (cb_ctry, cptr);
        strtok (cb_ctry, "@ ");
        cb_ext = strtok (NULL, "@ ");
        if (cb_ext[0] == '(') cb_ext[0] = 0;
    }
    else cb_ctry[0] = 0;

    // build the grep expression to search the file of supported formats
    if (!cb_ctry[0])
        sprintf (buffer, "grep %s /usr/share/i18n/SUPPORTED", cb_lang);
    else if (!cb_ext[0])
        sprintf (buffer, "grep %s_%s /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry);
    else
        sprintf (buffer, "grep -E '%s_%s.*%s' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext);

    // run the grep and parse the returned lines
    fp = popen (buffer, "r");
    if (fp == NULL) return;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
        // find the second part of the returned line (separated by a space) and add to combo box
        strtok (buffer, " ");
        cptr = strtok (NULL, " \n\r");
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (locchar_cb), cptr);

        // check to see if it matches the initial string and set active if so
        if (!strcmp (cptr, init_char)) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), char_count);
        char_count++;
    }
    pclose (fp);

    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (locchar_cb), 0);
}

static void language_changed (GtkComboBox *cb, gpointer ptr)
{
    struct dirent **filelist, *dp;
    char buffer[1024], result[128], cb_lang[64], init_ctry[32], file_lang[8], file_ctry[64], *cptr;
    int entries, entry;

    // clear the combo box
    while (country_count--) gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (loccount_cb), 0);
    country_count = 0;
    
    // if an initial setting is supplied at ptr, extract the country code from the supplied string
    if (ptr) get_country (ptr, init_ctry);
    else init_ctry[0] = 0;
    
    // read the language from the combo box and split off the code into lang
    cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
    if (cptr)
    {
        strcpy (cb_lang, cptr);
        strtok (cb_lang, " ");
    }
    else cb_lang[0] = 0;

    // loop through locale files
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language and country codes from the locale file name
        get_language (dp->d_name, file_lang);
        get_country (dp->d_name, file_ctry);

        // if the country code from the filename is valid,
        // and the language code from the filename matches the one in the combo box...
        if (*file_ctry && !strcmp (cb_lang, file_lang))
        {
            // read the territory description from the file
            get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "territory", result);

            // add country code and description to combo box
            sprintf (buffer, "%s (%s)", file_ctry, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loccount_cb), buffer);

            // check to see if it matches the initial string and set active if so
            if (!strcmp (file_ctry, init_ctry)) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), country_count);
            country_count++;
        }
        free (dp);
    }
    free (filelist);
    
    // set the first entry active if not initialising from file
    if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (loccount_cb), 0);
    g_signal_connect (loccount_cb, "changed", G_CALLBACK (country_changed), NULL);
}

static gboolean close_msg (gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (msg_dlg));
    return FALSE;
}

static gpointer locale_thread (gpointer data)
{
    char buffer[256];

    system ("sudo locale-gen");
    sprintf (buffer, "sudo update-locale LANG=%s", glocale);
    system (buffer);
    g_idle_add (close_msg, NULL);
    return NULL;
}

static void on_set_locale (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    struct dirent **filelist, *dp;
    char buffer[1024], result[128], init_locale[64], file_lang[8], last_lang[8], init_lang[8], *cptr;
    int count, entries, entry;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "localedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "loctable");
    loclang_cb = (GObject *) gtk_combo_box_text_new ();
    loccount_cb = (GObject *) gtk_combo_box_text_new ();
    locchar_cb = (GObject *) gtk_combo_box_text_new ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (loclang_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (loccount_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (locchar_cb), 1, 2, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (loclang_cb));
    gtk_widget_show_all (GTK_WIDGET (loccount_cb));
    gtk_widget_show_all (GTK_WIDGET (locchar_cb));


    // get the current locale setting and save as init_locale
    FILE *fp = popen ("grep LANG /etc/default/locale", "r");
    if (fp == NULL) return;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
        strtok (buffer, "=");
        cptr = strtok (NULL, "\n\r");
    }
    pclose (fp);
    strcpy (init_locale, cptr);

    // parse the initial locale to get the initial language code
    get_language (init_locale, init_lang);


    // loop through locale files
    last_lang[0] = 0;
    count = 0;
    entries = scandir ("/usr/share/i18n/locales", &filelist, 0, alphasort);
    
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        // get the language code from the locale file name
        get_language (dp->d_name, file_lang);

        // if it differs from the last one read, create a new entry
        if (file_lang[0] && strcmp (file_lang, last_lang))
        {
            // read the language description from the file
            get_quoted_param ("/usr/share/i18n/locales", dp->d_name, "language", result);

            // add language code and description to combo box
            sprintf (buffer, "%s (%s)", file_lang, result);
            gtk_combo_box_append_text (GTK_COMBO_BOX (loclang_cb), buffer);

            // make a local copy of the language code for comparisons
            strcpy (last_lang, file_lang);

            // highlight the current language setting...
            if (!strcmp (file_lang, init_lang)) gtk_combo_box_set_active (GTK_COMBO_BOX (loclang_cb), count);
            count++;
        }
        free (dp);
    }
    free (filelist);

    // populate the country and character lists and set the current values
    country_count = char_count = 0;
    language_changed (GTK_COMBO_BOX (loclang_cb), init_locale);
    country_changed (GTK_COMBO_BOX (loclang_cb), init_locale);
    g_signal_connect (loclang_cb, "changed", G_CALLBACK (language_changed), NULL);
    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        char cb_lang[64], cb_ctry[64], *cb_ext;
        // read the language from the combo box and split off the code into lang
        cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loclang_cb));
        if (cptr)
        {
            strcpy (cb_lang, cptr);
            strtok (cb_lang, " ");
        }
        else cb_lang[0] = 0;

        // read the country from the combo box and split off code and extension
        cptr = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (loccount_cb));
        if (cptr)
        {
            strcpy (cb_ctry, cptr);
            strtok (cb_ctry, "@ ");
            cb_ext = strtok (NULL, "@ ");
            if (cb_ext[0] == '(') cb_ext[0] = 0;
        }
        else cb_ctry[0] = 0;

        // build the relevant grep expression to search the file of supported formats
        cptr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (locchar_cb));
        if (cptr)
        {
            if (!cb_ctry[0])
                sprintf (buffer, "grep ^%s.*%s$ /usr/share/i18n/SUPPORTED", cb_lang, cptr);
            else if (!cb_ext[0])
                sprintf (buffer, "grep ^%s_%s.*%s$ /usr/share/i18n/SUPPORTED | grep -v @", cb_lang, cb_ctry, cptr);
            else
                sprintf (buffer, "grep -E '^%s_%s.*%s.*%s$' /usr/share/i18n/SUPPORTED", cb_lang, cb_ctry, cb_ext, cptr);

            // run the grep and parse the returned line
            fp = popen (buffer, "r");
            if (fp != NULL)
            {
                fgets (buffer, sizeof (buffer) - 1, fp);
                cptr = strtok (buffer, " ");
                strcpy (glocale, cptr);
                pclose (fp);
            }

            if (glocale[0] && strcmp (glocale, init_locale))
            {
                // look up the current locale setting from init_locale in /etc/locale.gen
                sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", init_locale);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    pclose (fp);
                }

                // use sed to comment that line if uncommented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sudo sed -i 's/^%s/# %s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // look up the new locale setting from glocale in /etc/locale.gen
                sprintf (buffer, "grep '%s ' /usr/share/i18n/SUPPORTED", glocale);
                fp = popen (buffer, "r");
                if (fp != NULL)
                {
                    fgets (cb_lang, sizeof (cb_lang) - 1, fp);
                    strtok (cb_lang, "\n\r");
                    pclose (fp);
                }

                // use sed to uncomment that line if commented
                if (cb_lang[0])
                {
                    sprintf (buffer, "sudo sed -i 's/^# %s/%s/g' /etc/locale.gen", cb_lang, cb_lang);
                    system (buffer);
                }

                // warn about a short delay...
                msg_dlg = (GtkWidget *) gtk_dialog_new ();
                gtk_window_set_title (GTK_WINDOW (msg_dlg), "");
                gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);
                gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_skip_taskbar_hint (GTK_WINDOW (msg_dlg), TRUE);
                gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
                GtkWidget *frame = gtk_frame_new (NULL);
                GtkWidget *label = (GtkWidget *) gtk_label_new (_("Setting locale - please wait..."));
                gtk_misc_set_padding (GTK_MISC (label), 20, 20);
                gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (msg_dlg))), frame);
                gtk_container_add (GTK_CONTAINER (frame), label);
                gtk_widget_show_all (msg_dlg);

                // launch a thread with the system call to update the generated locales
                g_thread_new (NULL, locale_thread, NULL);

                // set reboot flag
                needs_reboot = 1;
            }
        }
    }

    gtk_widget_destroy (dlg);
}

/* Timezone setting */

int dirfilter (const struct dirent *entry)
{
    if (entry->d_name[0] != '.') return 1;
    return 0;
}

static void area_changed (GtkComboBox *cb, gpointer ptr)
{
    char buffer[128];
    //DIR *dirp, *sdirp;
    struct dirent **filelist, *dp, **sfilelist, *sdp;
    struct stat st_buf;
    int entries, entry, sentries, sentry;

    while (loc_count--) gtk_combo_box_remove_text (GTK_COMBO_BOX (tzloc_cb), 0);
    loc_count = 0;

    sprintf (buffer, "/usr/share/zoneinfo/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));
    stat (buffer, &st_buf);

    if (S_ISDIR (st_buf.st_mode))
    {
        entries = scandir (buffer, &filelist, dirfilter, alphasort);
        for (entry = 0; entry < entries; entry++)
        {
            dp = filelist[entry];
            if (dp->d_type == DT_DIR)
            {
                sprintf (buffer, "/usr/share/zoneinfo/%s/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)), dp->d_name);
                sentries = scandir (buffer, &sfilelist, dirfilter, alphasort);
                for (sentry = 0; sentry < sentries; sentry++)
                {
                    sdp = sfilelist[sentry];
                    sprintf (buffer, "%s/%s", dp->d_name, sdp->d_name);
                    gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), buffer);
                    if (ptr && !strcmp (ptr, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                    loc_count++;
                    free (sdp);
                }
                free (sfilelist);
            }
            else
            {
                gtk_combo_box_append_text (GTK_COMBO_BOX (tzloc_cb), dp->d_name);
                if (ptr && !strcmp (ptr, dp->d_name)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), loc_count);
                loc_count++;
            }
            free (dp);
        }
        free (filelist);
        if (!ptr) gtk_combo_box_set_active (GTK_COMBO_BOX (tzloc_cb), 0);
    }
}

static gpointer timezone_thread (gpointer data)
{
    system ("sudo rm /etc/localtime");
    system ("sudo dpkg-reconfigure --frontend noninteractive tzdata");
    g_idle_add (close_msg, NULL);
    return NULL;
}

int tzfilter (const struct dirent *entry)
{
    if (entry->d_name[0] >= 'A' && entry->d_name[0] <= 'Z') return 1;
    return 0;
}

static void on_set_timezone (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128], before[128], *cptr;
    struct dirent **filelist, *dp;
    int entries, entry;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "tzdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "tztable");
    tzarea_cb = (GObject *) gtk_combo_box_new_text ();
    tzloc_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzarea_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (tzloc_cb), 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (tzarea_cb));
    gtk_widget_show_all (GTK_WIDGET (tzloc_cb));

    // select the current timezone area
    get_string ("cat /etc/timezone | tr -d \" \t\n\r\"", buffer);
    strcpy (before, buffer);
    strtok (buffer, "/");
    cptr = strtok (NULL, "");

    // populate the area combo box from the timezone database
    loc_count = 0;
    int count = 0;
    entries = scandir ("/usr/share/zoneinfo", &filelist, tzfilter, alphasort);
    for (entry = 0; entry < entries; entry++)
    {
        dp = filelist[entry];
        gtk_combo_box_append_text (GTK_COMBO_BOX (tzarea_cb), dp->d_name);
        if (!strcmp (dp->d_name, buffer)) gtk_combo_box_set_active (GTK_COMBO_BOX (tzarea_cb), count);
        count++;
        free (dp);
    }
    free (filelist);
    g_signal_connect (tzarea_cb, "changed", G_CALLBACK (area_changed), NULL);

    // populate the location list and set the current location
    area_changed (GTK_COMBO_BOX (tzarea_cb), cptr);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
            sprintf (buffer, "%s/%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
                gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
        else
            sprintf (buffer, "%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));

        if (strcmp (before, buffer))
        {
            if (gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)))
                sprintf (buffer, "echo '%s/%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)),
                    gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzloc_cb)));
            else
                sprintf (buffer, "echo '%s' | sudo tee /etc/timezone", gtk_combo_box_get_active_text (GTK_COMBO_BOX (tzarea_cb)));
            system (buffer);

            // warn about a short delay...
            msg_dlg = (GtkWidget *) gtk_dialog_new ();
            gtk_window_set_title (GTK_WINDOW (msg_dlg), "");
            gtk_window_set_modal (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);
            gtk_window_set_destroy_with_parent (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_skip_taskbar_hint (GTK_WINDOW (msg_dlg), TRUE);
            gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));
            GtkWidget *frame = gtk_frame_new (NULL);
            GtkWidget *label = (GtkWidget *) gtk_label_new (_("Setting timezone - please wait..."));
            gtk_misc_set_padding (GTK_MISC (label), 20, 20);
            gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (msg_dlg))), frame);
            gtk_container_add (GTK_CONTAINER (frame), label);
            gtk_widget_show_all (msg_dlg);

            // launch a thread with the system call to update the timezone
            g_thread_new (NULL, timezone_thread, NULL);
        }
    }
    gtk_widget_destroy (dlg);
}

/* Wifi country setting */

static void on_set_wifi (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128], cnow[16], *cptr;
    FILE *fp;
    int n, found;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "wcdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    GtkWidget *table = (GtkWidget *) gtk_builder_get_object (builder, "wctable");
    wccountry_cb = (GObject *) gtk_combo_box_new_text ();
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (wccountry_cb), 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (GTK_WIDGET (wccountry_cb));

    // get the current country setting
    get_string (GET_WIFI_CTRY, cnow);

    // populate the combobox
    fp = fopen ("/usr/share/zoneinfo/iso3166.tab", "rb");
    n = 0;
    found = 0;
    while (fgets (buffer, sizeof (buffer) - 1, fp))
    {
        if (buffer[0] != 0x0A && buffer[0] != '#')
        {
            buffer[strlen(buffer) - 1] = 0;
            gtk_combo_box_append_text (GTK_COMBO_BOX (wccountry_cb), buffer);
            if (!strncmp (cnow, buffer, 2)) found = n;
            n++;
        }
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (wccountry_cb), found);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        // update the wpa_supplicant.conf file
        sprintf (buffer, "%s", gtk_combo_box_get_active_text (GTK_COMBO_BOX (wccountry_cb)));
        if (strncmp (cnow, buffer, 2))
        {
            strncpy (cnow, buffer, 2);
            cnow[2] = 0;
            sprintf (buffer, SET_WIFI_CTRY, cnow);
            system (buffer);
            needs_reboot = 1;
        }
    }
    gtk_widget_destroy (dlg);
}

/* Resolution setting */

static void on_set_res (GtkButton* btn, gpointer ptr)
{    
	system ("lxrandr");
}

static void set_cus_res (GtkEntry *entry, gpointer ptr)
{
    if ( strlen (gtk_entry_get_text (GTK_ENTRY (cusresentry2_tb))) == 0
        || strlen (gtk_entry_get_text (GTK_ENTRY (cusresentry3_tb))) == 0
        || strlen (gtk_entry_get_text (GTK_ENTRY (cusresentry4_tb))) == 0 )
        gtk_widget_set_sensitive (GTK_WIDGET (cusresok_btn), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (cusresok_btn), TRUE);
}

static void on_set_cus_res (GtkButton* btn, gpointer ptr)
{
    GtkBuilder *builder;
    GtkWidget *dlg;
    char buffer[128];

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "cusresdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));

    cusresentry2_tb = gtk_builder_get_object (builder, "cusresentry2");
    cusresentry3_tb = gtk_builder_get_object (builder, "cusresentry3");
    cusresentry4_tb = gtk_builder_get_object (builder, "cusresentry4");
    gtk_entry_set_visibility (GTK_ENTRY (cusresentry2_tb), TRUE);
    gtk_entry_set_visibility (GTK_ENTRY (cusresentry3_tb), TRUE);
    gtk_entry_set_visibility (GTK_ENTRY (cusresentry4_tb), TRUE);
    cusresok_btn = gtk_builder_get_object (builder, "cusresok");
    gtk_widget_set_sensitive (GTK_WIDGET (cusresok_btn), FALSE);
    g_signal_connect (cusresentry2_tb, "changed", G_CALLBACK (set_cus_res), NULL);
    g_signal_connect (cusresentry3_tb, "changed", G_CALLBACK (set_cus_res), NULL);
    g_signal_connect (cusresentry4_tb, "changed", G_CALLBACK (set_cus_res), NULL);

    g_object_unref (builder);
    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
        sprintf (buffer, SET_CUS_RES
            , atoi (gtk_entry_get_text (GTK_ENTRY (cusresentry2_tb)))
            , atoi (gtk_entry_get_text (GTK_ENTRY (cusresentry3_tb)))
            , atoi (gtk_entry_get_text (GTK_ENTRY (cusresentry4_tb))));
        system (buffer);
    }
    gtk_widget_destroy (dlg);
}

/* Button handlers */

static void on_expand_fs (GtkButton* btn, gpointer ptr)
{
    system (EXPAND_FS);
    needs_reboot = 1;

    GtkBuilder *builder;
    GtkWidget *dlg;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "fsdonedlg");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    g_object_unref (builder);
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}

static void on_set_keyboard (GtkButton* btn, gpointer ptr)
{
    system ("xkeycaps");
}

static void on_boot_cli (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_off_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_off_rb), FALSE);
    }
}

static void on_boot_gui (GtkButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (splash_on_rb), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (splash_off_rb), TRUE);
    }
}

/* Write the changes to the system when OK is pressed */

static int process_changes (void)
{
    char buffer[128];
    int reboot = 0;

    if (orig_boot != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb)) 
        || orig_autolog == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (autologin_cb)))
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) system (SET_BOOT_GUIA);
            else system (SET_BOOT_CLIA);
        }
        else
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (boot_desktop_rb))) system (SET_BOOT_GUI);
            else system (SET_BOOT_CLI);
        }
        reboot = 1;
    }
    
    if (orig_netwait == gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (netwait_cb)))
    {
        sprintf (buffer, SET_BOOT_WAIT, (1 - orig_netwait));
        system (buffer);
        reboot = 1;
    }

    if (orig_splash != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (splash_off_rb)))
    {
        sprintf (buffer, SET_SPLASH, (1 - orig_splash));
        system (buffer);
    }

    if (orig_ssh != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_off_rb)))
    {
        sprintf (buffer, SET_SSH, (1 - orig_ssh));
        system (buffer);
    }

    if (orig_vnc != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vnc_off_rb)))
    {
        sprintf (buffer, SET_VNC, (1 - orig_vnc));
        system (buffer);
    }

    if (orig_serial != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (serial_off_rb)))
    {
        sprintf (buffer, SET_SERIAL, (1 - orig_serial));
        system (buffer);
        reboot = 1;
    }

    if (orig_onewire != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (onewire_off_rb)))
    {
        sprintf (buffer, SET_1WIRE, (1 - orig_onewire));
        system (buffer);
        reboot = 1;
    }

    if (orig_rgpio != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rgpio_off_rb)))
    {
        sprintf (buffer, SET_RGPIO, (1 - orig_rgpio));
        system (buffer);
    }

    if (strcmp (orig_hostname, gtk_entry_get_text (GTK_ENTRY (hostname_tb))))
    {
        sprintf (buffer, SET_HOSTNAME, gtk_entry_get_text (GTK_ENTRY (hostname_tb)));
        system (buffer);
        reboot = 1;
    }

    return reboot;
}


/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GObject *item;
    GtkWidget *dlg;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    // GTK setup
    gdk_threads_init ();
    gdk_threads_enter ();
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "tc_gui.ui", NULL);

    // check sudo privilege
    get_string ("sudo whoami", username);
    if ( strcmp(username, "root") != 0 )
    {
        dlg = (GtkWidget *) gtk_builder_get_object (builder, "errordialog");
        gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
        g_object_unref (builder);
        gtk_dialog_run (GTK_DIALOG (dlg));
        gtk_widget_destroy (dlg);
        return 0;
    }

    // Set the UI status
    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");

    passwd_btn = gtk_builder_get_object (builder, "button_pw");
    g_signal_connect (passwd_btn, "clicked", G_CALLBACK (on_change_passwd), NULL);

    vncpasswd_btn = gtk_builder_get_object (builder, "button_vnc_pw");
    g_signal_connect (vncpasswd_btn, "clicked", G_CALLBACK (on_change_vnc_passwd), NULL);

    locale_btn = gtk_builder_get_object (builder, "button_loc");
    g_signal_connect (locale_btn, "clicked", G_CALLBACK (on_set_locale), NULL);

    timezone_btn = gtk_builder_get_object (builder, "button_tz");
    g_signal_connect (timezone_btn, "clicked", G_CALLBACK (on_set_timezone), NULL);

    keyboard_btn = gtk_builder_get_object (builder, "button_kb");
    g_signal_connect (keyboard_btn, "clicked", G_CALLBACK (on_set_keyboard), NULL);

    wifi_btn = gtk_builder_get_object (builder, "button_wifi");
    g_signal_connect (wifi_btn, "clicked", G_CALLBACK (on_set_wifi), NULL);

    splash_on_rb = gtk_builder_get_object (builder, "rb_splash_on");
    splash_off_rb = gtk_builder_get_object (builder, "rb_splash_off");
    if (orig_splash = get_status (GET_SPLASH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (splash_on_rb), TRUE);

    boot_desktop_rb = gtk_builder_get_object (builder, "rb_desktop");
    g_signal_connect (boot_desktop_rb, "toggled", G_CALLBACK (on_boot_gui), NULL);
    boot_cli_rb = gtk_builder_get_object (builder, "rb_cli");
    g_signal_connect (boot_cli_rb, "toggled", G_CALLBACK (on_boot_cli), NULL);
    if (orig_boot = get_status (GET_BOOT_CLI)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_desktop_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (boot_cli_rb), TRUE);

    autologin_cb = gtk_builder_get_object (builder, "cb_login");
    if (orig_autolog = get_status (GET_AUTOLOGIN)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), FALSE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autologin_cb), TRUE);

    netwait_cb = gtk_builder_get_object (builder, "cb_network");
    if (orig_netwait = get_status (GET_BOOT_WAIT)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), FALSE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (netwait_cb), TRUE);

    hostname_tb = gtk_builder_get_object (builder, "entry_hn");
    get_string (GET_HOSTNAME, orig_hostname);
    gtk_entry_set_text (GTK_ENTRY (hostname_tb), orig_hostname);

    res_btn = gtk_builder_get_object (builder, "button_res");
    g_signal_connect (res_btn, "clicked", G_CALLBACK (on_set_res), NULL);

    cus_res_btn = gtk_builder_get_object (builder, "button_cus_res");
    g_signal_connect (cus_res_btn, "clicked", G_CALLBACK (on_set_cus_res), NULL);

    ssh_on_rb = gtk_builder_get_object (builder, "rb_ssh_on");
    ssh_off_rb = gtk_builder_get_object (builder, "rb_ssh_off");
    if (orig_ssh = get_status (GET_SSH)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssh_on_rb), TRUE);

    vnc_on_rb = gtk_builder_get_object (builder, "rb_vnc_on");
    vnc_off_rb = gtk_builder_get_object (builder, "rb_vnc_off");
    if (orig_vnc = get_status (GET_VNC)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vnc_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vnc_on_rb), TRUE);

    spi_btn = gtk_builder_get_object (builder, "button_spi");
    g_signal_connect (spi_btn, "clicked", G_CALLBACK (on_set_spi), NULL);

    i2c_btn = gtk_builder_get_object (builder, "button_i2c");
    g_signal_connect (i2c_btn, "clicked", G_CALLBACK (on_set_i2c), NULL);

    uart_btn = gtk_builder_get_object (builder, "button_uart");
    g_signal_connect (uart_btn, "clicked", G_CALLBACK (on_set_uart), NULL);

    serial_on_rb = gtk_builder_get_object (builder, "rb_ser_on");
    serial_off_rb = gtk_builder_get_object (builder, "rb_ser_off");
    if (orig_serial = (get_status (GET_SERIAL) | get_status (GET_SERIALHW))) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_on_rb), TRUE);

    onewire_on_rb = gtk_builder_get_object (builder, "rb_one_on");
    onewire_off_rb = gtk_builder_get_object (builder, "rb_one_off");
    if (orig_onewire = get_status (GET_1WIRE)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_off_rb), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (onewire_on_rb), TRUE);

    rgpio_on_rb = gtk_builder_get_object (builder, "rb_rgp_on");
    rgpio_off_rb = gtk_builder_get_object (builder, "rb_rgp_off");
    if ( ! get_status (CHECK_RGPIO) )
    {
        if (orig_rgpio = get_status (GET_RGPIO)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgpio_off_rb), TRUE);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgpio_on_rb), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgpio_off_rb), TRUE);
        orig_rgpio = 1;
        gtk_widget_set_sensitive (GTK_WIDGET (rgpio_on_rb), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (rgpio_off_rb), FALSE);
    }

    // disable the buttons if VNC isn't installed
    gboolean enable = TRUE;
    struct stat buf;
    if (stat ("/usr/share/doc/tightvncserver", &buf)) enable = FALSE;
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_on_rb), enable);
    gtk_widget_set_sensitive (GTK_WIDGET (vnc_off_rb), enable);

    // show message page
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "msgdialog");
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);

    needs_reboot = 0;

    if (gtk_dialog_run (GTK_DIALOG (main_dlg)) == GTK_RESPONSE_OK)
    {
        // check vnc password exist
        if( gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vnc_on_rb)) && system(VNC_PASSWD_EXIST) )
        {
            on_change_vnc_passwd( NULL, NULL );
        }
        if ( process_changes() ) needs_reboot = 1;
    }
    if (needs_reboot)
    {
        dlg = (GtkWidget *) gtk_builder_get_object (builder, "rebootdlg");
        gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main_dlg));
        if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES)
        {
            system ("sudo reboot");
        }
        gtk_widget_destroy (dlg);
    }

    g_object_unref (builder);
    gtk_widget_destroy (main_dlg);
    gdk_threads_leave ();

    return 0;
}
