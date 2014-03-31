/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: platform.c 14110 2013-07-08 17:07:31Z jordan $
 */

#define _XOPEN_SOURCE 600  /* needed for recursive locks. */
#ifndef __USE_UNIX98
 #define __USE_UNIX98 /* some older Linuxes need it spelt out for them */
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getuid(), close() */
#include <sys/stat.h>

#ifdef WIN32
 #include <w32api.h>
 #define WINVER  WindowsXP
 #include <windows.h>
 #include <shlobj.h> /* for CSIDL_APPDATA, CSIDL_MYDOCUMENTS */
#else
 #ifdef SYS_DARWIN
  #include <CoreFoundation/CoreFoundation.h>
 #endif
 #ifdef __HAIKU__
  #include <FindDirectory.h>
 #endif
 #include <pthread.h>
#endif

#ifdef WIN32
#include <libgen.h> /* dirname() */
#endif

#include "transmission.h"
#include "session.h"
#include "list.h"
#include "log.h"
#include "platform.h"

/***
****  THREADS
***/

#ifdef WIN32
typedef DWORD tr_thread_id;
#else
typedef pthread_t tr_thread_id;
#endif

static tr_thread_id
tr_getCurrentThread (void)
{
#ifdef WIN32
  return GetCurrentThreadId ();
#else
  return pthread_self ();
#endif
}

static bool
tr_areThreadsEqual (tr_thread_id a, tr_thread_id b)
{
#ifdef WIN32
  return a == b;
#else
  return pthread_equal (a, b) != 0;
#endif
}

/** @brief portability wrapper around OS-dependent threads */
struct tr_thread
{
  void          (* func)(void *);
  void           * arg;
  tr_thread_id     thread;
#ifdef WIN32
  HANDLE           thread_handle;
#endif
};

bool
tr_amInThread (const tr_thread * t)
{
  return tr_areThreadsEqual (tr_getCurrentThread (), t->thread);
}

#ifdef WIN32
 #define ThreadFuncReturnType unsigned WINAPI
#else
 #define ThreadFuncReturnType void
#endif

static ThreadFuncReturnType
ThreadFunc (void * _t)
{
  tr_thread * t = _t;

  t->func (t->arg);

  tr_free (t);
#ifdef WIN32
  _endthreadex (0);
  return 0;
#endif
}

tr_thread *
tr_threadNew (void (*func)(void *), void * arg)
{
  tr_thread * t = tr_new0 (tr_thread, 1);

  t->func = func;
  t->arg  = arg;

#ifdef WIN32
  {
    unsigned int id;
    t->thread_handle = (HANDLE) _beginthreadex (NULL, 0, &ThreadFunc, t, 0, &id);
    t->thread = (DWORD) id;
  }
#else
  pthread_create (&t->thread, NULL, (void* (*)(void*))ThreadFunc, t);
  pthread_detach (t->thread);
#endif

  return t;
}

/***
****  LOCKS
***/

/** @brief portability wrapper around OS-dependent thread mutexes */
struct tr_lock
{
  int                 depth;
#ifdef WIN32
  CRITICAL_SECTION    lock;
  DWORD               lockThread;
#else
  pthread_mutex_t     lock;
  pthread_t           lockThread;
#endif
};

tr_lock*
tr_lockNew (void)
{
  tr_lock * l = tr_new0 (tr_lock, 1);

#ifdef WIN32
  InitializeCriticalSection (&l->lock); /* supports recursion */
#else
  pthread_mutexattr_t attr;
  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&l->lock, &attr);
#endif

  return l;
}

void
tr_lockFree (tr_lock * l)
{
#ifdef WIN32
    DeleteCriticalSection (&l->lock);
#else
    pthread_mutex_destroy (&l->lock);
#endif
    tr_free (l);
}

void
tr_lockLock (tr_lock * l)
{
#ifdef WIN32
  EnterCriticalSection (&l->lock);
#else
  pthread_mutex_lock (&l->lock);
#endif

  assert (l->depth >= 0);
  assert (!l->depth || tr_areThreadsEqual (l->lockThread, tr_getCurrentThread ()));
  l->lockThread = tr_getCurrentThread ();
  ++l->depth;
}

int
tr_lockHave (const tr_lock * l)
{
  return (l->depth > 0) &&
         (tr_areThreadsEqual (l->lockThread, tr_getCurrentThread ()));
}

void
tr_lockUnlock (tr_lock * l)
{
  assert (l->depth > 0);
  assert (tr_areThreadsEqual (l->lockThread, tr_getCurrentThread ()));

  --l->depth;
  assert (l->depth >= 0);
#ifdef WIN32
  LeaveCriticalSection (&l->lock);
#else
  pthread_mutex_unlock (&l->lock);
#endif
}

/***
****  PATHS
***/

#ifndef WIN32
 #include <pwd.h>
#endif

static const char *
getHomeDir (void)
{
  static char * home = NULL;

  if (!home)
    {
      home = tr_strdup (getenv ("HOME"));

      if (!home)
        {
#ifdef WIN32
          char appdata[MAX_PATH]; /* SHGetFolderPath () requires MAX_PATH */
          *appdata = '\0';
          SHGetFolderPath (NULL, CSIDL_PERSONAL, NULL, 0, appdata);
          home = tr_strdup (appdata);
#else
          struct passwd * pw = getpwuid (getuid ());
          if (pw)
            home = tr_strdup (pw->pw_dir);
          endpwent ();
#endif
        }

      if (!home)
        home = tr_strdup ("");
    }

  return home;
}

#if defined (SYS_DARWIN) || defined (WIN32)
 #define RESUME_SUBDIR  "Resume"
 #define TORRENT_SUBDIR "Torrents"
#else
 #define RESUME_SUBDIR  "resume"
 #define TORRENT_SUBDIR "torrents"
#endif

void
tr_setConfigDir (tr_session * session, const char * configDir)
{
  char * path;

  session->configDir = tr_strdup (configDir);

  path = tr_buildPath (configDir, RESUME_SUBDIR, NULL);
  tr_mkdirp (path, 0777);
  session->resumeDir = path;

  path = tr_buildPath (configDir, TORRENT_SUBDIR, NULL);
  tr_mkdirp (path, 0777);
  session->torrentDir = path;
}

const char *
tr_sessionGetConfigDir (const tr_session * session)
{
  return session->configDir;
}

const char *
tr_getTorrentDir (const tr_session * session)
{
  return session->torrentDir;
}

const char *
tr_getResumeDir (const tr_session * session)
{
  return session->resumeDir;
}

const char*
tr_getDefaultConfigDir (const char * appname)
{
  static char * s = NULL;

  if (!appname || !*appname)
    appname = "Transmission";

  if (!s)
    {
      if ((s = getenv ("TRANSMISSION_HOME")))
        {
          s = tr_strdup (s);
        }
      else
        {
#ifdef SYS_DARWIN
          s = tr_buildPath (getHomeDir (), "Library", "Application Support", appname, NULL);
#elif defined (WIN32)
          char appdata[TR_PATH_MAX]; /* SHGetFolderPath () requires MAX_PATH */
          SHGetFolderPath (NULL, CSIDL_APPDATA, NULL, 0, appdata);
          s = tr_buildPath (appdata, appname, NULL);
#elif defined (__HAIKU__)
          char buf[TR_PATH_MAX];
          find_directory (B_USER_SETTINGS_DIRECTORY, -1, true, buf, sizeof (buf));
          s = tr_buildPath (buf, appname, NULL);
#else
          if ((s = getenv ("XDG_CONFIG_HOME")))
            s = tr_buildPath (s, appname, NULL);
          else
            s = tr_buildPath (getHomeDir (), ".config", appname, NULL);
#endif
        }
    }

  return s;
}

const char*
tr_getDefaultDownloadDir (void)
{
  static char * user_dir = NULL;

  if (user_dir == NULL)
    {
      const char * config_home;
      char * config_file;
      char * content;
      size_t content_len;

      /* figure out where to look for user-dirs.dirs */
      config_home = getenv ("XDG_CONFIG_HOME");
      if (config_home && *config_home)
        config_file = tr_buildPath (config_home, "user-dirs.dirs", NULL);
      else
        config_file = tr_buildPath (getHomeDir (), ".config", "user-dirs.dirs", NULL);

      /* read in user-dirs.dirs and look for the download dir entry */
      content = (char *) tr_loadFile (config_file, &content_len);
      if (content && content_len>0)
        {
          const char * key = "XDG_DOWNLOAD_DIR=\"";
          char * line = strstr (content, key);
          if (line != NULL)
            {
              char * value = line + strlen (key);
              char * end = strchr (value, '"');

              if (end)
                {
                  *end = '\0';

                  if (!memcmp (value, "$HOME/", 6))
                    user_dir = tr_buildPath (getHomeDir (), value+6, NULL);
                  else if (!strcmp (value, "$HOME"))
                    user_dir = tr_strdup (getHomeDir ());
                  else
                    user_dir = tr_strdup (value);
                }
            }
        }

      if (user_dir == NULL)
#ifdef __HAIKU__
        user_dir = tr_buildPath (getHomeDir (), "Desktop", NULL);
#else
        user_dir = tr_buildPath (getHomeDir (), "Downloads", NULL);
#endif

      tr_free (content);
      tr_free (config_file);
    }

  return user_dir;
}

/***
****
***/

static int
isWebClientDir (const char * path)
{
  struct stat sb;
  char * tmp = tr_buildPath (path, "index.html", NULL);
  const int ret = !stat (tmp, &sb);
  tr_logAddInfo (_("Searching for web interface file \"%s\""), tmp);
  tr_free (tmp);

  return ret;
}

const char *
tr_getWebClientDir (const tr_session * session UNUSED)
{
  static char * s = NULL;

  if (!s)
    {
      if ((s = getenv ("CLUTCH_HOME")))
        {
          s = tr_strdup (s);
        }
      else if ((s = getenv ("TRANSMISSION_WEB_HOME")))
        {
          s = tr_strdup (s);
        }
      else
        {

#ifdef SYS_DARWIN /* on Mac, look in the Application Support folder first, then in the app bundle. */

          /* Look in the Application Support folder */
          s = tr_buildPath (tr_sessionGetConfigDir (session), "web", NULL);

          if (!isWebClientDir (s))
            {
              tr_free (s);

              CFURLRef appURL = CFBundleCopyBundleURL (CFBundleGetMainBundle ());
              CFStringRef appRef = CFURLCopyFileSystemPath (appURL,
                                                            kCFURLPOSIXPathStyle);
              const CFIndex appStringLength = CFStringGetMaximumSizeOfFileSystemRepresentation (appRef);

              char * appString = tr_malloc (appStringLength);
              const bool success = CFStringGetFileSystemRepresentation (appRef, appString, appStringLength);
              assert (success);

              CFRelease (appURL);
              CFRelease (appRef);

              /* Fallback to the app bundle */
              s = tr_buildPath (appString, "Contents", "Resources", "web", NULL);
              if (!isWebClientDir (s))
                {
                  tr_free (s);
                  s = NULL;
                }

              tr_free (appString);
            }

#elif defined (WIN32)

          /* SHGetFolderPath explicitly requires MAX_PATH length */
          char dir[MAX_PATH];

          /* Generally, Web interface should be stored in a Web subdir of
           * calling executable dir. */

          if (s == NULL) /* check personal AppData/Transmission/Web */
            {
              SHGetFolderPath (NULL, CSIDL_COMMON_APPDATA, NULL, 0, dir);
              s = tr_buildPath (dir, "Transmission", "Web", NULL);
              if (!isWebClientDir (s))
                {
                  tr_free (s);
                  s = NULL;
                }
            }

          if (s == NULL) /* check personal AppData */
            {
              SHGetFolderPath (NULL, CSIDL_APPDATA, NULL, 0, dir);
              s = tr_buildPath (dir, "Transmission", "Web", NULL);
              if (!isWebClientDir (s))
                {
                  tr_free (s);
                  s = NULL;
                }
            }

            if (s == NULL) /* check calling module place */
              {
                GetModuleFileName (GetModuleHandle (NULL), dir, sizeof (dir));
                s = tr_buildPath (dirname (dir), "Web", NULL);
                if (!isWebClientDir (s))
                  {
                    tr_free (s);
                    s = NULL;
                  }
            }

#else /* everyone else, follow the XDG spec */

          tr_list *candidates = NULL, *l;
          const char * tmp;

          /* XDG_DATA_HOME should be the first in the list of candidates */
          tmp = getenv ("XDG_DATA_HOME");
          if (tmp && *tmp)
            {
              tr_list_append (&candidates, tr_strdup (tmp));
            }
          else
            {
              char * dhome = tr_buildPath (getHomeDir (), ".local", "share", NULL);
              tr_list_append (&candidates, dhome);
            }

          /* XDG_DATA_DIRS are the backup directories */
          {
            const char * pkg = PACKAGE_DATA_DIR;
            const char * xdg = getenv ("XDG_DATA_DIRS");
            const char * fallback = "/usr/local/share:/usr/share";
            char * buf = tr_strdup_printf ("%s:%s:%s", (pkg?pkg:""), (xdg?xdg:""), fallback);
            tmp = buf;
            while (tmp && *tmp)
              {
                const char * end = strchr (tmp, ':');
                if (end)
                  {
                    if ((end - tmp) > 1)
                      tr_list_append (&candidates, tr_strndup (tmp, end - tmp));
                    tmp = end + 1;
                  }
                else if (tmp && *tmp)
                  {
                    tr_list_append (&candidates, tr_strdup (tmp));
                    break;
                  }
              }
            tr_free (buf);
          }

          /* walk through the candidates & look for a match */
          for (l=candidates; l; l=l->next)
            {
              char * path = tr_buildPath (l->data, "transmission", "web", NULL);
              const int found = isWebClientDir (path);
              if (found)
                {
                  s = path;
                  break;
                }
              tr_free (path);
            }

          tr_list_free (&candidates, tr_free);

#endif

        }
    }

  return s;
}


#ifdef WIN32

/* The following mmap functions are by Joerg Walter, and were taken from
 * his paper at: http://www.genesys-e.de/jwalter/mix4win.htm */

#if defined (_MSC_VER)
__declspec (align (4)) static LONG volatile g_sl;
#else
static LONG volatile g_sl __attribute__((aligned (4)));
#endif

/* Wait for spin lock */
static int
slwait (LONG volatile *sl)
{
  while (InterlockedCompareExchange (sl, 1, 0) != 0)
    Sleep (0);

  return 0;
}

/* Release spin lock */
static int
slrelease (LONG volatile *sl)
{
  InterlockedExchange (sl, 0);
  return 0;
}

/* getpagesize for windows */
static long
getpagesize (void)
{
  static long g_pagesize = 0;

  if (!g_pagesize)
    {
      SYSTEM_INFO system_info;
      GetSystemInfo (&system_info);
      g_pagesize = system_info.dwPageSize;
    }

  return g_pagesize;
}

static long
getregionsize (void)
{
  static long g_regionsize = 0;

  if (!g_regionsize)
    {
      SYSTEM_INFO system_info;
      GetSystemInfo (&system_info);
      g_regionsize = system_info.dwAllocationGranularity;
    }

  return g_regionsize;
}

void *
mmap (void *ptr, long  size, long  prot, long  type, long  handle, long  arg)
{
  static long g_pagesize;
  static long g_regionsize;

  /* Wait for spin lock */
  slwait (&g_sl);

  /* First time initialization */
  if (!g_pagesize)
    g_pagesize = getpagesize ();
  if (!g_regionsize)
    g_regionsize = getregionsize ();

  /* Allocate this */
  ptr = VirtualAlloc (ptr, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
  if (!ptr)
    {
      ptr = (void *) -1;
      goto mmap_exit;
    }

mmap_exit:
  /* Release spin lock */
  slrelease (&g_sl);
  return ptr;
}

long
munmap (void *ptr, long size)
{
  static long g_pagesize;
  static long g_regionsize;
  int rc = -1;

  /* Wait for spin lock */
  slwait (&g_sl);

  /* First time initialization */
  if (!g_pagesize)
    g_pagesize = getpagesize ();
  if (!g_regionsize)
    g_regionsize = getregionsize ();

  /* Free this */
  if (!VirtualFree (ptr, 0, MEM_RELEASE))
    goto munmap_exit;

  rc = 0;

munmap_exit:
  /* Release spin lock */
  slrelease (&g_sl);
  return rc;
}

#endif
