//
//  handy_functions.h
//  reaper_csurf_integrator
//
//

#ifndef handy_functions_h
#define handy_functions_h

#include "../WDL/db2val.h"

#ifdef _DEBUG
  #if defined(__cpp_lib_stacktrace)
#include <stacktrace>
  #endif
#endif
#include <iostream>
#include <vector>

using namespace std;

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
    vector<char> buffer(size);
    snprintf(buffer.data(), buffer.size(), format, args...);
    ShowConsoleMsg(buffer.data());
#ifdef _DEBUG
    ofstream logFile(string(GetResourcePath()) + "/CSI/CSI.log", ios::app);
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
// to enable stacktrace change LanguageStandard to stdcpp23
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
#ifdef _DEBUG
  #if defined(__cpp_lib_stacktrace)
    auto trace = stacktrace::current();
    LogToConsole(256, "===== Stack Trace Start =====\n");
    for (const auto& frame : trace) {
        stringstream ss;
        ss << frame;
        string line = ss.str();
        if (line.find('\\') != string::npos || line.find('/') != string::npos)
        {
            size_t pos = 0;
            while ((pos = line.find("reaper_csurf_integrator!", pos)) != string::npos)
                line.replace(pos, 24, "");

            pos = line.find("+0x");
            if (pos != string::npos)
                line = line.substr(0, pos);

            LogToConsole(1024, "%s\n", line.c_str());
        }
    }
    LogToConsole(256, "===== Stack Trace End =====\n");
    #else
    LogToConsole(256, "LogStackTraceToConsole not supported on this compiler. Refer to reaper_csurf_integrator/handy_functions.h LogStackTraceToConsole() on how to enable it.\n");
  #endif
#endif
}

static const char* GetRelativePath(const char* absolutePath)
{
    const char* resourcePath = GetResourcePath();
    size_t resourcePathLen = strlen(resourcePath);

    if (strncmp(absolutePath, resourcePath, resourcePathLen) == 0)
    {
        const char* rel = absolutePath + resourcePathLen;
        if (*rel == '/' || *rel == '\\')
            ++rel;
        return rel;
    }

    return absolutePath;
}

#endif /* handy_functions_h */
