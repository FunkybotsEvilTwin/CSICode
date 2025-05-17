//
//  control_surface_integrator.h
//  reaper_control_surface_integrator
//
//

#ifndef control_surface_integrator
#define control_surface_integrator

#if __cplusplus < 201100
#if !defined(_MSC_VER) || _MSC_VER < 1400
#define override
#endif
#endif

#ifdef _WIN32
#if _MSC_VER <= 1400
#define _CRT_SECURE_NO_DEPRECATE
#endif
#if _MSC_VER >= 1800
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#endif

#include <fstream>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <algorithm>

#ifdef _WIN32
#include "oscpkt.hh"
#include "udp.hh"
#endif

#include <filesystem>
#include <map>

#include "../WDL/win32_utf8.h"
#include "../WDL/ptrlist.h"
#include "../WDL/queue.h"

#include "control_surface_integrator_Reaper.h"

#include "handy_functions.h"

#ifdef INCLUDE_LOCALIZE_IMPORT_H
#define LOCALIZE_IMPORT_PREFIX "csi_"
#include "../WDL/localize/localize-import.h"
#endif
#include "../WDL/localize/localize.h"

#ifdef _WIN32
#include "commctrl.h"
#else
#include "oscpkt.hh"
#include "udp.hh"
#endif

#define NUM_ELEM(array) (int(sizeof(array)/sizeof(array[0])))

extern void TrimLine(string &line);
extern void ReplaceAllWith(string &output, const char *replaceAny, const char *replacement);
extern int strToHex(string &valueStr);

class ZoneManager;
class Zone;

extern void RequestFocusedFXDialog(ZoneManager *zoneManager);
extern void CloseFocusedFXDialog();
extern void UpdateLearnWindow(ZoneManager *zoneManager);
extern void InitBlankLearnFocusedFXZone(ZoneManager *zoneManager, Zone *fxZone, MediaTrack *track, int fxSlot);
extern void ShutdownLearn();

extern int g_debugLevel;
extern bool g_surfaceRawInDisplay;
extern bool g_surfaceInDisplay;
extern bool g_surfaceOutDisplay;
extern bool g_fxParamsWrite;

extern REAPER_PLUGIN_HINSTANCE g_hInst;

static const int REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES = 41743;
static const int REAPER__FILE_NEW_PROJECT = 40023;
static const int REAPER__CLOSE_CURRENT_PROJECT_TAB = 40860;
static const int REAPER__TRACK_INSERT_TRACK_FROM_TEMPLATE = 46000;

static const char * const s_CSIName = "CSI";
static const char * const s_CSIVersionDisplay = "v7.0";
static const char * const s_MajorVersionToken = "7.0";
static const char * const s_PageToken = "Page";
static const char * const s_MidiSurfaceToken = "MIDI";
static const char * const s_OSCSurfaceToken = "OSC";
static const char * const s_OSCX32SurfaceToken = "OSCX32";

static const char * const s_BadFileChars = " \\:*?<>|.,()/";
static const char * const s_BeginAutoSection = "#Begin auto generated section";
static const char * const s_EndAutoSection = "#End auto generated section";

static char *format_number(double v, char *buf, int bufsz)
{
  snprintf(buf,bufsz,"%.12f", v);
  WDL_remove_trailing_decimal_zeros(buf,2); // trim 1.0000 down to 1.00
  return buf;
}

extern void GetTokens(vector<string> &tokens, const string &line);
extern void GetTokens(vector<string> &tokens, const string &line, char delimiter);

class ReloadPluginException : public std::runtime_error {
public:
    explicit ReloadPluginException(const std::string& message)
        : std::runtime_error(message) {}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum PropertyType {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_PROPERTY_TYPES(D) \
  D(Font) \
  D(TopMargin) \
  D(BottomMargin) \
  D(BackgroundColorOff) \
  D(TextColorOff) \
  D(BackgroundColorOn) \
  D(TextColorOn) \
  D(DisplayText) \
  D(BackgroundColor) \
  D(TextColor) \
  D(RingStyle) \
  D(Push) \
  D(PushColor) \
  D(LEDRingColor) \
  D(LEDRingColors) \
  D(BarStyle) \
  D(TextAlign) \
  D(TextInvert) \
  D(Mode) \
  D(OffColor) \
  D(OnColor) \
  D(Background) \
  D(Foreground) \
  D(Feedback) \
  D(HoldDelay) \
  D(HoldRepeatInterval) \
  D(Version) \
  D(SurfaceType) \
  D(SurfaceName) \
  D(SurfaceChannelCount) \
  D(MidiInput) \
  D(MidiOutput) \
  D(MIDISurfaceRefreshRate) \
  D(MaxMIDIMesssagesPerRun) \
  D(ReceiveOnPort) \
  D(TransmitToPort) \
  D(TransmitToIPAddress) \
  D(MaxPacketsPerRun) \
  D(PageName) \
  D(PageFollowsMCP) \
  D(SynchPages) \
  D(ScrollLink) \
  D(ScrollSynch) \
  D(Broadcaster) \
  D(Listener) \
  D(Surface) \
  D(StartChannel) \
  D(GoHome) \
  D(Modifiers) \
  D(FXMenu) \
  D(SelectedTrackFX) \
  D(SelectedTrackSends) \
  D(SelectedTrackReceives) \
  D(SurfaceFolder) \
  D(ZoneFolder) \
  D(FXZoneFolder) \

  PropertyType_Unknown = 0, // in this case, string is type=value pair
#define DEFPT(x) PropertyType_##x ,
  DECLARE_PROPERTY_TYPES(DEFPT)
#undef DEFPT
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PropertyList
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    enum { MAX_PROP=24, RECLEN=10 };
    int nprops_;
    PropertyType props_[MAX_PROP];
    char vals_[MAX_PROP][RECLEN]; // if last byte is nonzero, pointer, otherwise, string

    static char *get_item_ptr(char *vp) // returns a strdup'd string
    {
        if (!vp[RECLEN-1]) return NULL;
        char *ret;
        memcpy(&ret,vp,sizeof(char *));
        return ret;
    }

  public:
    PropertyList() : nprops_(0) { }
    ~PropertyList()
    {
        for (int x = 0; x < nprops_; ++x) free(get_item_ptr(&vals_[x][0]));
    }
    
    void delete_props()
    {
        for (int x = 0; x < nprops_; ++x) free(get_item_ptr(&vals_[x][0]));
        nprops_ = 0;
    }

    void set_prop(PropertyType prop, const char *val)
    {
        int x;
        if (prop == PropertyType_Unknown)
            x = nprops_;
        else
            for (x = 0; x < nprops_ && props_[x] != prop; ++x);

        if (WDL_NOT_NORMALLY(x >= MAX_PROP)) return;

        char *rec = &vals_[x][0];
        if (x == nprops_)
        {
            nprops_++;
            props_[x] = prop;
        }
        else
        {
            free(get_item_ptr(rec));
        }

        if (strlen(val) < RECLEN)
        {
          lstrcpyn_safe(rec, val, RECLEN);
          rec[RECLEN-1] = 0;
        }
        else
        {
#ifdef WIN32
          char *v = _strdup(val);
#else
          char *v = strdup(val);
#endif
          memcpy(rec, &v, sizeof(v));
          rec[RECLEN-1] = 1;
        }
    }
    void set_prop_int(PropertyType prop, int v) { char tmp[64]; snprintf(tmp,sizeof(tmp),"%d",v); set_prop(prop,tmp); }

    const char *get_prop(PropertyType prop) const
    {
        for (int x = 0; x < nprops_; ++x)
            if (props_[x] == prop)
            {
                char *p = get_item_ptr((char *) (&vals_[x][0]));
                return p ? p : &vals_[x][0];
            }
        return NULL;
    }

    const char *enum_props(int x, PropertyType &type) const
    {
        if (x<0 || x >= nprops_) return NULL;
        type = props_[x];
        return get_item_ptr((char*)&vals_[x][0]);
    }

    static PropertyType prop_from_string(const char *str)
    {
#define CHK(x) if (!strcmp(str,#x)) return PropertyType_##x;
        DECLARE_PROPERTY_TYPES(CHK)
#undef CHK
        return PropertyType_Unknown;
    }
    static const char *string_from_prop(PropertyType type)
    {
#define CHK(x) if (type == PropertyType_##x) return #x;
        DECLARE_PROPERTY_TYPES(CHK)
#undef CHK
        return NULL;
    }

    void save_list(FILE *fxFile) const
    {
        for (int x = 0; x < nprops_; ++x)
        {
            //const char *value = get_item_ptr((char *)&vals_[x][0]), *key = string_from_prop(props_[x]);
            const char *value = get_prop(props_[x]);
            const char *key = string_from_prop(props_[x]);
            
            if (key && value)
                fprintf(fxFile, "%s=%s ", key, value);
            //else if (WDL_NORMALLY(props_[x] == PropertyType_Unknown && value && strstr(value,"=")))
                //fprintf(fxFile, " %s", value);
        }
        //fprintf(fxFile,"\n");
    }

    void print_to_buf(char * buf, int buf_size, PropertyType prop)
    {
        const char *key = string_from_prop(prop);
        const char *value = get_prop(prop);

        if (key && value)
            snprintf_append(buf, buf_size, "%s=%s ", key, value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSurfIntegrator;
class FeedbackProcessor;
class Widget;
class Page;
class ControlSurface;
class Midi_ControlSurface;
class OSC_ControlSurface;
class TrackNavigationManager;
class ActionContext;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Action() {}
    
    virtual const char *GetName() { return "Action"; }

    virtual void Touch(ActionContext *context, double value) {}
    virtual void RequestUpdate(ActionContext *context) {}
    virtual void Do(ActionContext *context, double value) {}
    virtual double GetCurrentNormalizedValue(ActionContext *context) { return 0.0; }
    virtual double GetCurrentDBValue(ActionContext *context) { return 0.0; }

    int GetPanMode(MediaTrack *track)
    {
        double pan1, pan2 = 0.0;
        int panMode = 0;
        GetTrackUIPan(track, &pan1, &pan2, &panMode);
        return panMode;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    const CSurfIntegrator *const csi_;
    Page *const page_;
    bool isVolumeTouched_ = false;
    bool isPanTouched_ = false;
    bool isPanWidthTouched_ = false;
    bool isPanLeftTouched_ = false;
    bool isPanRightTouched_ = false;
    bool isMCUTrackPanWidth_ = false;

    Navigator(const CSurfIntegrator *const csi, Page * page) : csi_(csi), page_(page) {}

public:
    virtual ~Navigator() {}
    
    virtual const char *GetName() { return "Navigator"; }
    virtual MediaTrack *GetTrack() { return NULL; }
    virtual int GetChannelNum() { return 0; }

    bool GetIsNavigatorTouched() { return isVolumeTouched_ || isPanTouched_ || isPanWidthTouched_ || isPanLeftTouched_ || isPanRightTouched_; }
    
    void SetIsVolumeTouched(bool isVolumeTouched) { isVolumeTouched_ = isVolumeTouched;  }
    bool GetIsVolumeTouched() { return isVolumeTouched_;  }
    
    void SetIsPanTouched(bool isPanTouched) { isPanTouched_ = isPanTouched; }
    bool GetIsPanTouched() { return isPanTouched_;  }
    
    void SetIsPanWidthTouched(bool isPanWidthTouched) { isPanWidthTouched_ = isPanWidthTouched; }
    bool GetIsPanWidthTouched() { return isPanWidthTouched_;  }
    
    void SetIsPanLeftTouched(bool isPanLeftTouched) { isPanLeftTouched_ = isPanLeftTouched; }
    bool GetIsPanLeftTouched() { return isPanLeftTouched_;  }
    
    void SetIsPanRightTouched(bool isPanRightTouched) { isPanRightTouched_ = isPanRightTouched; }
    bool GetIsPanRightTouched() { return isPanRightTouched_;  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int const channelNum_;
    
protected:
    TrackNavigationManager *const trackNavigationManager_;

public:
    TrackNavigator(CSurfIntegrator *const csi, Page *page, TrackNavigationManager *trackNavigationManager, int channelNum) : Navigator(csi, page), trackNavigationManager_(trackNavigationManager), channelNum_(channelNum) {}
    virtual ~TrackNavigator() {}
    
    virtual const char *GetName() override { return "TrackNavigator"; }
   
    virtual MediaTrack *GetTrack() override;
    
    virtual int GetChannelNum() override { return channelNum_; }

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    MediaTrack *const track_;
    
public:
    FixedTrackNavigator(CSurfIntegrator *const csi, Page *page, MediaTrack *const track) : Navigator(csi, page), track_(track) {}
    virtual ~FixedTrackNavigator() {}
    
    virtual const char *GetName() override { return "FixedTrackNavigator"; }
   
    virtual MediaTrack *GetTrack() override { return track_; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MasterTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    MasterTrackNavigator(CSurfIntegrator *const csi, Page * page) : Navigator(csi, page) {}
    virtual ~MasterTrackNavigator() {}
    
    virtual const char *GetName() override { return "MasterTrackNavigator"; }
    
    virtual MediaTrack *GetTrack() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SelectedTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    SelectedTrackNavigator(CSurfIntegrator *const csi, Page * page) : Navigator(csi, page) {}
    virtual ~SelectedTrackNavigator() {}
    
    virtual const char *GetName() override { return "SelectedTrackNavigator"; }
    
    virtual MediaTrack *GetTrack() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FocusedFXNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    FocusedFXNavigator(CSurfIntegrator *const csi, Page * page) : Navigator(csi, page) {}
    virtual ~FocusedFXNavigator() {}
    
    virtual const char *GetName() override { return "FocusedFXNavigator"; }
    
    virtual MediaTrack *GetTrack() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ActionContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    CSurfIntegrator *const csi_;
    Action *action_;
    Widget  *const widget_;
    Zone  *const zone_;

    int intParam_ = 0;
    
    string stringParam_;
    
    int paramIndex_ = 0;
    string fxParamDisplayName_;
    
    int commandId_ = 0;
    
    double rangeMinimum_ = 0.0;
    double rangeMaximum_ = 1.0;
    
    vector<double> steppedValues_;
    int steppedValuesIndex_= 0;
    
    double deltaValue_ = 0.0;
    vector<double> acceleratedDeltaValues_;
    vector<int> acceleratedTickValues_;
    int accumulatedIncTicks_ = 0;
    int accumulatedDecTicks_ = 0;
    
    bool isValueInverted_ = false;
    bool isFeedbackInverted_ = false;

    int  holdDelayMs_ = 0;
    int  holdRepeatIntervalMs_ = 0;
    int  lastHoldRepeatTs_ = 0;
    int  lastHoldStartTs_ = 0;
    bool holdActive_= false;
    bool holdRepeatActive_ = false;
    double deferredValue_ = 0.0;
    
    bool supportsColor_ = false;
    vector<rgba_color> colorValues_;
    int currentColorIndex_ = 0;
    
    bool supportsTrackColor_ = false;
        
    bool provideFeedback_= true;

    string m_freeFormText;

    PropertyList widgetProperties_;
        
    void UpdateTrackColor();
    void GetSteppedValues(Widget *widget, Action *action,  Zone *zone, int paramNumber, const vector<string> &params, const PropertyList &widgetProperties, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues);
    void SetColor(const vector<string> &params, bool &supportsColor, bool &supportsTrackColor, vector<rgba_color> &colorValues);
    void GetColorValues(vector<rgba_color> &colorValues, const vector<string> &colors);
    void LogAction(double value);
public:
    static int constexpr HOLD_DELAY_INHERIT_VALUE = -1;
    static double constexpr BUTTON_RELEASE_MESSAGE_VALUE = 0.0;
    ActionContext(CSurfIntegrator *const csi, Action *action, Widget *widget, Zone *zone, int paramIndex, const vector<string> &params);

    virtual ~ActionContext() {}
    
    CSurfIntegrator *GetCSI() { return csi_; }
    
    Action *GetAction() { return action_; }
    Widget *GetWidget() { return widget_; }
    Zone *GetZone() { return zone_; }
    int GetSlotIndex();
    const char *GetName();

    int GetIntParam() { return intParam_; }
    int GetCommandId() { return commandId_; }
    
    const char *GetFXParamDisplayName() { return fxParamDisplayName_.c_str(); }
    
    MediaTrack *GetTrack();
    
    void DoRangeBoundAction(double value);
    void DoSteppedValueAction(double value);
    void DoAcceleratedSteppedValueAction(int accelerationIndex, double value);
    void DoAcceleratedDeltaValueAction(int accelerationIndex, double value);
    
    Page *GetPage();
    ControlSurface *GetSurface();
    int GetParamIndex() { return paramIndex_; }
    void SetParamIndex(int paramIndex) { paramIndex_ = paramIndex; }
      
    PropertyList &GetWidgetProperties() { return widgetProperties_; }
    
    void SetIsValueInverted() { isValueInverted_ = true; }
    void SetIsFeedbackInverted() { isFeedbackInverted_ = true; }
    void SetHoldDelay(int value) { holdDelayMs_ = value; }
    int GetHoldDelay() { return holdDelayMs_; }

    void SetAction(Action *action) { action_ = action; RequestUpdate(); }
    void DoAction(double value);
    void PerformAction(double value);
    void DoRelativeAction(double value);
    void DoRelativeAction(int accelerationIndex, double value);
    
    void RequestUpdate();
    void RunDeferredActions();
    void ClearWidget();
    void UpdateWidgetValue(double value); // note: if passing the constant 0, must be 0.0 to avoid ambiguous type vs pointer
    void UpdateWidgetValue(const char *value);
    void ForceWidgetValue(const char *value);
    void UpdateJSFXWidgetSteppedValue(double value);
    void UpdateColorValue(double value);

    const char *GetStringParam() { return stringParam_.c_str(); }
    const   vector<double> &GetAcceleratedDeltaValues() { return acceleratedDeltaValues_; }
    void    SetAccelerationValues(const vector<double> &acceleratedDeltaValues) { acceleratedDeltaValues_ = acceleratedDeltaValues; }
    const   vector<int> &GetAcceleratedTickCounts() { return acceleratedTickValues_; }
    void    SetTickCounts(const vector<int> &acceleratedTickValues) { acceleratedTickValues_ = acceleratedTickValues; }
    int     GetNumberOfSteppedValues() { return (int)steppedValues_.size(); }
    const   vector<double> &GetSteppedValues() { return steppedValues_; }
    double  GetDeltaValue() { return deltaValue_; }
    void    SetDeltaValue(double deltaValue) { deltaValue_ = deltaValue; }
    double  GetRangeMinimum() const { return rangeMinimum_; }
    void    SetRangeMinimum(double rangeMinimum) { rangeMinimum_ = rangeMinimum; }
    double  GetRangeMaximum() const { return rangeMaximum_; }
    void    SetRangeMaximum(double rangeMaximum) { rangeMaximum_ = rangeMaximum; }
    bool    GetProvideFeedback() { return provideFeedback_; }
       
    void SetStringParam(const char *stringParam) 
    {
        stringParam_ = stringParam;
        RequestUpdate();
    }

    void SetStepValues(const vector<double> &steppedValues) 
    {
        steppedValues_ = steppedValues;
        if (steppedValuesIndex_ >= steppedValues.size())
            steppedValuesIndex_ = 0;
        RequestUpdate();
    }

    void DoTouch(double value)
    {
        action_->Touch(this, value);
    }

    void SetRange(const vector<double> &range)
    {
        if (range.size() != 2)
            return;
        
        rangeMinimum_ = range[0];
        rangeMaximum_ = range[1];
    }
        
    void SetSteppedValueIndex(double value)
    {
        int index = 0;
        double delta = 100000000.0;
        
        for (int i = 0; i < steppedValues_.size(); ++i)
            if (fabs(steppedValues_[i] - value) < delta)
            {
                delta = fabs(steppedValues_[i] - value);
                index = i;
            }
        
        steppedValuesIndex_ = index;
    }

    char *GetPanValueString(double panVal, const char *dualPan, char *buf, int bufsz) const
    {
        bool left = false;
        
        if (panVal < 0)
        {
            left = true;
            panVal = -panVal;
        }
        
        int panIntVal = int(panVal  *100.0);
        
        if (left)
        {
            const char *prefix;
            if (panIntVal == 100)
                prefix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                prefix = " ";
            else
                prefix = "  ";
            
            snprintf(buf, bufsz, "<%s%d%s", prefix, panIntVal, dualPan ? dualPan : "");
        }
        else
        {
            const char *suffix;
            
            if (panIntVal == 100)
                suffix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                suffix = " ";
            else
                suffix = "  ";

            snprintf(buf, bufsz, "  %s%d%s>", dualPan && *dualPan ? dualPan : " ", panIntVal, suffix);
        }
        
        if (panIntVal == 0)
        {
            if (!dualPan || !dualPan[0])
                lstrcpyn_safe(buf, "  <C>  ", bufsz);
            else if (!strcmp(dualPan, "L"))
                lstrcpyn_safe(buf, " L<C>  ", bufsz);
            else if (!strcmp(dualPan, "R"))
                lstrcpyn_safe(buf, " <C>R  ", bufsz);
        }

        return buf;
    }
    
    char *GetPanWidthValueString(double widthVal, char *buf, int bufsz) const
    {
        bool reversed = false;
        
        if (widthVal < 0)
        {
            reversed = true;
            widthVal = -widthVal;
        }
        
        int widthIntVal = int(widthVal  *100.0);
        
        if (widthIntVal == 0)
            lstrcpyn_safe(buf, "<Mono> ", bufsz);
        else
            snprintf(buf, bufsz, "%s %d", reversed ? "Rev" : "Wid", widthIntVal);
        
        return buf;
    }

    const char* GetFreeFormText() const { return m_freeFormText.c_str(); }
    void SetFreeFormText(const char* text) { m_freeFormText = (text ? text : ""); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Zone
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    ZoneManager  *const zoneManager_;
    CSurfIntegrator *const csi_;
    Navigator *navigator_;
    int slotIndex_;
    string const name_;
    string const alias_;
    string const sourceFilePath_;
    
    bool isActive_= false;
    
    // these do not own the widgets, ultimately the ControlSurface contains the list of widgets
    vector<Widget *> widgets_;
      
    vector<unique_ptr<ActionContext>> emptyContexts_;
    map<Widget *, int> currentActionContextModifiers_;
    map<Widget *, map<int, vector<unique_ptr<ActionContext>>>> actionContextDictionary_;

    vector<unique_ptr<Zone>> includedZones_;
    vector<unique_ptr<Zone>> subZones_;

    void UpdateCurrentActionContextModifier(Widget *widget);
    
public:
    Zone(CSurfIntegrator *const csi, ZoneManager  *const zoneManager, Navigator *navigator, int slotIndex, const string &name, const string &alias, const string &sourceFilePath): csi_(csi), zoneManager_(zoneManager), navigator_(navigator), slotIndex_(slotIndex), name_(name), alias_(alias), sourceFilePath_(sourceFilePath) {}

    virtual ~Zone()
    {
        includedZones_.clear();
        subZones_.clear();
        actionContextDictionary_.clear();
    }
    
    void InitSubZones(const vector<string> &subZones, const char *widgetSuffix);
    int GetSlotIndex();
    void SetXTouchDisplayColors(const char *colors);
    void RestoreXTouchDisplayColors();
    void UpdateCurrentActionContextModifiers();
    
    const vector<unique_ptr<ActionContext>> &GetActionContexts(Widget *widget);
    ActionContext *AddActionContext(Widget *widget, int modifier, Zone *zone, const char *actionName, vector<string> &params);

    void AddWidget(Widget *widget);
    void Activate();
    void Deactivate();
    void DoAction(Widget *widget, bool &isUsed, double value);
    void DoRelativeAction(Widget *widget, bool &isUsed, double delta);
    void DoRelativeAction(Widget *widget, bool &isUsed, int accelerationIndex, double delta);
    void DoTouch(Widget *widget, const char *widgetName, bool &isUsed, double value);
    void RequestUpdate();
    const vector<Widget *> &GetWidgets() { return widgets_; }

    const char *GetSourceFilePath() { return sourceFilePath_.c_str(); }
    vector<unique_ptr<Zone>> &GetIncludedZones() { return includedZones_; }

    Navigator *GetNavigator() { return navigator_; }
    void SetNavigator(Navigator *navigator) {  navigator_ = navigator; }
    void SetSlotIndex(int index) { slotIndex_ = index; }
    bool GetIsActive() { return isActive_; }
        
    void Toggle()
    {
        if (isActive_)
            Deactivate();
        else
            Activate();
    }

    const char *GetName()
    {
        return name_.c_str();
    }
    
    const char *GetAlias()
    {
        if (alias_.size() > 0)
            return alias_.c_str();
        else
            return name_.c_str();
    }
            
    const vector<unique_ptr<ActionContext>> &GetActionContexts(Widget *widget, int modifier)
    {
        if(actionContextDictionary_.count(widget) > 0 && actionContextDictionary_[widget].count(modifier) > 0)
            return actionContextDictionary_[widget][modifier];
        else
            return emptyContexts_;
    }
    
    void OnTrackDeselection()
    {
        isActive_ = true;
        
        for (auto &includedZone : includedZones_)
            includedZone->Activate();
    }

    void RequestUpdateWidget(Widget *widget)
    {
        for (auto &actionContext : GetActionContexts(widget))
        {
            actionContext->RunDeferredActions();
            actionContext->RequestUpdate();
        }
    }
    
    virtual void GoSubZone(const char *subZoneName)
    {
        for (auto &subZone : subZones_)
        {
            if ( ! strcmp(subZone->GetName(), subZoneName))
            {
                subZone->SetSlotIndex(GetSlotIndex());
                subZone->Activate();
            }
            else
                subZone->Deactivate();
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SubZone : public Zone
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    Zone  *const enclosingZone_;
    
public:
    SubZone(CSurfIntegrator *const csi, ZoneManager  *const zoneManager, Navigator *navigator, int slotIndex, const string &name, const string &alias, const string &sourceFilePath, Zone *enclosingZone) : Zone(csi, zoneManager, navigator, slotIndex, name, alias, sourceFilePath), enclosingZone_(enclosingZone) {}

    virtual ~SubZone() {}
    
    virtual void GoSubZone(const char *subZoneName) override
    {
        enclosingZone_->GoSubZone(subZoneName);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    Widget  *const widget_;

    double lastDoubleValue_ = 0.0;
    string lastStringValue_;
    rgba_color lastColor_;

public:
    FeedbackProcessor(CSurfIntegrator *const csi, Widget *widget) : csi_(csi), widget_(widget) {}
    virtual ~FeedbackProcessor() {}
    virtual const char *GetName()  { return "FeedbackProcessor"; }
    Widget *GetWidget() { return widget_; }
    virtual void Configure(const vector<unique_ptr<ActionContext>> &contexts) {}
    virtual void ForceValue(const PropertyList &properties, double value) {}
    virtual void ForceValue(const PropertyList &properties, const char * const &value) {}
    virtual void ForceColorValue(const rgba_color &color) {}
    virtual void ForceUpdateTrackColors() {}
    virtual void RunDeferredActions() {}
    virtual void ForceClear() {}

    virtual void SetXTouchDisplayColors(const char *colors) {}
    virtual void RestoreXTouchDisplayColors() {}

    virtual void SetColorValue(const rgba_color &color) {}

    virtual void SetValue(const PropertyList &properties, double value)
    {
        if (lastDoubleValue_ != value)
        {
            lastDoubleValue_ = value;
            ForceValue(properties, value);
        }
    }

    virtual void SetValue(const PropertyList &properties, const char * const & value)
    {
        if (lastStringValue_ != value)
        {
            lastStringValue_ = value;
            ForceValue(properties, value);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Midi_FeedbackProcessor : public FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    Midi_ControlSurface *const surface_;

    MIDI_event_ex_t lastMessageSent_;
    MIDI_event_ex_t midiFeedbackMessage1_;
    MIDI_event_ex_t midiFeedbackMessage2_;

    Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget) : FeedbackProcessor(csi, widget), surface_(surface) {}

    Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : FeedbackProcessor(csi, widget), surface_(surface), midiFeedbackMessage1_(feedback1) {}

    Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2) : FeedbackProcessor(csi, widget), surface_(surface), midiFeedbackMessage1_(feedback1), midiFeedbackMessage2_(feedback2) {}

    void SendMidiSysExMessage(MIDI_event_ex_t *midiMessage);
    void SendMidiMessage(int first, int second, int third);
    void ForceMidiMessage(int first, int second, int third);
    void LogMessage(char* value);

public:
    ~Midi_FeedbackProcessor()
    { }

    virtual const char *GetName() override { return "Midi_FeedbackProcessor"; }
};

void ReleaseMidiInput(midi_Input *input);
void ReleaseMidiOutput(midi_Output *output);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Widget
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    ControlSurface *const surface_;
    string const name_;
    vector<unique_ptr<FeedbackProcessor>> feedbackProcessors_; // owns the objects
    int channelNumber_ = 0;
    int lastIncomingMessageTime_ = GetTickCount() - 30000;
    double lastIncomingDelta_ = 0.0;
    
    double stepSize_ = 0.0;
    vector<double> accelerationValues_;
    
    bool hasBeenUsedByUpdate_ = false;
    
    bool isTwoState_ = false;
    
public:
    // all Widgets are owned by their ControlSurface!
    Widget(CSurfIntegrator *const csi,  ControlSurface *surface, const char *name) : csi_(csi), surface_(surface), name_(name)
    {
        int index = (int)strlen(name) - 1;
        if (isdigit(name[index]))
        {
            while (index>=0 && isdigit(name[index]))
                index--;
               
            index++;
            
            channelNumber_ = atoi(name + index);
        }
    }
    
    ~Widget()
    {
      feedbackProcessors_.clear();
    }
    
    vector<unique_ptr<FeedbackProcessor>> &GetFeedbackProcessors() { return feedbackProcessors_; }
    
    void ClearHasBeenUsedByUpdate() { hasBeenUsedByUpdate_ = false; }
    void SetHasBeenUsedByUpdate() { hasBeenUsedByUpdate_ = true; }
    bool GetHasBeenUsedByUpdate() { return hasBeenUsedByUpdate_; }
    
    const char *GetName() { return name_.c_str(); }
    ControlSurface *GetSurface() { return surface_; }
    ZoneManager *GetZoneManager();
    int GetChannelNumber() { return channelNumber_; }
    
    void SetStepSize(double stepSize) { stepSize_ = stepSize; }
    double GetStepSize() { return stepSize_; }
    bool GetIsTwoState() { return isTwoState_; }
    void SetIsTwoState() { isTwoState_ = true; }
    
    void SetAccelerationValues(const vector<double> accelerationValues) { accelerationValues_ = accelerationValues; }
    const vector<double> &GetAccelerationValues() { return accelerationValues_; }
    
    void SetIncomingMessageTime(int lastIncomingMessageTime) { lastIncomingMessageTime_ = lastIncomingMessageTime; }
    int GetLastIncomingMessageTime() { return lastIncomingMessageTime_; }
    
    void SetLastIncomingDelta(double delta) { lastIncomingDelta_ = delta; }
    double GetLastIncomingDelta() { return lastIncomingDelta_; }

    void Configure(const vector<unique_ptr<ActionContext>> &contexts);
    void UpdateValue(const PropertyList &properties, double value);
    void UpdateValue(const PropertyList &properties, const char * const &value);
    void ForceValue(const PropertyList &properties, const char * const &value);
    void RunDeferredActions();
    void UpdateColorValue(const rgba_color &color);
    void SetXTouchDisplayColors(const char *colors);
    void RestoreXTouchDisplayColors();
    void ForceClear();
    void LogInput(double value);
};

////////////////////////////
// For Zone Manager
////////////////////////////

void GetPropertiesFromTokens(int start, int finish, const vector<string> &tokens, PropertyList &properties);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct CSIZoneInfo
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    string filePath;
    string alias;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ZoneManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    CSurfIntegrator *const csi_;
    ControlSurface *const surface_;
    string const zoneFolder_;
    string const fxZoneFolder_;
   
    vector<unique_ptr<ActionContext>> emptyContexts_;
    
    map<const string, CSIZoneInfo> zoneInfo_;

    shared_ptr<Zone> learnFocusedFXZone_ = NULL;

    unique_ptr<Zone> homeZone_;

    vector<unique_ptr<Zone>> goZones_;

    vector<ZoneManager *> listeners_; // does not own pointers
    
    vector<shared_ptr<Zone>> zonesToBeDeleted_;
    
    bool listensToGoHome_ = false;
    bool listensToSends_ = false;
    bool listensToReceives_ = false;
    bool listensToFXMenu_ = false;
    bool usesLocalFXSlot_ = false;
    bool listensToSelectedTrackFX_ = false;

    shared_ptr<Zone> lastTouchedFXParamZone_ = NULL;
    bool isLastTouchedFXParamMappingEnabled_= false;
    
    shared_ptr<Zone> focusedFXZone_ = NULL;
    bool isFocusedFXMappingEnabled_ = true;
    
    vector<shared_ptr<Zone>> selectedTrackFXZones_;
    shared_ptr<Zone> fxSlotZone_ = NULL;
    
    int trackSendOffset_ = 0;
    int trackReceiveOffset_ = 0;
    int trackFXMenuOffset_ = 0;
    int selectedTrackSendOffset_ = 0;
    int selectedTrackReceiveOffset_ = 0;
    int selectedTrackFXMenuOffset_ = 0;
    int masterTrackFXMenuOffset_ = 0;

    void GoFXSlot(MediaTrack *track, Navigator *navigator, int fxSlot);
    void GoSelectedTrackFX();
    void GetWidgetNameAndModifiers(const string &line, string &baseWidgetName, int &modifier, bool &isValueInverted, bool &isFeedbackInverted, bool &hasHoldModifier, bool &isDecrease, bool &isIncrease);
    void GetNavigatorsForZone(const char *zoneName, const char *navigatorName, vector<Navigator *> &navigators);
    void LoadZones(vector<unique_ptr<Zone>> &zones, vector<string> &zoneList);
         
    void DoAction(Widget *widget, double value, bool &isUsed);
    void DoRelativeAction(Widget *widget, double delta, bool &isUsed);
    void DoRelativeAction(Widget *widget, int accelerationIndex, double delta, bool &isUsed);
    void DoTouch(Widget *widget, double value, bool &isUsed);
    
    void LoadZoneMetadata(const char *filePath, vector<string> &metadata)
    {
        int lineNumber = 0;
        
        try
        {
            ifstream file(filePath);
            
            if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] LoadZoneMetadata: %s\n", GetRelativePath(filePath));
            for (string line; getline(file, line) ; )
            {
                TrimLine(line);
                
                lineNumber++;
                
                if (line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                    continue;
                
                vector<string> tokens;
                GetTokens(tokens, line);
                
                if (tokens[0] == "Zone" || tokens[0] == "ZoneEnd")
                    continue;
                
                metadata.push_back(line);
            }
        }
        catch (const std::exception& e)
        {
            LogToConsole(256, "[ERROR] FAILED to LoadZoneMetadata in %s, around line %d\n", filePath, lineNumber);
            LogToConsole(2048, "Exception: %s\n", e.what());
        }
    }
    
    void AdjustBank(int &bankOffset, int amount)
    {
        bankOffset += amount;
            
        if (bankOffset < 0)
            bankOffset = 0;
    }
    
    void ResetOffsets()
    {
        trackSendOffset_ = 0;
        trackReceiveOffset_ = 0;
        trackFXMenuOffset_ = 0;
        selectedTrackSendOffset_ = 0;
        selectedTrackReceiveOffset_ = 0;
        selectedTrackFXMenuOffset_ = 0;
        masterTrackFXMenuOffset_ = 0;
    }
    
    void ResetSelectedTrackOffsets()
    {
        selectedTrackSendOffset_ = 0;
        selectedTrackReceiveOffset_ = 0;
        selectedTrackFXMenuOffset_ = 0;
    }
   
    bool GetIsListener()
    {
        return listensToGoHome_ || listensToSends_ || listensToReceives_ || listensToFXMenu_ || listensToSelectedTrackFX_;
    }
    
    void ListenToGoZone(const char *zoneName)
    {
        if (!strcmp("SelectedTrackSend", zoneName))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToSends())
                    listener->GoZone(zoneName);
            }
        else if (!strcmp("SelectedTrackReceive", zoneName))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToReceives())
                    listener->GoZone(zoneName);
            }
        else if (!strcmp("SelectedTrackFX", zoneName))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToSelectedTrackFX())
                    listener->GoZone(zoneName);
            }
        else if (!strcmp("SelectedTrackFXMenu", zoneName))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToFXMenu())
                    listener->GoZone(zoneName);
            }
        else
            GoZone(zoneName);
    }
    
    void ListenToClearFXZone(const char *zoneToClear)
    {
        if (!strcmp("LastTouchedFXParam", zoneToClear))
            for (auto &listener : listeners_)
                listener->ClearLastTouchedFXParam();
        else if (!strcmp("FocusedFX", zoneToClear))
            for (auto &listener : listeners_)
                listener->ClearFocusedFX();
        else if (!strcmp("SelectedTrackFX", zoneToClear))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToSelectedTrackFX())
                    listener->GoZone(zoneToClear);
            }
        else if (!strcmp("FXSlot", zoneToClear))
            for (auto &listener : listeners_)
            {
                if (listener->GetListensToFXMenu())
                    listener->GoZone(zoneToClear);
            }
    }
    
    void ListenToGoHome()
    {
        if (listensToGoHome_)
            GoHome();
    }
    
    void ListenToGoFXSlot(MediaTrack *track, Navigator *navigator, int fxSlot)
    {
        if (listensToFXMenu_)
            GoFXSlot(track, navigator, fxSlot);
    }

    void ListenToToggleEnableLastTouchedFXParamMapping()
    {
        ToggleEnableLastTouchedFXParamMapping();
    }

    void ToggleEnableLastTouchedFXParamMapping()
    {
        isLastTouchedFXParamMappingEnabled_ = ! isLastTouchedFXParamMappingEnabled_;
        
        if (lastTouchedFXParamZone_ != NULL)
        {
            if (isLastTouchedFXParamMappingEnabled_)
                lastTouchedFXParamZone_->Activate();
            else
                lastTouchedFXParamZone_->Deactivate();
        }
    }

    void ListenToToggleEnableFocusedFXMapping()
    {
        ToggleEnableFocusedFXMapping();
    }

    void ToggleEnableFocusedFXMapping()
    {
        isFocusedFXMappingEnabled_ = ! isFocusedFXMappingEnabled_;
    }

    void ClearLastTouchedFXParam()
    {
        if (lastTouchedFXParamZone_ != NULL)
        {
            lastTouchedFXParamZone_->Deactivate();
            zonesToBeDeleted_.push_back(lastTouchedFXParamZone_);
            lastTouchedFXParamZone_ = NULL;
        }
    }
    
    void ClearFocusedFX()
    {
        if (focusedFXZone_ != NULL)
        {
            focusedFXZone_->Deactivate();
            zonesToBeDeleted_.push_back(focusedFXZone_);
            focusedFXZone_ = NULL;
        }
    }
        
    void ClearSelectedTrackFX()
    {
        for (auto &selectedTrackFXZone : selectedTrackFXZones_)
        {
            selectedTrackFXZone->Deactivate();
            zonesToBeDeleted_.push_back(selectedTrackFXZone);
        }
        
        selectedTrackFXZones_.clear();
    }
    
    void ClearFXSlot()
    {
        if (fxSlotZone_ != NULL)
        {
            fxSlotZone_->Deactivate();
            zonesToBeDeleted_.push_back(fxSlotZone_);
            fxSlotZone_ = NULL;
            ReactivateFXMenuZone();
        }
    }

    void ReactivateFXMenuZone()
    {
        for (int i = 0; i < goZones_.size(); ++i)
            if (!strcmp(goZones_[i]->GetName(), "TrackFXMenu") || !strcmp(goZones_[i]->GetName(), "SelectedTrackFXMenu"))
                if (goZones_[i]->GetIsActive())
                    goZones_[i]->Activate();
    }

public:
    ZoneManager(CSurfIntegrator *const csi, ControlSurface *surface, const string &zoneFolder, const string &fxZoneFolder);

    ~ZoneManager()
    {            
        focusedFXZone_ = NULL;
        lastTouchedFXParamZone_ = NULL;
        fxSlotZone_ = NULL;
        learnFocusedFXZone_ = NULL;
        selectedTrackFXZones_.clear();
        
        goZones_.clear();

        zoneInfo_.clear();
    }
    
    void Initialize();
    
    Navigator *GetNavigatorForTrack(MediaTrack* track);
    Navigator *GetMasterTrackNavigator();
    Navigator *GetSelectedTrackNavigator();
    Navigator *GetFocusedFXNavigator();
    
    bool GetIsBroadcaster() { return  ! (listeners_.size() == 0); }
    void AddListener(ControlSurface *surface);
    void SetListenerCategories(PropertyList &pList);
    const vector<ZoneManager *> &GetListeners() { return listeners_; }
    void ToggleUseLocalFXSlot() { usesLocalFXSlot_ = ! usesLocalFXSlot_; }
    bool GetToggleUseLocalFXSlot() { return usesLocalFXSlot_; }

    bool GetListensToSends() { return listensToSends_; }
    bool GetListensToReceives() { return listensToReceives_; }
    bool GetListensToFXMenu() { return listensToFXMenu_; }
    bool GetListensToSelectedTrackFX() { return listensToSelectedTrackFX_; }
    
    int  GetNumChannels();
    
    void PreProcessZones();
    void PreProcessZoneFile(const string &filePath);
    void LoadZoneFile(Zone *zone, const char *widgetSuffix);
    void LoadZoneFile(Zone *zone, const char *filePath, const char *widgetSuffix);

    void UpdateCurrentActionContextModifiers();
    void CheckFocusedFXState();

    void DoAction(Widget *widget, double value);
    void DoRelativeAction(Widget *widget, double delta);
    void DoRelativeAction(Widget *widget, int accelerationIndex, double delta);
    void DoTouch(Widget *widget, double value);
    
    const char *GetFXZoneFolder() { return fxZoneFolder_.c_str(); }
    map<const string, CSIZoneInfo> &GetZoneInfo() { return zoneInfo_; }

    CSurfIntegrator *GetCSI() { return csi_; }
    ControlSurface *GetSurface() { return surface_; }
    
    int GetTrackSendOffset() { return trackSendOffset_; }
    int GetTrackReceiveOffset() { return trackReceiveOffset_; }
    int GetTrackFXMenuOffset() { return trackFXMenuOffset_; }
    int GetSelectedTrackSendOffset() { return selectedTrackSendOffset_; }
    int GetSelectedTrackReceiveOffset() { return selectedTrackReceiveOffset_; }
    int GetSelectedTrackFXMenuOffset() { return selectedTrackFXMenuOffset_; }
    int GetMasterTrackFXMenuOffset() { return masterTrackFXMenuOffset_; }

    bool GetIsFocusedFXMappingEnabled() { return isFocusedFXMappingEnabled_; }
    bool GetIsLastTouchedFXParamMappingEnabled() { return isLastTouchedFXParamMappingEnabled_; }
    
    Zone *GetLearnedFocusedFXZone() { return  learnFocusedFXZone_.get();  }
    
    const vector<unique_ptr<ActionContext>> &GetLearnFocusedFXActionContexts(Widget *widget, int modifier)
    {
        if (learnFocusedFXZone_ != NULL)
            return learnFocusedFXZone_->GetActionContexts(widget, modifier);
        else
            return emptyContexts_;
    }
        
    void GetAlias(const char *fxName, char *alias, int aliassz)
    {
        static const char  *const prefixes[] =
        {
            "AU: Tube-Tech ",
            "AU: AU ",
            "AU: UAD UA ",
            "AU: UAD Pultec ",
            "AU: UAD Tube-Tech ",
            "AU: UAD Softube ",
            "AU: UAD Teletronix ",
            "AU: UADx ",
            "AU: UAD ",
            "AU: ",
            "AUi: ",
            "VST: TDR ",
            "VST: UAD UA ",
            "VST: UAD Pultec ",
            "VST: UAD Tube-Tech ",
            "VST: UAD Softube ",
            "VST: UAD Teletronix ",
            "VST: UAD ",
            "VST3: UADx ",
            "VST3i: UADx ",
            "VST: ",
            "VSTi: ",
            "VST3: ",
            "VST3i: ",
            "JS: ",
            "Rewire: ",
            "CLAP: ",
            "CLAPi: ",
        };
        
        // skip over known prefixes
        for (int i = 0; i < NUM_ELEM(prefixes); ++i)
        {
            const int l = (int) strlen(prefixes[i]);
            if (!strncmp(fxName, prefixes[i], l))
            {
                fxName += l;
                break;
            }
        }

        lstrcpyn_safe(alias, fxName, aliassz);
        char *p = strstr(alias," (");
        if (p) *p = 0;
    }
    
    void ClearLearnFocusedFXZone()
    {
        if (learnFocusedFXZone_ != NULL)
        {
            learnFocusedFXZone_->Deactivate();
            zonesToBeDeleted_.push_back(learnFocusedFXZone_);
            learnFocusedFXZone_ = NULL;
        }
    }
    
    void LoadLearnFocusedFXZone(MediaTrack *track, const char *fxName, int fxIndex)
    {
        if (zoneInfo_.find(fxName) != zoneInfo_.end())
        {
            learnFocusedFXZone_ = make_shared<Zone>(csi_, this, GetNavigatorForTrack(track), fxIndex, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
            LoadZoneFile(learnFocusedFXZone_.get(), "");
            
            learnFocusedFXZone_->Activate();
        }
        else
        {
            char alias[BUFSIZ];
            GetAlias(fxName, alias, sizeof(alias));

            CSIZoneInfo info;
            info.alias = alias;
            
            char fxFilePath[BUFSIZ];
            snprintf(fxFilePath, sizeof(fxFilePath), "%s/AutoGeneratedFXZones", GetFXZoneFolder());
            RecursiveCreateDirectory(fxFilePath, 0);
            string fxFileName = fxName;
            ReplaceAllWith(fxFileName, s_BadFileChars, "_");
            char fxFullFilePath[BUFSIZ];
            snprintf(fxFullFilePath, sizeof(fxFullFilePath), "%s/%s.zon", fxFilePath, fxFileName.c_str());
            info.filePath = fxFullFilePath;
            
            learnFocusedFXZone_ = make_shared<Zone>(csi_, this, GetNavigatorForTrack(track), fxIndex, fxName, info.alias, info.filePath);
            
            if (learnFocusedFXZone_)
            {
                InitBlankLearnFocusedFXZone(this, learnFocusedFXZone_.get(), track, fxIndex);
                learnFocusedFXZone_->Activate();
            }
        }
    }
                
    void DeclareGoZone(const char *zoneName)
    {
        if (! GetIsBroadcaster() && ! GetIsListener()) // No Broadcasters/Listeners relationships defined
            GoZone(zoneName);
        else
            for (auto &listener : listeners_)
                listener->ListenToGoZone(zoneName);
    }
    
    void GoZone(const char *zoneName)
    {
        ClearFXMapping();
        ResetOffsets();
        
        for (int i = 0; i < goZones_.size(); ++i)
        {
            if (!strcmp(zoneName, goZones_[i]->GetName()))
            {
                if (goZones_[i]->GetIsActive())
                {
                    for (int j = i; j < goZones_.size(); ++j)
                        if (!strcmp(zoneName, goZones_[j]->GetName()))
                            goZones_[j]->Deactivate();
                    
                    return;
                }
            }
        }
        
        for (auto &goZone : goZones_)
            if (strcmp(zoneName, goZone->GetName()))
                goZone->Deactivate();
        
        for (auto &goZone : goZones_)
            if (!strcmp(zoneName, goZone->GetName()))
               goZone->Activate();
        
        if ( ! strcmp(zoneName, "SelectedTrackFX"))
            GoSelectedTrackFX();
    }
    
    void DeclareClearFXZone(const char *zoneName)
    {
        if (! GetIsBroadcaster() && ! GetIsListener()) // No Broadcasters/Listeners relationships defined
        {
            if (!strcmp("LastTouchedFXParam", zoneName))
                ClearLastTouchedFXParam();
            else if (!strcmp("FocusedFX", zoneName))
                ClearFocusedFX();
            else if (!strcmp("SelectedTrackFX", zoneName))
                ClearSelectedTrackFX();
            else if (!strcmp("FXSlot", zoneName))
                ClearFXSlot();
        }
        else
            for (auto &listener : listeners_)
                listener->ListenToClearFXZone(zoneName);
    }
    
    void DeclareGoFXSlot(MediaTrack *track, Navigator *navigator, int fxSlot)
    {
        if (usesLocalFXSlot_ || ( ! GetIsBroadcaster() && ! GetIsListener())) // No Broadcasters/Listeners relationships defined
            GoFXSlot(track, navigator, fxSlot);
        else
            for (auto &listener : listeners_)
                listener->ListenToGoFXSlot(track, navigator, fxSlot);
    }
                      
    void GetName(MediaTrack *track, int fxIndex, char *name, int namesz)
    {
        char fxName[MEDBUF];
        TrackFX_GetFXName(track, fxIndex, fxName, sizeof(fxName));

        if (zoneInfo_.find(fxName) != zoneInfo_.end())
            lstrcpyn_safe(name, zoneInfo_[fxName].alias.c_str(), namesz);
        else
            GetAlias(fxName, name, namesz);
    }
          
    void HideAllFXWindows()
    {
        for (int i = -1; i < GetNumTracks(); ++i)
        {
            MediaTrack *tr = i < 0 ? GetMasterTrack(NULL) : GetTrack(NULL, i);
            if (WDL_NOT_NORMALLY(tr == NULL)) continue;

            for (int j = TrackFX_GetCount(tr) - 1; j >= -1; j --)
                TrackFX_Show(tr, j, j < 0 ? 0 : 2);

            for (int j = CountTrackMediaItems(tr) - 1; j >= 0; j --)
            {
                MediaItem *item = GetTrackMediaItem(tr, j);
                for (int k = CountTakes(item)-1; k >= 0; k --)
                {
                    MediaItem_Take *take = GetMediaItemTake(item, k);
                    if (take)
                    {
                        for (int l = TakeFX_GetCount(take) - 1; l >= -1; l --)
                            TakeFX_Show(take, l, l < 0 ? 0 : 2);
                    }
                }
            }
        }
    }

    void GoHome()
    {
        HideAllFXWindows();
        ClearFXMapping();
        ResetOffsets();

        for (auto &goZone : goZones_)
            goZone->Deactivate();
        
        homeZone_->Activate();
    }
    
    void DeclareGoHome()
    {
        if (! GetIsBroadcaster() && ! GetIsListener()) // No Broadcasters/Listeners relationships defined
            GoHome();
        else
            for (auto &listener : listeners_)
                listener->ListenToGoHome();
    }
        
    void OnTrackSelection()
    {
        if (fxSlotZone_ != NULL)
            ClearFXSlot();
    }

    void OnTrackDeselection()
    {
        ResetSelectedTrackOffsets();
        
        selectedTrackFXZones_.clear();
        
        for (auto &goZone : goZones_)
        {
            if (!strcmp(goZone->GetName(), "SelectedTrack") ||
                !strcmp(goZone->GetName(), "SelectedTrackSend") ||
                !strcmp(goZone->GetName(), "SelectedTrackReceive") ||
                !strcmp(goZone->GetName(), "SelectedTrackFXMenu"))
            {
                goZone->Deactivate();
            }
        }
        
        homeZone_->OnTrackDeselection();
    }
    
    void DeclareToggleEnableLastTouchedFXParamMapping()
    {
        if (! GetIsBroadcaster() && ! GetIsListener()) // No Broadcasters/Listeners relationships defined
            ToggleEnableLastTouchedFXParamMapping();
        else
            for (auto &listener : listeners_)
                listener->ListenToToggleEnableLastTouchedFXParamMapping();
    }

    void DisableLastTouchedFXParamMapping()
    {
        isLastTouchedFXParamMappingEnabled_ = false;
    }
    
    void DeclareToggleEnableFocusedFXMapping()
    {
        if (! GetIsBroadcaster() && ! GetIsListener()) // No Broadcasters/Listeners relationships defined
            ToggleEnableFocusedFXMapping();
        else
            for (auto &listener : listeners_)
                listener->ListenToToggleEnableFocusedFXMapping();
    }
    
    void DisableFocusedFXMapping()
    {
        isFocusedFXMappingEnabled_ = false;
    }
    
    bool GetIsGoZoneActive(const char *zoneName)
    {
        for (auto &goZone : goZones_)
            if (!strcmp(zoneName, goZone->GetName()))
                return goZone->GetIsActive();
        
        return false;
    }
    
    bool GetIsHomeZoneOnlyActive()
    {
        for (auto &goZone : goZones_)
            if (goZone->GetIsActive())
                return false;

        return true;
    }

    void ClearFXMapping()
    {
        ClearLearnFocusedFXZone();
        CloseFocusedFXDialog();
        ClearFocusedFX();
        ClearSelectedTrackFX();
        ClearFXSlot();
    }
        
    void AdjustBank(const char *zoneName, int amount)
    {
        if (!strcmp(zoneName, "TrackSend"))
            AdjustBank(trackSendOffset_, amount);
        else if (!strcmp(zoneName, "TrackReceive"))
            AdjustBank(trackReceiveOffset_, amount);
        else if (!strcmp(zoneName, "TrackFXMenu"))
            AdjustBank(trackFXMenuOffset_, amount);
        else if (!strcmp(zoneName, "SelectedTrackSend"))
            AdjustBank(selectedTrackSendOffset_, amount);
        else if (!strcmp(zoneName, "SelectedTrackReceive"))
            AdjustBank(selectedTrackReceiveOffset_, amount);
        else if (!strcmp(zoneName, "SelectedTrackFXMenu"))
            AdjustBank(selectedTrackFXMenuOffset_, amount);
        else if (!strcmp(zoneName, "MasterTrackFXMenu"))
            AdjustBank(masterTrackFXMenuOffset_, amount);
    }
                
    void AddZoneFilePath(const string &name, CSIZoneInfo &zoneInfo)
    {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(256, "[DEBUG] AddZoneFilePath %s\n", GetRelativePath(name.c_str()));
        if (zoneInfo_.find(name) == zoneInfo_.end())
            zoneInfo_[name] = zoneInfo;
        else
        {
            CSIZoneInfo &info = zoneInfo_[name];
            info.alias = zoneInfo.alias;
        }
    }

    void RequestUpdate()
    {
        CheckFocusedFXState();
          
        if (learnFocusedFXZone_ != NULL)
        {
            UpdateLearnWindow(this);
            learnFocusedFXZone_->RequestUpdate();
        }

        if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
            lastTouchedFXParamZone_->RequestUpdate();

        if (focusedFXZone_ != NULL)
            focusedFXZone_->RequestUpdate();
        
        for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
            selectedTrackFXZones_[i]->RequestUpdate();
        
        if (fxSlotZone_ != NULL)
            fxSlotZone_->RequestUpdate();
        
        for (int i = 0; i < goZones_.size(); ++i)
            goZones_[i]->RequestUpdate();

        if (homeZone_ != NULL)
            homeZone_->RequestUpdate();
        
        zonesToBeDeleted_.clear();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    Widget  *const widget_;
    
public:
    CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : csi_(csi), widget_(widget) {}
    virtual ~CSIMessageGenerator() {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) {}
    virtual void ProcessMessage(double value)
    {
        widget_->GetZoneManager()->DoAction(widget_, value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AnyPress_CSIMessageGenerator : public CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    AnyPress_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : CSIMessageGenerator(csi, widget) {}
    virtual ~AnyPress_CSIMessageGenerator() {}
    
    virtual void ProcessMessage(double value) override
    {
        widget_->GetZoneManager()->DoAction(widget_, 1.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Touch_CSIMessageGenerator : public CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    Touch_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : CSIMessageGenerator(csi, widget) {}
    virtual ~Touch_CSIMessageGenerator() {}
    
    virtual void ProcessMessage(double value) override
    {
        widget_->GetZoneManager()->DoTouch(widget_, value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Midi_CSIMessageGenerator : public CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : CSIMessageGenerator(csi, widget) {}
    
public:
    virtual ~Midi_CSIMessageGenerator() {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ModifierManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    CSurfIntegrator *const csi_;
    Page *page_;
    ControlSurface *surface_;
    
    int latchTime_ = 100;
    
    enum Modifiers
    {
        ErrorModifier = -1,
        Shift = 0,
        Option,
        Control,
        Alt,
        Flip,
        Global,
        Marker,
        Nudge,
        Zoom,
        Scrub,
        MaxModifiers
    };
    
    static int maskFromModifier(Modifiers m)
    {
      if (WDL_NOT_NORMALLY(m == ErrorModifier)) return 0;
      return 4 << (int) m;
    }
    
    static Modifiers modifierFromString(const char *s)
    {
         if (!strcmp(s,"Shift")) return Shift;
         if (!strcmp(s,"Option")) return Option;
         if (!strcmp(s,"Control")) return Control;
         if (!strcmp(s,"Alt")) return Alt;
         if (!strcmp(s,"Flip")) return Flip;
         if (!strcmp(s,"Global")) return Global;
         if (!strcmp(s,"Marker")) return Marker;
         if (!strcmp(s,"Nudge")) return Nudge;
         if (!strcmp(s,"Zoom")) return Zoom;
         if (!strcmp(s,"Scrub")) return Scrub;
         return ErrorModifier;
    }

    static const char *stringFromModifier(Modifiers mod)
    {
      switch (mod)
      {
        case Shift: return "Shift";
        case Option: return "Option";
        case Control: return "Control";
        case Alt: return "Alt";
        case Flip: return "Flip";
        case Global: return "Global";
        case Marker: return "Marker";
        case Nudge: return "Nudge";
        case Zoom: return "Zoom";
        case Scrub: return "Scrub";
        default:
          WDL_ASSERT(false);
        return "";
      }
    }

    struct ModifierState
    {
        bool isEngaged;
        bool isLocked;
        DWORD pressedTime;
    };
    
    ModifierState modifiers_[MaxModifiers];
    
    WDL_TypedBuf<int> modifierCombinations_;
    vector<int> modifierVector_;

    static int intcmp_rev(const void *a, const void *b) { return *(const int *)a > *(const int *)b ? -1 : *(const int *)a < *(const int *)b ? 1 : 0; }
    
    void GetCombinations(const Modifiers *indices, int num_indices, WDL_TypedBuf<int> &combinations)
    {
        for (int mask = 0; mask < (1 << num_indices); ++mask)
        {
            int combination = 0;
            
            for (int position = 0; position < num_indices; ++position)
                if (mask & (1 << position))
                    combination |= maskFromModifier(indices[position]);
            
            if (combination != 0)
                combinations.Add(combination);
        }
    }

    void SetLatchModifier(bool value, Modifiers modifier, int latchTime);

public:
    ModifierManager(CSurfIntegrator *const csi, Page *page = NULL, ControlSurface *surface = NULL) : csi_(csi), page_(page), surface_(surface)
    {
        int *p = modifierCombinations_.ResizeOK(1);
        if (WDL_NORMALLY(p)) p[0]=0;

        modifierVector_.push_back(0);
        
        memset(modifiers_,0,sizeof(modifiers_));
    }
    
    void RecalculateModifiers();
    const vector<int> &GetModifiers() { return modifierVector_; }

    bool GetShift() { return modifiers_[Shift].isEngaged; }
    bool GetOption() { return modifiers_[Option].isEngaged; }
    bool GetControl() { return modifiers_[Control].isEngaged; }
    bool GetAlt() { return modifiers_[Alt].isEngaged; }
    bool GetFlip() { return modifiers_[Flip].isEngaged; }
    bool GetGlobal() { return modifiers_[Global].isEngaged; }
    bool GetMarker() { return modifiers_[Marker].isEngaged; }
    bool GetNudge() { return modifiers_[Nudge].isEngaged; }
    bool GetZoom() { return modifiers_[Zoom].isEngaged; }
    bool GetScrub() { return modifiers_[Scrub].isEngaged; }

    void ClearModifier(const char *modifierString)
    {
        Modifiers m = modifierFromString(modifierString);
        if (m != ErrorModifier)
        {
            modifiers_[m].isEngaged = false;
            RecalculateModifiers();
        }
    }

    static char *GetModifierString(int modifierValue, char *buf, int bufsz)
    {
        buf[0]=0;
        for (int x = 0; x < MaxModifiers; ++x)
            if (modifierValue & maskFromModifier((Modifiers)x))
                snprintf_append(buf, bufsz, "%s+",stringFromModifier((Modifiers)x));

        return buf;
    }

    int GetModifierValue(const char *modifierString)
    {
        vector<string> modifierTokens;
        GetTokens(modifierTokens, modifierString, '+');
        return GetModifierValue(modifierTokens);
    }
    
    int GetModifierValue(const vector<string> &tokens)
    {
        int modifierValue = 0;
        for (int i = 0; i < tokens.size(); ++i)
        {
            Modifiers m = modifierFromString(tokens[i].c_str());
            if (m != ErrorModifier)
                modifierValue |= maskFromModifier(m);
        }
        
        return modifierValue;
    }

    void SetModifierValue(int value)
    {
        for (int i = 0; i < MaxModifiers; ++i)
            modifiers_[i].isEngaged = false;

        if (value & maskFromModifier(Shift))
            modifiers_[Shift].isEngaged = true;
     
        if (value & maskFromModifier(Option))
            modifiers_[Option].isEngaged = true;
     
        if (value & maskFromModifier(Control))
            modifiers_[Control].isEngaged = true;
     
        if (value & maskFromModifier(Alt))
            modifiers_[Alt].isEngaged = true;
     
        if (value & maskFromModifier(Flip))
            modifiers_[Flip].isEngaged = true;
        
        if (value & maskFromModifier(Global))
            modifiers_[Global].isEngaged = true;
     
        if (value & maskFromModifier(Marker))
            modifiers_[Marker].isEngaged = true;
     
        if (value & maskFromModifier(Nudge))
            modifiers_[Nudge].isEngaged = true;
     
        if (value & maskFromModifier(Zoom))
            modifiers_[Zoom].isEngaged = true;
     
        if (value & maskFromModifier(Scrub))
            modifiers_[Scrub].isEngaged = true;
    }
    
    void SetShift(bool value, int latchTime)
    {
        SetLatchModifier(value, Shift, latchTime);
    }

    void SetOption(bool value, int latchTime)
    {
        SetLatchModifier(value, Option, latchTime);
    }
    
    void SetControl(bool value, int latchTime)
    {
        SetLatchModifier(value, Control, latchTime);
    }
    
    void SetAlt(bool value, int latchTime)
    {
        SetLatchModifier(value, Alt, latchTime);
    }
  
    void SetFlip(bool value, int latchTime)
    {
        SetLatchModifier(value, Flip, latchTime);
    }
  
    void SetGlobal(bool value, int latchTime)
    {
        SetLatchModifier(value, Global, latchTime);
    }
    
    void SetMarker(bool value, int latchTime)
    {
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;

        SetLatchModifier(value, Marker, latchTime);
    }
    
    void SetNudge(bool value, int latchTime)
    {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;

        SetLatchModifier(value, Nudge, latchTime);
    }
  
    void SetZoom(bool value, int latchTime)
    {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;

        SetLatchModifier(value, Zoom, latchTime);
    }
  
    void SetScrub(bool value, int latchTime)
    {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;

        SetLatchModifier(value, Scrub, latchTime);
    }
    
    void ClearModifiers()
    {
        for (int i = 0; i < MaxModifiers; ++i)
            modifiers_[i].isEngaged = false;
        
        RecalculateModifiers();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ChannelTouch
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int channelNum = 0;;
    bool isTouched = false;
    
    ChannelTouch() {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ChannelToggle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int channelNum = 0;
    bool isToggled = false;
    
    ChannelToggle() {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ControlSurface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{    
private:
    int *scrubModePtr_ = NULL;
    int configScrubMode_ = 0;
    
    bool isRewinding_ = false;
    bool isFastForwarding_ = false;
    
    bool isTextLengthRestricted_ = false;
    int restrictedTextLength_ = 6;
    
    bool usesLocalModifiers_ = false;
    bool listensToModifiers_ = false;
        
    int latchTime_ = 100;
        
    vector<FeedbackProcessor *> trackColorFeedbackProcessors_; // does not own pointers
    
    vector<ChannelTouch> channelTouches_;
    vector<ChannelToggle> channelToggles_;

protected:
    map<const string, double> stepSize_;
    
    map<const string, map<int, int>> accelerationValuesForDecrement_;
    map<const string, map<int, int>> accelerationValuesForIncrement_;
    map<int, int> emptyAccelerationMap_;
    
    map<const string, vector<double>> accelerationValues_;
    vector<double> emptyAccelerationValues_;
    
    void ProcessValues(const vector<vector<string>> &lines);
    
    CSurfIntegrator *const csi_;
    Page *const page_;
    string const name_;
    unique_ptr<ZoneManager> zoneManager_;
    unique_ptr<ModifierManager> modifierManager_;
    
    int const numChannels_;
    int const channelOffset_;
    
    int holdTimeMs_ = 1000;

    vector<Widget *> widgets_; // owns list
    map<const string, unique_ptr<Widget>> widgetsByName_;
    map<const string, unique_ptr<CSIMessageGenerator>> CSIMessageGeneratorsByMessage_;

    bool speedX5_ = false;

    ControlSurface(CSurfIntegrator *const csi, Page *page, const string &name, int numChannels, int channelOffset) : csi_(csi), page_(page), name_(name), numChannels_(numChannels), channelOffset_(channelOffset), modifierManager_(make_unique<ModifierManager>(csi_, (Page *)NULL, this))
    {
        int size = 0;
        scrubModePtr_ = (int*)get_config_var("scrubmode", &size);
        
        for (int i = 1 ; i <= numChannels; ++i)
        {
            ChannelTouch channelTouch;
            channelTouch.channelNum = i;
            channelTouches_.push_back(channelTouch);
            
            ChannelToggle channelToggle;
            channelToggle.channelNum = i;
            channelToggles_.push_back(channelToggle);
        }
    }
    
    void InitZoneManager(CSurfIntegrator *const csi, ControlSurface *surface, const string &zoneFolder, const string &fxZoneFolder)
    {
        zoneManager_ = make_unique<ZoneManager>(csi_, this, zoneFolder, fxZoneFolder);
        zoneManager_->Initialize();
    }
    
    void StopRewinding()
    {
        isRewinding_ = false;
        *scrubModePtr_ = configScrubMode_;
    
        speedX5_ = false;
    }
    
    void StopFastForwarding()
    {
        isFastForwarding_ = false;
        *scrubModePtr_ = configScrubMode_;
    
        speedX5_ = false;
    }
        
    void CancelRewindAndFastForward()
    {
        if (isRewinding_)
            StopRewinding();
        else if (isFastForwarding_)
            StopFastForwarding();
    }
    
    virtual void InitHardwiredWidgets(ControlSurface *surface)
    {
        // Add the "hardwired" widgets
        AddWidget(surface, "OnTrackSelection");
        AddWidget(surface, "OnPageEnter");
        AddWidget(surface, "OnPageLeave");
        AddWidget(surface, "OnInitialization");
        AddWidget(surface, "OnPlayStart");
        AddWidget(surface, "OnPlayStop");
        AddWidget(surface, "OnRecordStart");
        AddWidget(surface, "OnRecordStop");
        AddWidget(surface, "OnZoneActivation");
        AddWidget(surface, "OnZoneDeactivation");
    }
    
    void DoWidgetAction(const string &widgetName)
    {
        if (widgetsByName_.count(widgetName) > 0)
            zoneManager_->DoAction(widgetsByName_[widgetName].get(), 1.0);
    }
    
public:
    virtual ~ControlSurface()
    {
        widgets_.clear();
        widgetsByName_.clear();
        CSIMessageGeneratorsByMessage_.clear();
    }
    
    void Stop();
    void Play();
    void Record();
    
    bool GetShift();
    bool GetOption();
    bool GetControl();
    bool GetAlt();
    bool GetFlip();
    bool GetGlobal();
    bool GetMarker();
    bool GetNudge();
    bool GetZoom();
    bool GetScrub();

    void SetModifierValue(int value);
    void SetShift(bool value);
    void SetOption(bool value);
    void SetControl(bool value);
    void SetAlt(bool value);
    void SetFlip(bool value);
    void SetGlobal(bool value);
    void SetMarker(bool value);
    void SetNudge(bool value);
    void SetZoom(bool value);
    void SetScrub(bool value);
    
    const vector<int> &GetModifiers();
    void ClearModifiers();
    void ClearModifier(const char *modifier);
        
    virtual void RequestUpdate();
    void ForceClearTrack(int trackNum);
    void ForceUpdateTrackColors();
    void OnTrackSelection(MediaTrack *track);
    virtual void SendOSCMessage(const char *zoneName) {}
    virtual void SendOSCMessage(const char *zoneName, int value) {}
    virtual void SendOSCMessage(const char *zoneName, double value) {}
    virtual void SendOSCMessage(const char *zoneName, const char *value) {}

    virtual void HandleExternalInput() {}
    virtual void UpdateTimeDisplay() {}
    virtual void FlushIO() {}
    
    virtual void SendMidiSysExMessage(MIDI_event_ex_t *midiMessage) {}
    virtual void SendMidiMessage(int first, int second, int third) {}
    
    ModifierManager *GetModifierManager() { return modifierManager_.get(); }
    ZoneManager *GetZoneManager() { return zoneManager_.get(); }
    Page *GetPage() { return page_; }
    const char *GetName() { return name_.c_str(); }
    
    int GetNumChannels() { return numChannels_; }
    int GetChannelOffset() { return channelOffset_; }
    rgba_color GetTrackColorForChannel(int channel);

    bool GetIsRewinding() { return isRewinding_; }
    bool GetIsFastForwarding() { return isFastForwarding_; }

    bool GetUsesLocalModifiers() { return usesLocalModifiers_; }
    void ToggleUseLocalModifiers() { usesLocalModifiers_ = ! usesLocalModifiers_; }
    bool GetListensToModifiers() { return listensToModifiers_; }
    void SetListensToModifiers() { listensToModifiers_ = true; }

    void SetLatchTime(int latchTime) { latchTime_ = latchTime; }
    int GetLatchTime() { return latchTime_; }

    void SetHoldTime(int value) { holdTimeMs_ = value; }
    int GetHoldTime() { return holdTimeMs_; }

    void UpdateCurrentActionContextModifiers()
    {
        if (! usesLocalModifiers_)
            GetZoneManager()->UpdateCurrentActionContextModifiers();
    }
    
    double GetStepSize(const char * const widgetClass)
    {
        if (stepSize_.find(widgetClass) != stepSize_.end())
            return stepSize_[widgetClass];
        else
            return 0;
    }

    const vector<double> GetAccelerationValues(const char * const  widgetClass)
    {
        if (accelerationValues_.find(widgetClass) != accelerationValues_.end())
            return accelerationValues_[widgetClass];
        else
            return emptyAccelerationValues_;
    }

    map<int, int> &GetAccelerationValuesForDecrement(const char * const  widgetClass)
    {
        if (accelerationValuesForDecrement_.count(widgetClass) > 0)
             return accelerationValuesForDecrement_[widgetClass];
        else
            return emptyAccelerationMap_;
    }
    
    map<int, int> &GetAccelerationValuesForIncrement(const char * const  widgetClass)
    {
        if (accelerationValuesForIncrement_.count(widgetClass) > 0)
            return accelerationValuesForIncrement_[widgetClass];
       else
           return emptyAccelerationMap_;
    }
    
    void TouchChannel(int channelNum, bool isTouched)
    {
        for (auto &channelTouch : channelTouches_)
            if (channelTouch.channelNum == channelNum)
            {
                channelTouch.isTouched = isTouched;
                break;
            }
    }
    
    bool GetIsChannelTouched(int channelNum)
    {
        for (auto &channelTouch : channelTouches_)
            if (channelTouch.channelNum == channelNum)
                return channelTouch.isTouched;

        return false;
    }
       
    void ToggleChannel(int channelNum)
    {
        for (auto &channelToggle : channelToggles_)
            if (channelToggle.channelNum == channelNum)
            {
                channelToggle.isToggled = ! channelToggle.isToggled;
                break;
            }
    }
    
    bool GetIsChannelToggled(int channelNum)
    {
        for (auto &channelToggle : channelToggles_)
            if (channelToggle.channelNum == channelNum)
                return channelToggle.isToggled;

        return false;
    }

    void ToggleRestrictTextLength(int length)
    {
        isTextLengthRestricted_ = ! isTextLengthRestricted_;
        restrictedTextLength_ = length;
    }
    
    const char *GetRestrictedLengthText(const char *textc, char *buf, int bufsz) // may return textc if not restricted
    {
        if (isTextLengthRestricted_ && strlen(textc) > restrictedTextLength_ && restrictedTextLength_ >= 0)
        {
            static const char * const filter_lists[3] = {
              " \t\r\n",
              " \t\r\n`~!@#$%^&*:()_|=?;:'\",",
              " \t\r\n`~!@#$%^&*:()_|=?;:'\",aeiou"
            };

            for (int pass = 0; pass < 3; ++pass)
            {
                const char *rd = textc;
                int l = 0;
                while (*rd && l < bufsz-1 && l <= restrictedTextLength_)
                {
                     if (!l || !strchr(filter_lists[pass], *rd))
                         buf[l++] = *rd;
                     rd++;
                }
                if (pass < 2 && l > restrictedTextLength_) continue; // keep filtering

                buf[wdl_min(restrictedTextLength_, l)] = 0;
                return buf;
            }
        }
        return textc;

    }
           
    void AddTrackColorFeedbackProcessor(FeedbackProcessor *feedbackProcessor) // does not own this pointer
    {
        if (feedbackProcessor != NULL)
        trackColorFeedbackProcessors_.push_back(feedbackProcessor);
    }
        
    void ForceClear()
    {
        for (auto widget : widgets_)
            widget->ForceClear();
        
        FlushIO();
    }
           
    void TrackFXListChanged(MediaTrack *track)
    {
        OnTrackSelection(track);
    }

    void HandleStop()
    {
        DoWidgetAction("OnRecordStop");
        DoWidgetAction("OnPlayStop");
    }
    
    void HandlePlay()
    {
        DoWidgetAction("OnPlayStart");
    }
    
    void HandleRecord()
    {
        DoWidgetAction("OnRecordStart");
    }
        
    void StartRewinding()
    {
        if (isFastForwarding_)
            StopFastForwarding();

        if (isRewinding_) // on 2nd, 3rd, etc. press
        {
            speedX5_ = ! speedX5_;
            return;
        }
        
        int playState = GetPlayState();
        if (playState == 1 || playState == 2 || playState == 5 || playState == 6) // playing or paused or recording or paused whilst recording
            SetEditCurPos(GetPlayPosition(), true, false);

        CSurf_OnStop();
        
        isRewinding_ = true;
        configScrubMode_ = *scrubModePtr_;
        *scrubModePtr_ = 2;
    }
       
    void StartFastForwarding()
    {
        if (isRewinding_)
            StopRewinding();

        if (isFastForwarding_) // on 2nd, 3rd, etc. press
        {
            speedX5_ = ! speedX5_;
            return;
        }
        
        int playState = GetPlayState();
        if (playState == 1 || playState == 2 || playState == 5 || playState == 6) // playing or paused or recording or paused whilst recording
            SetEditCurPos(GetPlayPosition(), true, false);

        CSurf_OnStop();
        
        isFastForwarding_ = true;
        configScrubMode_ = *scrubModePtr_;
        *scrubModePtr_ = 2;
    }
    
    void AddWidget(ControlSurface * surface, const char *widgetName)
    {
        if (widgetsByName_.count(string(widgetName)) == 0)
        {
            widgetsByName_.insert(make_pair(widgetName, make_unique<Widget>(csi_, surface, widgetName)));
            
            if (widgetsByName_.count(widgetName) > 0)
                widgets_.push_back(GetWidgetByName(widgetName));
        }
    }

    Widget *GetWidgetByName(const string &widgetName)
    {
        if (widgetsByName_.count(widgetName.c_str()) > 0)
            return widgetsByName_[widgetName].get();
        else
            return NULL;
    }

    void OnPageEnter()
    {
        ForceClear();
        
        DoWidgetAction("OnPageEnter");
    }
    
    void OnPageLeave()
    {
        ForceClear();
        
        DoWidgetAction("OnPageLeave");
    }
    
    void OnInitialization()
    {
        DoWidgetAction("OnInitialization");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Midi_ControlSurfaceIO
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    string const name_;
    int const channelCount_;
    midi_Input *const midiInput_;
    midi_Output *const midiOutput_;
    WDL_Queue messageQueue_;
    const int maxMesssagesPerRun_;
    
    void SendMidiSysexMessage(MIDI_event_ex_t *midiMessage)
    {
        if (midiOutput_)
            midiOutput_->SendMsg(midiMessage, -1);
    }

public:
    Midi_ControlSurfaceIO(CSurfIntegrator *csi, const char *name, int channelCount, midi_Input *midiInput, midi_Output *midiOutput, int surfaceRefreshRate, int maxMesssagesPerRun) : csi_(csi), name_(name), channelCount_(channelCount), midiInput_(midiInput), midiOutput_(midiOutput), surfaceRefreshRate_(surfaceRefreshRate), maxMesssagesPerRun_(maxMesssagesPerRun) {}

    ~Midi_ControlSurfaceIO()
    {
        if (midiInput_) ReleaseMidiInput(midiInput_);
        if (midiOutput_) ReleaseMidiOutput(midiOutput_);
    }
    
    int surfaceRefreshRate_;

    const char *GetName() { return name_.c_str(); }
    
    const int GetChannelCount() { return channelCount_; }

    void HandleExternalInput(Midi_ControlSurface *surface);
    
    void QueueMidiSysExMessage(MIDI_event_ex_t *midiMessage)
    {
        if (WDL_NOT_NORMALLY(midiMessage->size > 255)) return;

        unsigned char size = (unsigned char)midiMessage->size;
        messageQueue_.Add(&size, 1);
        messageQueue_.Add(midiMessage->midi_message, midiMessage->size);
    }

    void SendMidiMessage(int first, int second, int third)
    {
        if (midiOutput_)
            midiOutput_->Send(first, second, third, -1);
    }
    
    void Run()
    {
        int numSent = 0;
        
        while ((maxMesssagesPerRun_ == 0 || numSent < maxMesssagesPerRun_) && messageQueue_.Available() >= 1)
        {
            const unsigned char *msg = (const unsigned char *)messageQueue_.Get();
            const int msg_len = (int) *msg;
            if (WDL_NOT_NORMALLY(messageQueue_.Available() < 1 + msg_len)) // not enough data in queue, should not happen
                break;
            
            struct
            {
                MIDI_event_ex_t evt;
                char data[256];
            } midiSysExData;

            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = msg_len;
            memcpy(midiSysExData.evt.midi_message, msg + 1, msg_len);
            messageQueue_.Advance(1 + msg_len);
            SendMidiSysexMessage(&midiSysExData.evt);
            numSent++;
        }
        
        messageQueue_.Compact();
    }
    
    void Flush()
    {
        while (messageQueue_.Available() >= 1)
        {
            Sleep(2);
            
            const unsigned char *msg = (const unsigned char *)messageQueue_.Get();
            const int msg_len = (int) *msg;
            if (WDL_NOT_NORMALLY(messageQueue_.Available() < 1 + msg_len)) // not enough data in queue, should not happen
                break;
            
            struct
            {
                MIDI_event_ex_t evt;
                char data[256];
            } midiSysExData;

            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = msg_len;
            memcpy(midiSysExData.evt.midi_message, msg + 1, msg_len);
            messageQueue_.Advance(1 + msg_len);
            SendMidiSysexMessage(&midiSysExData.evt);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Midi_ControlSurface : public ControlSurface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    Midi_ControlSurfaceIO *const surfaceIO_;

    DWORD lastRun_ = 0;

    void ProcessMidiWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens);
    
    void ProcessMIDIWidgetFile(const string &filePath, Midi_ControlSurface *surface);
    
    // special processing for MCU meters
    bool hasMCUMeters_ = false;
    int displayType_ = 0x14;

    void InitializeMCU();
    void InitializeMCUXT();
    
    virtual void InitializeMeters()
    {
        if (hasMCUMeters_)
        {
            if (displayType_ == 0x14)
                InitializeMCU();
            else
                InitializeMCUXT();
        }
    }

    void SendSysexInitData(int line[], int numElem);
    
public:
    Midi_ControlSurface(CSurfIntegrator *const csi, Page *page, const char *name, int channelOffset, const char *surfaceFile, const char *zoneFolder, const char *fxZoneFolder, Midi_ControlSurfaceIO *surfaceIO);

    virtual ~Midi_ControlSurface() {}
    
    void ProcessMidiMessage(const MIDI_event_ex_t *evt);
    virtual void SendMidiSysExMessage(MIDI_event_ex_t *midiMessage) override;
    virtual void SendMidiMessage(int first, int second, int third) override;

    virtual void SetHasMCUMeters(int displayType)
    {
        hasMCUMeters_ = true;
        displayType_ = displayType;
    }
    
    virtual void HandleExternalInput() override
    {
        surfaceIO_->HandleExternalInput(this);
    }
        
    virtual void FlushIO() override
    {
        surfaceIO_->Flush();
    }
    
    virtual void RequestUpdate() override
    {
        const DWORD now = GetTickCount();
        const DWORD threshold = (DWORD) (1000 / max(surfaceIO_->surfaceRefreshRate_, 1));
        if ((now - lastRun_) < threshold) return;
        lastRun_=now;

        surfaceIO_->Run();
        
        ControlSurface::RequestUpdate();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_FeedbackProcessor : public FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    OSC_ControlSurface *const surface_;
    string const oscAddress_;
    
public:
    OSC_FeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : FeedbackProcessor(csi, widget), surface_(surface), oscAddress_(oscAddress) {}
    ~OSC_FeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_FeedbackProcessor"; }

    virtual void SetColorValue(const rgba_color &color) override;
    virtual void ForceValue(const PropertyList &properties, double value) override;
    virtual void ForceValue(const PropertyList &properties, const char * const &value) override;
    virtual void ForceClear() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_IntFeedbackProcessor : public OSC_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    OSC_IntFeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_FeedbackProcessor(csi, surface, widget, oscAddress) {}
    ~OSC_IntFeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_IntFeedbackProcessor"; }

    virtual void ForceValue(const PropertyList &properties, double value) override;
    virtual void ForceClear() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_ControlSurfaceIO
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    string const name_;
    int const channelCount_;
    oscpkt::UdpSocket *inSocket_ = NULL;
    oscpkt::UdpSocket *outSocket_ = NULL;
    oscpkt::PacketReader packetReader_;
    oscpkt::PacketWriter packetWriter_;
    oscpkt::Storage storageTmp_;
    int maxBundleSize_ = 0; // 0 = no bundles (would only be useful if the destination doesn't support bundles)
    int maxPacketsPerRun_; // 0 = no limit
    int sentPacketCount_= 0; // count of packets sent this Run() slice, after maxPacketsPerRun_ packtees go into packetQueue_
    WDL_Queue packetQueue_;
    
public:
    OSC_ControlSurfaceIO(CSurfIntegrator *const csi, const char *name, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun);
    virtual ~OSC_ControlSurfaceIO();

    const char *GetName() { return name_.c_str(); }

    const int GetChannelCount() { return channelCount_; }
    
    virtual void HandleExternalInput(OSC_ControlSurface *surface);

    void QueuePacket(const void *p, int sz)
    {
        if (WDL_NOT_NORMALLY(!outSocket_)) return;
        if (WDL_NOT_NORMALLY(!p || sz < 1)) return;
        if (WDL_NOT_NORMALLY(packetQueue_.GetSize() > 32*1024*1024)) return; // drop packets after 32MB queued
        if (maxPacketsPerRun_ != 0 && sentPacketCount_ >= maxPacketsPerRun_)
        {
            void *wr = packetQueue_.Add(NULL,sz + sizeof(int));
            if (WDL_NORMALLY(wr != NULL))
            {
                memcpy(wr, &sz, sizeof(int));
                memcpy((char *)wr + sizeof(int), p, sz);
            }
        }
        else
        {
            outSocket_->sendPacket(p, sz);
            sentPacketCount_++;
        }
    }

    void QueueOSCMessage(oscpkt::Message *message) // NULL message flushes any latent bundles
    {
        if (outSocket_ != NULL && outSocket_->isOk())
        {
            if (maxBundleSize_ > 0 && packetWriter_.packetSize() > 0)
            {
                bool send_bundle;
                if (message)
                {
                    // oscpkt lacks the ability to calculate the size of a Message?
                    storageTmp_.clear();
                    message->packMessage(storageTmp_, true);
                    send_bundle = (packetWriter_.packetSize() + storageTmp_.size() > maxBundleSize_);
                }
                else
                {
                    send_bundle = true;
                }

                if (send_bundle)
                {
                    packetWriter_.endBundle();
                    QueuePacket(packetWriter_.packetData(), packetWriter_.packetSize());
                    packetWriter_.init();
                }
            }

            if (message)
            {
                if (maxBundleSize_ > 0 && packetWriter_.packetSize() == 0)
                {
                    packetWriter_.startBundle();
                }

                packetWriter_.addMessage(*message);

                if (maxBundleSize_ <= 0)
                {
                    QueuePacket(packetWriter_.packetData(), packetWriter_.packetSize());
                    packetWriter_.init();
                }
            }
        }
    }
    
    void SendOSCMessage(const char *oscAddress, double value)
    {
        if (outSocket_ != NULL && outSocket_->isOk())
        {
            oscpkt::Message message;
            message.init(oscAddress).pushFloat((float)value);
            QueueOSCMessage(&message);
        }
    }
    
    void SendOSCMessage(const char *oscAddress, int value)
    {
        if (outSocket_ != NULL && outSocket_->isOk())
        {
            oscpkt::Message message;
            message.init(oscAddress).pushInt32(value);
            QueueOSCMessage(&message);
        }
    }
    
    void SendOSCMessage(const char *oscAddress, const char *value)
    {
        if (outSocket_ != NULL && outSocket_->isOk())
        {
            oscpkt::Message message;
            message.init(oscAddress).pushStr(value);
            QueueOSCMessage(&message);
        }
    }
    
    void SendOSCMessage(const char *value)
    {
        if (outSocket_ != NULL && outSocket_->isOk())
        {
            oscpkt::Message message;
            message.init(value);
            QueueOSCMessage(&message);
        }
    }
    
    void BeginRun()
    {
        sentPacketCount_ = 0;
        // send any latent packets first
        while (packetQueue_.GetSize()>=sizeof(int))
        {
            int sza;
            if (maxPacketsPerRun_ != 0 && sentPacketCount_ >= maxPacketsPerRun_) break;

            memcpy(&sza, packetQueue_.Get(), sizeof(int));
            packetQueue_.Advance(sizeof(int));
            if (WDL_NOT_NORMALLY(sza < 0 || packetQueue_.GetSize() < sza))
            {
                packetQueue_.Clear();
            }
            else
            {
                if (WDL_NORMALLY(outSocket_ != NULL))
                {
                    outSocket_->sendPacket(packetQueue_.Get(), sza);
                }
                packetQueue_.Advance(sza);
                sentPacketCount_++;
            }
        }
        packetQueue_.Compact();
    }

    virtual void Run()
    {
        QueueOSCMessage(NULL); // flush any latent bundles
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_X32ControlSurfaceIO :public OSC_ControlSurfaceIO
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    DWORD X32HeartBeatRefreshInterval_ = 5000;
    DWORD X32HeartBeatLastRefreshTime_ = GetTickCount() - 30000;
    
public:
    OSC_X32ControlSurfaceIO(CSurfIntegrator *const csi, const char *name, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun);
    virtual ~OSC_X32ControlSurfaceIO() {}

    virtual void HandleExternalInput(OSC_ControlSurface *surface) override;

    void Run() override
    {
        DWORD currentTime = GetTickCount();

        if ((currentTime - X32HeartBeatLastRefreshTime_) > X32HeartBeatRefreshInterval_)
        {
            X32HeartBeatLastRefreshTime_ = currentTime;
            SendOSCMessage("/xremote");
        }
        
        OSC_ControlSurfaceIO::Run();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_ControlSurface : public ControlSurface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    OSC_ControlSurfaceIO *const surfaceIO_;
    void ProcessOSCWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens);
    void ProcessOSCWidgetFile(const string &filePath);
public:
    OSC_ControlSurface(CSurfIntegrator *const csi, Page *page, const char *name, int channelOffset, const char *templateFilename, const char *zoneFolder, const char *fxZoneFolder, OSC_ControlSurfaceIO *surfaceIO);

    virtual ~OSC_ControlSurface() {}
    
    void ProcessOSCMessage(const char *message, double value);
    void SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, double value);
    void SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, int value);
    void SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, const char *value);
    virtual void SendOSCMessage(const char *zoneName) override;
    virtual void SendOSCMessage(const char *zoneName, int value) override;
    virtual void SendOSCMessage(const char *zoneName, double value) override;
    virtual void SendOSCMessage(const char *zoneName, const char *value) override;

    virtual void RequestUpdate() override
    {
        surfaceIO_->BeginRun();
        ControlSurface::RequestUpdate();
        surfaceIO_->Run();
    }

    virtual void HandleExternalInput() override
    {
        surfaceIO_->HandleExternalInput(this);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNavigationManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    Page *const page_;
    bool followMCP_;
    bool synchPages_;
    bool isScrollLinkEnabled_;
    bool isScrollSynchEnabled_;
    int currentTrackVCAFolderMode_ = 0;
    int targetScrollLinkChannel_ = 0;
    int trackOffset_ = 0; // Offset in tracks_ of the first channel on the surface
    int vcaTrackOffset_ = 0;
    int folderTrackOffset_ = 0;
    int selectedTracksOffset_ = 0;
    vector<MediaTrack *> tracks_;
    vector<MediaTrack *> selectedTracks_;

    bool isFolderViewActive_ = false;
    int currentFolderTrackID_ = 0; // 0 is for root folder
    MediaTrack* parentOfCurrentFolderTrack_ = nullptr;

    vector<MediaTrack *> vcaTopLeadTracks_;
    MediaTrack           *vcaLeadTrack_ = NULL;
    vector<MediaTrack *> vcaLeadTracks_;
    vector<MediaTrack *> vcaSpillTracks_;
    
    vector<MediaTrack *> folderTopParentTracks_;
    MediaTrack           *folderParentTrack_ = NULL;
    vector<MediaTrack *> folderParentTracks_;
    vector<MediaTrack *> folderSpillTracks_;
    map<MediaTrack*, vector<MediaTrack*>> folderDictionary_;

    vector<unique_ptr<Navigator>> fixedTrackNavigators_;
    vector<unique_ptr<Navigator>> trackNavigators_;
    unique_ptr<Navigator> masterTrackNavigator_;
    unique_ptr<Navigator> selectedTrackNavigator_;
    unique_ptr<Navigator> focusedFXNavigator_;
    
    void setTrackOffset(int trackOffset)
    {
        if (trackOffset <= 0)
        {
            trackOffset_ = 0;
            return;
        }

        int maxOffset = static_cast<int>(tracks_.size() - trackNavigators_.size());
        if (maxOffset < 0)
            maxOffset = 0;

        if (trackOffset > maxOffset)
            trackOffset_ = maxOffset;
        else
            trackOffset_ = trackOffset;
    }

    void ForceScrollLink()
    {
        // Make sure selected track is visible on the control surface
        MediaTrack *selectedTrack = GetSelectedTrack();
        
        if (selectedTrack != NULL)
        {
            // Is the selected track already visible on the surface?
            for (auto &trackNavigator : trackNavigators_)
                if (selectedTrack == trackNavigator->GetTrack())
                    return;
            
            // Check if the selected track is in the current folder
            MediaTrack* parentTrack = GetParentTrack(selectedTrack);
            int parentTrackId = parentTrack ? GetIdFromTrack(parentTrack) : 0;
            if (currentFolderTrackID_ = parentTrackId)
            {
                // If not, chenge the current folder to the selected track's parent
                currentFolderTrackID_ = parentTrack ? GetIdFromTrack(parentTrack) : 0;
                RebuildTracks();
            }

            // Find the selected track in the tracks_ list
            auto it = std::find(tracks_.begin(), tracks_.end(), selectedTrack);
            if (it != tracks_.end())
            {
                setTrackOffset(static_cast<int>(std::distance(tracks_.begin(), it)));
            }
        }
    }
    
public:
    TrackNavigationManager(CSurfIntegrator *const csi, Page *page, bool followMCP,  bool synchPages, bool isScrollLinkEnabled, bool isScrollSynchEnabled) :
    csi_(csi),
    page_(page),
    followMCP_(followMCP),
    synchPages_(synchPages),
    isScrollLinkEnabled_(isScrollLinkEnabled),
    isScrollSynchEnabled_(isScrollSynchEnabled),
    masterTrackNavigator_(make_unique<MasterTrackNavigator>(csi_, page_)),
    selectedTrackNavigator_(make_unique< SelectedTrackNavigator>(csi_, page_)),
    focusedFXNavigator_(make_unique<FocusedFXNavigator>(csi_, page_)) {}
    
    ~TrackNavigationManager()
    {
        fixedTrackNavigators_.clear();
        trackNavigators_.clear();
    }
    
    void RebuildTracks();
    void RebuildSelectedTracks();
    void AdjustSelectedTrackBank(int amount);
    bool GetSynchPages() { return synchPages_; }
    bool GetScrollLink() { return isScrollLinkEnabled_; }
    bool GetFollowMCP() { return followMCP_; }
    int  GetNumTracks() { return CSurf_NumTracks(followMCP_); }
    Navigator *GetMasterTrackNavigator() { return masterTrackNavigator_.get(); }
    Navigator *GetSelectedTrackNavigator() { return selectedTrackNavigator_.get(); }
    Navigator *GetFocusedFXNavigator() { return focusedFXNavigator_.get(); }
    
    bool GetIsTrackVisible(MediaTrack *track)
    {
        return IsTrackVisible(track, followMCP_);
    }

    void ToggleFolderView()
    {
        isFolderViewActive_ = !isFolderViewActive_;

        if (isFolderViewActive_)
        {
            // Entering folder view: show the root level
            currentFolderTrackID_ = 0;
            trackOffset_ = 0;
        }
        else
        {
            // currentFolderTrackID_ is equal to the absolute offset of the first track in tracks_ before toggling
            trackOffset_ += currentFolderTrackID_;
            // When in flat mode, currentFolderTrackID_ must be zero
            currentFolderTrackID_ = 0;
        }
    }

    bool GetIsFolderViewActive() const
    {
        return isFolderViewActive_;
    }

    void VCAModeActivated()
    {
        currentTrackVCAFolderMode_ = 1;
    }
    
    void FolderModeActivated()
    {
        currentTrackVCAFolderMode_ = 2;
    }
    
    void SelectedTracksModeActivated()
    {
        currentTrackVCAFolderMode_ = 3;
    }
    
    void VCAModeDeactivated()
    {
        if (currentTrackVCAFolderMode_ == 1)
            currentTrackVCAFolderMode_ = 0;
    }
    
    void FolderModeDeactivated()
    {
        if (currentTrackVCAFolderMode_ == 2)
            currentTrackVCAFolderMode_ = 0;
    }
    
    void SelectedTracksModeDeactivated()
    {
        if (currentTrackVCAFolderMode_ == 3)
            currentTrackVCAFolderMode_ = 0;
    }
    
    string GetCurrentTrackVCAFolderModeDisplay()
    {
        if (currentTrackVCAFolderMode_ == 0)
            return "Track";
        else if (currentTrackVCAFolderMode_ == 1)
            return "VCA";
        else if (currentTrackVCAFolderMode_ == 2)
            return "Folder";
        else if (currentTrackVCAFolderMode_ == 3)
            return "SelectedTracks";
        else
            return "";
    }
    static const char *GetAutoModeDisplayNameNoOverride(int modeIndex)
    {
        switch (modeIndex)
        {
            case 0: return "Trim";
            case 1: return "Read";
            case 2: return "Touch";
            case 3: return "Write";
            case 4: return "Latch";
            case 5: return "LtchPre";
            default: WDL_ASSERT(false); return "?";
        }
    }

    const char *GetAutoModeDisplayName(int modeIndex)
    {
        int globalOverride = GetGlobalAutomationOverride();

        if (globalOverride > -1) // -1=no override, 0=trim/read, 1=read, 2=touch, 3=write, 4=latch, 5=bypass
            return GetAutoModeDisplayNameNoOverride(globalOverride);
        else
            return GetAutoModeDisplayNameNoOverride(modeIndex);
    }

    const char *GetGlobalAutoModeDisplayName()
    {
        int globalOverride = GetGlobalAutomationOverride();

        if (globalOverride == -1)
            return "NoOverride";
        else if (globalOverride > -1) // -1=no override, 0=trim/read, 1=read, 2=touch, 3=write, 4=latch, 5=bypass
            return GetAutoModeDisplayNameNoOverride(globalOverride);
        else
            return "";
    }

    void NextInputMonitorMode(MediaTrack *track)
    {
        // I_RECMON : int  *: record monitor (0=off, 1=normal, 2=not when playing (tapestyle))
        int recMonitorMode = (int)GetMediaTrackInfo_Value(track,"I_RECMON");

        // I_RECMONITEMS : int  *: monitor items while recording (0=off, 1=on)
        int recMonitorItemMode = (int)GetMediaTrackInfo_Value(track,"I_RECMONITEMS");

        if (recMonitorMode == 0)
        {
            recMonitorMode = 1;
            recMonitorItemMode = 0;
        }
        else if (recMonitorMode == 1 && recMonitorItemMode == 0)
        {
            recMonitorMode = 2;
            recMonitorItemMode = 0;
        }
        else if (recMonitorMode == 2 && recMonitorItemMode == 0)
        {
            recMonitorMode = 1;
            recMonitorItemMode = 1;
        }
        else if (recMonitorMode == 1 && recMonitorItemMode == 1)
        {
            recMonitorMode = 2;
            recMonitorItemMode = 1;
        }
        else if (recMonitorMode == 2 && recMonitorItemMode == 1)
        {
            recMonitorMode = 0;
            recMonitorItemMode = 0;
        }

        GetSetMediaTrackInfo(track, "I_RECMON", &recMonitorMode);
        GetSetMediaTrackInfo(track, "I_RECMONITEMS", &recMonitorItemMode);
    }
    
    const char *GetCurrentInputMonitorMode(MediaTrack *track)
    {
        // I_RECMON : int  *: record monitor (0=off, 1=normal, 2=not when playing (tapestyle))
        int recMonitorMode = (int)GetMediaTrackInfo_Value(track,"I_RECMON");

        // I_RECMONITEMS : int  *: monitor items while recording (0=off, 1=on)
        int recMonitorItemMode = (int)GetMediaTrackInfo_Value(track,"I_RECMONITEMS");

        if (recMonitorMode == 0)
            return "Off";
        else if (recMonitorMode == 1 && recMonitorItemMode == 0)
            return "Input";
        else if (recMonitorMode == 2 && recMonitorItemMode == 0)
            return "Auto";
        else if (recMonitorMode == 1 && recMonitorItemMode == 1)
            return "Input+";
        else if (recMonitorMode == 2 && recMonitorItemMode == 1)
            return "Auto+";
        else
            return "";
    }
    
    const vector<MediaTrack *> &GetSelectedTracks()
    {
        selectedTracks_.clear();
        
        for (int i = 0; i < CountSelectedTracks2(NULL, false); ++i)
            selectedTracks_.push_back(DAW::GetSelectedTrack(i));
        
        return selectedTracks_;
    }

    void SetTrackOffset(int trackOffset)
    {
        if (isScrollSynchEnabled_)
        {
            if (isFolderViewActive_)
            {
                // Find the track at trackOffset in the tracks_ list
                MediaTrack* track = GetTrackFromId(trackOffset + 1);
                auto it = std::find(tracks_.begin(), tracks_.end(), track);
                if (it == tracks_.end())
                    return; // not in the current folder, don't scroll the channels

                trackOffset = static_cast<int>(std::distance(tracks_.begin(), it));
            }

            setTrackOffset(trackOffset);
        }
    }
    
    void AdjustTrackBank(int amount)
    {
        if (currentTrackVCAFolderMode_ != 0)
            return;

        setTrackOffset(trackOffset_ + amount);
        
        if (isScrollSynchEnabled_)
        {
            if (MediaTrack *leftmostTrack = DAW::GetTrack(currentFolderTrackID_ + 1 + trackOffset_))
                SetMixerScroll(leftmostTrack);
        }
    }
    
    void AdjustVCABank(int amount)
    {
        if (currentTrackVCAFolderMode_ != 1)
            return;
       
        vcaTrackOffset_ += amount;
        
        if (vcaTrackOffset_ <  0)
            vcaTrackOffset_ =  0;
        
        int top = 0;
        
        if (vcaLeadTrack_ == NULL)
            top = (int) vcaTopLeadTracks_.size() - 1;
        else
            top = (int) vcaSpillTracks_.size() - 1;

        if (vcaTrackOffset_ >  top)
            vcaTrackOffset_ = top;
    }
    
    void AdjustFolderBank(int amount)
    {
        if (currentTrackVCAFolderMode_ != 2)
            return;
                   
        folderTrackOffset_ += amount;
        
        if (folderTrackOffset_ <  0)
            folderTrackOffset_ =  0;
              
        int top = 0;
        
        if (folderParentTrack_ == NULL)
            top = (int) folderTopParentTracks_.size() - 1;
        else
            top = (int) folderSpillTracks_.size() - 1;
        
        if (folderTrackOffset_ > top)
            folderTrackOffset_ = top;
    }
    
    void AdjustSelectedTracksBank(int amount)
    {
        if (currentTrackVCAFolderMode_ != 3)
            return;
        
        selectedTracksOffset_ += amount;
        
        if (selectedTracksOffset_ < 0)
            selectedTracksOffset_ = 0;
        
        int top = (int) selectedTracks_.size() - 1;
        
        if (selectedTracksOffset_ > top)
            selectedTracksOffset_ = top;
    }

    Navigator *GetNavigatorForChannel(int channelNum)
    {
        for (auto &trackNavigator : trackNavigators_)
            if (trackNavigator->GetChannelNum() == channelNum)
                return trackNavigator.get();
          
        trackNavigators_.push_back(make_unique<TrackNavigator>(csi_, page_, this, channelNum));
            
        return  trackNavigators_.back().get();
    }
    
    Navigator *GetNavigatorForTrack(MediaTrack *track)
    {
        for (auto &fixedTrackNavigator : fixedTrackNavigators_)
            if (fixedTrackNavigator->GetTrack() == track)
                return fixedTrackNavigator.get();
        
        fixedTrackNavigators_.push_back(make_unique<FixedTrackNavigator>(csi_, page_, track));
            
        return fixedTrackNavigators_.back().get();
    }
    
    MediaTrack *GetTrackFromChannel(int channelNumber)
    {
        if (currentTrackVCAFolderMode_ == 0)
        {
            channelNumber += trackOffset_;

            if (channelNumber < GetNumTracks() && channelNumber < tracks_.size() && DAW::ValidateTrackPtr(tracks_[channelNumber]))
                return tracks_[channelNumber];
            else
                return NULL;
        }
        else if (currentTrackVCAFolderMode_ == 1)
        {
            channelNumber += vcaTrackOffset_;

            if (vcaLeadTrack_ == NULL)
            {
                if (channelNumber < vcaTopLeadTracks_.size() && DAW::ValidateTrackPtr(vcaTopLeadTracks_[channelNumber]))
                    return vcaTopLeadTracks_[channelNumber];
                else
                    return NULL;
            }
            else
            {
                if (channelNumber < vcaSpillTracks_.size() && DAW::ValidateTrackPtr(vcaSpillTracks_[channelNumber]))
                    return vcaSpillTracks_[channelNumber];
                else
                    return NULL;
            }
        }
        else if (currentTrackVCAFolderMode_ == 2)
        {
            channelNumber += folderTrackOffset_;

            if (folderParentTrack_ == NULL)
            {
                if (channelNumber < folderTopParentTracks_.size() && DAW::ValidateTrackPtr(folderTopParentTracks_[channelNumber]))
                    return folderTopParentTracks_[channelNumber];
                else
                    return NULL;
            }
            else
            {
                if (channelNumber < folderSpillTracks_.size() && DAW::ValidateTrackPtr(folderSpillTracks_[channelNumber]))
                    return folderSpillTracks_[channelNumber];
                else
                    return NULL;
            }
        }
        else if (currentTrackVCAFolderMode_ == 3)
        {
            channelNumber += selectedTracksOffset_;
            
            if (channelNumber < selectedTracks_.size() && DAW::ValidateTrackPtr(selectedTracks_[channelNumber]))
                return selectedTracks_[channelNumber];
            else
                return NULL;
        }
        
        return NULL;
    }
    
    MediaTrack *GetTrackFromId(int trackNumber)
    {
        if (trackNumber <= GetNumTracks())
            return CSurf_TrackFromID(trackNumber, followMCP_);
        else
            return NULL;
    }
    
    int GetIdFromTrack(MediaTrack *track)
    {
        return CSurf_TrackToID(track, followMCP_);
    }
    
    MediaTrack* SetCurrentFolder(MediaTrack* track)
    {
        if (track == nullptr)
            currentFolderTrackID_ = 0;
        else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") != 1)
            return nullptr;
        else
            currentFolderTrackID_ = CSurf_TrackToID(track, followMCP_);

        trackOffset_ = 0;

        // If CSI follows the TCP or the MPC, then the selection cannot be outside the folder we just enter:
        // as we were previously outside this folder, the selected track cannot be inside: it needs to be changed.
        // Select the first track in the folder
        if (isScrollLinkEnabled_)
            return GetTrackFromId(currentFolderTrackID_ + 1);
        else
            return nullptr;
    }

    MediaTrack* ExitCurrentFolder()
    {
        MediaTrack* exitedFolderTrack = GetTrackFromId(currentFolderTrackID_);

        SetCurrentFolder(parentOfCurrentFolderTrack_); // parentOfCurrentFolderTrack_ will be updated on track list rebuild

        // If CSI follows the TCP or the MPC, then the selection cannot be outside the folder we just enter:
        // as we were previously in a child folder, the selected track cannot be at this level: it needs to be changed.
        // Select the folder just exited
        if (isScrollLinkEnabled_)
            return exitedFolderTrack;
        else
            return nullptr;
    }

    bool IsAtRootFolderLevel()
    {
        if (currentFolderTrackID_ == 0)
            return true;
        else
            return false;
    }

    bool GetIsVCASpilled(MediaTrack *track)
    {
        if (vcaLeadTrack_ == NULL && (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 || DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0))
            return true;
        else if (vcaLeadTrack_ == track)
            return true;
        else
            return false;
    }
    
    void ToggleVCASpill(MediaTrack *track)
    {
        if (currentTrackVCAFolderMode_ != 1)
            return;
        
        if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") == 0 && DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") == 0)
            return;

        if (vcaLeadTrack_ == track)
        {
            if (vcaLeadTracks_.size() > 0)
            {
                vcaLeadTrack_ = vcaLeadTracks_[vcaLeadTracks_.size() - 1];
                vcaLeadTracks_.erase(vcaLeadTracks_.begin() + vcaLeadTracks_.size() - 1);
            }
            else
                vcaLeadTrack_ = NULL;
        }
        else if (vcaLeadTrack_ != NULL)
        {
            vcaLeadTracks_.push_back(vcaLeadTrack_);
            vcaLeadTrack_ = track;
        }
        else
            vcaLeadTrack_ = track;
       
        vcaTrackOffset_ = 0;
    }

    bool GetIsFolderSpilled(MediaTrack *track)
    {
        if (find(folderTopParentTracks_.begin(), folderTopParentTracks_.end(), track) != folderTopParentTracks_.end())
            return true;
        else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1)
            return true;
        else
            return false;
    }

    void ToggleFolderSpill(MediaTrack *track)
    {
        if (currentTrackVCAFolderMode_ != 2)
            return;
        
        if (folderTopParentTracks_.size() == 0)
            return;

        else if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") != 1)
            return;
        
        if (folderParentTrack_ == track)
        {
            if (folderParentTracks_.size() > 0)
            {
                folderParentTrack_ = folderParentTracks_[folderParentTracks_.size() - 1];
                folderParentTracks_.erase(folderParentTracks_.begin() + folderParentTracks_.size() - 1);
            }
            else
                folderParentTrack_ = NULL;
        }
        else if (folderParentTrack_ != NULL)
        {
            folderParentTracks_.push_back(folderParentTrack_);
            folderParentTrack_ = track;
        }
        else
            folderParentTrack_ = track;
       
        folderTrackOffset_ = 0;
    }
    
    void ToggleSynchPages()
    {
        synchPages_ = ! synchPages_;
    }
    
    void ToggleFollowMCP()
    {
        followMCP_ = ! followMCP_;
    }
    
    void ToggleScrollLink(int targetChannel)
    {
        targetScrollLinkChannel_ = targetChannel - 1 < 0 ? 0 : targetChannel - 1;
        
        isScrollLinkEnabled_ = ! isScrollLinkEnabled_;
        
        OnTrackSelection();
    }
    
    MediaTrack *GetSelectedTrack()
    {
        if (CountSelectedTracks2(NULL, false) != 1)
            return NULL;
        else
            return DAW::GetSelectedTrack(0);
    }
    
//  Page only uses the following:
       
    void OnTrackSelection()
    {
        if (isScrollLinkEnabled_)
            ForceScrollLink();
    }
    
    void OnTrackListChange()
    {
        RebuildTracks(); 
        AdjustTrackBank(0); // make sure the track offset is correct

        if (isScrollLinkEnabled_)
            ForceScrollLink();
    }

    void OnTrackSelectionBySurface(MediaTrack *track)
    {
        if (isScrollLinkEnabled_)
        {
            if (IsTrackVisible(track, true))
                SetMixerScroll(track); // scroll selected MCP tracks into view
            
            if (IsTrackVisible(track, false))
                DAW::SendCommandMessage(40913); // scroll selected TCP tracks into view
        }
    }

    bool GetIsControlTouched(MediaTrack *track, int touchedControl)
    {
        if (track == GetMasterTrackNavigator()->GetTrack())
            return GetIsNavigatorTouched(GetMasterTrackNavigator(), touchedControl);
        
        for (auto &trackNavigator : trackNavigators_)
            if (track == trackNavigator->GetTrack())
                return GetIsNavigatorTouched(trackNavigator.get(), touchedControl);
 
        if (MediaTrack *selectedTrack = GetSelectedTrack())
             if (track == selectedTrack)
                return GetIsNavigatorTouched(GetSelectedTrackNavigator(), touchedControl);
        
        if (MediaTrack *focusedFXTrack = GetFocusedFXNavigator()->GetTrack())
            if (track == focusedFXTrack)
                return GetIsNavigatorTouched(GetFocusedFXNavigator(), touchedControl);

        return false;
    }
    
    bool GetIsNavigatorTouched(Navigator *navigator,  int touchedControl)
    {
        if (touchedControl == 0)
            return navigator->GetIsVolumeTouched();
        else if (touchedControl == 1)
        {
            if (navigator->GetIsPanTouched() || navigator->GetIsPanLeftTouched())
                return true;
        }
        else if (touchedControl == 2)
        {
            if (navigator->GetIsPanWidthTouched() || navigator->GetIsPanRightTouched())
                return true;
        }

        return false;
    }
    
    void RebuildVCASpill()
    {   
        if (currentTrackVCAFolderMode_ != 1)
            return;
    
        vcaTopLeadTracks_.clear();
        vcaSpillTracks_.clear();
        
        unsigned int leadTrackVCALeaderGroup = 0;
        unsigned int leadTrackVCALeaderGroupHigh = 0;
        
        if (vcaLeadTrack_ != NULL)
        {
            leadTrackVCALeaderGroup = DAW::GetTrackGroupMembership(vcaLeadTrack_, "VOLUME_VCA_LEAD");
            leadTrackVCALeaderGroupHigh = DAW::GetTrackGroupMembershipHigh(vcaLeadTrack_, "VOLUME_VCA_LEAD");
            vcaSpillTracks_.push_back(vcaLeadTrack_);
        }
        
        // Get Visible Tracks
        for (int tidx = 1; tidx <= GetNumTracks(); ++tidx)
        {
            MediaTrack *track = CSurf_TrackFromID(tidx, followMCP_);
            
            if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 && DAW::GetTrackGroupMembership(track, "VOLUME_VCA_FOLLOW") == 0)
                vcaTopLeadTracks_.push_back(track);
            
            if (DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0 && DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_FOLLOW") == 0)
                vcaTopLeadTracks_.push_back(track);
            
            if (vcaLeadTrack_ != NULL)
            {
                bool isFollower = false;
                
                unsigned int leadTrackVCAFollowerGroup = DAW::GetTrackGroupMembership(track, "VOLUME_VCA_FOLLOW");
                unsigned int leadTrackVCAFollowerGroupHigh = DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_FOLLOW");

                if ((leadTrackVCALeaderGroup     & leadTrackVCAFollowerGroup) ||
                    (leadTrackVCALeaderGroupHigh & leadTrackVCAFollowerGroupHigh))
                {
                    isFollower = true;
                }
                
                if (isFollower)
                    vcaSpillTracks_.push_back(track);
            }
        }
    }
    
    void RebuildFolderTracks()
    {
        if (currentTrackVCAFolderMode_ != 2)
            return;
        
        folderTopParentTracks_.clear();
        folderDictionary_.clear();
        folderSpillTracks_.clear();
       
        vector<vector<MediaTrack*>*> currentDepthTracks;
        
        for (int i = 1; i <= GetNumTracks(); i++)
        {
            MediaTrack* track = CSurf_TrackFromID(i, followMCP_);

            if(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1)
            {
                if(currentDepthTracks.size() == 0)
                    folderTopParentTracks_.push_back(track);
                else
                    currentDepthTracks.back()->push_back(track);
                
                folderDictionary_[track].push_back(track);
                
                currentDepthTracks.push_back(&folderDictionary_[track]);
            }
            else if(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 0 && currentDepthTracks.size() > 0)
            {
                currentDepthTracks.back()->push_back(track);
            }
            else if(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") < 0 && currentDepthTracks.size() > 0)
            {
                currentDepthTracks.back()->push_back(track);
                
                int folderBackTrack = (int)-GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH");
                
                for(int i = 0; i < folderBackTrack && currentDepthTracks.size() > 0; i++)
                    currentDepthTracks.pop_back();
            }
        }

        if (folderParentTrack_ != NULL)
            for (int i = 0; i < folderDictionary_[folderParentTrack_].size(); ++i)
                folderSpillTracks_.push_back(folderDictionary_[folderParentTrack_][i]);
     }
    
    void EnterPage()
    {
        /*
         if (colorTracks_)
         {
         // capture track colors
         for (auto *navigator : trackNavigators_)
         if (MediaTrack *track = DAW::GetTrackFromGUID(navigator->GetTrackGUID(), followMCP_))
         trackColors_[navigator->GetTrackGUID()] = DAW::GetTrackColor(track);
         }
         */
    }
    
    void LeavePage()
    {
        /*
         if (colorTracks_)
         {
         DAW::PreventUIRefresh(1);
         // reset track colors
         for (auto *navigator : trackNavigators_)
         if (MediaTrack *track = DAW::GetTrackFromGUID(navigator->GetTrackGUID(), followMCP_))
         if (trackColors_.count(navigator->GetTrackGUID()) > 0)
         GetSetMediaTrackInfo(track, "I_CUSTOMCOLOR", &trackColors_[navigator->GetTrackGUID()]);
         DAW::PreventUIRefresh(-1);
         }
         */
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Page
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    CSurfIntegrator *const csi_;
    string const name_;
    unique_ptr<TrackNavigationManager> trackNavigationManager_;
    unique_ptr<ModifierManager> modifierManager_;
    vector<unique_ptr<ControlSurface>> surfaces_;
    
public:
    Page(CSurfIntegrator *const csi, const char *name, bool followMCP,  bool synchPages, bool isScrollLinkEnabled, bool isScrollSynchEnabled) : csi_(csi), name_(name), trackNavigationManager_(make_unique<TrackNavigationManager>(csi_, this, followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled)), modifierManager_(make_unique<ModifierManager>(csi_, this, (ControlSurface *)NULL)) {}

    ~Page()
    {
        surfaces_.clear();
    }
        
    const char *GetName() { return name_.c_str(); }
    
    ModifierManager *GetModifierManager() { return modifierManager_.get(); }
    
    vector<unique_ptr<ControlSurface>> &GetSurfaces() { return surfaces_; }
       
    void UpdateCurrentActionContextModifiers()
    {
        for (auto &surface : surfaces_)
            surface->UpdateCurrentActionContextModifiers();
    }
    
    void ForceClear()
    {
        for (auto &surface : surfaces_)
            surface->ForceClear();
    }
    
    void ForceClearTrack(int trackNum)
    {
        for (auto &surface : surfaces_)
            surface->ForceClearTrack(trackNum);
    }
    
    void ForceUpdateTrackColors()
    {
        for (auto &surface : surfaces_)
            surface->ForceUpdateTrackColors();
    }
      
    bool GetTouchState(MediaTrack *track, int touchedControl)
    {
        return trackNavigationManager_->GetIsControlTouched(track, touchedControl);
    }
    
    void OnTrackSelection(MediaTrack *track)
    {
        trackNavigationManager_->OnTrackSelection();
        
        for (auto &surface : surfaces_)
            surface->OnTrackSelection(track);
    }
    
    void OnTrackListChange()
    {
        trackNavigationManager_->OnTrackListChange();
    }
    
    void OnTrackSelectionBySurface(MediaTrack *track)
    {
        trackNavigationManager_->OnTrackSelectionBySurface(track);
        
        for (auto &surface : surfaces_)
            surface->OnTrackSelection(track);
    }
    
    void TrackFXListChanged(MediaTrack *track)
    {
        for (auto &surface : surfaces_)
            surface->TrackFXListChanged(track);
    }
    
    void EnterPage()
    {
        trackNavigationManager_->EnterPage();
        
        for (auto &surface : surfaces_)
            surface->OnPageEnter();
    }
    
    void LeavePage()
    {
        trackNavigationManager_->LeavePage();
        
        for (auto &surface : surfaces_)
            surface->OnPageLeave();
    }
    
    void OnInitialization()
    {
        for (auto &surface : surfaces_)
            surface->OnInitialization();
    }
    
    void SignalStop()
    {
        for (auto &surface : surfaces_)
            surface->HandleStop();
    }
    
    void SignalPlay()
    {
        for (auto &surface : surfaces_)
            surface->HandlePlay();
    }
    
    void SignalRecord()
    {
        for (auto &surface : surfaces_)
            surface->HandleRecord();
    }
            
    void GoHome()
    {
        for (auto &surface : surfaces_)
            surface->GetZoneManager()->GoHome();
    }
    
    void GoZone(const char *name)
    {
        for (auto &surface : surfaces_)
            surface->GetZoneManager()->GoZone(name);
    }
    
    void AdjustBank(const char *zoneName, int amount)
    {
        if (!strcmp(zoneName, "Track"))
            trackNavigationManager_->AdjustTrackBank(amount);
        else if (!strcmp(zoneName, "VCA"))
            trackNavigationManager_->AdjustVCABank(amount);
        else if (!strcmp(zoneName, "Folder"))
            trackNavigationManager_->AdjustFolderBank(amount);
        else if (!strcmp(zoneName, "SelectedTracks"))
            trackNavigationManager_->AdjustSelectedTracksBank(amount);
        else if (!strcmp(zoneName, "SelectedTrack"))
            trackNavigationManager_->AdjustSelectedTrackBank(amount);
        else
            for (auto &surface : surfaces_)
                surface->GetZoneManager()->AdjustBank(zoneName, amount);
    }
    
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Page facade for TrackNavigationManager
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool GetSynchPages() { return trackNavigationManager_->GetSynchPages(); }
    bool GetScrollLink() { return trackNavigationManager_->GetScrollLink(); }
    bool GetFollowMCP() { return trackNavigationManager_->GetFollowMCP(); }
    int  GetNumTracks() { return trackNavigationManager_->GetNumTracks(); }
    Navigator *GetMasterTrackNavigator() { return trackNavigationManager_->GetMasterTrackNavigator(); }
    Navigator * GetSelectedTrackNavigator() { return trackNavigationManager_->GetSelectedTrackNavigator(); }
    Navigator * GetFocusedFXNavigator() { return trackNavigationManager_->GetFocusedFXNavigator(); }
    void ToggleFolderView() { trackNavigationManager_->ToggleFolderView(); }
    bool GetIsFolderViewActive() { return trackNavigationManager_->GetIsFolderViewActive(); }
    MediaTrack* SetCurrentFolder(MediaTrack* track) { return trackNavigationManager_->SetCurrentFolder(track); }
    MediaTrack* ExitCurrentFolder() { return trackNavigationManager_->ExitCurrentFolder(); }
    bool IsAtRootFolderLevel() { return trackNavigationManager_->IsAtRootFolderLevel(); }
    void VCAModeActivated() { trackNavigationManager_->VCAModeActivated(); }
    void VCAModeDeactivated() { trackNavigationManager_->VCAModeDeactivated(); }
    void FolderModeActivated() { trackNavigationManager_->FolderModeActivated(); }
    void FolderModeDeactivated() { trackNavigationManager_->FolderModeDeactivated(); }
    void SelectedTracksModeActivated() { trackNavigationManager_->SelectedTracksModeActivated(); }
    void SelectedTracksModeDeactivated() { trackNavigationManager_->SelectedTracksModeDeactivated(); }
    Navigator * GetNavigatorForTrack(MediaTrack *track) { return trackNavigationManager_->GetNavigatorForTrack(track); }
    Navigator * GetNavigatorForChannel(int channelNum) { return trackNavigationManager_->GetNavigatorForChannel(channelNum); }
    MediaTrack *GetTrackFromId(int trackNumber) { return trackNavigationManager_->GetTrackFromId(trackNumber); }
    int GetIdFromTrack(MediaTrack *track) { return trackNavigationManager_->GetIdFromTrack(track); }
    bool GetIsTrackVisible(MediaTrack *track) { return trackNavigationManager_->GetIsTrackVisible(track); }
    bool GetIsVCASpilled(MediaTrack *track) { return trackNavigationManager_->GetIsVCASpilled(track); }
    void ToggleVCASpill(MediaTrack *track) { trackNavigationManager_->ToggleVCASpill(track); }
    bool GetIsFolderSpilled(MediaTrack *track) { return trackNavigationManager_->GetIsFolderSpilled(track); }
    void ToggleFolderSpill(MediaTrack *track) { trackNavigationManager_->ToggleFolderSpill(track); }
    void ToggleScrollLink(int targetChannel) { trackNavigationManager_->ToggleScrollLink(targetChannel); }
    void ToggleSynchPages() { trackNavigationManager_->ToggleSynchPages(); }
    void ToggleFollowMCP() { trackNavigationManager_->ToggleFollowMCP(); }
    void SetTrackOffset(int offset) { trackNavigationManager_->SetTrackOffset(offset); }
    MediaTrack *GetSelectedTrack() { return trackNavigationManager_->GetSelectedTrack(); }
    void NextInputMonitorMode(MediaTrack *track) { trackNavigationManager_->NextInputMonitorMode(track); }
    const char *GetAutoModeDisplayName(int modeIndex) { return trackNavigationManager_->GetAutoModeDisplayName(modeIndex); }
    const char *GetGlobalAutoModeDisplayName() { return trackNavigationManager_->GetGlobalAutoModeDisplayName(); }
    const char *GetCurrentInputMonitorMode(MediaTrack *track) { return trackNavigationManager_->GetCurrentInputMonitorMode(track); }
    const vector<MediaTrack *> &GetSelectedTracks() { return trackNavigationManager_->GetSelectedTracks(); }
    
    
    /*
    int repeats = 0;
    
    void Run()
    {
        int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                
        repeats++;
         
        if (repeats > 50)
        {
            repeats = 0;
            
            int totalDuration = 0;

            start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            trackNavigationManager_->RebuildTracks();
            trackNavigationManager_->RebuildVCASpill();
            trackNavigationManager_->RebuildFolderTracks();
            int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
            totalDuration += duration;
            ShowDuration("Rebuild Track/VCA/Folder List", duration);
            
            for (int i = 0; i < surfaces_.size(); ++i)
            {
                start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                surfaces_[i]->HandleExternalInput();
                duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
                totalDuration += duration;
                ShowDuration(surfaces_[i]->GetName(), "HandleExternalInput", duration);
            }
            
            for (int i = 0; i < surfaces_.size(); ++i)
            {
                start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                surfaces_[i]->RequestUpdate();
                duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
                totalDuration += duration;
                ShowDuration(surfaces_[i]->GetName(), "Request Update", duration);
            }
            
            LogToConsole(256, "Total duration = %d\n\n\n", totalDuration);
        }
    }
    
    
    void ShowDuration(string item, int duration)
    {
        LogToConsole(256, "%s - %d microseconds\n", item.c_str(), duration);
    }
    
    void ShowDuration(string surface, string item, int duration)
    {
        LogToConsole(256, "%s - %s - %d microseconds\n", surface.c_str(), item.c_str(), duration);
    }
   */

//*
    void Run()
    {
        trackNavigationManager_->RebuildTracks();
        trackNavigationManager_->RebuildVCASpill();
        trackNavigationManager_->RebuildFolderTracks();
        trackNavigationManager_->RebuildSelectedTracks();
        
        for (auto &surface : surfaces_)
            surface->HandleExternalInput();
        
        for (auto &surface : surfaces_)
            surface->RequestUpdate();
    }
//*/
};

static const int s_tickCounts_[] = { 250, 235, 220, 205, 190, 175, 160, 145, 130, 115, 100, 90, 80, 70, 60, 50, 45, 40, 35, 30, 25, 20, 20, 20 };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSurfIntegrator : public IReaperControlSurface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    char configtmp[1024];
   
    vector<unique_ptr<Midi_ControlSurfaceIO>> midiSurfacesIO_;
    vector<unique_ptr<OSC_ControlSurfaceIO>> oscSurfacesIO_;

    map<const string, unique_ptr<Action>> actions_;

    vector<unique_ptr<Page>> pages_;

    int currentPageIndex_ = 0;
    
    bool shouldRun_ = true;
    
    ReaProject* currentProject_ = NULL;
    
    // these are offsets to be passed to projectconfig_var_addr() when needed in order to get the actual pointers
    int timeModeOffs_;
    int timeMode2Offs_;
    int measOffsOffs_;
    int timeOffsOffs_; // for a double
    int projectPanModeOffs_;
    
    int projectMetronomePrimaryVolumeOffs_; // for double -- if invalid, use fallbacks
    int projectMetronomeSecondaryVolumeOffs_; // for double -- if invalid, use fallbacks
        
    void InitActionsDictionary();

    double GetPrivateProfileDouble(const char *key)
    {
        char tmp[512];
        memset(tmp, 0, sizeof(tmp));
        
        GetPrivateProfileString("REAPER", key, "", tmp, sizeof(tmp), get_ini_file());
        
        return strtod (tmp, NULL);
    }

public:
    CSurfIntegrator();
    
    ~CSurfIntegrator();

    virtual int Extended(int call, void *parm1, void *parm2, void *parm3) override;
    const char *GetTypeString() override;
    const char *GetDescString() override;
    const char *GetConfigString() override; // string of configuration data

    void ResetWidgets()
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->ForceClear();
    }

    void Shutdown()
    {
        // GAW -- IMPORTANT
        
        // We want to stop polling
        shouldRun_ = false;
        
        ResetWidgets();

        ShutdownLearn();
    }
    
    void Init();

    double GetFaderMaxDB() { return GetPrivateProfileDouble("slidermaxv"); }
    double GetFaderMinDB() { return GetPrivateProfileDouble("sliderminv"); }
    double GetVUMaxDB() { return GetPrivateProfileDouble("vumaxvol"); }
    double GetVUMinDB() { return GetPrivateProfileDouble("vuminvol"); }
    
    int *GetTimeModePtr() { return (int *) projectconfig_var_addr(NULL,timeModeOffs_); }
    int *GetTimeMode2Ptr() { return (int *) projectconfig_var_addr(NULL,timeMode2Offs_); }
    int *GetMeasOffsPtr() { return (int *) projectconfig_var_addr(NULL,measOffsOffs_); }
    double *GetTimeOffsPtr() { return (double *) projectconfig_var_addr(NULL,timeOffsOffs_); }
    int GetProjectPanMode() { int *p = (int *) projectconfig_var_addr(NULL,projectPanModeOffs_); return p ? *p : 0; }
   
    double *GetMetronomePrimaryVolumePtr()
    {
      void *ret = projectconfig_var_addr(NULL,projectMetronomePrimaryVolumeOffs_);
      if (ret) return (double *)ret;
      // REAPER 7.09 and earlier require this:
      int size=0;
      ret = get_config_var("projmetrov1", &size);
      if (size==8) return (double *)ret;
      return NULL;
    }
    
    double *GetMetronomeSecondaryVolumePtr()
    { 
      void *ret = projectconfig_var_addr(NULL,projectMetronomeSecondaryVolumeOffs_);
      if (ret) return (double *)ret;
      // REAPER 7.09 and earlier require this:
      int size=0;
      ret = get_config_var("projmetrov2", &size);
      if (size==8) return (double *)ret;
      return NULL;
    }

    void Speak(const char *phrase)
    {
        static void (*osara_outputMessage)(const char *message);
        static bool chk;
    
        if (!chk)
        {
            *(void **)&osara_outputMessage = plugin_getapi("osara_outputMessage");
            chk = true;
        }

        if (osara_outputMessage)
            osara_outputMessage(phrase);
    }
    
    Action *GetFXParamAction(char *FXName)
    {
       if (strstr(FXName, "JS: "))
           return actions_["JSFXParam"].get();
       else
           return actions_["FXParam"].get();
    }
    
    Action *GetAction(const char *actionName)
    {
        if (actions_.find(actionName) != actions_.end())
            return actions_[actionName].get();
        else
        {
            LogToConsole(256, "[ERROR] FAILED to GetAction '%s'\n", actionName);

            return actions_["NoAction"].get();
        }
    }

    void OnTrackSelection(MediaTrack *track) override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackSelection(track);
    }
    
    void SetTrackListChange() override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackListChange();
    }
    
    void NextTimeDisplayMode()
    {
        int *tmodeptr = GetTimeMode2Ptr();
        if (tmodeptr && *tmodeptr>=0)
        {
            (*tmodeptr)++;
            if ((*tmodeptr)>5)
                (*tmodeptr)=0;
        }
        else
        {
            tmodeptr = GetTimeModePtr();
            
            if (tmodeptr)
            {
                (*tmodeptr)++;
                if ((*tmodeptr)>5)
                    (*tmodeptr)=0;
            }
        }
    }
    
    void SetTrackOffset(int offset)
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->SetTrackOffset(offset);
    }
    
    void AdjustBank(Page *sendingPage, const char *zoneName, int amount)
    {
        if (! sendingPage->GetSynchPages())
            sendingPage->AdjustBank(zoneName, amount);
        else
            for (int i = 0; i < pages_.size(); ++i)
                if (pages_[currentPageIndex_]->GetSynchPages())
                    pages_[currentPageIndex_]->AdjustBank(zoneName, amount);
    }
       
    void NextPage()
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
        {
            pages_[currentPageIndex_]->LeavePage();
            currentPageIndex_ = currentPageIndex_ == pages_.size() - 1 ? 0 : (currentPageIndex_ + 1);
            if (pages_[currentPageIndex_])
                pages_[currentPageIndex_]->EnterPage();
        }
    }
    
    void GoToPage(const char *pageName)
    {
        for (int i = 0; i < pages_.size(); ++i)
        {
            if (! strcmp(pages_[i]->GetName(), pageName))
            {
                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->LeavePage();
                
                currentPageIndex_ = i;
                
                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->EnterPage();
                break;
            }
        }
    }
    
    bool GetTouchState(MediaTrack *track, int touchedControl) override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            return pages_[currentPageIndex_]->GetTouchState(track, touchedControl);
        else
            return false;
    }
    
    void TrackFXListChanged(MediaTrack *track)
    {
        for (auto &page : pages_)
            page->TrackFXListChanged(track);
        
        if (g_fxParamsWrite)
        {
            char fxName[MEDBUF];
            
            for (int i = 0; i < TrackFX_GetCount(track); ++i)
            {
                TrackFX_GetFXName(track, i, fxName, sizeof(fxName));
                FILE *fxFile = NULL;
                                
                if (g_fxParamsWrite)
                {
                    string fxNameNoBadChars(fxName);
                    ReplaceAllWith(fxNameNoBadChars, s_BadFileChars, "_");

                    fxFile = fopenUTF8((string(GetResourcePath()) + "/CSI/ZoneRawFXFiles/" + fxNameNoBadChars + ".txt").c_str(), "wb");
                    
                    if (fxFile)
                        fprintf(fxFile, "Zone \"%s\"\n", fxName);
                }

                for (int j = 0; j < TrackFX_GetNumParams(track, i); ++j)
                {
                    char fxParamName[MEDBUF];
                    TrackFX_GetParamName(track, i, j, fxParamName, sizeof(fxParamName));
 
                    if (fxFile)
                        fprintf(fxFile, "\tFXParam %d \"%s\"\n", j, fxParamName);
                        
                    /* step sizes
                    double stepOut = 0;
                    double smallstepOut = 0;
                    double largestepOut = 0;
                    bool istoggleOut = false;
                    TrackFX_GetParameterStepSizes(track, i, j, &stepOut, &smallstepOut, &largestepOut, &istoggleOut);

                    ShowConsoleMsg(("\n\n" + to_string(j) + " - \"" + string(fxParamName) + "\"\t\t\t\t Step = " +  to_string(stepOut) + " Small Step = " + to_string(smallstepOut)  + " LargeStep = " + to_string(largestepOut)  + " Toggle Out = " + (istoggleOut == 0 ? "false" : "true")).c_str());
                    */
                }
                
                if (fxFile)
                {
                    fprintf(fxFile,"ZoneEnd");
                    fclose(fxFile);
                }
            }
        }
    }
    
    const char *GetTCPFXParamName(MediaTrack *track, int fxIndex, int paramIndex, char *buf, int bufsz)
    {
        buf[0]=0;
        TrackFX_GetParamName(track, fxIndex, paramIndex, buf, bufsz);
        return buf;
    }
        
    //int repeats = 0;
    
    void Run() override
    {
        //int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        ReaProject* currentProject = (*EnumProjects)(-1, NULL, 0);

        if (currentProject_ != currentProject)
        {
            currentProject_ = currentProject;
            DAW::SendCommandMessage(REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES);
        }
        
        if (shouldRun_ && pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            try {
                pages_[currentPageIndex_]->Run();
            } catch (const ReloadPluginException& e) {
                if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole(256, "[NOTICE] RELOADING: : %s\n", e.what());
                ResetWidgets();
            } catch (const std::exception& e) {
                LogToConsole(256, "[ERROR] # CSurfIntegrator::RUN: %s\n", e.what());
                LogStackTraceToConsole();
            }
        }


        /*
         repeats++;
         
         if (repeats > 50)
         {
         repeats = 0;
         
         int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
         
         LogToConsole(256, "%d microseconds\n", duration);
         }
        */
    }
};

/*
 int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
 
 
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 
 
 
 int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
 
 LogToConsole(256, "%d microseconds\n", duration);
 
 */

#endif /* control_surface_integrator.h */
