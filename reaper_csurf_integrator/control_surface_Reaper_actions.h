//
//  control_surface_Reaper_actions.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_Reaper_actions_h
#define control_surface_Reaper_actions_h

#include "control_surface_action_contexts.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// class FXParam : public FXAction
//////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParam : public FXAction
{
public:
    virtual const char* GetName() override { return "FXParam"; }

    virtual void Do(ActionContext* context, double value) override
    {
        if (MediaTrack* track = context->GetTrack())
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
            TrackFX_SetParamNormalized(track, context->GetSlotIndex(), context->GetParamIndex(), value);
#else
            TrackFX_SetParam(track, context->GetSlotIndex(), context->GetParamIndex(), value);
#endif
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (MediaTrack* track = context->GetTrack())
            if (value == 0)
                TrackFX_EndParamEdit(track, context->GetSlotIndex(), context->GetParamIndex());
            else
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
                TrackFX_SetParamNormalized(track, context->GetSlotIndex(), context->GetParamIndex(),
                    TrackFX_GetParamNormalized(track, context->GetSlotIndex(), context->GetParamIndex()));
#else
        {
            double min, max;
            TrackFX_SetParam(track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                TrackFX_GetParam(track,
                    context->GetSlotIndex(),
                    context->GetParamIndex(),
                    &min, &max));
        }
#endif
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class JSFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class JSFXParam : public FXAction
{
public:
    virtual const char* GetName() override { return "JSFXParam"; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
            // use host's normalized getter
            return TrackFX_GetParamNormalized(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex()
            );
#else
            // fallback: manual raw->normalized
            double raw = 0.0, min = 0.0, max = 0.0;
            raw = TrackFX_GetParam(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                &min,
                &max
            );
            double range = max - min;
            if (range <= 0.0) return 0.0;
            if (min < 0.0) raw += fabs(min);
            return raw / range;
#endif
        }
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            // update main widget
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));

            // update stepped display if needed
            if (context->GetNumberOfSteppedValues() > 0)
            {
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
                // normalized getter
                context->UpdateJSFXWidgetSteppedValue(
                    TrackFX_GetParamNormalized(
                        track,
                        context->GetSlotIndex(),
                        context->GetParamIndex()
                    )
                );
#else
                // raw getter + remap to 0-1
                double raw = 0.0, min = 0.0, max = 0.0;
                raw = TrackFX_GetParam(
                    track,
                    context->GetSlotIndex(),
                    context->GetParamIndex(),
                    &min,
                    &max
                );
                double range = max - min;
                double norm = (range > 0.0) ? ((raw - min) / range) : 0.0;
                context->UpdateJSFXWidgetSteppedValue(norm);
#endif
            }
        }
        else
        {
            context->ClearWidget();
        }
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
            // normalized setter
            TrackFX_SetParamNormalized(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                value
            );
#else
            // fallback: raw setter with manual range mapping
            double min = 0.0, max = 0.0;
            TrackFX_GetParam(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                &min,
                &max
            );
            double raw = min + value * (max - min);
            TrackFX_SetParam(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                raw
            );
#endif
        }
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            if (value == 0.0)
            {
                TrackFX_EndParamEdit(
                    track,
                    context->GetSlotIndex(),
                    context->GetParamIndex()
                );
            }
            else
            {
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
                // re-apply current normalized value
                TrackFX_SetParamNormalized(
                    track,
                    context->GetSlotIndex(),
                    context->GetParamIndex(),
                    GetCurrentNormalizedValue(context)
                );
#else
                // fallback: re-apply raw
                double min = 0.0, max = 0.0;
                double raw = TrackFX_GetParam(
                    track,
                    context->GetSlotIndex(),
                    context->GetParamIndex(),
                    &min,
                    &max
                );
                TrackFX_SetParam(
                    track,
                    context->GetSlotIndex(),
                    context->GetParamIndex(),
                    raw
                );
#endif
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class TCPFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParam : public FXAction
{
public:
    virtual const char* GetName() override { return "TCPFXParam"; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            int index = context->GetIntParam();
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0, paramIndex = 0;
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                {
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
                    return TrackFX_GetParamNormalized(track, fxIndex, paramIndex);
#else
                    double raw = TrackFX_GetParam(track, fxIndex, paramIndex, &min, &max);
                    double range = max - min;
                    return (range > 0.0) ? ((raw - min) / range) : 0.0;
#endif
                }
            }
        }
        return 0.0;
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            int index = context->GetIntParam();
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0, paramIndex = 0;
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                {
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
                    TrackFX_SetParamNormalized(track, fxIndex, paramIndex, value);
#else
                    TrackFX_SetParam(track, fxIndex, paramIndex, value);
#endif
                }
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        // no-op
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class LastTouchedFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParam : public FXAction
{
public:
    virtual const char* GetName() override { return "LastTouchedFXParam"; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) return 0.0;
        MediaTrack* track = DAW::GetTrack(trackNum);
        if (!track) return 0.0;
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
        return TrackFX_GetParamNormalized(track, fxSlotNum, fxParamNum);
#else
        double raw = 0.0, min = 0.0, max = 0.0;
        raw = TrackFX_GetParam(track, fxSlotNum, fxParamNum, &min, &max);
        double range = max - min;
        return (range > 0.0) ? ((raw - min) / range) : 0.0;
#endif
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            context->ClearWidget();
            return;
        }
        MediaTrack* track = DAW::GetTrack(trackNum);
        if (!track) { context->ClearWidget(); return; }
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
        context->UpdateWidgetValue(
            TrackFX_GetParamNormalized(track, fxSlotNum, fxParamNum)
        );
#else
        double raw = 0.0, min = 0.0, max = 0.0;
        raw = TrackFX_GetParam(track, fxSlotNum, fxParamNum, &min, &max);
        double range = max - min;
        double norm = (range > 0.0) ? ((raw - min) / range) : 0.0;
        context->UpdateWidgetValue(norm);
#endif
    }

    virtual void Do(ActionContext* context, double value) override
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) return;
        MediaTrack* track = DAW::GetTrack(trackNum);
        if (!track) return;
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
        TrackFX_SetParamNormalized(track, fxSlotNum, fxParamNum, value);
#else
        TrackFX_SetParam(track, fxSlotNum, fxParamNum, value);
#endif
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) return;
        MediaTrack* track = DAW::GetTrack(trackNum);
        if (!track) return;

        if (value == 0.0)
            TrackFX_EndParamEdit(track, fxSlotNum, fxParamNum);
        else
        {
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
            TrackFX_SetParamNormalized(
                track, fxSlotNum, fxParamNum,
                GetCurrentNormalizedValue(context)
            );
#else
            double rawMin = 0.0, rawMax = 0.0;
            TrackFX_GetParam(track, fxSlotNum, fxParamNum, &rawMin, &rawMax);
            double norm = GetCurrentNormalizedValue(context);
            double raw = rawMin + norm * (rawMax - rawMin);
            TrackFX_SetParam(track, fxSlotNum, fxParamNum, raw);
#endif
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFXBypass : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleFXBypass"; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            //I_FXEN : fx enabled, 0=bypassed, !0=fx active
            if (GetMediaTrackInfo_Value(track, "I_FXEN") == 0)
                context->UpdateWidgetValue(0.0);
            else if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(1.0);
                else
                    context->UpdateWidgetValue(0.0);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetEnabled(track, context->GetSlotIndex(), ! TrackFX_GetEnabled(track, context->GetSlotIndex()));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXBypassDisplay : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXBypassDisplay"; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            //I_FXEN : fx enabled, 0=bypassed, !0=fx active
            if (GetMediaTrackInfo_Value(track, "I_FXEN") == 0)
                context->UpdateWidgetValue("Bypassed");
            else if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Enabled");
                else
                    context->UpdateWidgetValue("Bypassed");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFXOffline : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleFXOffline"; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(0.0);
                else
                    context->UpdateWidgetValue(1.0);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetOffline(track, context->GetSlotIndex(), ! TrackFX_GetOffline(track, context->GetSlotIndex()));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXOfflineDisplay : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXOfflineDisplay"; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Offline");
                else
                    context->UpdateWidgetValue("Online");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolume : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVolume"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SoftTakeover7BitTrackVolume : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SoftTakeover7BitTrackVolume"; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double trackVolume, trackPan = 0.0;
            GetTrackUIVolPan(track, &trackVolume, &trackPan);
            trackVolume = volToNormalized(trackVolume);
            
            if ( fabs(value - trackVolume) < 0.025) // GAW -- Magic number -- ne touche pas
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SoftTakeover14BitTrackVolume : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SoftTakeover14BitTrackVolume"; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double trackVolume, trackPan = 0.0;
            GetTrackUIVolPan(track, &trackVolume, &trackPan);
            trackVolume = volToNormalized(trackVolume);
            
            if ( fabs(value - trackVolume) < 0.0025) // GAW -- Magic number -- ne touche pas
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeDB : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVolumeDB"; }
    
    virtual double GetCurrentDBValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return VAL2DB(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(GetCurrentDBValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(GetCurrentDBValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPan : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPan"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                return panToNormalized(pan);
            }
        }
        
        return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanPercent"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                context->UpdateWidgetValue(pan  *100.0);
            }
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != 6)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, value / 100.0, false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, pan, false), NULL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidth : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanWidth"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != 6)
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != 6)
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidthPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanWidthPercent"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_WIDTH")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != 6)
                CSurf_OnWidthChange(track, value / 100.0, false);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != 6)
                CSurf_OnWidthChange(track, GetMediaTrackInfo_Value(track, "D_WIDTH"), false);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanL : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanL"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanLPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanLPercent"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANL")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panFromPercent);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanR : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanR"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanRPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanRPercent"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANR")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panFromPercent);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoLeft : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanAutoLeft"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
            else
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                return panToNormalized(pan);
            }
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            }
            else
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
            else
            {
                context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoRight : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanAutoRight"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
            else
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            }
            else
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == 6)
                context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
            else
            {
                context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);

            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordArm : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackRecordArm"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_RECARM");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            CSurf_SetSurfaceRecArm(track, CSurf_OnRecArmChange(track, ! GetMediaTrackInfo_Value(track, "I_RECARM")), NULL);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordArmDisplay : public Action
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char* GetName() override { return "TrackRecordArmDisplay"; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            double state = GetMediaTrackInfo_Value(track, "I_RECARM");

            if (state > 0.5)
                context->UpdateWidgetValue("ARM");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackMute : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackMute"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            bool mute = false;
            GetTrackUIMute(track, &mute);
            return mute;
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool mute = false;
            GetTrackUIMute(track, &mute);
            CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, ! mute), NULL);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSolo : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSolo"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SOLO") > 0 ? 1 : 0;
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            if (track != GetMasterTrack(NULL))
                CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, ! GetMediaTrackInfo_Value(track, "I_SOLO")), NULL);
            else
            {
                int muteSoloFlags = GetMasterMuteSoloFlags();
                if (muteSoloFlags & 2)
                    muteSoloFlags &= !2;
                else
                    muteSoloFlags |= 2;
                
                CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, muteSoloFlags), NULL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInvertPolarity : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackInvertPolarity"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "B_PHASE");
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool reversed = ! GetMediaTrackInfo_Value(track, "B_PHASE");
            
            GetSetMediaTrackInfo(track, "B_PHASE", &reversed);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSelect : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSelect"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            CSurf_SetSurfaceSelected(track, CSurf_OnSelectedChange(track, ! GetMediaTrackInfo_Value(track, "I_SELECTED")), NULL);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackUniqueSelect : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackUniqueSelect"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            SetOnlyTrackSelected(track);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRangeSelect : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackRangeSelect"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        int currentlySelectedCount = 0;
        int selectedTrackIndex = 0;
        int trackIndex = 0;
        
       
        for (int i = 1; i <= context->GetPage()->GetNumTracks(); ++i)
        {
            MediaTrack *currentTrack = context->GetPage()->GetTrackFromId(i);
           
            if (currentTrack == NULL)
                continue;
            
            if (currentTrack == context->GetTrack())
                trackIndex = i;
            
            if (GetMediaTrackInfo_Value(currentTrack, "I_SELECTED"))
            {
                selectedTrackIndex = i;
                currentlySelectedCount++;
            }
        }
        
        if (currentlySelectedCount != 1)
            return;
        
        int lowerBound = trackIndex < selectedTrackIndex ? trackIndex : selectedTrackIndex;
        int upperBound = trackIndex > selectedTrackIndex ? trackIndex : selectedTrackIndex;

        for (int i = lowerBound; i <= upperBound; ++i)
        {
            MediaTrack *currentTrack = context->GetPage()->GetTrackFromId(i);
            
            if (currentTrack == NULL)
                continue;
            
            if (context->GetPage()->GetIsTrackVisible(currentTrack))
                CSurf_SetSurfaceSelected(currentTrack, CSurf_OnSelectedChange(currentTrack, 1), NULL);
        }
        
        MediaTrack *lowestTrack = context->GetPage()->GetTrackFromId(lowerBound);
        
        if (lowestTrack != NULL)
            context->GetPage()->OnTrackSelectionBySurface(lowestTrack);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolume : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendVolume"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, normalizedToVol(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, vol, 1);
            else
                SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolumeDB : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendVolumeDB"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            context->UpdateWidgetValue(VAL2DB(vol));
        }
        else
            context->ClearWidget();
        
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, DB2VAL(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, vol, 1);
            else
                SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPan : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendPan"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            return panToNormalized(pan);
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, normalizedToPan(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, pan, 1);
            else
                SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPanPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendPanPercent"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            context->UpdateWidgetValue(pan  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, value / 100.0, 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, pan, 1);
            else
                SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendMute : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendMute"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            bool mute = false;
            GetTrackSendUIMute(track, context->GetSlotIndex() + numHardwareSends, &mute);
            return mute;
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            ToggleTrackSendUIMute(track, context->GetSlotIndex() + GetTrackNumSends(track, 1));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendInvertPolarity : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendInvertPolarity"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_PHASE");
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool reversed = ! GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_PHASE");
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "B_PHASE", &reversed);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendStereoMonoToggle : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendStereoMonoToggle"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO");
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool mono = ! GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO");
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "B_MONO", &mono);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPrePost : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendPrePost"; }
       
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            int mode = (int)GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "I_SENDMODE");
            
            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolume : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveVolume"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), normalizedToVol(value), 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 1);
            else
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolumeDB : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveVolumeDB"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            context->UpdateWidgetValue(VAL2DB(vol));
        }
        else
            context->ClearWidget();
        
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), DB2VAL(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 1);
            else
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePan : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivePan"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            return panToNormalized(pan);
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), normalizedToPan(value), 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 1);
            else
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePanPercent : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivePanPercent"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            context->UpdateWidgetValue(pan  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), value / 100.0, 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 1);
            else
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveMute : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivMute"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            bool mute = false;
            GetTrackReceiveUIMute(track, context->GetSlotIndex(), &mute);
            return mute;
        }
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool isMuted = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MUTE");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MUTE", &isMuted);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveInvertPolarity : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveInvertPolarity"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_PHASE");
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool reversed = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_PHASE");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_PHASE", &reversed);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveStereoMonoToggle : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveStereoMonoToggle"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO");
        else
            return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            bool mono = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MONO", &mono);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePrePost : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivePrePost"; }
        
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            int mode = (int)GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "I_SENDMODE");
            
            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan

            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXNameDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetName());
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXMenuNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXMenuNameDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char alias[MEDBUF];
            alias[0] = 0;
            
            if (context->GetSlotIndex() < TrackFX_GetCount(track))
            {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->UpdateWidgetValue(alias);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakFXMenuName : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SpeakFXMenuName"; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            char alias[MEDBUF];
            alias[0] = 0;
            
            if (context->GetSlotIndex() < TrackFX_GetCount(track))
            {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->GetCSI()->Speak(alias);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParamNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXParamNameDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (context->GetFXParamDisplayName()[0])
                context->UpdateWidgetValue(context->GetFXParamDisplayName());
            else
            {
                char tmp[MEDBUF];
                TrackFX_GetParamName(track, context->GetSlotIndex(), context->GetParamIndex(), tmp, sizeof(tmp));
                context->UpdateWidgetValue(tmp);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParamNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TCPFXParamNameDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int index = context->GetIntParam();
            
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0;
                int paramIndex = 0;
                
                char tmp[MEDBUF];
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                    context->UpdateWidgetValue(context->GetCSI()->GetTCPFXParamName(track, fxIndex, paramIndex, tmp, sizeof(tmp)));
                else
                    context->ClearWidget();
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParamValueDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXParamValueDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char fxParamValue[128];
            TrackFX_GetFormattedParamValue(track, context->GetSlotIndex(), context->GetParamIndex(), fxParamValue, sizeof(fxParamValue));
            context->UpdateWidgetValue(fxParamValue);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParamValueDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TCPFXParamValueDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int index = context->GetIntParam();
            
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0;
                int paramIndex = 0;
                
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                {
                    char fxParamValue[128];
                    TrackFX_GetFormattedParamValue(track, fxIndex, paramIndex, fxParamValue, sizeof(fxParamValue));
                    context->UpdateWidgetValue(fxParamValue);
                }
                else
                    context->ClearWidget();
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParamNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "LastTouchedFXParamNameDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        int trackNum = 0;
        int fxSlotNum = 0;
        int fxParamNum = 0;
        
        if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            if (MediaTrack *track = DAW::GetTrack(trackNum))
            {
                char tmp[MEDBUF];
                TrackFX_GetParamName(track, fxSlotNum, fxParamNum, tmp, sizeof(tmp));
                context->UpdateWidgetValue(tmp);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParamValueDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "LastTouchedFXParamValueDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        int trackNum = 0;
        int fxSlotNum = 0;
        int fxParamNum = 0;
        
        if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            if (MediaTrack *track = DAW::GetTrack(trackNum))
            {
                char fxParamValue[128];
                TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, fxParamValue, sizeof(fxParamValue));
                context->UpdateWidgetValue(fxParamValue);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendNameDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            const char *sendTrackName = "";
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
                sendTrackName = (const char *)GetSetMediaTrackInfo(destTrack, "P_NAME", NULL);
            context->UpdateWidgetValue(sendTrackName);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakTrackSendDestination : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SpeakTrackSendDestination"; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                const char *sendTrackName = (const char *)GetSetMediaTrackInfo(destTrack, "P_NAME", NULL);
                char tmp[MEDBUF];
                snprintf(tmp, sizeof(tmp), "Track %d%s%s",
                    context->GetPage()->GetIdFromTrack(destTrack),
                    sendTrackName && *sendTrackName ? " " : "",
                    sendTrackName ? sendTrackName : "");
                context->GetCSI()->Speak(tmp);
            }
            else
                context->GetCSI()->Speak("No Send Track");
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolumeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendVolumeDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                int numHardwareSends = GetTrackNumSends(track, 1);
                double vol, pan = 0.0;
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);

                char trackVolume[128];
                snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(vol));
                context->UpdateWidgetValue(trackVolume);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPanDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendPanDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                int numHardwareSends = GetTrackNumSends(track, 1);
                double vol, pan = 0.0;
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);

                char tmp[MEDBUF];
                context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendStereoMonoDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendStereoMonoDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);
            if (destTrack)
            {
                if (GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO"))
                    context->UpdateWidgetValue("mono");
                else
                    context->UpdateWidgetValue("stereo");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPrePostDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackSendPrePostDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                // I_SENDMODE : returns int *, 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                
                double prePostVal = GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "I_SENDMODE");
                
                const char *prePostValueString = "";
                
                if (prePostVal == 0)
                    prePostValueString = "PostPan";
                else if (prePostVal == 1)
                    prePostValueString = "PreFX";
                else if (prePostVal == 2 || prePostVal == 3)
                    prePostValueString = "PostFX";
                
                context->UpdateWidgetValue(prePostValueString);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveNameDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                const char *receiveTrackName = (const char *)GetSetMediaTrackInfo(srcTrack, "P_NAME", NULL);
                context->UpdateWidgetValue(receiveTrackName);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakTrackReceiveSource : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SpeakTrackReceiveSource"; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                const char *receiveTrackName = (const char *)GetSetMediaTrackInfo(srcTrack, "P_NAME", NULL);
                char tmp[MEDBUF];
                snprintf(tmp, sizeof(tmp), "Track %d%s%s", context->GetPage()->GetIdFromTrack(srcTrack),
                    receiveTrackName && *receiveTrackName ? " " : "",
                    receiveTrackName ? receiveTrackName : "");
                context->GetCSI()->Speak(tmp);
            }
            else
                context->GetCSI()->Speak("No Receive Track");
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolumeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveVolumeDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                char trackVolume[128];
                snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_VOL")));
                context->UpdateWidgetValue(trackVolume);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePanDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivePanDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                double panVal = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_PAN");
                
                char tmp[MEDBUF];
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "", tmp, sizeof(tmp)));
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveStereoMonoDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceiveStereoMonoDisplay "; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                if (GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO"))
                    context->UpdateWidgetValue("mono");
                else
                    context->UpdateWidgetValue("stereo");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePrePostDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackReceivePrePostDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                // I_SENDMODE : returns int *, 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                
                double prePostVal = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "I_SENDMODE");
                
                const char *prePostValueString = "";
                
                if (prePostVal == 0)
                    prePostValueString = "PostPan";
                else if (prePostVal == 1)
                    prePostValueString = "PreFX";
                else if (prePostVal == 2 || prePostVal == 3)
                    prePostValueString = "PostFX";
                
                context->UpdateWidgetValue(prePostValueString);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedTextDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FixedTextDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedRGBColorDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FixedRGBColorDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(0.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackNameDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char buf[MEDBUF];
            
            GetTrackName(track, buf, sizeof(buf));
            
            context->UpdateWidgetValue(buf);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNumberDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackNumberDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double index = GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
            char idx[128];
            snprintf(idx, sizeof(idx), "%d", (int)index);

            context->UpdateWidgetValue(idx);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordInputDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackRecordInputDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            /*
            // I_RECINPUT : int  *: record input, <0=no input.
            if 4096 set, input is MIDI and low 5 bits represent channel (0=all, 1-16=only chan), next 6 bits represent physical input (63=all, 62=VKB).
            If 4096 is not set, low 10 bits (0..1023) are input start channel (ReaRoute/Loopback start at 512).
            If 2048 is set, input is multichannel input (using track channel count).
            If 1024 is set, input is stereo input, otherwise input is mono.
            */
            
            char inputDisplay[MEDBUF];
            
            int input = (int)GetMediaTrackInfo_Value(track, "I_RECINPUT");

            if (input < 0)
                lstrcpyn_safe(inputDisplay, "None", sizeof(inputDisplay));
            else if (input & 4096)
            {
                int channel = input & 0x1f;
                
                if (channel == 0)
                    lstrcpyn_safe(inputDisplay, "MD All", sizeof(inputDisplay));
                else
                    snprintf(inputDisplay, sizeof(inputDisplay), "MD %d", channel);
            }
            else if (input & 2048)
            {
                lstrcpyn_safe(inputDisplay, "Multi", sizeof(inputDisplay));
            }
            else if (input & 1024)
            {
                int channels = input ^ 1024;
                
                snprintf(inputDisplay, sizeof(inputDisplay), "%d+%d", channels + 1, channels + 2);
            }
            else
            {
                snprintf(inputDisplay, sizeof(inputDisplay), "Mno %d", input + 1);
            }

            context->UpdateWidgetValue(inputDisplay);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInvertPolarityDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackInvertPolarityDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetMediaTrackInfo_Value(track, "B_PHASE") == 0)
                context->UpdateWidgetValue("Normal");
            else
                context->UpdateWidgetValue("Invert");
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVolumeDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);

            char trackVolume[128];
            snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(vol));
            context->UpdateWidgetValue(trackVolume);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);

            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidthDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanWidthDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanLeftDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanLeftDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanRightDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanRightDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoLeftDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanAutoLeftDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char tmp[MEDBUF];
            if (GetPanMode(track) == 6)
            {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
            }
            else
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoRightDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackPanAutoRightDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char tmp[MEDBUF];
            if (GetPanMode(track) == 6)
            {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
            }
            else
            {
                double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");
                context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Rewind : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Rewind"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetIsRewinding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->StartRewinding();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MoveCursor : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "MoveCursor"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return  0.5;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(1.0);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        // Below is the Reaper API call, might be worth investigating using this.
        // MoveEditCursor(double adjamt, bool dosel);
        
        const char *amount = context->GetStringParam();
        
        if (value > 0.5)
        {
            if ( ! strcmp(amount, "Bar"))
                DAW::SendCommandMessage(41042); // move to next bar
            
            else if ( ! strcmp(amount, "Marker"))
                DAW::SendCommandMessage(40173); // move to next marker/region

            else if ( ! strcmp(amount, "Beat"))
                DAW::SendCommandMessage(41044); // move to next beat
        }
        else if (value < 0.5)
        {
            if ( ! strcmp(amount, "Bar"))
                DAW::SendCommandMessage(41043); // move to previous bar
            
            else if ( ! strcmp(amount, "Marker"))
                DAW::SendCommandMessage(40172); // move to previous marker/region

            else if ( ! strcmp(amount, "Beat"))
                DAW::SendCommandMessage(41045); // move to previous beat
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FastForward : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FastForward"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetIsFastForwarding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->StartFastForwarding();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Play : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Play"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int playState = GetPlayState();
        if (playState == 1 || playState == 2 || playState == 5 || playState == 6) // playing or paused or recording or paused whilst recording
            playState = 1;
        else playState = 0;

        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            playState = 0;

        return playState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Play();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Stop : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Stop"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int stopState = GetPlayState();
        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
            stopState = 1;
        else stopState = 0;
       
        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            stopState = 0;
        
        return stopState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Stop();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Record : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Record"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int recordState = GetPlayState();
        if (recordState == 5 || recordState == 6) // recording or paused whilst recording
            recordState = 1;
        else recordState = 0;
        
        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            recordState = 0;
        
        return recordState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Record();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackToggleVCASpill : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackToggleVCASpill"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetIsVCASpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->ToggleVCASpill(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackToggleFolderSpill : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackToggleFolderSpill"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetIsFolderSpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->ToggleFolderSpill(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearAllSolo : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearAllSolo"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return AnyTrackSolo(NULL);
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        SoloAllTracks(0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GlobalAutoMode : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GlobalAutoMode"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (context->GetIntParam() == GetGlobalAutomationOverride())
            return 1.0;
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        SetGlobalAutomationOverride(context->GetIntParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackAutoMode : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackAutoMode"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        double retVal = 0.0;
        
        const vector<MediaTrack *> &selectedTracks = context->GetPage()->GetSelectedTracks();
        for (auto selectedTrack : selectedTracks)
        {
            if (context->GetIntParam() == GetMediaTrackInfo_Value(selectedTrack, "I_AUTOMODE"))
            {
                retVal = 1.0;
                break;
            }
        }

        return retVal;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        int mode = context->GetIntParam();
        
        const vector<MediaTrack *> &selectedTracks = context->GetPage()->GetSelectedTracks();
        for (auto selectedTrack : selectedTracks)
            GetSetMediaTrackInfo(selectedTrack, "I_AUTOMODE", &mode);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTrackAutoMode : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CycleTrackAutoMode"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetAutoModeDisplayName((int)GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            int autoModeIndex_ = (int)GetMediaTrackInfo_Value(track, "I_AUTOMODE");
            
            if (autoModeIndex_ == 2) // skip over write mode when cycling
                autoModeIndex_ += 2;
            else
                autoModeIndex_++;
            
            if (autoModeIndex_ > 5)
                autoModeIndex_ = 0;

            GetSetMediaTrackInfo(track, "I_AUTOMODE", &autoModeIndex_);

        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTimeline : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CycleTimeline"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return GetSetRepeatEx(NULL, -1);
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        GetSetRepeatEx(NULL, ! GetSetRepeatEx(NULL, -1));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTrackInputMonitor : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CycleTrackInputMonitor"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->NextInputMonitorMode(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackAutoModeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackAutoModeDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetAutoModeDisplayName((int)GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVCALeaderDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVCALeaderDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 || DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0)
                context->UpdateWidgetValue("Leader");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->UpdateWidgetValue("");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackFolderParentDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackFolderParentDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1)
                context->UpdateWidgetValue("Parent");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->UpdateWidgetValue("");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFolderView : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleFolderView"; }
    
    virtual void RequestUpdate(ActionContext* context) override
    {
        if (context->GetPage()->GetIsFolderViewActive())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        context->GetPage()->ToggleFolderView();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackEnterFolder : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char* GetName() override { return "TrackEnterFolder"; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        if (MediaTrack* track = context->GetTrack())
        {
            MediaTrack* trackToSelect = context->GetPage()->SetCurrentFolder(track);
            if (trackToSelect != nullptr)
            {
                SetOnlyTrackSelected(trackToSelect);
                context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ExitCurrentFolder : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char* GetName() override { return "ExitCurrentFolder"; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        context->UpdateColorValue(0.0);

        if (context->GetPage()->IsAtRootFolderLevel())
            context->UpdateWidgetValue(0.0);
        else
            context->UpdateWidgetValue(1.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        MediaTrack* trackToSelect = context->GetPage()->ExitCurrentFolder();
        if (trackToSelect != nullptr)
        {
            SetOnlyTrackSelected(trackToSelect);
            context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GlobalAutoModeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GlobalAutoModeDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetGlobalAutoModeDisplayName());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInputMonitorDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackInputMonitorDisplay"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetCurrentInputMonitorMode(track));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MCUTimeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "MCUTimeDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(0.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSCTimeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "OSCTimeDisplay"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        char timeStr[MEDBUF];
        
        double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();

        int *tmodeptr = context->GetCSI()->GetTimeMode2Ptr();
        
        int tmode = 0;
        
        if (tmodeptr && (*tmodeptr)>=0) tmode = *tmodeptr;
        else
        {
            tmodeptr = context->GetCSI()->GetTimeModePtr();
            if (tmodeptr)
                tmode=*tmodeptr;
        }

        if (tmode == 3) // seconds
        {
            double *toptr = context->GetCSI()->GetTimeOffsPtr();
            
            if (toptr)
                pp+=*toptr;
            
            snprintf(timeStr, sizeof(timeStr), "%d %d", (int)pp, ((int)(pp*100.0))%100);
        }
        else if (tmode==4) // samples
        {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), 4);
        }
        else if (tmode == 5) // frames
        {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), 5);
        }
        else if (tmode > 0)
        {
            int num_measures=0;
            int currentTimeSignatureNumerator=0;
            double beats=TimeMap2_timeToBeats(NULL,pp,&num_measures,&currentTimeSignatureNumerator,NULL,NULL)+ 0.000000000001;
            double nbeats = floor(beats);
            
            beats -= nbeats;
               
            if (num_measures <= 0 && pp < 0.0)
                --num_measures;
            
            int *measptr = context->GetCSI()->GetMeasOffsPtr();
            int subBeats = (int)(1000.0  *beats);
          
            snprintf(timeStr, sizeof(timeStr), "%d %d %03d", (num_measures+1+(measptr ? *measptr : 0)), ((int)(nbeats + 1)), subBeats);
        }
        else
        {
            double *toptr = context->GetCSI()->GetTimeOffsPtr();
            if (toptr) pp+=(*toptr);
            
            int ipp=(int)pp;
            int fr=(int)((pp-ipp)*1000.0);
            
            int hours = (int)(ipp/3600);
            int minutes = ((int)(ipp/60)) %3600;
            int seconds = ((int)ipp) %60;
            int frames =(int)fr;
            snprintf(timeStr, sizeof(timeStr), "%03d:%02d:%02d:%03d", hours, minutes, seconds, frames);
        }

        context->UpdateWidgetValue(timeStr);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeter : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackOutputMeter"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {           
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(Track_GetPeakInfo(track, context->GetIntParam())));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeterAverageLR : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackOutputMeterAverageLR"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
            
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(lrVol));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeWithMeterAverageLR : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVolumeWithMeterAverageLR"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        int stopState = GetPlayState();

        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
        {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        }
        else
        {
            if (MediaTrack *track = context->GetTrack())
            {
                double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
                
                if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                    context->ClearWidget();
                else
                    context->UpdateWidgetValue(volToNormalized(lrVol));
            }
            else
                context->ClearWidget();
        }
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeterMaxPeakLR : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackOutputMeterMaxPeakLR"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double lVol = Track_GetPeakInfo(track, 0);
            double rVol = Track_GetPeakInfo(track, 1);
            
            double lrVol =  lVol > rVol ? lVol : rVol;
            
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(lrVol));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeWithMeterMaxPeakLR : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "TrackVolumeWithMeterMaxPeakLR"; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        int stopState = GetPlayState();
        
        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
        {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        }
        else
        {
            if (MediaTrack *track = context->GetTrack())
            {
                double lVol = Track_GetPeakInfo(track, 0);
                double rVol = Track_GetPeakInfo(track, 1);
                
                double lrVol =  lVol > rVol ? lVol : rVol;
                
                if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                    context->ClearWidget();
                else
                    context->UpdateWidgetValue(volToNormalized(lrVol));
            }
            else
                context->ClearWidget();
        }
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXGainReductionMeter : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXGainReductionMeter"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        char buffer[MEDBUF];
        
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetNamedConfigParm(track, context->GetSlotIndex(), "GainReduction_dB", buffer, sizeof(buffer)))
                context->UpdateWidgetValue(-atof(buffer)/20.0);
            else
                context->UpdateWidgetValue(0.0);
        }
        else
            context->ClearWidget();
    }
};

#endif /* control_surface_Reaper_actions_h */
