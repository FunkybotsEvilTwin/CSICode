//
//  control_surface_manager_actions.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_manager_actions_h
#define control_surface_manager_actions_h

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SendMIDIMessage : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SendMIDIMessage"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        vector<string> tokens;
        GetTokens(tokens, context->GetStringParam());
        
        if (tokens.size() == 3)
        {
            context->GetSurface()->SendMidiMessage(strToHex(tokens[0]), strToHex(tokens[1]), strToHex(tokens[2]));
        }
        else
        {
            struct
            {
                MIDI_event_ex_t evt;
                char data[128];
            } midiSysExData;
            
            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = 0;
            
            for (int i = 0; i < tokens.size(); ++i)
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = strToHex(tokens[i]);
            
            context->GetSurface()->SendMidiSysExMessage(&midiSysExData.evt);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SendOSCMessage : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SendOSCMessage"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        vector<string> tokens;
        GetTokens(tokens, context->GetStringParam());

        if (tokens.size() == 1)
        {
            context->GetSurface()->SendOSCMessage(tokens[0].c_str());
            return;
        }
        
        if (tokens.size() != 2)
            return;
        
        const char *t1 = tokens[1].c_str(), *t1e = NULL;
        
        if (strstr(t1,"."))
        {
            const double dv = strtod(t1, (char **)&t1e);
            if (t1e && t1e != t1 && !*t1e)
            {
                context->GetSurface()->SendOSCMessage(tokens[0].c_str(), dv);
                return;
            }
        }
        else if (*t1)
        {
            const int v = (int)strtol(t1, (char **)&t1e, 10);
            if (t1e && t1e != t1 && !*t1e)
            {
                context->GetSurface()->SendOSCMessage(tokens[0].c_str(), v);
                return;
            }
        }

        context->GetSurface()->SendOSCMessage(tokens[0].c_str(), tokens[1].c_str());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakOSARAMessage : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SpeakOSARAMessage"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetCSI()->Speak(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetXTouchDisplayColors : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetXTouchDisplayColors"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetZone()->SetXTouchDisplayColors(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RestoreXTouchDisplayColors : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "RestoreXTouchDisplayColors"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetZone()->RestoreXTouchDisplayColors();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SaveProject : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SaveProject"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        if (IsProjectDirty(NULL))
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (IsProjectDirty(NULL))
            Main_SaveProject(NULL, false);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Undo : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Undo"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        if (DAW::CanUndo())
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (DAW::CanUndo())
            DAW::Undo();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Redo : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Redo"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        if (DAW::CanRedo())
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (DAW::CanRedo())
            DAW::Redo();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleSynchPageBanking : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleSynchPageBanking"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetPage()->GetSynchPages());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetPage()->ToggleSynchPages();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFollowMCP : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleFollowMCP"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetPage()->GetFollowMCP());
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetPage()->ToggleFollowMCP();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleScrollLink : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleScrollLink"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetPage()->GetScrollLink());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetPage()->ToggleScrollLink(context->GetIntParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleRestrictTextLength : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleRestrictTextLength"; }
        
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->ToggleRestrictTextLength(context->GetIntParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSINameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CSINameDisplay"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(s_CSIName);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSIVersionDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CSIVersionDisplay"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(s_CSIVersionDisplay);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GlobalModeDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GlobalModeDisplay"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetGlobal());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTimeDisplayModes : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "CycleTimeDisplayModes"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetCSI()->NextTimeDisplayMode();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoNextPage : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoNextPage"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetCSI()->NextPage();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoPage : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoPage"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetCSI()->GoToPage(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PageNameDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "PageNameDisplay"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetPage()->GetName());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoHome : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoHome"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetZoneManager()->GetIsHomeZoneOnlyActive())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->DeclareGoHome();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AllSurfacesGoHome : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "AllSurfacesGoHome"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetPage()->GoHome();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoSubZone : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoSubZone"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetZone()->GoSubZone(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LeaveSubZone : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "LeaveSubZone"; }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(1.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetZone()->Deactivate();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoFXSlot  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoFXSlot"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            context->GetSurface()->GetZoneManager()->DeclareGoFXSlot(track, context->GetZone()->GetNavigator(), context->GetSlotIndex());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ShowFXSlot  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ShowFXSlot"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), true);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class HideFXSlot  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "HideFXSlot"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), false);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleUseLocalModifiers  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleUseLocalModifiers"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->ToggleUseLocalModifiers();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleUseLocalFXSlot  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleUseLocalFXSlot"; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetToggleUseLocalFXSlot());
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->ToggleUseLocalFXSlot();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetLatchTime  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetLatchTime"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->SetLatchTime(context->GetIntParam());
    }
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleEnableFocusedFXMapping  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleEnableFocusedFXMapping"; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableFocusedFXMapping();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DisableFocusedFXMapping  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "DisableFocusedFXMapping"; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->DisableFocusedFXMapping();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleEnableLastTouchedFXParamMapping  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ToggleEnableLastTouchedFXParamMapping"; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableLastTouchedFXParamMapping();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DisableLastTouchedFXParamMapping  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "DisableLastTouchedFXParamMapping"; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->GetZoneManager()->DisableLastTouchedFXParamMapping();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LearnFocusedFX  : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "LearnFocusedFX"; }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        RequestFocusedFXDialog(context->GetSurface()->GetZoneManager());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GoZone : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "GoZone"; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetZoneManager()->GetIsGoZoneActive(context->GetStringParam()))
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
       
        const char *name = context->GetStringParam();
        
        if (!strcmp(name, "Folder") ||
            !strcmp(name, "VCA") ||
            !strcmp(name, "TrackSend") ||
            !strcmp(name, "TrackReceive") ||
            !strcmp(name, "MasterTrackFXMenu") ||
            !strcmp(name, "TrackFXMenu"))
            context->GetPage()->GoZone(name);
        else
            context->GetSurface()->GetZoneManager()->DeclareGoZone(name);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearLastTouchedFXParam : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearLastTouchedFXParam"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("LastTouchedFXParam");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearFocusedFX : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearFocusedFX"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("FocusedFX");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearSelectedTrackFX : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearSelectedTrackFX"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("SelectedTrackFX");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearFXSlot : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearFXSlot"; }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("FXSlot");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Bank : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "Bank"; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value < 0 && context->GetRangeMinimum() < 0)
            context->GetCSI()->AdjustBank(context->GetPage(), context->GetStringParam(), context->GetIntParam());
        else if (value > 0 && context->GetRangeMinimum() >= 0)
            context->GetCSI()->AdjustBank(context->GetPage(), context->GetStringParam(), context->GetIntParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetShift : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetShift"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetShift();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetShift(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetOption : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetOption"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetOption();
    }
    
    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetOption(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetControl : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetControl"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetControl();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetControl(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetAlt : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetAlt"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetAlt();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetAlt(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetFlip : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetFlip"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetFlip();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetFlip(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetGlobal : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetGlobal"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetGlobal();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetGlobal(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetMarker : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetMarker"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetMarker();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetMarker(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetNudge : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetNudge"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetNudge();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetNudge(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetZoom : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetZoom"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetZoom();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetZoom(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetScrub : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetScrub"; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return context->GetSurface()->GetScrub();
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->SetScrub(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearModifier : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearModifier"; }
   
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->ClearModifier(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearModifiers : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "ClearModifiers"; }
   
    void Do(ActionContext *context, double value) override
    {
        context->GetSurface()->ClearModifiers();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SetToggleChannel : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual const char *GetName() override { return "SetToggleChannel"; }
     
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->ToggleChannel(context->GetWidget()->GetChannelNumber());
    }
};

#endif /* control_surface_manager_actions_h */
