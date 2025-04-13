//
//  handy_functions.h
//  reaper_csurf_integrator
//
//

#ifndef handy_functions_h
#define handy_functions_h

#include "../WDL/db2val.h"

#include <stacktrace>
#include <iostream>

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

enum DebugLevel {
    DEBUG_LEVEL_ERROR   = 0,
    DEBUG_LEVEL_WARNING = 1,
    DEBUG_LEVEL_NOTICE  = 2,
    DEBUG_LEVEL_INFO    = 3,
    DEBUG_LEVEL_DEBUG   = 4
};

template <typename... Args>
static void LogToConsole(int size, const char* format, Args... args)
{
    std::vector<char> buffer(size);
    std::snprintf(buffer.data(), buffer.size(), format, args...);
    ShowConsoleMsg(buffer.data());
#ifdef _DEBUG
    // std::ofstream logFile(std::string(GetResourcePath()) + "/CSI/CSI.log", std::ios::app);
    std::ofstream logFile(std::string(__FILE__) + ".log", std::ios::app);
    if (logFile.is_open())
    {
        char timeStr[32];
        time_t rawtime;
        time(&rawtime);
        struct tm* timeinfo = localtime(&rawtime);
        strftime(timeStr, sizeof(timeStr), "[%y-%m-%d %H:%M:%S] ", timeinfo);

        logFile << timeStr << buffer.data();
    }
#endif
}

static void LogStackTraceToConsole() {
// to enable std::stacktrace change LanguageStandard to stdcpp23
// Windows\reaper_csurf_integrator\reaper_csurf_integrator\reaper_csurf_integrator.vcxproj
//   <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
//     <ClCompile>
//       <WarningLevel>Level3</WarningLevel>
//       <Optimization>Disabled</Optimization>
//       <SDLCheck>true</SDLCheck>
//       <PreprocessorDefinitions>_WINDLL;%(PreprocessorDefinitions) _CRT_SECURE_NO_WARNINGS</PreprocessorDefinitions>
//       <LanguageStandard>stdcpp23</LanguageStandard>
//     </ClCompile>
//   </ItemDefinitionGroup>
    auto trace = std::stacktrace::current();
    LogToConsole(256, "===== Stack Trace Start =====\n");
    for (const auto& frame : trace) {
        std::stringstream ss;
        ss << frame;
        std::string line = ss.str();
        LogToConsole(1024, "%s\n", line.c_str());
    }
    LogToConsole(256, "===== Stack Trace End =====\n");
}

static const char* GetRelativePath(const char* absolutePath)
{
    const char* resourcePath = GetResourcePath();
    size_t resourcePathLen = std::strlen(resourcePath);

    if (std::strncmp(absolutePath, resourcePath, resourcePathLen) == 0)
    {
        const char* rel = absolutePath + resourcePathLen;
        if (*rel == '/' || *rel == '\\')
            ++rel;
        return rel;
    }

    return absolutePath;
}

#endif /* handy_functions_h */
