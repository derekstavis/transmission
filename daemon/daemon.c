/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: daemon.c 14084 2013-06-07 23:31:26Z jordan $
 */

#include <errno.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* exit, atoi */

#include <fcntl.h> /* open */
#include <signal.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif
#include <unistd.h> /* daemon */

#include <event2/buffer.h>

#include <libtransmission/transmission.h>
#include <libtransmission/tr-getopt.h>
#include <libtransmission/log.h>
#include <libtransmission/utils.h>
#include <libtransmission/variant.h>
#include <libtransmission/version.h>

#ifdef USE_SYSTEMD_DAEMON
 #include <systemd/sd-daemon.h>
#else
 static void sd_notify (int status UNUSED, const char * str UNUSED) { }
 static void sd_notifyf (int status UNUSED, const char * fmt UNUSED, ...) { }
#endif

#include "watch.h"

#define MY_NAME "transmission-daemon"

#define MEM_K 1024
#define MEM_K_STR "KiB"
#define MEM_M_STR "MiB"
#define MEM_G_STR "GiB"
#define MEM_T_STR "TiB"

#define DISK_K 1000
#define DISK_B_STR  "B"
#define DISK_K_STR "kB"
#define DISK_M_STR "MB"
#define DISK_G_STR "GB"
#define DISK_T_STR "TB"

#define SPEED_K 1000
#define SPEED_B_STR  "B/s"
#define SPEED_K_STR "kB/s"
#define SPEED_M_STR "MB/s"
#define SPEED_G_STR "GB/s"
#define SPEED_T_STR "TB/s"

#define LOGFILE_MODE_STR "a+"

static bool paused = false;
static bool closing = false;
static bool seenHUP = false;
static const char *logfileName = NULL;
static FILE *logfile = NULL;
static tr_session * mySession = NULL;
static tr_quark key_pidfile = 0;

/***
****  Config File
***/

static const char *
getUsage (void)
{
    return "Transmission " LONG_VERSION_STRING
           "  http://www.transmissionbt.com/\n"
           "A fast and easy BitTorrent client\n"
           "\n"
           MY_NAME " is a headless Transmission session\n"
           "that can be controlled via transmission-remote\n"
           "or the web interface.\n"
           "\n"
           "Usage: " MY_NAME " [options]";
}

static const struct tr_option options[] =
{

    { 'a', "allowed", "Allowed IP addresses. (Default: " TR_DEFAULT_RPC_WHITELIST ")", "a", 1, "<list>" },
    { 'b', "blocklist", "Enable peer blocklists", "b", 0, NULL },
    { 'B', "no-blocklist", "Disable peer blocklists", "B", 0, NULL },
    { 'c', "watch-dir", "Where to watch for new .torrent files", "c", 1, "<directory>" },
    { 'C', "no-watch-dir", "Disable the watch-dir", "C", 0, NULL },
    { 941, "incomplete-dir", "Where to store new torrents until they're complete", NULL, 1, "<directory>" },
    { 942, "no-incomplete-dir", "Don't store incomplete torrents in a different location", NULL, 0, NULL },
    { 'd', "dump-settings", "Dump the settings and exit", "d", 0, NULL },
    { 'e', "logfile", "Dump the log messages to this filename", "e", 1, "<filename>" },
    { 'f', "foreground", "Run in the foreground instead of daemonizing", "f", 0, NULL },
    { 'g', "config-dir", "Where to look for configuration files", "g", 1, "<path>" },
    { 'p', "port", "RPC port (Default: " TR_DEFAULT_RPC_PORT_STR ")", "p", 1, "<port>" },
    { 't', "auth", "Require authentication", "t", 0, NULL },
    { 'T', "no-auth", "Don't require authentication", "T", 0, NULL },
    { 'u', "username", "Set username for authentication", "u", 1, "<username>" },
    { 'v', "password", "Set password for authentication", "v", 1, "<password>" },
    { 'V', "version", "Show version number and exit", "V", 0, NULL },
    { 810, "log-error", "Show error messages", NULL, 0, NULL },
    { 811, "log-info", "Show error and info messages", NULL, 0, NULL },
    { 812, "log-debug", "Show error, info, and debug messages", NULL, 0, NULL },
    { 'w', "download-dir", "Where to save downloaded data", "w", 1, "<path>" },
    { 800, "paused", "Pause all torrents on startup", NULL, 0, NULL },
    { 'o', "dht", "Enable distributed hash tables (DHT)", "o", 0, NULL },
    { 'O', "no-dht", "Disable distributed hash tables (DHT)", "O", 0, NULL },
    { 'y', "lpd", "Enable local peer discovery (LPD)", "y", 0, NULL },
    { 'Y', "no-lpd", "Disable local peer discovery (LPD)", "Y", 0, NULL },
    { 830, "utp", "Enable uTP for peer connections", NULL, 0, NULL },
    { 831, "no-utp", "Disable uTP for peer connections", NULL, 0, NULL },
    { 'P', "peerport", "Port for incoming peers (Default: " TR_DEFAULT_PEER_PORT_STR ")", "P", 1, "<port>" },
    { 'm', "portmap", "Enable portmapping via NAT-PMP or UPnP", "m", 0, NULL },
    { 'M', "no-portmap", "Disable portmapping", "M", 0, NULL },
    { 'L', "peerlimit-global", "Maximum overall number of peers (Default: " TR_DEFAULT_PEER_LIMIT_GLOBAL_STR ")", "L", 1, "<limit>" },
    { 'l', "peerlimit-torrent", "Maximum number of peers per torrent (Default: " TR_DEFAULT_PEER_LIMIT_TORRENT_STR ")", "l", 1, "<limit>" },
    { 910, "encryption-required",  "Encrypt all peer connections", "er", 0, NULL },
    { 911, "encryption-preferred", "Prefer encrypted peer connections", "ep", 0, NULL },
    { 912, "encryption-tolerated", "Prefer unencrypted peer connections", "et", 0, NULL },
    { 'i', "bind-address-ipv4", "Where to listen for peer connections", "i", 1, "<ipv4 addr>" },
    { 'I', "bind-address-ipv6", "Where to listen for peer connections", "I", 1, "<ipv6 addr>" },
    { 'r', "rpc-bind-address", "Where to listen for RPC connections", "r", 1, "<ipv4 addr>" },
    { 953, "global-seedratio", "All torrents, unless overridden by a per-torrent setting, should seed until a specific ratio", "gsr", 1, "ratio" },
    { 954, "no-global-seedratio", "All torrents, unless overridden by a per-torrent setting, should seed regardless of ratio", "GSR", 0, NULL },
    { 'x', "pid-file", "Enable PID file", "x", 1, "<pid-file>" },
    { 0, NULL, NULL, NULL, 0, NULL }
};

static void
showUsage (void)
{
    tr_getopt_usage (MY_NAME, getUsage (), options);
    exit (0);
}

static void
gotsig (int sig)
{
    switch (sig)
    {
        case SIGHUP:
        {
            if (!mySession)
            {
                tr_logAddInfo ("Deferring reload until session is fully started.");
                seenHUP = true;
            }
            else
            {
                tr_variant settings;
                const char * configDir;

                /* reopen the logfile to allow for log rotation */
                if (logfileName) {
                    logfile = freopen (logfileName, LOGFILE_MODE_STR, logfile);
                    if (!logfile)
                        fprintf (stderr, "Couldn't reopen \"%s\": %s\n", logfileName, tr_strerror (errno));
                }

                configDir = tr_sessionGetConfigDir (mySession);
                tr_logAddInfo ("Reloading settings from \"%s\"", configDir);
                tr_variantInitDict (&settings, 0);
                tr_variantDictAddBool (&settings, TR_KEY_rpc_enabled, true);
                tr_sessionLoadSettings (&settings, configDir, MY_NAME);
                tr_sessionSet (mySession, &settings);
                tr_variantFree (&settings);
                tr_sessionReloadBlocklists (mySession);
            }
            break;
        }

        default:
            tr_logAddError ("Unexpected signal (%d) in daemon, closing.", sig);
            /* no break */

        case SIGINT:
        case SIGTERM:
            closing = true;
            break;
    }
}

#if defined (WIN32)
 #define USE_NO_DAEMON
#elif !defined (HAVE_DAEMON) || defined (__UCLIBC__)
 #define USE_TR_DAEMON
#else
 #define USE_OS_DAEMON
#endif

static int
tr_daemon (int nochdir, int noclose)
{
#if defined (USE_OS_DAEMON)

    return daemon (nochdir, noclose);

#elif defined (USE_TR_DAEMON)

    /* this is loosely based off of glibc's daemon () implementation
     * http://sourceware.org/git/?p=glibc.git;a=blob_plain;f=misc/daemon.c */

    switch (fork ()) {
        case -1: return -1;
        case 0: break;
        default: _exit (0);
    }

    if (setsid () == -1)
        return -1;

    if (!nochdir)
        chdir ("/");

    if (!noclose) {
        int fd = open ("/dev/null", O_RDWR, 0);
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        close (fd);
    }

    return 0;

#else /* USE_NO_DAEMON */
    return 0;
#endif
}

static const char*
getConfigDir (int argc, const char ** argv)
{
    int c;
    const char * configDir = NULL;
    const char * optarg;
    const int ind = tr_optind;

    while ((c = tr_getopt (getUsage (), argc, argv, options, &optarg))) {
        if (c == 'g') {
            configDir = optarg;
            break;
        }
    }

    tr_optind = ind;

    if (configDir == NULL)
        configDir = tr_getDefaultConfigDir (MY_NAME);

    return configDir;
}

static void
onFileAdded (tr_session * session, const char * dir, const char * file)
{
    char * filename = tr_buildPath (dir, file, NULL);
    tr_ctor * ctor = tr_ctorNew (session);
    int err = tr_ctorSetMetainfoFromFile (ctor, filename);

    if (!err)
    {
        tr_torrentNew (ctor, &err, NULL);

        if (err == TR_PARSE_ERR)
            tr_logAddError ("Error parsing .torrent file \"%s\"", file);
        else
        {
            bool trash = false;
            int test = tr_ctorGetDeleteSource (ctor, &trash);

            tr_logAddInfo ("Parsing .torrent file successful \"%s\"", file);

            if (!test && trash)
            {
                tr_logAddInfo ("Deleting input .torrent file \"%s\"", file);
                if (tr_remove (filename))
                    tr_logAddError ("Error deleting .torrent file: %s", tr_strerror (errno));
            }
            else
            {
                char * new_filename = tr_strdup_printf ("%s.added", filename);
                tr_rename (filename, new_filename);
                tr_free (new_filename);
            }
        }
    }

    tr_ctorFree (ctor);
    tr_free (filename);
}

static void
printMessage (FILE * logfile, int level, const char * name, const char * message, const char * file, int line)
{
    if (logfile != NULL)
    {
        char timestr[64];
        tr_logGetTimeStr (timestr, sizeof (timestr));
        if (name)
            fprintf (logfile, "[%s] %s %s (%s:%d)\n", timestr, name, message, file, line);
        else
            fprintf (logfile, "[%s] %s (%s:%d)\n", timestr, message, file, line);
    }
#ifdef HAVE_SYSLOG
    else /* daemon... write to syslog */
    {
        int priority;

        /* figure out the syslog priority */
        switch (level) {
            case TR_LOG_ERROR: priority = LOG_ERR; break;
            case TR_LOG_DEBUG: priority = LOG_DEBUG; break;
            default:           priority = LOG_INFO; break;
        }

        if (name)
            syslog (priority, "%s %s (%s:%d)", name, message, file, line);
        else
            syslog (priority, "%s (%s:%d)", message, file, line);
    }
#endif
}

static void
pumpLogMessages (FILE * logfile)
{
    const tr_log_message * l;
    tr_log_message * list = tr_logGetQueue ();

    for (l=list; l!=NULL; l=l->next)
        printMessage (logfile, l->level, l->name, l->message, l->file, l->line);

    if (logfile != NULL)
        fflush (logfile);

    tr_logFreeQueue (list);
}

static tr_rpc_callback_status
on_rpc_callback (tr_session            * session UNUSED,
                 tr_rpc_callback_type    type,
                 struct tr_torrent     * tor UNUSED,
                 void                  * user_data UNUSED)
{
    if (type == TR_RPC_SESSION_CLOSE)
        closing = true;
    return TR_RPC_OK;
}

int
main (int argc, char ** argv)
{
    int c;
    const char * optarg;
    tr_variant settings;
    bool boolVal;
    bool loaded;
    bool foreground = false;
    bool dumpSettings = false;
    const char * configDir = NULL;
    const char * pid_filename;
    dtr_watchdir * watchdir = NULL;
    bool pidfile_created = false;
    tr_session * session = NULL;

    key_pidfile = tr_quark_new ("pidfile",  7);

    signal (SIGINT, gotsig);
    signal (SIGTERM, gotsig);
#ifndef WIN32
    signal (SIGHUP, gotsig);
#endif

    /* load settings from defaults + config file */
    tr_variantInitDict (&settings, 0);
    tr_variantDictAddBool (&settings, TR_KEY_rpc_enabled, true);
    configDir = getConfigDir (argc, (const char**)argv);
    loaded = tr_sessionLoadSettings (&settings, configDir, MY_NAME);

    /* overwrite settings from the comamndline */
    tr_optind = 1;
    while ((c = tr_getopt (getUsage (), argc, (const char**)argv, options, &optarg))) {
        switch (c) {
            case 'a': tr_variantDictAddStr  (&settings, TR_KEY_rpc_whitelist, optarg);
                      tr_variantDictAddBool (&settings, TR_KEY_rpc_whitelist_enabled, true);
                      break;
            case 'b': tr_variantDictAddBool (&settings, TR_KEY_blocklist_enabled, true);
                      break;
            case 'B': tr_variantDictAddBool (&settings, TR_KEY_blocklist_enabled, false);
                      break;
            case 'c': tr_variantDictAddStr  (&settings, TR_KEY_watch_dir, optarg);
                      tr_variantDictAddBool (&settings, TR_KEY_watch_dir_enabled, true);
                      break;
            case 'C': tr_variantDictAddBool (&settings, TR_KEY_watch_dir_enabled, false);
                      break;
            case 941: tr_variantDictAddStr  (&settings, TR_KEY_incomplete_dir, optarg);
                      tr_variantDictAddBool (&settings, TR_KEY_incomplete_dir_enabled, true);
                      break;
            case 942: tr_variantDictAddBool (&settings, TR_KEY_incomplete_dir_enabled, false);
                      break;
            case 'd': dumpSettings = true;
                      break;
            case 'e': logfile = fopen (optarg, LOGFILE_MODE_STR);
                      if (logfile)
                          logfileName = optarg;
                      else
                          fprintf (stderr, "Couldn't open \"%s\": %s\n", optarg, tr_strerror (errno));
                      break;
            case 'f': foreground = true;
                      break;
            case 'g': /* handled above */
                      break;
            case 'V': /* version */
                      fprintf (stderr, "%s %s\n", MY_NAME, LONG_VERSION_STRING);
                      exit (0);
            case 'o': tr_variantDictAddBool (&settings, TR_KEY_dht_enabled, true);
                      break;
            case 'O': tr_variantDictAddBool (&settings, TR_KEY_dht_enabled, false);
                      break;
            case 'p': tr_variantDictAddInt (&settings, TR_KEY_rpc_port, atoi (optarg));
                      break;
            case 't': tr_variantDictAddBool (&settings, TR_KEY_rpc_authentication_required, true);
                      break;
            case 'T': tr_variantDictAddBool (&settings, TR_KEY_rpc_authentication_required, false);
                      break;
            case 'u': tr_variantDictAddStr (&settings, TR_KEY_rpc_username, optarg);
                      break;
            case 'v': tr_variantDictAddStr (&settings, TR_KEY_rpc_password, optarg);
                      break;
            case 'w': tr_variantDictAddStr (&settings, TR_KEY_download_dir, optarg);
                      break;
            case 'P': tr_variantDictAddInt (&settings, TR_KEY_peer_port, atoi (optarg));
                      break;
            case 'm': tr_variantDictAddBool (&settings, TR_KEY_port_forwarding_enabled, true);
                      break;
            case 'M': tr_variantDictAddBool (&settings, TR_KEY_port_forwarding_enabled, false);
                      break;
            case 'L': tr_variantDictAddInt (&settings, TR_KEY_peer_limit_global, atoi (optarg));
                      break;
            case 'l': tr_variantDictAddInt (&settings, TR_KEY_peer_limit_per_torrent, atoi (optarg));
                      break;
            case 800: paused = true;
                      break;
            case 910: tr_variantDictAddInt (&settings, TR_KEY_encryption, TR_ENCRYPTION_REQUIRED);
                      break;
            case 911: tr_variantDictAddInt (&settings, TR_KEY_encryption, TR_ENCRYPTION_PREFERRED);
                      break;
            case 912: tr_variantDictAddInt (&settings, TR_KEY_encryption, TR_CLEAR_PREFERRED);
                      break;
            case 'i': tr_variantDictAddStr (&settings, TR_KEY_bind_address_ipv4, optarg);
                      break;
            case 'I': tr_variantDictAddStr (&settings, TR_KEY_bind_address_ipv6, optarg);
                      break;
            case 'r': tr_variantDictAddStr (&settings, TR_KEY_rpc_bind_address, optarg);
                      break;
            case 953: tr_variantDictAddReal (&settings, TR_KEY_ratio_limit, atof (optarg));
                      tr_variantDictAddBool (&settings, TR_KEY_ratio_limit_enabled, true);
                      break;
            case 954: tr_variantDictAddBool (&settings, TR_KEY_ratio_limit_enabled, false);
                      break;
            case 'x': tr_variantDictAddStr (&settings, key_pidfile, optarg);
                      break;
            case 'y': tr_variantDictAddBool (&settings, TR_KEY_lpd_enabled, true);
                      break;
            case 'Y': tr_variantDictAddBool (&settings, TR_KEY_lpd_enabled, false);
                      break;
            case 810: tr_variantDictAddInt (&settings,  TR_KEY_message_level, TR_LOG_ERROR);
                      break;
            case 811: tr_variantDictAddInt (&settings,  TR_KEY_message_level, TR_LOG_INFO);
                      break;
            case 812: tr_variantDictAddInt (&settings,  TR_KEY_message_level, TR_LOG_DEBUG);
                      break;
            case 830: tr_variantDictAddBool (&settings, TR_KEY_utp_enabled, true);
                      break;
            case 831: tr_variantDictAddBool (&settings, TR_KEY_utp_enabled, false);
                      break;
            default:  showUsage ();
                      break;
        }
    }

    if (foreground && !logfile)
        logfile = stderr;

    if (!loaded)
    {
        printMessage (logfile, TR_LOG_ERROR, MY_NAME, "Error loading config file -- exiting.", __FILE__, __LINE__);
        return -1;
    }

    if (dumpSettings)
    {
        char * str = tr_variantToStr (&settings, TR_VARIANT_FMT_JSON, NULL);
        fprintf (stderr, "%s", str);
        tr_free (str);
        return 0;
    }

    if (!foreground && tr_daemon (true, false) < 0)
    {
        char buf[256];
        tr_snprintf (buf, sizeof (buf), "Failed to daemonize: %s", tr_strerror (errno));
        printMessage (logfile, TR_LOG_ERROR, MY_NAME, buf, __FILE__, __LINE__);
        exit (1);
    }

    sd_notifyf (0, "MAINPID=%d\n", (int)getpid()); 

    /* start the session */
    tr_formatter_mem_init (MEM_K, MEM_K_STR, MEM_M_STR, MEM_G_STR, MEM_T_STR);
    tr_formatter_size_init (DISK_K, DISK_K_STR, DISK_M_STR, DISK_G_STR, DISK_T_STR);
    tr_formatter_speed_init (SPEED_K, SPEED_K_STR, SPEED_M_STR, SPEED_G_STR, SPEED_T_STR);
    session = tr_sessionInit ("daemon", configDir, true, &settings);
    tr_sessionSetRPCCallback (session, on_rpc_callback, NULL);
    tr_logAddNamedInfo (NULL, "Using settings from \"%s\"", configDir);
    tr_sessionSaveSettings (session, configDir, &settings);

    pid_filename = NULL;
    tr_variantDictFindStr (&settings, key_pidfile, &pid_filename, NULL);
    if (pid_filename && *pid_filename)
    {
        FILE * fp = fopen (pid_filename, "w+");
        if (fp != NULL)
        {
            fprintf (fp, "%d", (int)getpid ());
            fclose (fp);
            tr_logAddInfo ("Saved pidfile \"%s\"", pid_filename);
            pidfile_created = true;
        }
        else
            tr_logAddError ("Unable to save pidfile \"%s\": %s", pid_filename, tr_strerror (errno));
    }

    if (tr_variantDictFindBool (&settings, TR_KEY_rpc_authentication_required, &boolVal) && boolVal)
        tr_logAddNamedInfo (MY_NAME, "requiring authentication");

    mySession = session;

    /* If we got a SIGHUP during startup, process that now. */
    if (seenHUP)
        gotsig (SIGHUP);

    /* maybe add a watchdir */
    {
        const char * dir;

        if (tr_variantDictFindBool (&settings, TR_KEY_watch_dir_enabled, &boolVal)
            && boolVal
            && tr_variantDictFindStr (&settings, TR_KEY_watch_dir, &dir, NULL)
            && dir
            && *dir)
        {
            tr_logAddInfo ("Watching \"%s\" for new .torrent files", dir);
            watchdir = dtr_watchdir_new (mySession, dir, onFileAdded);
        }
    }

    /* load the torrents */
    {
        tr_torrent ** torrents;
        tr_ctor * ctor = tr_ctorNew (mySession);
        if (paused)
            tr_ctorSetPaused (ctor, TR_FORCE, true);
        torrents = tr_sessionLoadTorrents (mySession, ctor, NULL);
        tr_free (torrents);
        tr_ctorFree (ctor);
    }

#ifdef HAVE_SYSLOG
    if (!foreground)
        openlog (MY_NAME, LOG_CONS|LOG_PID, LOG_DAEMON);
#endif

  sd_notify( 0, "READY=1\n" ); 

  while (!closing)
    { 
      double up;
      double dn;

      tr_wait_msec (1000); /* sleep one second */ 

      dtr_watchdir_update (watchdir); 
      pumpLogMessages (logfile); 

      up = tr_sessionGetRawSpeed_KBps (mySession, TR_UP);
      dn = tr_sessionGetRawSpeed_KBps (mySession, TR_DOWN);
      if (up>0 || dn>0)
        sd_notifyf (0, "STATUS=Uploading %.2f KBps, Downloading %.2f KBps.\n", up, dn); 
      else
        sd_notify (0, "STATUS=Idle.\n"); 
    } 

    sd_notify( 0, "STATUS=Closing transmission session...\n" );
    printf ("Closing transmission session...");
    tr_sessionSaveSettings (mySession, configDir, &settings);
    dtr_watchdir_free (watchdir);
    tr_sessionClose (mySession);
    pumpLogMessages (logfile);
    printf (" done.\n");

    /* shutdown */
#if HAVE_SYSLOG
    if (!foreground)
    {
        syslog (LOG_INFO, "%s", "Closing session");
        closelog ();
    }
#endif

    /* cleanup */
    if (pidfile_created)
        tr_remove (pid_filename);
    tr_variantFree (&settings);
    sd_notify (0, "STATUS=\n");
    return 0;
}
