#define REAPERAPI_IMPLEMENT
#define REAPERAPI_DECL

#define REAPERAPI_WANT_TrackFX_GetParamNormalized
#define REAPERAPI_WANT_TrackFX_SetParamNormalized
#include "reaper_plugin_functions.h"
#include "resource.h"

extern reaper_csurf_reg_t csurf_integrator_reg;
extern void localize_init(void * (*GetFunc)(const char *name));

REAPER_PLUGIN_HINSTANCE g_hInst; // used for dialogs, if any
HWND g_hwnd;
reaper_plugin_info_t *g_reaper_plugin_info;

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *reaper_plugin_info)
{
    g_hInst = hInstance;
    
    if (!reaper_plugin_info)
        return 0;
    
    if (reaper_plugin_info->caller_version != REAPER_PLUGIN_VERSION || !reaper_plugin_info->GetFunc)
        return 0;

    g_hwnd = reaper_plugin_info->hwnd_main;
    g_reaper_plugin_info = reaper_plugin_info;

    if (!g_hwnd)
        return 0;

    if (REAPERAPI_LoadAPI(reaper_plugin_info->GetFunc) > 0)
        return 0;

    localize_init(reaper_plugin_info->GetFunc);
    
    reaper_plugin_info->Register("csurf", &csurf_integrator_reg);

    return 1;
}
    
#ifndef _WIN32 // import the resources. Note: if you do not have these files, run "php WDL/swell/mac_resgen.php res.rc" from this directory
#ifdef USING_CMAKE
  #include "../lib/WDL/WDL/swell/swell-dlggen.h"
#else
  #include "../WDL/swell/swell-dlggen.h"
#endif
#include "res.rc_mac_dlg"
#endif
