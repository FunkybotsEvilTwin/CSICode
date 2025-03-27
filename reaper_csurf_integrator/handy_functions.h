//
//  handy_reaper_functions.h
//  reaper_csurf_integrator
//
//

#ifndef handy_functions_h
#define handy_functions_h

#include "../WDL/db2val.h"

static double int14ToNormalized(unsigned char msb, unsigned char lsb)
{
    int val = lsb | (msb<<7);
    double normalizedVal = val/16383.0;
    
    normalizedVal = normalizedVal < 0.0 ? 0.0 : normalizedVal;
    normalizedVal = normalizedVal > 1.0 ? 1.0 : normalizedVal;
    
    return normalizedVal;
}

static double normalizedToVol(double val)
{
    double pos=val*1000.0;
    pos=SLIDER2DB(pos);
    return DB2VAL(pos);
}

static double volToNormalized(double vol)
{
    double d=(DB2SLIDER(VAL2DB(vol))/1000.0);
    if (d<0.0)d=0.0;
    else if (d>1.0)d=1.0;
    
    return d;
}

static double normalizedToPan(double val)
{
    return 2.0 * val - 1.0;
}

static double panToNormalized(double val)
{
    return 0.5 * (val + 1.0);
}

static void WindowsDebugOutput(const char * format, ...)
{
    #if defined (_WIN32) && defined (_DEBUG)  
        char buffer             [2056];
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        OutputDebugString(buffer);
    #endif
}

namespace BehringerColor {

    enum XColor {
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

    int rgbToColor(int r, int g, int b)
    {
        // Doing a RGB to HSV conversion since HSV is better for light
        // Converting RGB to floats between 0 and 1.0 (percentage)
        float rf = r / 255.0;
        float gf = g / 255.0;
        float bf = b / 255.0;

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
            h = ((bf - rf) / delta) + 2.0;
        else
            h = ((rf - gf) / delta) + 4.0;

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

    int colorFromString(const char *str)
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
}

#endif /* handy_functions_h */
