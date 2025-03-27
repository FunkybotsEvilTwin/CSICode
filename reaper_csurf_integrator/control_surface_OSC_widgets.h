//
//  control_surface_OSC_widgets.h
//  reaper_csurf_integrator
//

#ifndef control_surface_OSC_widgets_h
#define control_surface_OSC_widgets_h

#include "handy_functions.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_X32FeedbackProcessor : public OSC_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    OSC_X32FeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_FeedbackProcessor(csi, surface, widget, oscAddress)  {}
    ~OSC_X32FeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_X32FeedbackProcessor"; }

    virtual void SetColorValue(const rgba_color &color) override
    {
        if (lastColor_ != color)
        {
            lastColor_ = color;

            surface_->SendOSCMessage(this, oscAddress_.c_str(), BehringerColor::rgbToColor(color.r, color.g, color.b));
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_X32IntFeedbackProcessor : public OSC_IntFeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    OSC_X32IntFeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_IntFeedbackProcessor(csi, surface, widget, oscAddress) {}
    ~OSC_X32IntFeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_X32IntFeedbackProcessor"; }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastDoubleValue_ = value;
        
        if (oscAddress_.find("/-stat/selidx/") != string::npos  && value != 0.0)
        {
            string selectIndex = oscAddress_.substr(oscAddress_.find_last_of('/') + 1);
            surface_->SendOSCMessage(this, "/-stat/selidx", (int)atoi(selectIndex.c_str()));
        }
        else
            surface_->SendOSCMessage(this, oscAddress_.c_str(), (int)value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class X32_Fader_OSC_MessageGenerator : public CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    X32_Fader_OSC_MessageGenerator(CSurfIntegrator *const csi, Widget *widget) : CSIMessageGenerator(csi, widget) {}
    ~X32_Fader_OSC_MessageGenerator() {}

    virtual void ProcessMessage(double value) override
    {
        if      (value >= 0.5)    value = value *  40.0 - 30.0;  // max dB value: +10.
        else if (value >= 0.25)   value = value *  80.0 - 50.0;
        else if (value >= 0.0625) value = value * 160.0 - 70.0;
        else if (value >= 0.0)    value = value * 480.0 - 90.0;  // min dB value: -90 or -oo

        value = volToNormalized(DB2VAL(value));

        widget_->SetIncomingMessageTime(GetTickCount());
        widget_->GetZoneManager()->DoAction(widget_, value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_X32FaderFeedbackProcessor : public OSC_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    OSC_X32FaderFeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_FeedbackProcessor(csi, surface, widget, oscAddress) {}
    ~OSC_X32FaderFeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_X32FaderFeedbackProcessor"; }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        lastDoubleValue_ = value;

        value = VAL2DB(normalizedToVol(value));

        if      (value < -60.0) value = (value + 90.0) / 480.0;
        else if (value < -30.0) value = (value + 70.0) / 160.0;
        else if (value < -10.0) value = (value + 50.0) /  80.0;
        else if (value <= 10.0) value = (value + 30.0) /  40.0;

        if ((GetTickCount() - GetWidget()->GetLastIncomingMessageTime()) >= 30)
            surface_->SendOSCMessage(this, oscAddress_.c_str(), value);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class X32_RotaryToEncoder_OSC_MessageGenerator : public CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    X32_RotaryToEncoder_OSC_MessageGenerator(CSurfIntegrator *const csi, Widget *widget) : CSIMessageGenerator(csi, widget) {}
    ~X32_RotaryToEncoder_OSC_MessageGenerator() {}

    virtual void ProcessMessage(double value) override
    {
        double delta = (1 / 128.0) * value ;
        
        if (delta < 0.5)
            delta = -delta;
        
        delta *= 0.1;
        
        widget_->SetLastIncomingDelta(delta);
        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSC_X32_RotaryToEncoderFeedbackProcessor : public OSC_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    OSC_X32_RotaryToEncoderFeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_FeedbackProcessor(csi, surface, widget, oscAddress) {}
    ~OSC_X32_RotaryToEncoderFeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_X32_RotaryToEncoderFeedbackProcessor"; }
    
    virtual void SetValue(const PropertyList &properties, double value) override
    {
        if (widget_->GetLastIncomingDelta() != 0.0)
        {
            widget_->SetLastIncomingDelta(0.0);
            ForceValue(properties, value);
        }
    }
    
    virtual void ForceValue(const PropertyList &properties, double value) override
    {
        surface_->SendOSCMessage(this, oscAddress_.c_str(), 64);
    }
};

#endif /* control_surface_OSC_widgets_h */
