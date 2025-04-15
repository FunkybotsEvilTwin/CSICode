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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
         //FIXME: refactor the logic to support all commands without crashing. "it causes crash -- CSI receives the surface control release message after this but no one is home :)"
         static const int ignoredCommands[] = { 
             41743 // Refresh all surfaces
            ,40023 // Open new project
            ,40860 // Close current project tab
            ,46000 // Insert track from template
        };
        static const size_t ignoredCommandCount = sizeof(ignoredCommands) / sizeof(int);
        for (size_t i = 0; i < ignoredCommandCount; ++i)
        {
            if (ignoredCommands[i] == context->GetCommandId())
            {
                if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole(256, "[NOTICE] Ignoring command '%d' ('%s'), it causes crash", context->GetCommandId(), DAW::GetCommandName(context->GetCommandId()));
                return;
            }
        }
        // used for Increase/Decrease
        if (value < 0 && context->GetRangeMinimum() < 0)
            DAW::SendCommandMessage(context->GetCommandId());
        else if (value > 0 && context->GetRangeMinimum() >= 0)
            DAW::SendCommandMessage(context->GetCommandId());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "FXAction"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
            return TrackFX_GetParamNormalized(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex()
            );
#else
            double raw = 0.0, min = 0.0, max = 0.0;
            raw = TrackFX_GetParam(
                track,
                context->GetSlotIndex(),
                context->GetParamIndex(),
                &min, &max
            );
            double range = max - min;
            return (range > 0.0) ? ((raw - min) / range) : 0.0;
#endif
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double currentValue = GetCurrentNormalizedValue(context);
            context->UpdateWidgetValue(currentValue);
        }
        else
            context->ClearWidget();
    }
};

#endif /* control_surface_action_contexts_h */
