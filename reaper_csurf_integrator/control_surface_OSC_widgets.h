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
private:

    enum X32Color
    {
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

    /*
     * NOTE: Any changes made to rgbToColor here must also be mirrored in class
     * XTouchDisplay_Midi_FeedbackProcessor in control_surface_midi_widgets.h
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
        v = (float) max(max(rf, gf), bf);

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
        if (rf >= v)        h =  (gf - bf) / delta;
        else if (gf >= v)   h = ((bf - rf) / delta) + 2.0f;
        else                h = ((rf - gf) / delta) + 4.0f;

        h *= 60.0;
        if (h < 0)  h += 360.0;

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
    OSC_X32FeedbackProcessor(CSurfIntegrator *const csi, OSC_ControlSurface *surface, Widget *widget, const string &oscAddress) : OSC_FeedbackProcessor(csi, surface, widget, oscAddress)  {}
    ~OSC_X32FeedbackProcessor() {}

    virtual const char *GetName() override { return "OSC_X32FeedbackProcessor"; }

    virtual void SetColorValue(const rgba_color &color) override
    {
        if (lastColor_ != color)
        {
            lastColor_ = color;

            surface_->SendOSCMessage(this, oscAddress_.c_str(), rgbToColor(color.r, color.g, color.b));
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
