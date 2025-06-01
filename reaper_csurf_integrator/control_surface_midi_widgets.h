//
//  control_surface_midi_widgets.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_midi_widgets_h
#define control_surface_midi_widgets_h

#include <algorithm>

#include "handy_functions.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSIMessageGenerators
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PressRelease_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t press_;
    MIDI_event_ex_t release_;
    
public:
    virtual ~PressRelease_Midi_CSIMessageGenerator() {}

    PressRelease_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press) : Midi_CSIMessageGenerator(csi, widget), press_(press)
    {
        widget->SetIsTwoState();
    }
    
    PressRelease_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press, MIDI_event_ex_t release) : Midi_CSIMessageGenerator(csi, widget), press_(press), release_(release)
    {
        widget->SetIsTwoState();
    }

    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, midiMessage->IsEqualTo(&press_) ? 1 : 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Touch_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t press_;
    MIDI_event_ex_t release_;

public:
    virtual ~Touch_Midi_CSIMessageGenerator() {}
    
    Touch_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press, MIDI_event_ex_t release) : Midi_CSIMessageGenerator(csi, widget), press_(press), release_(release) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoTouch(widget_, midiMessage->IsEqualTo(&press_) ? 1 : 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AnyPress_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~AnyPress_Midi_CSIMessageGenerator() {}
    AnyPress_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget)
    {
        widget->SetIsTwoState();
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        // Doesn't matter what value was sent, just do it
        widget_->GetZoneManager()->DoAction(widget_, 1);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader14Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Fader14Bit_Midi_CSIMessageGenerator() {}
    Fader14Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, int14ToNormalized(midiMessage->midi_message[2], midiMessage->midi_message[1]));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FaderportClassicFader14Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t message1_;
    MIDI_event_ex_t message2_;

public:
    virtual ~FaderportClassicFader14Bit_Midi_CSIMessageGenerator() {}
    FaderportClassicFader14Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t message1, MIDI_event_ex_t message2) : Midi_CSIMessageGenerator(csi, widget), message1_(message1), message2_(message2) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        if (message1_.midi_message[1] == midiMessage->midi_message[1])
            message1_.midi_message[2] = midiMessage->midi_message[2];
        else if (message2_.midi_message[1] == midiMessage->midi_message[1])
            widget_->GetZoneManager()->DoAction(widget_, int14ToNormalized(message1_.midi_message[2], midiMessage->midi_message[2]));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader7Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Fader7Bit_Midi_CSIMessageGenerator() {}
    Fader7Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, midiMessage->midi_message[2] / 127.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    map<int, int> accelerationValuesForIncrement_;
    map<int, int> accelerationValuesForDecrement_;
    
public:
    virtual ~AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator() {}
    AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) :  Midi_CSIMessageGenerator(csi, widget)
    {
        const char * const widgetClass = "RotaryWidgetClass";
        accelerationValuesForIncrement_ = widget->GetSurface()->GetAccelerationValuesForIncrement(widgetClass);
        accelerationValuesForDecrement_ = widget->GetSurface()->GetAccelerationValuesForDecrement(widgetClass);
        widget->SetStepSize(widget->GetSurface()->GetStepSize(widgetClass));
        widget->SetAccelerationValues(widget->GetSurface()->GetAccelerationValues(widgetClass));
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int val = midiMessage->midi_message[2];
        
        if (accelerationValuesForIncrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForIncrement_[val], 0.001);
        else if (accelerationValuesForDecrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForDecrement_[val], -0.001);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MFT_AcceleratedEncoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    map<int, int> accelerationValuesForIncrement_;
    map<int, int> accelerationValuesForDecrement_;

public:
    virtual ~MFT_AcceleratedEncoder_Midi_CSIMessageGenerator() {}
    MFT_AcceleratedEncoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, vector<string> &params) : Midi_CSIMessageGenerator(csi, widget)
    {
        accelerationValuesForIncrement_[0x3f] = 0;
        accelerationValuesForIncrement_[0x3e] = 1;
        accelerationValuesForIncrement_[0x3d] = 2;
        accelerationValuesForIncrement_[0x3c] = 3;
        accelerationValuesForIncrement_[0x3b] = 4;
        accelerationValuesForIncrement_[0x3a] = 5;
        accelerationValuesForIncrement_[0x39] = 6;
        accelerationValuesForIncrement_[0x38] = 7;
        accelerationValuesForIncrement_[0x36] = 8;
        accelerationValuesForIncrement_[0x33] = 9;
        accelerationValuesForIncrement_[0x2f] = 10;

        accelerationValuesForDecrement_[0x41] = 0;
        accelerationValuesForDecrement_[0x42] = 1;
        accelerationValuesForDecrement_[0x43] = 2;
        accelerationValuesForDecrement_[0x44] = 3;
        accelerationValuesForDecrement_[0x45] = 4;
        accelerationValuesForDecrement_[0x46] = 5;
        accelerationValuesForDecrement_[0x47] = 6;
        accelerationValuesForDecrement_[0x48] = 7;
        accelerationValuesForDecrement_[0x4a] = 8;
        accelerationValuesForDecrement_[0x4d] = 9;
        accelerationValuesForDecrement_[0x51] = 10;
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int val = midiMessage->midi_message[2];
        
        if (accelerationValuesForIncrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForIncrement_[val], 0.001);
        else if (accelerationValuesForDecrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForDecrement_[val], -0.001);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Encoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Encoder_Midi_CSIMessageGenerator() {}
    Encoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        double delta = (midiMessage->midi_message[2] & 0x3f) / 63.0;
        
        if (midiMessage->midi_message[2] & 0x40)
            delta = -delta;
        
        delta = delta / 2.0;

        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class EncoderPlain_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~EncoderPlain_Midi_CSIMessageGenerator() {}
    EncoderPlain_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        double delta = 1.0 / 64.0;
        
        if (midiMessage->midi_message[2] & 0x40)
            delta = -delta;
        
        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Encoder7Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int lastMessage;
    
public:
    virtual ~Encoder7Bit_Midi_CSIMessageGenerator() {}
    Encoder7Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget)
    {
        lastMessage = -1;
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int currentMessage = midiMessage->midi_message[2];
        double delta = 1.0 / 64.0;
        
        if (lastMessage > currentMessage || (lastMessage == 0 && currentMessage == 0))
            delta = -delta;
            
        lastMessage = currentMessage;
        
        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FeedbackProcessors
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TwoState_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~TwoState_Midi_FeedbackProcessor() {}
    TwoState_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2) : Midi_FeedbackProcessor(csi, surface, widget, feedback1, feedback2) { }
    
    virtual const char *GetName() override { return "TwoState_Midi_FeedbackProcessor"; }
    
    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE)
        {
            if (midiFeedbackMessage2_.midi_message[0] != 0)
                ForceMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], midiFeedbackMessage2_.midi_message[2]);
            else
                ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], 0x00);
        }
        else
            ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], midiFeedbackMessage1_.midi_message[2]);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FPTwoStateRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    bool active_;
    
public:
    virtual ~FPTwoStateRGB_Midi_FeedbackProcessor() {}
    FPTwoStateRGB_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        active_ = false;
    }
    
    virtual const char *GetName() override { return "FPTwoStateRGB_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        rgba_color color;
        
        ForceColorValue(color);
        active_ = false;
    }

    virtual void SetValue(const PropertyList &properties, double active) override
    {
        active_ = active != 0;
    }
    
    virtual void SetColorValue(const rgba_color &color) override
    {
        int RGBIndexDivider = 1  *2;
        
        if (active_ == false)
            RGBIndexDivider = 9  *2;
        
        rgba_color c;
        c.r = color.r / RGBIndexDivider;
        c.g = color.g / RGBIndexDivider;
        c.b = color.b / RGBIndexDivider;
        c.a = color.a;
        
        if (c != lastColor_)
            ForceColorValue(c);
    }

    virtual void ForceColorValue(const rgba_color &color) override
    {
        lastColor_ = color;
        
        SendMidiMessage(0x90, midiFeedbackMessage1_.midi_message[1], 0x7f);
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1],  color.r);  // only 127 bit allowed in Midi byte 3
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1],  color.g);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1],  color.b);

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) {
            LogToConsole(256, "[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SCE24TwoStateLED_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    double lastValue_;

public:
    virtual ~SCE24TwoStateLED_Midi_FeedbackProcessor() {}
    SCE24TwoStateLED_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        lastValue_ = 0.0;
    }
    
    virtual const char *GetName() override { return "SCE24TwoStateLED_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (lastValue_ != value)
            ForceValue(properties, value);
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastValue_ = value;
     
        rgba_color color;

        if (value == 0 && properties.get_prop(PropertyType_OffColor))
            GetColorValue(properties.get_prop(PropertyType_OffColor), color);
        else if (value == 1 && properties.get_prop(PropertyType_OnColor))
            GetColorValue(properties.get_prop(PropertyType_OnColor), color);

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.b / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
         
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SCE24OLED_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int topMargin_;
    int bottomMargin_;
    int font_;
    string lastStringSent_;
    rgba_color lastTextColorSent_;
    rgba_color lastBackgroundColorSent_;

public:
    virtual ~SCE24OLED_Midi_FeedbackProcessor() {}
    SCE24OLED_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1, int topMargin, int bottomMargin, int font) :
      Midi_FeedbackProcessor(csi, surface, widget, feedback1), topMargin_(topMargin), bottomMargin_(bottomMargin), font_(font)
    {
        lastStringSent_ = "";
    }

    virtual const char *GetName() override { return "SCE24OLED_Midi_FeedbackProcessor"; }
    
    virtual void ForceClear() override
    {
        char buf[SMLBUF];
    
        PropertyList properties;
        properties.set_prop(PropertyType_DisplayText, "");
        snprintf(buf, sizeof(buf), "%d", topMargin_);
        properties.set_prop(PropertyType_TopMargin, buf);
        snprintf(buf, sizeof(buf), "%d", bottomMargin_);
        properties.set_prop(PropertyType_BottomMargin, buf);
        snprintf(buf, sizeof(buf), "%d", font_);
        properties.set_prop(PropertyType_Font, buf);
        
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        ForceValue(properties, value);
    }
        
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        rgba_color backgroundColor;
        rgba_color textColor;
       
        const char *top = properties.get_prop(PropertyType_TopMargin);
        if (top)
            topMargin_ = atoi(top);

        const char *bottom = properties.get_prop(PropertyType_BottomMargin);
        if (bottom)
            bottomMargin_ = atoi(bottom);

        const char *font = properties.get_prop(PropertyType_Font);
        if (font)
            font_ = atoi(font);

        const char *col = properties.get_prop((value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? PropertyType_BackgroundColorOff : PropertyType_BackgroundColorOn);
        if (col)
            GetColorValue(col, backgroundColor);

        col = properties.get_prop((value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? PropertyType_TextColorOff : PropertyType_TextColorOn);
        if (col)
            GetColorValue(col, textColor);

        if (lastBackgroundColorSent_ == backgroundColor && lastTextColorSent_ == textColor)
            return;
        else
        {
            lastBackgroundColorSent_ = backgroundColor;
            lastTextColorSent_ = textColor;
        }
    
        
        const char *displayText = properties.get_prop(PropertyType_DisplayText);
        if (!displayText) displayText = "";
        
        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = topMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = bottomMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = font_;

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.b / 2;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.b / 2;
        
        for (int i = 0; displayText[i]; ++i)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayText[i];
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
         
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SCE24Text_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int topMargin_;
    int bottomMargin_;
    int font_;
    string lastStringSent_;

public:
    virtual ~SCE24Text_Midi_FeedbackProcessor() {}
    SCE24Text_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1, int topMargin, int bottomMargin, int font) :
      Midi_FeedbackProcessor(csi, surface, widget, feedback1),  topMargin_(topMargin), bottomMargin_(bottomMargin), font_(font)
    {
        lastStringSent_ = "";
    }
    virtual const char *GetName() override { return "SCE24Text_Midi_FeedbackProcessor"; }
    
    virtual void ForceClear() override
    {
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 63;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
         
        SendMidiSysExMessage(&midiSysExData.evt);
    }

    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str()))
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;

        char tmp[MEDBUF];
        const char *displayText = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));
        
        rgba_color backgroundColor;
        rgba_color textColor;

        const char *top = properties.get_prop(PropertyType_TopMargin);
        if (top)
            topMargin_ = atoi(top);

        const char *bottom = properties.get_prop(PropertyType_BottomMargin);
        if (bottom)
            bottomMargin_ = atoi(bottom);

        const char *font = properties.get_prop(PropertyType_Font);
        if (font)
            font_ = atoi(font);

        const char *col = properties.get_prop(PropertyType_BackgroundColor);
        if (col)
            GetColorValue(col, backgroundColor);
        
        col = properties.get_prop(PropertyType_TextColor);
        if (col)
            GetColorValue(col, textColor);

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = topMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = bottomMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = font_;

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.b / 2;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.b / 2;
        
        for (int i = 0; i < displayText[i]; ++i)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayText[i];
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
         
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

struct LEDRingRangeColor
{
    rgba_color ringColor;
    int ringRangeHigh;
    int ringRangeMedium;
    int ringRangeLow;
    
    LEDRingRangeColor()
    {
        ringRangeHigh = 0;
        ringRangeMedium = 0;
        ringRangeLow = 0;
    }
};

vector<LEDRingRangeColor> s_encoderRingColors;

static const vector<LEDRingRangeColor> &GetColorValues(const string &inputProperty)
{
    s_encoderRingColors.clear();
    
    string property = inputProperty;
    ReplaceAllWith(property, "\"", "");
    
    vector<string> colorDefs;
    GetTokens(colorDefs, property.c_str(), '+');

    for (int defi = 0; defi < (int)colorDefs.size(); ++defi)
    {
        vector<string> rangeDefs;
        
        GetTokens(rangeDefs, colorDefs[defi], '-');
        
        if (rangeDefs.size() > 2)
        {
            LEDRingRangeColor color;
            
            GetColorValue(rangeDefs[2].c_str(), color.ringColor);

            for (int i = atoi(rangeDefs[0].c_str()); i <= atoi(rangeDefs[1].c_str()); ++i)
            {
                if (i < 7)
                    color.ringRangeLow += 1 << i;
                else if (i > 6 && i < 14)
                    color.ringRangeMedium += 1 << (i - 7);
                else if (i > 13)
                    color.ringRangeHigh += 1 << (i - 14);
            }
  
            s_encoderRingColors.push_back(color);
        }
    }
    
    LEDRingRangeColor color;

    color.ringColor.r = 0;
    color.ringColor.g = 0;
    color.ringColor.b = 0;
    color.ringRangeLow = 7;
    color.ringRangeMedium = 0;
    color.ringRangeHigh = 0;
    
    s_encoderRingColors.push_back(color);
    
    return s_encoderRingColors;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SCE24Encoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~SCE24Encoder_Midi_FeedbackProcessor() {}
    SCE24Encoder_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "SCE24Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 120;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 127;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 5;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
        
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(properties, value));
    }
    
    int GetMidiValue(const PropertyList &properties, double value)
    {
        int valueInt = int(value  *127);
        
        int displayMode = 0;
        
        const char *ringstyle = properties.get_prop(PropertyType_RingStyle);
        if (ringstyle)
        {
            if (!strcmp(ringstyle, "Dot"))
                displayMode = 0;
            else if (!strcmp(ringstyle, "BoostCut"))
                displayMode = 1;
            else if (!strcmp(ringstyle, "Fill"))
                displayMode = 2;
            else if (!strcmp(ringstyle, "Spread"))
                displayMode = 3;
        }

        int val = 0;
        
        if (displayMode == 3)
            val = (1+((valueInt*15)>>8)) | (displayMode << 4);
        else
            val = (1+((valueInt*15)>>7)) | (displayMode << 4);

        //if (displayMode) // Should light up lower middle light
        //val |= 0x40;

        return val + 64;
    }
        
    virtual void Configure(const vector<unique_ptr<ActionContext>> &contexts) override
    {
        if (contexts.size() == 0)
            return;
        
        const PropertyList &properties = contexts[0]->GetWidgetProperties();
        
        vector<LEDRingRangeColor> colors;
        
        const char *ledringcolor = properties.get_prop(PropertyType_LEDRingColor);
        const char *ledringcolors = properties.get_prop(PropertyType_LEDRingColors);
        const char *pushcolor = properties.get_prop(PropertyType_PushColor);
        if (properties.get_prop(PropertyType_Push) && pushcolor)
        {
            LEDRingRangeColor color;
            
            GetColorValue(pushcolor, color.ringColor);
            color.ringRangeLow = 7;
            color.ringRangeMedium = 0;
            color.ringRangeHigh = 0;
            
            colors.push_back(color);
            
            color.ringColor.r = 0;
            color.ringColor.g = 0;
            color.ringColor.b = 0;
            color.ringRangeLow = 120;
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;
            
            colors.push_back(color);
        }
        else if (ledringcolor)
        {
            LEDRingRangeColor color;
            
            GetColorValue(ledringcolor, color.ringColor);

            color.ringRangeLow = 120;
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;

            colors.push_back(color);
            
            color.ringColor.r = 0;
            color.ringColor.g = 0;
            color.ringColor.b = 0;
            color.ringRangeLow = 7;
            color.ringRangeMedium = 0;
            color.ringRangeHigh = 0;

            colors.push_back(color);
        }
        else if (ledringcolors)
        {
            colors = GetColorValues(ledringcolors);
        }

        if (colors.size() == 0)
        {
            LEDRingRangeColor color;
            
            color.ringRangeLow = 127;
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;

            colors.push_back(color);
        }
        
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
         
        for (int i = 0; i < (int)colors.size(); ++i)
        {
            midiSysExData.evt.frame_offset=0;
            midiSysExData.evt.size=0;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeLow;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeMedium;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeHigh;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.r / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.g / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.b / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
            
            SendMidiSysExMessage(&midiSysExData.evt);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor() {}
    NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        rgba_color color;
        ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color &color) override
    {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color &color) override
    {
        lastColor_ = color;
        
        struct
        {
            MIDI_event_ex_t evt;
            char data[64];
        } midiSysExData;
        
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x20;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x29;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x0d;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x03;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x03;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1] ;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.r / 2; // only 127 bit max for this device
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.b / 2;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) {
            LogToConsole(256, "[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FaderportRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~FaderportRGB_Midi_FeedbackProcessor() {}
    FaderportRGB_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "FaderportRGB_Midi_FeedbackProcessor"; }
    
    virtual void ForceClear() override
    {
        rgba_color color;
        ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color &color) override
    {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color &color) override
    {
        lastColor_ = color;
        
        SendMidiMessage(0x90, midiFeedbackMessage1_.midi_message[1], 0x7f);
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1], color.r / 2);  // only 127 bit allowed in Midi byte 3
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1], color.g / 2);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1], color.b / 2);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) {
            LogToConsole(256, "[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AsparionRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int preventUpdateTrackColors_;

public:
    virtual ~AsparionRGB_Midi_FeedbackProcessor() {}
    AsparionRGB_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        preventUpdateTrackColors_ = false;
    }
    
    virtual const char *GetName() override { return "AsparionRGB_Midi_FeedbackProcessor"; }
    
    virtual void ForceClear() override
    {
        rgba_color color;
        ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color &color) override
    {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color &color) override
    {
        lastColor_ = color;
        
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1], color.r / 2);  // max 127 allowed in Midi byte 3
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1], color.g / 2);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1], color.b / 2);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) {
            LogToConsole(256, "[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
        }
    }
    
    virtual void ForceUpdateTrackColors() override
    {
        if (preventUpdateTrackColors_)
            return;
        
        ForceColorValue(surface_->GetTrackColorForChannel(widget_->GetChannelNumber() - 1));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader14Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    bool shouldSetToZero_;
    DWORD timeZeroValueReceived_;
    
public:
    virtual ~Fader14Bit_Midi_FeedbackProcessor() {}
    Fader14Bit_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        shouldSetToZero_ = false;
        timeZeroValueReceived_ = 0;
    }
    
    virtual const char *GetName() override { return "Fader14Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void RunDeferredActions() override
    {
        if (shouldSetToZero_ && (GetTickCount() - timeZeroValueReceived_) > 250)
        {
            ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], 0x00, 0x00);
            shouldSetToZero_ = false;
        }
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (value == 0.0)
        {
            shouldSetToZero_ = true;
            timeZeroValueReceived_ = GetTickCount();
            return;
        }
        else
            shouldSetToZero_ = false;
    
        int volInt = int(value  *16383.0);
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], volInt&0x7f, (volInt>>7)&0x7f);
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        int volInt = int(value  *16383.0);
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], volInt&0x7f, (volInt>>7)&0x7f);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FaderportClassicFader14Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    bool shouldSetToZero_;
    DWORD timeZeroValueReceived_;
    
public:
    virtual ~FaderportClassicFader14Bit_Midi_FeedbackProcessor() {}
    FaderportClassicFader14Bit_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2) : Midi_FeedbackProcessor(csi, surface, widget, feedback1, feedback2)
    {
        shouldSetToZero_ = false;
        timeZeroValueReceived_ = 0;
    }
    
    virtual const char *GetName() override { return "FaderportClassicFader14Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void RunDeferredActions() override
    {
        if (shouldSetToZero_ && (GetTickCount() - timeZeroValueReceived_) > 250)
        {
            ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], 0x00);
            ForceMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], 0x00);

            shouldSetToZero_ = false;
        }
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (value == 0.0)
        {
            shouldSetToZero_ = true;
            timeZeroValueReceived_ = GetTickCount();
            return;
        }
        else
            shouldSetToZero_ = false;
    
        int volInt = int(value  *1024.0);
        
        if (midiFeedbackMessage1_.midi_message[2] != ((volInt>>7)&0x7f) || midiFeedbackMessage2_.midi_message[2] != (volInt&0x7f))
        {
            midiFeedbackMessage1_.midi_message[2] = (volInt>>7)&0x7f;
            midiFeedbackMessage2_.midi_message[2] = volInt&0x7f;
         
            SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], midiFeedbackMessage1_.midi_message[2]);
            SendMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], midiFeedbackMessage2_.midi_message[2]);
        }
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        int volInt = int(value  *16383.0);
        
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], (volInt>>7)&0x7f);
        ForceMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], volInt&0x7f);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader7Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Fader7Bit_Midi_FeedbackProcessor() {}
    Fader7Bit_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "Fader7Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(value  *127.0));
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(value  *127.0));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Encoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Encoder_Midi_FeedbackProcessor() {}
    Encoder_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }
    
    int GetMidiValue(const PropertyList &properties, double value)
    {
        int valueInt = int(value  *127);
        
        int displayMode = 0;
        
        const char *ringstyle = properties.get_prop(PropertyType_RingStyle);
        if (ringstyle)
        {
            if (!strcmp(ringstyle, "Dot"))
                displayMode = 0;
            else if (!strcmp(ringstyle, "BoostCut"))
                displayMode = 1;
            else if (!strcmp(ringstyle, "Fill"))
                displayMode = 2;
            else if (!strcmp(ringstyle, "Spread"))
                displayMode = 3;
        }

        int val = 0;
        
        if (displayMode == 3)
            val = (1+((valueInt*11)>>8)) | (displayMode << 4);
        else
            val = (1+((valueInt*11)>>7)) | (displayMode << 4);

        //if (displayMode) // Should light up lower middle light
        //val |= 0x40;

        return val;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AsparionEncoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int displayMode_;
    
public:
    virtual ~AsparionEncoder_Midi_FeedbackProcessor() {}
    AsparionEncoder_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        displayMode_ = 0;
    }
    
    virtual const char *GetName() override { return "Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0] + displayMode_, midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0] + displayMode_, midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }
    
    int GetMidiValue(const PropertyList &properties, double value)
    {
        displayMode_ = 2;
        
        const char *ringstyle = properties.get_prop(PropertyType_RingStyle);
        if (ringstyle)
        {
            if (!strcmp(ringstyle, "Fill"))
                displayMode_ = 1;
            else if (!strcmp(ringstyle, "Dot"))
                displayMode_ = 2;
        }

        return int(value  *127);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ConsoleOneVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~ConsoleOneVUMeter_Midi_FeedbackProcessor() {}
    ConsoleOneVUMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }
    
    virtual const char *GetName() override { return "ConsoleOneVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(value));
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(value));
    }
    
    int GetMidiValue(double value)
    {
        double dB = VAL2DB(normalizedToVol(value)) + 2.5;
        
        double midiVal = 0;
        
        if (dB < 0)
            midiVal = pow(10.0, dB / 48)  *96;
        else
            midiVal = pow(10.0, dB / 60)  *96;

        return (int)midiVal;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ConsoleOneGainReductionMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    double minDB_;
    double maxDB_;
    
public:
    virtual ~ConsoleOneGainReductionMeter_Midi_FeedbackProcessor() {}
    ConsoleOneGainReductionMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, MIDI_event_ex_t feedback1) : Midi_FeedbackProcessor(csi, surface, widget, feedback1)
    {
        minDB_ = 0.0;
        maxDB_ = 24.0;
    }
    
    virtual const char *GetName() override { return "ConsoleOneGainReductionMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(fabs(1.0 - value)  *127.0));
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(fabs(1.0 - value)  *127.0));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class QConProXMasterVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    double minDB_;
    double maxDB_;
    int param_;
    
public:
    virtual ~QConProXMasterVUMeter_Midi_FeedbackProcessor() {}
    QConProXMasterVUMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int param) : Midi_FeedbackProcessor(csi, surface, widget), param_(param)
    {
        minDB_ = 0.0;
        maxDB_ = 24.0;
    }
    
    virtual const char *GetName() override { return "QConProXMasterVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        //Master Channel:
        //Master Level 1 : 0xd1, 0x0L
        //L = 0x0 – 0xD = Meter level 0% thru 100% (does not affect peak indicator)
        
        //Master Level 2 : 0xd1, 0x1L
        //L = 0x0 – 0xD = Meter level 0% thru 100% (does not affect peak indicator)
        
        int midiValue = int(value  *0x0f);
        
        if (midiValue > 0x0d)
            midiValue = 0x0d;
        
        SendMidiMessage(0xd1, (param_ << 4) | midiValue, 0);
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        //Master Channel:
        //Master Level 1 : 0xd1, 0x0L
        //L = 0x0 – 0xD = Meter level 0% thru 100% (does not affect peak indicator)
        
        //Master Level 2 : 0xd1, 0x1L
        //L = 0x0 – 0xD = Meter level 0% thru 100% (does not affect peak indicator)
        
        int midiValue = int(value  *0x0f);
        
        if (midiValue > 0x0d)
            midiValue = 0x0d;
        
        ForceMidiMessage(0xd1, (param_ << 4) | midiValue, 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MCUVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    int displayType_;
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;

public:
    virtual ~MCUVUMeter_Midi_FeedbackProcessor() {}
    MCUVUMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayType, int channelNumber) : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channelNumber_(channelNumber)
    {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }
    
    virtual const char *GetName() override { return "MCUVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
// THE FOLLOWING COMMENTED OUT CODE WILL BE PUSHED AT A LATER DATE.
//       int midiValue = GetMidiValue(properties, value);
//       if (midiValue > 0)
//            SendMidiMessage(0xd0, (channelNumber_ << 4) | midiValue, 0);
        SendMidiMessage(0xd0, (channelNumber_ << 4) | GetMidiValue(properties, value), 0);
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(0xd0, (channelNumber_ << 4) | GetMidiValue(properties, value), 0);
    }
    
    int GetMidiValue(const PropertyList& properties, double value)
    {
        int    midiValue = 0;
        double   dbValue = VAL2DB(normalizedToVol(value));

        //---------------------------------------------------
        // HANDLE METER SPECIFIC SCALING, DEFAULT IS "XTOUCH"
        //---------------------------------------------------
        const char* meterMode = nullptr;

        PropertyType propertyType = properties.prop_from_string("MeterMode");
        if (propertyType)
            meterMode = (char*)properties.get_prop(propertyType);
        if (!meterMode)
            meterMode = "XTOUCH";

        //---------------------------------------------------//
        // XTOUCH                                            //
        //---------------------------------------------------//
        if (STRICASECMP(meterMode, "XTouch") == 0)
        {
            if      (dbValue >= -60.3 && dbValue < -54.1)  midiValue = 0x01;   // 1 Green1 (breakpoints offset slightly in the code to map better to surface)
            else if (dbValue >= -54.1 && dbValue < -48.2)  midiValue = 0x02;
            else if (dbValue >= -48.2 && dbValue < -42.1)  midiValue = 0x03;   // 3 Green2
            else if (dbValue >= -42.1 && dbValue < -36.2)  midiValue = 0x04;

            else if (dbValue >= -36.2 && dbValue < -30.1)  midiValue = 0x05;   // 5 Green3
            else if (dbValue >= -30.1 && dbValue < -18.1)  midiValue = 0x06;
            else if (dbValue >= -18.1 && dbValue < -15.1)  midiValue = 0x07;   // 7 Green4
            else if (dbValue >= -15.1 && dbValue < -12.1)  midiValue = 0x08;

            else if (dbValue >= -12.1 && dbValue < -9.1)   midiValue = 0x09;   // 9 Orange1
            else if (dbValue >= -9.1  && dbValue < -6.1)   midiValue = 0x0a;
            else if (dbValue >= -6.1  && dbValue < -4.6)   midiValue = 0x0b;   // 11 Orange2
            else if (dbValue >= -4.6  && dbValue < -3.1)   midiValue = 0x0c;
            else if (dbValue >= -3.1  && dbValue <= 0.1)   midiValue = 0x0d;   // 13 Orange3
            else if (dbValue >   0.1)                      midiValue = 0x0e;   // Red (clip)
        }

        //---------------------------------------------------//
        // MCU  "GAW-ORIGINAL CODE"                          //
        //---------------------------------------------------//
        else if (STRICASECMP(meterMode, "MCU") == 0)
        {
            midiValue = int(value * 0x0f);
            if (midiValue > 0x0e)
                midiValue = 0x0e;
        }

        //---------------------------------------------------//
        // SSLNucleus2                                       //
        //---------------------------------------------------//
        else if (STRICASECMP(meterMode, "SSLNucleus2") == 0)
        {
            if      (dbValue >= -40 && dbValue < -30) midiValue = 0x01; // -40 DB LED
            else if (dbValue >= -30 && dbValue < -20) midiValue = 0x03; // -30 DB LED
            else if (dbValue >= -20 && dbValue < -14) midiValue = 0x05; // -20 DB LED
            else if (dbValue >= -14 && dbValue < -10) midiValue = 0x07; // -14 DB LED
            else if (dbValue >= -10 && dbValue < -8 ) midiValue = 0x09; // -10 DB LED
            else if (dbValue >= -8  && dbValue < -6 ) midiValue = 0x0a; // -8  DB LED
            else if (dbValue >= -6  && dbValue < -4 ) midiValue = 0x0b; // -6  DB LED
            else if (dbValue >= -4  && dbValue <  2 ) midiValue = 0x0c; // -4  DB LED
            else if (dbValue >= -2  && dbValue <  0 ) midiValue = 0x0d; // -2  DB LED
            else if (dbValue >=  0                  ) midiValue = 0x0e; //  0  DB LED
        }

        //---------------------------------------------------//
        // SCALING COMPLETED - RETURN VALUE                  //
        //---------------------------------------------------//

       if (midiValue > 0)
           WindowsOutputDebugString("MeterMode=%s   track=%d   midvalue=0x%02x\n", meterMode, channelNumber_, midiValue);

        return  midiValue;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AsparionVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    int displayType_;
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;
    bool isRight_;

public:
    virtual ~AsparionVUMeter_Midi_FeedbackProcessor() {}
    AsparionVUMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayType, int channelNumber, bool isRight) : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channelNumber_(channelNumber),  isRight_(isRight)
    {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }
    
    virtual const char *GetName() override { return "AsparionVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
        SendMidiMessage(isRight_ ? 0xd1 : 0xd0, (channelNumber_ << 4) | GetMidiValue(value), 0);
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        ForceMidiMessage(isRight_ ? 0xd1 : 0xd0, (channelNumber_ << 4) | GetMidiValue(value), 0);
    }
    
    int GetMidiValue(double value)
    {
        //D0 yx    : update VU meter, y=channel, x=0..d=volume, e=clip on, f=clip off
        int midiValue = int(value  *0x0f);
        if (midiValue > 0x0d)
            midiValue = 0x0d;

        return midiValue;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FPVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;

public:
    virtual ~FPVUMeter_Midi_FeedbackProcessor() {}
    FPVUMeter_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int channelNumber) : Midi_FeedbackProcessor(csi, surface, widget), channelNumber_(channelNumber)
    {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }
    
    virtual const char *GetName() override { return "FPVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (lastMidiValue_ == value || GetMidiValue(value) < 7)
        {
            return;
        }
        
        if (channelNumber_ < 8)
        {
            SendMidiMessage(0xd0 + channelNumber_, GetMidiValue(value), 0);
        } else {
            SendMidiMessage(0xc0 + channelNumber_ - 8, GetMidiValue(value), 0);
        }
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastMidiValue_ = (int)value;
        if (channelNumber_ < 8)
        {
            ForceMidiMessage(0xd0 + channelNumber_, GetMidiValue(value), 0);
        } else {
            ForceMidiMessage(0xc0 + channelNumber_ - 8, GetMidiValue(value), 0);
        }
    }
    
    int GetMidiValue(double value)
    {
        //Dn, vv   : n meter address, vv meter value (0...7F)
        int midiValue = int(value  *0xa0);

        return midiValue;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FPValueBar_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    double lastValue_;
    int channel_;
    
    int GetValueBarType(const PropertyList &properties)
    {
        // 0: Normal, 1: Bipolar, 2: Fill, 3: Spread, 4: Off

        const char *barstyle = properties.get_prop(PropertyType_BarStyle);
        if (barstyle)
        {
            if (!strcmp(barstyle, "Normal"))
                return 0;
            else if (!strcmp(barstyle, "BiPolar"))
                return 1;
            else if (!strcmp(barstyle, "Fill"))
                return 2;
            else if (!strcmp(barstyle, "Spread"))
                return 3;
        }

        return 4;
    }
    
public:
    virtual ~FPValueBar_Midi_FeedbackProcessor() {}
    FPValueBar_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int channel) : Midi_FeedbackProcessor(csi, surface, widget), channel_(channel)
    {
        lastValue_ = 0;
    }

    virtual const char *GetName() override { return "FPValueBar_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (value == lastValue_)
            return;
        
        ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastValue_ = value;
        
        if (channel_ < 8)
        {
            SendMidiMessage(0xb0, channel_ + 0x30, int(lastValue_  *127.0));
            SendMidiMessage(0xb0, channel_ + 0x38, GetValueBarType(properties));
        }
        else
        {
            SendMidiMessage(0xb0, channel_ - 8 + 0x40, int(lastValue_  *127.0));
            SendMidiMessage(0xb0, channel_ - 8 + 0x48, GetValueBarType(properties));
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MCUDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~MCUDisplay_Midi_FeedbackProcessor() {}
    MCUDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel) : Midi_FeedbackProcessor(csi,surface, widget), offset_(displayUpperLower  *56), displayType_(displayType), displayRow_(displayRow), channel_(channel)
    {
    }
    
    virtual const char *GetName() override { return "MCUDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (!strcmp(text,"-150.00")) text="";

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IconDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int sysExByte1_;
    int sysExByte2_;
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~IconDisplay_Midi_FeedbackProcessor() {}
    IconDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel, int sysExByte1, int sysExByte2) : Midi_FeedbackProcessor(csi,surface, widget), offset_(displayUpperLower  *56), displayType_(displayType), displayRow_(displayRow), channel_(channel), sysExByte1_(sysExByte1), sysExByte2_(sysExByte2)
    {
    }
    
    virtual const char *GetName() override { return "IconDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (!strcmp(text,"-150.00")) text="";

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExByte1_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExByte2_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AsparionDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int displayRow_;
    int displayType_;
    int displayTextType_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~AsparionDisplay_Midi_FeedbackProcessor() {}
    AsparionDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayRow, int displayType, int displayTextType, int channel) : Midi_FeedbackProcessor(csi, surface, widget), displayRow_(displayRow), displayType_(displayType), displayTextType_(displayTextType), channel_(channel)
    {
    }
    
    virtual const char *GetName() override { return "AsparionDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const  &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (!strcmp(text,"-150.00")) text = "";

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayTextType_;
        
        if (displayRow_ != 3)
        {
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *12;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        }
        else
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *8;

        const int linelen = displayRow_ == 3 ? 8 : 12;
        int cnt = 0;
        while (cnt++ < linelen)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class XTouchDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    int preventUpdateTrackColors_;
    string lastStringSent_;
    vector<rgba_color> currentTrackColors_;

    enum XTouchColor {
        COLOR_INVALID = -1,
        COLOR_OFF = 0,
        COLOR_RED,
        COLOR_GREEN,
        COLOR_YELLOW,
        COLOR_BLUE,
        COLOR_MAGENTA,
        COLOR_CYAN,
        COLOR_WHITE
    };

    static XTouchColor colorFromString(const char *str)
    {
        if (!strcmp(str, "Black"))   return COLOR_OFF;
        if (!strcmp(str, "Red"))     return COLOR_RED;
        if (!strcmp(str, "Green"))   return COLOR_GREEN;
        if (!strcmp(str, "Yellow"))  return COLOR_YELLOW;
        if (!strcmp(str, "Blue"))    return COLOR_BLUE;
        if (!strcmp(str, "Magenta")) return COLOR_MAGENTA;
        if (!strcmp(str, "Cyan"))    return COLOR_CYAN;
        if (!strcmp(str, "White"))   return COLOR_WHITE;
        return COLOR_INVALID;
    }

    /*
     * NOTE: Any changes made to rgbToColor here must also be mirrored in class
     * OSC_X32FeedbackProcessor in control_surface_OSC_widgets.h
     */
    static int rgbToColor(int r, int g, int b)
    {
        // Doing a RGB to HSV conversion since HSV is better for light
        // Converting RGB to floats between 0 and 1.0 (percentage)
        float rf = r / 255.0f;
        float gf = g / 255.0f;
        float bf = b / 255.0f;

        // Hue will be between 0 and 360 to represent the color wheel.
        // Saturation and Value are a percentage (between 0 and 1.0)
        float h, s, v, colorMin, delta;
        v = max(max(rf, gf), bf);

        // If value is less than this percentage, LCD should be off.
        if (v <= 0.10)
            return COLOR_WHITE; // This could be OFF, but that would show nothing.

        colorMin = min(min(rf, gf), bf);
        delta = v - colorMin;
        // Don't need divide by zero check since if value is 0 it will return COLOR_OFF above.
        s = delta / v;

        // If saturation is less than this percentage, LCD should be white.
        if (s <= 0.10)
            return COLOR_WHITE;

        // Now we have a valid color. Figure out the hue and return the closest X-Touch value.
        if (rf >= v)
            h = (gf - bf) / delta;
        else if (gf >= v)
            h = ((bf - rf) / delta) + 2.0f;
        else
            h = ((rf - gf) / delta) + 4.0f;

        h *= 60.0;
        if (h < 0)
            h += 360.0;

        // The numbers represent the hue from 0-360.
        if (h >= 330 || h < 20)
            return COLOR_RED;
        if (h >= 250)
            return COLOR_MAGENTA;
        if (h >= 210)
            return COLOR_BLUE;
        if (h >= 160)
            return COLOR_CYAN;
        if (h >= 80)
            return COLOR_GREEN;
        if (h >= 20)
            return COLOR_YELLOW;

        return COLOR_WHITE; // failsafe
    }
        
public:
    virtual ~XTouchDisplay_Midi_FeedbackProcessor() {}
    XTouchDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel) : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower  *56), displayType_(displayType), displayRow_(displayRow), channel_(channel)
    {
        preventUpdateTrackColors_ = false;
        
        rgba_color color;
        
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
            currentTrackColors_.push_back(color);
    }
        
    virtual const char *GetName() override { return "XTouchDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }
    
    virtual void SetXTouchDisplayColors(const char *colors) override
    {
        preventUpdateTrackColors_ = true;
        
        vector<string> currentColors;
        GetTokens(currentColors, colors);
        
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x72;
        
        
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
        {
            // white by default
            XTouchColor msgColor = COLOR_WHITE;
            // either all 8 are defined, or there's just one
            const char *curColorStr = currentColors.size() == 1 ?
                currentColors[0].c_str() : currentColors[i].c_str();
            XTouchColor curColor = colorFromString(curColorStr);

            // if we can parse the color, override it
            if (curColor != COLOR_INVALID)
                msgColor = curColor;

            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = (int)msgColor;
        }
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
    
    virtual void RestoreXTouchDisplayColors() override
    {
        preventUpdateTrackColors_ = false;
    }
    
    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (!strcmp(text, "-150.00")) text = "";

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  * 7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
        
        ForceUpdateTrackColors();
    }
    
    virtual void ForceUpdateTrackColors() override
    {
        if (preventUpdateTrackColors_)
            return;
        
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x72;

        vector<rgba_color> trackColors;
        
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
            trackColors.push_back(surface_->GetTrackColorForChannel(i));

        for (int i = 0; i < trackColors.size(); ++i)
        {
            if (lastStringSent_ == "")
            {
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x07; // White
            }
            else
            {
                rgba_color color = trackColors[i];
                
                currentTrackColors_[i] = color;
                
                int r = color.r;
                int g = color.g;
                int b = color.b;

                int surfaceColor = (int)rgbToColor(r, g, b);
                
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = surfaceColor;
            }
        }

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FPDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;
    
    int GetTextAlign(const PropertyList &properties)
    {
        // Center: 0, Left: 1, Right: 2
        const char *textalign = properties.get_prop(PropertyType_TextAlign);
        if (textalign)
        {
            if (!strcmp(textalign, "Left"))
                return 1;
            else if (!strcmp(textalign, "Right"))
                return 2;
        }

        return 0;
    }
    
    int GetTextInvert(const PropertyList &properties)
    {
        const char *textinvert = properties.get_prop(PropertyType_TextInvert);
        if (textinvert && !strcmp(textinvert, "Yes"))
            return 4;

        return 0;
    }
    
public:
    virtual ~FPDisplay_Midi_FeedbackProcessor() {}
    FPDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayType, int channel, int displayRow) : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channel_(channel), displayRow_(displayRow)
    {
        lastStringSent_ = " ";
    }
    
    virtual const char *GetName() override { return "FPDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }
    
    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (text[0] == 0)
            text = "                            ";
        
        int invert = lastStringSent_ == "" ? 0 : GetTextInvert(properties); // prevent empty inverted lines
        int align = 0x0000000 + invert + GetTextAlign(properties);

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        }
        
        midiSysExData;
        
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x06;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_; // Faderport8=0x02, Faderport16=0x16
        
        // <SysExHdr> 12, xx, yy, zz, tx,tx,tx,... F7
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x12;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_;      // xx channel_ id
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;   // yy line number 0-3
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = align;         // zz alignment flag 0000000=centre, see manual for other setups.
        
        int length = (int)strlen(text);
        
        if (length > 30)
            length = 30;
        
        int count = 0;
        
        while (count < length)
        {
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text++;                // tx text in ASCII format
            count++;
        }
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FPScribbleStripMode_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int displayType_;
    int channel_;
    int lastMode_;

    int GetMode(const PropertyList &properties)
    {
        int param = 2;

        const char *mode = properties.get_prop(PropertyType_Mode);
        if (mode)
            param = atoi(mode);

        if (param >= 0 && param < 9)
            return param;
        
        return 2;
    }
    
public:
    virtual ~FPScribbleStripMode_Midi_FeedbackProcessor() {}
    FPScribbleStripMode_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayType, int channel) : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channel_(channel)
    {
        lastMode_ = 0;
    }
    
    virtual const char *GetName() override { return "FPScribbleStripMode_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (lastMode_ == GetMode(properties))
            return;
            
        ForceValue(properties, value);
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastMode_ = GetMode(properties);
        
        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        }
        midiSysExData;
        
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x06;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_; // Faderport8=0x02, Faderport16=0x16
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x13;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_;     // xx channel_ id
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00 + lastMode_; //    0x00 + value; // type of display layout

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class QConLiteDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;
    
public:
    virtual ~QConLiteDisplay_Midi_FeedbackProcessor() {}
    QConLiteDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel) : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower  *28), displayType_(displayType), displayRow_(displayRow), channel_(channel)
    {
    }
    
    virtual const char *GetName() override { return "QConLiteDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }
    
    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (strcmp(inputText, lastStringSent_.c_str())) // changes since last send
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int lastFirstLetter_;

public:
    FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget) : Midi_FeedbackProcessor(csi, surface, widget)
    {
        lastFirstLetter_ = 0x00;
    }
    
    virtual const char *GetName() override { return "FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);
        SendMidiMessage(0xB0, 0x4B, 0x20);
        SendMidiMessage(0xB0, 0x4A, 0x20);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (value == 0.0) // Selected Track
        {
            if (lastFirstLetter_ != 0x13)
            {
                lastFirstLetter_ = 0x13;
                SendMidiMessage(0xB0, 0x4B, 0x13); // S
                SendMidiMessage(0xB0, 0x4A, 0x05); // E
            }
        }
        else if (value == 1.0) // Track
        {
            if (lastFirstLetter_ != 0x07)
            {
                lastFirstLetter_ = 0x07;
                SendMidiMessage(0xB0, 0x4B, 0x07); // G
                SendMidiMessage(0xB0, 0x4A, 0x0C); // L
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MCU_TimeDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    char m_mackie_lasttime[10];
    int m_mackie_lasttime_mode;
    DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;
    
public:
    MCU_TimeDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget) : Midi_FeedbackProcessor(csi, surface, widget) {}
    
    virtual const char *GetName() override { return "MCU_TimeDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, 0.0);

        for (int i = 0; i < 10; ++i)
            SendMidiMessage(0xB0, 0x40 + i, 0x20);
    }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {        
//#ifndef timeGetTime
        DWORD now = GetTickCount();
//#else
        //DWORD now = timeGetTime();
//#endif
        double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();
        unsigned char bla[10];
        
        memset(bla,0,sizeof(bla));
        
        int *tmodeptr = csi_->GetTimeMode2Ptr();
        
        int tmode=0;
        
        if (tmodeptr && (*tmodeptr)>=0) tmode = *tmodeptr;
        else
        {
            tmodeptr = csi_->GetTimeModePtr();
            if (tmodeptr)
                tmode=*tmodeptr;
        }
        
        if (tmode==3) // seconds
        {
            double *toptr = csi_->GetTimeOffsPtr();
            
            if (toptr) pp+=*toptr;
            char buf[64];
            snprintf(buf, sizeof(buf),"%d %02d",(int)pp, ((int)(pp*100.0))%100);
            if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
            else
                memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
            
        }
        else if (tmode==4) // samples
        {
            char buf[128];
            format_timestr_pos(pp,buf,sizeof(buf),4);
            if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
            else
                memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
        }
        else if (tmode==5) // frames
        {
            char buf[128];
            format_timestr_pos(pp,buf,sizeof(buf),5);
            char *p=buf;
            char *op=buf;
            int ccnt=0;
            while (*p)
            {
                if (*p == ':')
                {
                    ccnt++;
                    if (ccnt!=3)
                    {
                        p++;
                        continue;
                    }
                    *p=' ';
                }
                
                *op++=*p++;
            }
            *op=0;
            if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
            else
                memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
        }
        else if (tmode>0)
        {
            int num_measures=0;
            int currentTimeSignatureNumerator=0;
            double beats=TimeMap2_timeToBeats(NULL,pp,&num_measures,&currentTimeSignatureNumerator,NULL,NULL)+ 0.000000000001;
            double nbeats = floor(beats);
            
            beats -= nbeats;
            
            if (num_measures <= 0 && pp < 0.0)
                --num_measures;
            
            int *measptr = csi_->GetMeasOffsPtr();
            int nm=num_measures+1+(measptr ? *measptr : 0);
            
            // Here we display a '-' minus sign so we make it clearer that we are on "count down".
            // Corner case: if the measure is less than -99 this won't work...
            if (nm < 0)
                bla[0] = '-';
            
            nm=std::abs(nm);
            
            if (nm >= 100) bla[0]='0'+(nm/100)%10;  //bars hundreds
            if (nm >= 10) bla[1]='0'+(nm/10)%10;    //bars tens
            bla[2]='0'+(nm)%10;//bars
            
            int nb=(pp < 0.0 ? currentTimeSignatureNumerator : 0) + (int)nbeats +1;
            if (nb >= 10) bla[3]='0'+(nb/10)%10;    //beats tens
            bla[4]='0'+(nb)%10;                     //beats
            
            const int fracbeats = (int) (1000.0  *beats);
            bla[7]='0' + (fracbeats/100)%10;
            bla[8]='0' + (fracbeats/10)%10;
            bla[9]='0' + (fracbeats%10);            // frames
        }
        else
        {
            double *toptr = csi_->GetTimeOffsPtr();
            if (toptr) pp+=(*toptr);
            
            int ipp=(int)pp;
            int fr=(int)((pp-ipp)*1000.0);
            
            if (ipp >= 360000) bla[0]='0'+(ipp/360000)%10;//hours hundreds
            if (ipp >= 36000) bla[1]='0'+(ipp/36000)%10;//hours tens
            if (ipp >= 3600) bla[2]='0'+(ipp/3600)%10;//hours
            
            bla[3]='0'+(ipp/600)%6;//min tens
            bla[4]='0'+(ipp/60)%10;//min
            bla[5]='0'+(ipp/10)%6;//sec tens
            bla[6]='0'+(ipp%10);//sec
            bla[7]='0' + (fr/100)%10;
            bla[8]='0' + (fr/10)%10;
            bla[9]='0' + (fr%10); // frames
        }
        
        if (m_mackie_lasttime_mode != tmode)
        {
            m_mackie_lasttime_mode=tmode;
            SendMidiMessage(0x90, 0x71, tmode==5?0x7F:0); // set smpte light
            SendMidiMessage(0x90, 0x72, m_mackie_lasttime_mode>0 && tmode<3?0x7F:0); // set beats light
            
            // Blank display on mode change
            for (int x = 0; x < sizeof(bla); ++x)
                SendMidiMessage(0xB0,0x40+x,0x20);
            
        }
        
        if (memcmp(m_mackie_lasttime,bla,sizeof(bla)))
        {
            bool force=false;
            if (now > m_mcu_timedisp_lastforce)
            {
                m_mcu_timedisp_lastforce=now+2000;
                force=true;
            }
            
            for (int x = 0; x < sizeof(bla); ++x)
            {
                int idx=sizeof(bla)-x-1;
                if (bla[idx]!=m_mackie_lasttime[idx]||force)
                {
                    SendMidiMessage(0xB0,0x40+x,bla[idx]);
                    m_mackie_lasttime[idx]=bla[idx];
                }
            }
        }
    }
};



// Color maps are stored in Blue Green Red format
static unsigned char s_colorMap7[128][3] = { {0, 0, 0},    // 0
    {255, 0, 0},    // 1 - Blue
    {255, 21, 0},    // 2 - Blue (Green Rising)
    {255, 34, 0},
    {255, 46, 0},
    {255, 59, 0},
    {255, 68, 0},
    {255, 80, 0},
    {255, 93, 0},
    {255, 106, 0},
    {255, 119, 0},
    {255, 127, 0},
    {255, 140, 0},
    {255, 153, 0},
    {255, 165, 0},
    {255, 178, 0},
    {255, 191, 0},
    {255, 199, 0},
    {255, 212, 0},
    {255, 225, 0},
    {255, 238, 0},
    {255, 250, 0},    // 21 - End of Blue's Reign
    
    {250, 255, 0}, // 22 - Green (Blue Fading)
    {237, 255, 0},
    {225, 255, 0},
    {212, 255, 0},
    {199, 255, 0},
    {191, 255, 0},
    {178, 255, 0},
    {165, 255, 0},
    {153, 255, 0},
    {140, 255, 0},
    {127, 255, 0},
    {119, 255, 0},
    {106, 255, 0},
    {93, 255, 0},
    {80, 255, 0},
    {67, 255, 0},
    {59, 255, 0},
    {46, 255, 0},
    {33, 255, 0},
    {21, 255, 0},
    {8, 255, 0},
    {0, 255, 0},    // 43 - Green
    
    {0, 255, 12},    // 44 - Green/ Red Rising
    {0, 255, 25},
    {0, 255, 38},
    {0, 255, 51},
    {0, 255, 63},
    {0, 255, 72},
    {0, 255, 84},
    {0, 255, 97},
    {0, 255, 110},
    {0, 255, 123},
    {0, 255, 131},
    {0, 255, 144},
    {0, 255, 157},
    {0, 255, 170},
    {0, 255, 182},
    {0, 255, 191},
    {0, 255, 203},
    {0, 255, 216},
    {0, 255, 229},
    {0, 255, 242},
    {0, 255, 255},    // 64 - Green + Red (Yellow)
    
    {0, 246, 255},    // 65 - Red, Green Fading
    {0, 233, 255},
    {0, 220, 255},
    {0, 208, 255},
    {0, 195, 255},
    {0, 187, 255},
    {0, 174, 255},
    {0, 161, 255},
    {0, 148, 255},
    {0, 135, 255},
    {0, 127, 255},
    {0, 114, 255},
    {0, 102, 255},
    {0, 89, 255},
    {0, 76, 255},
    {0, 63, 255},
    {0, 55, 255},
    {0, 42, 255},
    {0, 29, 255},
    {0, 16, 255},
    {0, 4, 255},    // 85 - End Red/Green Fading
    
    {4, 0, 255},    // 86 - Red/ Blue Rising
    {16, 0, 255},
    {29, 0, 255},
    {42, 0, 255},
    {55, 0, 255},
    {63, 0, 255},
    {76, 0, 255},
    {89, 0, 255},
    {102, 0, 255},
    {114, 0, 255},
    {127, 0, 255},
    {135, 0, 255},
    {148, 0, 255},
    {161, 0, 255},
    {174, 0, 255},
    {186, 0, 255},
    {195, 0, 255},
    {208, 0, 255},
    {221, 0, 255},
    {233, 0, 255},
    {246, 0, 255},
    {255, 0, 255},    // 107 - Blue + Red
    
    {255, 0, 242},    // 108 - Blue/ Red Fading
    {255, 0, 229},
    {255, 0, 216},
    {255, 0, 204},
    {255, 0, 191},
    {255, 0, 182},
    {255, 0, 169},
    {255, 0, 157},
    {255, 0, 144},
    {255, 0, 131},
    {255, 0, 123},
    {255, 0, 110},
    {255, 0, 97},
    {255, 0, 85},
    {255, 0, 72},
    {255, 0, 63},
    {255, 0, 50},
    {255, 0, 38},
    {255, 0, 25},    // 126 - Blue-ish
    {225, 240, 240}    // 127 - White ?
};

int GetColorIntFromRGB(int r, int g, int b)
{
    if (b == 0 && g == 0 && r == 0)
        return 0;
    else if (b > 224 && g > 239 && r > 239)
        return 127;
    else if (b == 255 && r == 0)
    {
        for (int i = 1; i < 22; ++i)
            if (g > s_colorMap7[i - 1][1] && g <= s_colorMap7[i][1])
                return i;
    }
    else if (g == 255 && r == 0)
    {
        for (int i = 22; i < 44; ++i)
            if (b < s_colorMap7[i - 1][0] && b >= s_colorMap7[i][0])
                return i;
    }
    else if (b == 0 && g == 255)
    {
        for (int i = 44; i < 65; ++i)
            if (r > s_colorMap7[i - 1][2] && r <= s_colorMap7[i][2])
                return i;
    }
    else if (b == 0 && r == 255)
    {
        for (int i = 65; i < 86; ++i)
            if (g < s_colorMap7[i - 1][1] && g >= s_colorMap7[i][1])
                return i;
    }
    else if (g == 0 && r == 255)
    {
        for (int i = 86; i < 108; ++i)
            if (b > s_colorMap7[i - 1][0] && b <= s_colorMap7[i][0])
                return i;
    }
    else if (b == 255 && g == 0)
    {
        for (int i = 108; i < 127; ++i)
            if (r < s_colorMap7[i - 1][2] && r >= s_colorMap7[i][2])
                return i;
    }
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MFT_RGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~MFT_RGB_Midi_FeedbackProcessor() {}
    MFT_RGB_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) { }

    virtual const char* GetName() override { return "MFT_RGB_Midi_FeedbackProcessor"; }

    // Originally, ForceClear() would create a "black" color (all zeros)
    // and call ForceColorValue() to turn off the LED.
    // Commenting out the code so that LED is not forced off on load.
    virtual void ForceClear() override
    {
        // rgba_color color;
        // ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color& color) override
    {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override
    {
        lastColor_ = color;

        if ((color.r == 177 || color.r == 181) && color.g == 31) // this sets the different MFT modes
            SendMidiMessage(color.r, color.g, color.b);
        else
        {
            int colorInt = GetColorIntFromRGB(color.r, color.g, color.b);
            // Originally, if the converted MIDI color is 0, it would send a message to turn off the LED.
            // Commenting this out prevents turning off the LED.
            if (colorInt == 0)
            {
                // SendMidiMessage(midiFeedbackMessage1_.midi_message[0] + 1, midiFeedbackMessage1_.midi_message[1], 17); // turn off led
            }
            else
            {
                SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], colorInt); // set color
                SendMidiMessage(midiFeedbackMessage1_.midi_message[0] + 1, midiFeedbackMessage1_.midi_message[1], 47);  // turn on led max brightness
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) {
            LogToConsole(256, "[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
        }
    }
};

#endif /* control_surface_midi_widgets_h */


