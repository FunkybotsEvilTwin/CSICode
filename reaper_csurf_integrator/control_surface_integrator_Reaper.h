//
//  control_surface_integrator_Reaper.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_integrator_Reaper_h
#define control_surface_integrator_Reaper_h

#ifndef WDL_NO_DEFINE_MINMAX
#define WDL_NO_DEFINE_MINMAX
#endif

#include "reaper_plugin_functions.h"
#include "handy_functions.h"

#include <string>
#include <vector>

using namespace std;

extern HWND g_hwnd;

const int MEDBUF = 512;
const int SMLBUF = 256;

static vector<string> ExplodeString(const char separator, const string& value)
{
    vector<string> result;
    size_t start = 0;
    size_t end = value.find(';');
    while (end != string::npos) {
        result.push_back(value.substr(start, end - start));
        start = end + 1;
        end = value.find(';', start);
    }
    result.push_back(value.substr(start));
    return result;
}

struct osd_data {
    inline static const string COLOR_ERROR = "#FF0000";
    string origValue;
    string message;
    int timeoutMs = 3000;
    vector<string> bgColors;
    string bgColor;
    string lastValue;

    DWORD startWaitFeedback = 0;

    osd_data() = default; 

    osd_data(string osdValue) {
        origValue = osdValue;

        if (osdValue.front() == '\"') osdValue.erase(0, 1);
        if (osdValue.back()  == '\"') osdValue.pop_back();

        vector<string> osdParams = ExplodeString(';', osdValue);

        message = osdParams[0];
        if (osdParams.size() >= 2 && !osdParams[1].empty()) bgColors = ExplodeString(' ', osdParams[1]);
        if (osdParams.size() >= 3 && !osdParams[2].empty()) timeoutMs = atoi(osdParams[2].c_str());
    }

    const string toString() const {
        return message + ";" + bgColor + ";" + to_string(timeoutMs);
    }
    const bool isEmpty() const {
        return message.empty();
    }
    bool IsAwaitFeedback() const {
        DWORD now = GetTickCount();
        return startWaitFeedback != 0 && (now - startWaitFeedback <= 100);
    }
    void SetAwaitFeedback(bool value) {
        startWaitFeedback = value ? GetTickCount() : 0;
    }
};

struct rgba_color
{
    int r;
    int g;
    int b;
    int a;
        
    bool operator == (const rgba_color &other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a ;
    }
    
    bool operator != (const rgba_color &other) const
    {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }
    
    const char *rgba_to_string(char *buf) const // buf must be at least 10 bytes
    {
      snprintf(buf,10,"#%02x%02x%02x%02x",r,g,b,a);
      return buf;
    }
    
    rgba_color()
    {
        r = 0;
        g = 0;
        b = 0;
        a = 255;
    }
};

static bool GetColorValue(const char *hexColor, rgba_color &colorValue)
{
    if (strlen(hexColor) == 7)
    {
      return sscanf(hexColor, "#%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b) == 3;
    }
    if (strlen(hexColor) == 9)
    {
      return sscanf(hexColor, "#%2x%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b, &colorValue.a) == 4;
    }
    return false;
}

struct MIDI_event_ex_t : MIDI_event_t
{
    MIDI_event_ex_t()
    {
        frame_offset = 0;
        size = 3;
        midi_message[0] = 0x00;
        midi_message[1] = 0x00;
        midi_message[2] = 0x00;
        midi_message[3] = 0x00;
    };
    
    MIDI_event_ex_t(const unsigned char first, const unsigned char second, const unsigned char third)
    {
        size = 3;
        midi_message[0] = first;
        midi_message[1] = second;
        midi_message[2] = third;
        midi_message[3] = 0x00;
    };
    
    bool IsEqualTo(const MIDI_event_ex_t *other) const
    {
        if (this->size != other->size)
            return false;
        
        for (int i = 0; i < size; ++i)
            if (this->midi_message[i] != other->midi_message[i])
                return false;
        
        return true;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DAW
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    static int constexpr QUERY_LAST_TOUCHED_PARAMETER = 0;
    static int constexpr QUERY_CURRENTLY_FOCUSED_FX = 1;
    
    static void SendCommandMessage(WPARAM wparam) { ::SendMessage(g_hwnd, WM_COMMAND, wparam, 0); }
    
    static MediaTrack *GetSelectedTrack(int seltrackidx) { return ::GetSelectedTrack(NULL, seltrackidx); }
    
    static bool ValidateTrackPtr(MediaTrack *track) { return ::ValidatePtr(track, "MediaTrack*"); }
    
    static bool CanUndo()
    {
        if (::Undo_CanUndo2(NULL))
           return true;
        else
            return false;
    }
    
    static bool CanRedo()
    {
        if (::Undo_CanRedo2(NULL))
           return true;
        else
            return false;
    }
    
    static void Undo()
    {
        if (CanUndo())
            ::Undo_DoUndo2(NULL);
    }
    
    static void Redo()
    {
        if (CanRedo())
            ::Undo_DoRedo2(NULL);
    }
       
    static MediaTrack *GetTrack(int trackidx)
    {
        trackidx--;
        
        if (trackidx < 0)
            trackidx = 0;
        
        return ::GetTrack(NULL, trackidx) ;
    }
    
    static rgba_color GetTrackColor(MediaTrack *track)
    {
        rgba_color color;
        
        if (ValidateTrackPtr(track))
            ::ColorFromNative(::GetTrackColor(track), &color.r, &color.g, &color.b);
        
        if (color.r == 0 && color.g == 0 && color.b == 0)
        {
            color.r = 64;
            color.g = 64;
            color.b = 64;
        }
        
        return color;
    }
    
    static unsigned int GetTrackGroupMembership(MediaTrack *track, const char *groupname)
    {
        if (ValidateTrackPtr(track))
            return ::GetSetTrackGroupMembership(track, groupname, 0, 0);
        else
            return 0;
    }
    
    static unsigned int GetTrackGroupMembershipHigh(MediaTrack *track, const char *groupname)
    {
        if (ValidateTrackPtr(track))
            return ::GetSetTrackGroupMembershipHigh(track, groupname, 0, 0);
        else
            return 0;
    }
    
    static const char* GetCommandName(int cmdID)
    {
        const char* actionName = ::kbd_getTextFromCmd(cmdID, ::SectionFromUniqueID(1));
        if (actionName)
            return actionName;
        else
            return "NOT FOUND!";
    }
    
    static void ShowOSD(const osd_data osdData)
    {
        static string lastValue;
        static DWORD lastUpdateTs = 0;
        DWORD now = GetTickCount();

        if (lastValue == osdData.lastValue) {
            if (osdData.timeoutMs == -1) return;
            if (osdData.timeoutMs >= 0 && (now - lastUpdateTs) < (DWORD)osdData.timeoutMs) return;
        }

        lastValue = osdData.toString();
        lastUpdateTs = now;
        ::SetExtState("CSI_TMP", "OSD", lastValue.c_str(), false);
    }
    static bool CheckTouchedOrFocusedFX(MediaTrack** outTrack, int* fxSlotNum, int* fxParamNum)
    {
        if (!outTrack || !fxSlotNum || !fxParamNum)
            return false;

        int trackNum, trackIdx, itemIdx, itemTake, slotIdx, paramIdx;

        if (GetTouchedOrFocusedFX) {
            if (GetTouchedOrFocusedFX(QUERY_CURRENTLY_FOCUSED_FX, &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx)) {
                if (paramIdx & 1) // open, but no longer focused
                    *fxParamNum = -1;
            } else {
                if (!GetTouchedOrFocusedFX(QUERY_LAST_TOUCHED_PARAMETER, &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx))
                    return false;
            }
            trackNum = trackIdx + 1;
        } else {
            int type = GetFocusedFX2(&trackNum, fxSlotNum, fxParamNum);
            if (!type || (type & 4)) // closed or open, but no longer focused
                *fxParamNum = -1;
        }

        MediaTrack* track = nullptr;
        if (trackNum > 0)
            track = GetTrack(trackNum);
        else if (trackNum == 0)
            track = GetMasterTrack(nullptr);

        if (!track)
            return false;

        *outTrack = track;
        *fxSlotNum = slotIdx;
        *fxParamNum = paramIdx;
        return true;
    }

    static std::string GetLastTouchedFXParamDisplay()
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) return "No FX was touched";
    
        char trackName[256] = "?";
        char fxName[256] = "?";
        char paramName[128] = "";
        char paramValue[128] = "";

        if (MediaTrack* track = GetTrack(trackNum)) {
            const char* tn = (const char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
            if (tn && strlen(tn) > 0) strncpy(trackName, tn, sizeof(trackName));

            TrackFX_GetFXName(track, fxSlotNum, fxName, sizeof(fxName));
            TrackFX_GetParamName(track, fxSlotNum, fxParamNum, paramName, sizeof(paramName));
            TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, paramValue, sizeof(paramValue));

            return "[" + GetShortFXName(fxName) + "] " + paramName + ": " + paramValue + " (" + trackName + ")";
        }
        return "FAILED to GetLastTouchedFXParamDisplay";
    }

    static std::string GetShortFXName(const char* fullName)
    {
        std::string name(fullName);

        size_t colonPos = name.find(": ");
        if (colonPos != std::string::npos)
            name = name.substr(colonPos + 2);

        size_t parenPos = name.find(" (");
        if (parenPos != std::string::npos)
            name = name.substr(0, parenPos);

        return name;
    }

    static bool CheckTrackFxParamHasVolumeCurve(MediaTrack *track, int fxSlotNum, int fxParamNum,
        double* valueOut = nullptr, double* minOut = nullptr, double* maxOut = nullptr, double* midOut = nullptr, double* stepSizeOut = nullptr
    ) {
        double min = 0.0, max = 0.0, mid = 0.0, value = TrackFX_GetParamEx(track, fxSlotNum, fxParamNum, &min, &max, &mid);
        double stepSize = DAW::GetTrackFxParamStepSize(track, fxSlotNum, fxParamNum);
        if (valueOut) *valueOut = value;
        if (minOut) *minOut = min;
        if (maxOut) *maxOut = max;
        if (midOut) *midOut = mid;
        if (stepSizeOut) *stepSizeOut = stepSize;

        return (stepSize == 1.0 && min == 0.0 && max == 2.0);
    }

    static double GetTrackFxParamValue(MediaTrack *track, int fxSlotNum, int fxParamNum)
    {
        double value = 0.0, rawValue = 0.0, min = 0.0, max = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max)) {
            value = (rawValue == 0.0) ? 0.0 : volToNormalized(rawValue);
        } else {
            #if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
                value = TrackFX_GetParamNormalized(track, fxSlotNum, fxParamNum);
            #else
                double range = max - min;
                value = (range > 0.0) ? ((rawValue - min) / range) : 0.0;
            #endif
        }
        return value;
    }

    static void SetTrackFxParamValue(MediaTrack *track, int fxSlotNum, int fxParamNum, double value)
    {
        double newValue = value, rawValue = 0.0, min = 0.0, max = 0.0, mid = 0.0, stepSize = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max, &mid, &stepSize)) {
            newValue = normalizedToVol(value);
            TrackFX_SetParam(track, fxSlotNum, fxParamNum, newValue);
        } else {
            #if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
                TrackFX_SetParamNormalized(track, fxSlotNum, fxParamNum,  newValue);
            #else
                newValue = min + value * (max - min);
                TrackFX_SetParam(track, fxSlotNum, fxParamNum, newValue);
            #endif
        }
    }

    static double GetTrackFxParamStepSize(MediaTrack *track, int fxSlotNum, int fxParamNum)
    {
        double stepSize = 1.0, smallstep, largestep;
        bool isToggle;
        TrackFX_GetParameterStepSizes(track, fxSlotNum, fxParamNum, &stepSize, &smallstep, &largestep, &isToggle);
        return stepSize;
    }

    static double GetTrackVolumeValue(MediaTrack *track)
    {
        double value = 0.0, pan = 0.0;
        GetTrackUIVolPan(track, &value, &pan);
        return volToNormalized(value);
    }

    static void SetTrackVolumeValue(MediaTrack *track, double value)
    {
        CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }

    static bool CompareFaderValues(double a, double b, int decimals = 3) {
        double tolerance = std::pow(10.0, -decimals);
        return std::fabs(a - b) < tolerance;
    }

    static double RoundDouble(double value, int decimals = 6) {
        double multiplier = std::pow(10.0, decimals);
        return std::round(value * multiplier) / multiplier;
    }

};

#endif /* control_surface_integrator_Reaper_h */
