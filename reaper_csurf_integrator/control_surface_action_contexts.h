//
//  control_surface_base_actions.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_action_contexts_h
#define control_surface_action_contexts_h

#define REAPERAPI_WANT_TrackFX_GetParamNormalized
#define REAPERAPI_WANT_TrackFX_SetParamNormalized
#include "reaper_plugin_functions.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NoAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "NoAction"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
        context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class InvalidAction : public NoAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "InvalidAction"; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ReaperAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ReaperAction"; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        int state = GetToggleCommandState(context->GetCommandId());
        
        if (state == -1) // this Action does not report state
            state = 0;
        
        if ( ! (context->GetRangeMinimum() == -2.0 || context->GetRangeMaximum() == 2.0)) // used for Increase/Decrease
            context->UpdateWidgetValue(state);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        int commandId = context->GetCommandId();
        // used for Increase/Decrease
        if (value < 0 && context->GetRangeMinimum() < 0)
            DAW::SendCommandMessage(commandId);
        else if (value > 0 && context->GetRangeMinimum() >= 0)
            DAW::SendCommandMessage(commandId);

        if (context->NeedsReloadAfterRun()) {
            throw ReloadPluginException(context->GetCommandText());
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MediaTrack* track_;
    int trackNum_ = -1, fxSlotNum_ = -1, fxParamNum_ = -1;
    double lastValue_ = 0.0;
public:
    virtual const char *GetName() override { return "FXAction"; }
    virtual bool IsFxRelated() { return true; }

    virtual bool ClearCurrentContext() {
        track_ = nullptr;
        trackNum_ = -1;
        fxSlotNum_ = -1;
        fxParamNum_ = -1;
        return false;
    }
    
    virtual bool CheckCurrentContext(ActionContext *context) {
        MediaTrack* currentTrack = context->GetTrack();

        if (!currentTrack)
            return ClearCurrentContext();

        if (track_ != currentTrack)
            track_ = currentTrack;

        fxSlotNum_ = context->GetSlotIndex();
        fxParamNum_ = context->GetParamIndex();
    
        return true;
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        if (!CheckCurrentContext(context)) return 0.0;
        return DAW::GetTrackFxParamValue(track_, fxSlotNum_, fxParamNum_);
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (!CheckCurrentContext(context))
            return context->ClearWidget();
        double value = DAW::GetTrackFxParamValue(track_, fxSlotNum_, fxParamNum_);
        if (DAW::CompareFaderValues(lastValue_, value)) return;
        context->UpdateWidgetValue(value);
        lastValue_ = value;
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (DAW::CompareFaderValues(lastValue_, value)) return;
        if (!CheckCurrentContext(context)) return;
        DAW::SetTrackFxParamValue(track_, fxSlotNum_, fxParamNum_, value);
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (!CheckCurrentContext(context)) return;
        TrackFX_EndParamEdit(track_, fxSlotNum_, fxParamNum_);
    }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SwitchAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() { return "SwitchAction"; }
    virtual bool IsSwitch() { return true; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ModifierAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() { return "ModifierAction"; }
    virtual bool IsModifier() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DisplayAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() { return "DisplayAction"; }
    virtual bool IsDisplayRelated() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MeterAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() { return "MeterAction"; }
    virtual bool IsDisplayRelated() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class VolumeAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MediaTrack* track_;
    double lastValue_ = 0.0;
public:
    virtual const char *GetName() { return "VolumeAction"; }
    virtual bool IsVolumeRelated() { return true; }

    virtual bool ClearCurrentContext() {
        track_ = nullptr;
        return false;
    }
    
    virtual bool CheckCurrentContext(ActionContext *context) {
        MediaTrack* currentTrack = context->GetTrack();

        if (!currentTrack)
            return ClearCurrentContext();

        if (track_ != currentTrack)
            track_ = currentTrack;
        
        return true;
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        if (!CheckCurrentContext(context)) return 0.0;
        return DAW::GetTrackVolumeValue(track_);
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (!CheckCurrentContext(context))
            return context->ClearWidget();
        double value = DAW::GetTrackVolumeValue(track_);
        if (DAW::CompareFaderValues(lastValue_, value)) return;
        context->UpdateWidgetValue(value);
        lastValue_ = value;
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (DAW::CompareFaderValues(lastValue_, value)) return;
        if (!CheckCurrentContext(context)) return;
        DAW::SetTrackVolumeValue(track_, value);
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (!CheckCurrentContext(context)) return;
        double currentValue = GetCurrentNormalizedValue(context);
        if (DAW::CompareFaderValues(lastValue_, currentValue)) return;
        DAW::SetTrackVolumeValue(track_, currentValue);
    }
    
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PanAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() { return "PanAction"; }
    virtual bool IsPanRelated() { return true; }
};

#endif /* control_surface_action_contexts_h */
