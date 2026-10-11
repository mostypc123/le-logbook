#include <string.h>
#include <stdio.h>
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include "XPLMGraphics.h"
#include "XPWidgets.h"
#include "XPStandardWidgets.h"
#if defined(__has_include)
#  if __has_include(<curl/curl.h>)
#    include <curl/curl.h>
#    define HAVE_CURL 1
#  endif
#endif
#include <stdlib.h>

// DEAR LIBCURL DEVS, FUCK YOUR LIB. JUST LET ME FETCH THE FUCKING SHIT I DONT NEED 7000 LINES TO DO IT.
// DEAR XPLANE DEVS, PLEASE EXPOSE THE FUCKING DIALOGS AND TOASTS YOUR SIM USES TO THE FUCKING PLUGINS.

#if defined(HAVE_CURL)
#if defined(_WIN32)
  #include <windows.h>
  typedef DWORD (WINAPI *thread_fn)(LPVOID);
  #define THREAD_FN(name) static DWORD WINAPI name(LPVOID arg)
  #define THREAD_DONE return 0
  static int spawn(thread_fn fn, void *arg)
  {
    HANDLE h = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!h) return -1;
    CloseHandle(h);
    return 0;
  }
#else
  #include <pthread.h>
  typedef void *(*thread_fn)(void *);
  #define THREAD_FN(name) static void *name(void *arg)
  #define THREAD_DONE return NULL
  static int spawn(thread_fn fn, void *arg) {
    pthread_t t;
    if (pthread_create(&t, NULL, fn, arg) != 0) return -1;
    pthread_detach(t);
    return 0;
  }
#endif

static int post_json(const char *url, const char *json) {
  CURL *c = curl_easy_init();
  struct curl_slist *h = NULL;
  CURLcode rc;
  long status = 0;

  if (!c) return -1;
  h = curl_slist_append(h, "Content-Type: application/json");
  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, json);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 5L);
  rc = curl_easy_perform(c);
  if (rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(h);
  curl_easy_cleanup(c);
  return rc == CURLE_OK ? (int)status : -1;
}

struct req { char url[300]; char body[1024]; };

THREAD_FN(post_thread) {
  struct req *r = arg;
  post_json(r->url, r->body);
  free(r);
  THREAD_DONE;
}

static void send_async(const char *url, const char *json) {
  struct req *r = malloc(sizeof *r);
  if (!r) return;
  snprintf(r->url, sizeof r->url, "%s", url);
  snprintf(r->body, sizeof r->body, "%s", json);
  if (spawn(post_thread, r) != 0) free(r);
}
#else
static void send_async(const char *url, const char *json) {
  (void)url; (void)json;
}
#endif

static XPLMDataRef alt_ref;
static XPLMDataRef log_ref;
static XPLMDataRef lat_ref;
static XPLMDataRef spd_ref;

static XPLMMenuID g_menu = NULL;
static XPWidgetID g_uuid_window = NULL;
static XPWidgetID g_url_window = NULL;
static XPWidgetID g_url_field = NULL;
static XPWidgetID g_url_save = NULL;

static char g_uuid[37] = "";
static char g_url[256] = "http://localhost:8080";

#if IBM
#include <windows.h>
#include <bcrypt.h>
#if defined(_MSC_VER)
#pragma comment(lib, "bcrypt")
#endif
static int uuid4(char *out) {
  unsigned char b[16];
  BCRYPT_ALG_HANDLE alg;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_RNG_ALGORITHM, NULL, 0) != 0) return -1;
  BCryptGenRandom(alg, b, sizeof b, 0);
  BCryptCloseAlgorithmProvider(alg, 0);
  b[6] = (b[6] & 0x0f) | 0x40;
  b[8] = (b[8] & 0x3f) | 0x80;
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
           b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
  return 0;
}
#else
#include <stdlib.h>
static int uuid4(char *out) {
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f) return -1;
  unsigned char b[16];
  if (fread(b, 1, sizeof b, f) != sizeof b) { fclose(f); return -1; }
  fclose(f);
  b[6] = (b[6] & 0x0f) | 0x40;
  b[8] = (b[8] & 0x3f) | 0x80;
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
           b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
  return 0;
}
#endif

static int label_draw_handler(XPWidgetMessage inMessage, XPWidgetID inWidget,
                              intptr_t inParam1, intptr_t inParam2) {
  (void)inParam1; (void)inParam2;
  if (inMessage == xpMsg_Draw) {
    float white[3] = {1.0f, 1.0f, 1.0f};
    char buf[256];
    int left = 0, bottom = 0;
    XPGetWidgetGeometry(inWidget, &left, NULL, NULL, &bottom);
    XPGetWidgetDescriptor(inWidget, buf, sizeof buf);
    XPLMDrawString(white, left, bottom, buf, NULL, xplmFont_Proportional);
    return 1;
  }
  return 0;
}

static XPWidgetID create_label(int left, int top, int right, int bottom,
                               const char *text, XPWidgetID container) {
  return XPCreateCustomWidget(left, top, right, bottom, 1, text, 0, container,
                              label_draw_handler);
}

static int widget_handler(XPWidgetMessage inMessage, XPWidgetID inWidget,
                          intptr_t inParam1, intptr_t inParam2) {
  (void)inParam2;
  if (inMessage == xpMessage_CloseButtonPushed) {
    XPHideWidget(inWidget);
    return 1;
  }
  if (inMessage == xpMsg_PushButtonPressed) {
    XPWidgetID button = (XPWidgetID)inParam1;
    if (button == g_url_save && g_url_field) {
      char buf[sizeof g_url];
      XPGetWidgetDescriptor(g_url_field, buf, sizeof buf);
      buf[sizeof buf - 1] = '\0';
      strncpy(g_url, buf, sizeof g_url - 1);
      g_url[sizeof g_url - 1] = '\0';
    }
    XPHideWidget(inWidget);
    return 1;
  }
  return 0;
}

static void show_uuid_dialog(void) {
  if (!g_uuid_window) {
    g_uuid_window = XPCreateWidget(300, 500, 700, 380, 1,
                                   "Le Logbook Tracker - UUID", 1, NULL,
                                   xpWidgetClass_MainWindow);
    XPSetWidgetProperty(g_uuid_window, xpProperty_MainWindowType,
                        xpMainWindowStyle_Translucent);
    XPSetWidgetProperty(g_uuid_window, xpProperty_MainWindowHasCloseBoxes, 1);
    XPAddWidgetCallback(g_uuid_window, widget_handler);

    create_label(312, 455, 688, 437, "Flight UUID:", g_uuid_window);
    create_label(312, 432, 688, 414,
                 g_uuid[0] ? g_uuid : "unavailable", g_uuid_window);
    {
      XPWidgetID close = XPCreateWidget(455, 395, 545, 377, 1, "Close", 0,
                                        g_uuid_window, xpWidgetClass_Button);
      XPSetWidgetProperty(close, xpProperty_ButtonType, xpPushButton);
    }
  } else {
    XPShowWidget(g_uuid_window);
  }
}

static void show_url_dialog(void) {
  if (!g_url_window) {
    g_url_window = XPCreateWidget(300, 500, 700, 355, 1,
                                  "Le Logbook Tracker - URL", 1, NULL,
                                  xpWidgetClass_MainWindow);
    XPSetWidgetProperty(g_url_window, xpProperty_MainWindowType,
                        xpMainWindowStyle_Translucent);
    XPSetWidgetProperty(g_url_window, xpProperty_MainWindowHasCloseBoxes, 1);
    XPAddWidgetCallback(g_url_window, widget_handler);

    create_label(312, 455, 688, 437, "Tracker URL:", g_url_window);
    g_url_field = XPCreateWidget(312, 428, 688, 408, 1, g_url, 0,
                                 g_url_window, xpWidgetClass_TextField);
    XPSetWidgetProperty(g_url_field, xpProperty_TextFieldType, xpTextEntryField);
    XPSetWidgetProperty(g_url_field, xpProperty_MaxCharacters,
                        (int)(sizeof g_url - 1));

    g_url_save = XPCreateWidget(405, 372, 485, 354, 1, "Save", 0,
                                g_url_window, xpWidgetClass_Button);
    XPSetWidgetProperty(g_url_save, xpProperty_ButtonType, xpPushButton);
    {
      XPWidgetID cancel = XPCreateWidget(515, 372, 595, 354, 1, "Cancel", 0,
                                         g_url_window, xpWidgetClass_Button);
      XPSetWidgetProperty(cancel, xpProperty_ButtonType, xpPushButton);
    }
  } else {
    XPSetWidgetDescriptor(g_url_field, g_url);
    XPShowWidget(g_url_window);
  }
}

static void menu_handler(void *inMenuRef, void *inItemRef) {
  (void)inMenuRef;
  if ((intptr_t)inItemRef == 1) {
    show_uuid_dialog();
  } else if ((intptr_t)inItemRef == 2) {
    show_url_dialog();
  }
}

static void create_menu(void) {
  XPLMMenuID plugins = XPLMFindPluginsMenu();
  int item = XPLMAppendMenuItem(plugins, "Le Logbook Tracker", NULL, 1);
  g_menu = XPLMCreateMenu("Le Logbook Tracker", plugins, item,
                          menu_handler, NULL);
  if (!g_menu) return;
  XPLMAppendMenuItem(g_menu, "UUID", (void *)(intptr_t)1, 1);
  XPLMAppendMenuItem(g_menu, "URL", (void *)(intptr_t)2, 1);
}

static void get_icao(char *out, int size) {
  XPLMDataRef r = XPLMFindDataRef("sim/aircraft/view/acf_ICAO");
  int n = r ? XPLMGetDatab(r, out, 0, size - 1) : 0;
  out[n] = '\0';
}

static float tick(float since_last, float since_loop, int counter, void *refcon) {
  char json[512];
  char pln[41];
  double lon = XPLMGetDatad(log_ref);
  double lat = XPLMGetDatad(lat_ref);
  int spd = (int)(XPLMGetDataf(spd_ref) * 1.94384f + 0.5f);
  (void)since_last; (void)since_loop; (void)counter; (void)refcon;

  get_icao(pln, sizeof pln);

  snprintf(json, sizeof json,
           "{\"uuid\":\"%s\",\"pln\":\"%s\",\"lat\":\"%.5f\",\"log\":\"%.5f\",\"spd\":%d}\n",
           g_uuid, pln, lat, lon, spd);
  XPLMDebugString(json);

  static const char fix[] = "/tracker/update";
  char sendto_url[sizeof g_url + sizeof fix];
  snprintf(sendto_url, sizeof sendto_url, "%s%s", g_url, fix);

  send_async(sendto_url, json);

  return 5.0f;
}

PLUGIN_API int XPluginStart(char *name, char *sig, char *desc) {
  strcpy(name, "Le Logbook Tracker");
  strcpy(sig, "lelogbook.tracker");
  strcpy(desc, "Tracks your flighta");

#if defined(HAVE_CURL)
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

  if (uuid4(g_uuid) != 0) g_uuid[0] = '\0';

  XPLMDebugString("lelogbook.tracker: started\n");
  XPLMDebugString("lelogbook.tracker: uuid ");
  XPLMDebugString(g_uuid[0] ? g_uuid : "unavailable");
  XPLMDebugString("\n");

  alt_ref = XPLMFindDataRef("sim/flightmodel/position/elevation");
  log_ref = XPLMFindDataRef("sim/flightmodel/position/longitude");
  lat_ref = XPLMFindDataRef("sim/flightmodel/position/latitude");
  spd_ref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");

  create_menu();

  if (!(alt_ref && log_ref && lat_ref && spd_ref)) return 0;

  XPLMRegisterFlightLoopCallback(tick, 5.0f, NULL);
  return 1;
}

PLUGIN_API void XPluginStop(void) {
  XPLMUnregisterFlightLoopCallback(tick, NULL);
  if (g_menu) { XPLMDestroyMenu(g_menu); g_menu = NULL; }
  if (g_uuid_window) { XPDestroyWidget(g_uuid_window, 1); g_uuid_window = NULL; }
  if (g_url_window) { XPDestroyWidget(g_url_window, 1); g_url_window = NULL; }
#if defined(HAVE_CURL)
  curl_global_cleanup();
#endif
  XPLMDebugString("lelogbook.tracker: stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
  return 1;
}

PLUGIN_API void XPluginDisable(void) {
  if (g_uuid_window) XPHideWidget(g_uuid_window);
  if (g_url_window) XPHideWidget(g_url_window);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int msg, void *param) {
  (void)from; (void)msg; (void)param;
}
