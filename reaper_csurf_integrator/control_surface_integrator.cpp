//
//  control_surface_integrator.cpp
//  reaper_control_surface_integrator
//
//

#define INCLUDE_LOCALIZE_IMPORT_H
#include "control_surface_integrator.h"
#include "control_surface_midi_widgets.h"
#include "control_surface_OSC_widgets.h"
#include "control_surface_Reaper_actions.h"
#include "control_surface_manager_actions.h"

#include "resource.h"

extern WDL_DLGRET dlgProcMainConfig(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern reaper_plugin_info_t *g_reaper_plugin_info;

extern void WidgetMoved(ZoneManager *zoneManager, Widget *widget, int modifier);

int g_minNumParamSteps = 1;
int g_maxNumParamSteps = 30;
#ifdef _DEBUG
int g_debugLevel = DEBUG_LEVEL_NOTICE;
#else
int g_debugLevel = DEBUG_LEVEL_ERROR;
#endif
bool g_surfaceRawInDisplay;
bool g_surfaceInDisplay;
bool g_surfaceOutDisplay;

bool g_fxParamsWrite;

void GetPropertiesFromTokens(int start, int finish, const vector<string> &tokens, PropertyList &properties)
{
    for (int i = start; i < finish; ++i)
    {
        const char *tok = tokens[i].c_str();
        const char *eq = strstr(tok,"=");
        if (eq != NULL && strstr(eq+1, "=") == NULL /* legacy behavior, don't allow = in value */)
        {
            char tmp[128];
            int pnlen = (int) (eq-tok) + 1; // +1 is for the null character to be added to tmp
            if (WDL_NOT_NORMALLY(pnlen > sizeof(tmp))) // if this asserts on a valid property name, increase tmp size
                pnlen = sizeof(tmp);
            lstrcpyn_safe(tmp, tok, pnlen);

            PropertyType prop = PropertyList::prop_from_string(tmp);
            if (prop != PropertyType_Unknown)
            {
                properties.set_prop(prop, eq+1);
            }
            else
            {
                properties.set_prop(prop, tok); // unknown properties are preserved as Unknown, key=value pair

                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole(256, "[WARNING] CSI does not support property named %s\n", tok);
                
               // WDL_ASSERT(false);
            }
        }
    }
}

void GetSteppedValues(const vector<string> &params, int start_idx, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues)
{
    int openSquareIndex = -1, closeSquareIndex = -1;
    
    for (int i = start_idx; i < params.size(); ++i)
        if (params[i] == "[")
        {
            openSquareIndex = i;
            break;
        }

    if (openSquareIndex < 0) return;
    
    for (int i = openSquareIndex + 1; i < params.size(); ++i)
        if (params[i] == "]")
        {
            closeSquareIndex = i;
            break;
        }
    
    if (closeSquareIndex > 0)
    {
        for (int i = openSquareIndex + 1; i < closeSquareIndex; ++i)
        {
            const char *str = params[i].c_str();

            if (str[0] == '(' && str[strlen(str)-1] == ')')
            {
                str++; // skip (

                // (1.0,2.0,3.0) -> acceleratedDeltaValues : mode = 2
                // (1.0) -> deltaValue : mode = 1
                // (1) or (1,2,3) -> acceleratedTickValues : mode = 0
                const int mode = strstr(str, ".") ? strstr(str, ",") ? 2 : 1 : 0;

                while (*str)
                {
                    if (mode == 0)
                    {
                        int v = 0;
                        if (WDL_NOT_NORMALLY(sscanf(str, "%d", &v) != 1)) break;
                        acceleratedTickValues.push_back(v);
                    }
                    else
                    {
                        double v = 0.0;
                        if (WDL_NOT_NORMALLY(sscanf(str,"%lf", &v) != 1)) break;
                        if (mode == 1)
                        {
                            deltaValue = v;
                            break;
                        }
                        acceleratedDeltaValues.push_back(v);
                    }

                    while (*str && *str != ',') str++;
                    if (*str == ',') str++;
                }
            }
            // todo: support 1-3 syntax? else if (!strstr(str,".") && str[0] != '-' && strstr(str,"-"))
            else
            {
                // 1.0>3.0 writes to rangeMinimum/rangeMaximum
                // 1 or 1.0 -> steppedValues
                double a = 0.0, b = 0.0;
                const int nmatch = sscanf(str, "%lf>%lf", &a, &b);

                if (nmatch == 2)
                {
                    rangeMinimum = wdl_min(a,b);
                    rangeMaximum = wdl_max(a,b);
                }
                else if (WDL_NORMALLY(nmatch == 1))
                {
                    steppedValues.push_back(a);
                }
            }
        }
    }
}

static double EnumSteppedValues(int numSteps, int stepNumber)
{
    return floor(stepNumber / (double)(numSteps - 1)  *100.0 + 0.5)  *0.01;
}

void GetParamStepsString(string& outputString, int numSteps) // appends to string 
{
    // When number of steps equals 1, users are typically looking to use a button to reset.
    // A halfway value (0.5) is chosen as a good reset value instead of the previous 0.1.
    if (numSteps == 1)
    {
        outputString = "0.5";
    }
    else
    {
        for (int i = 0; i < numSteps; ++i)
        {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "%.2f", EnumSteppedValues(numSteps, i));
            WDL_remove_trailing_decimal_zeros(tmp, 0);
            lstrcatn(tmp, " ", sizeof(tmp));
            outputString += tmp;
        }
    }
}

void GetParamStepsValues(vector<double> &outputVector, int numSteps)
{
    outputVector.clear();

    for (int i = 0; i < numSteps; ++i)
        outputVector.push_back(EnumSteppedValues(numSteps, i));
}

void TrimLine(string &line)
{
    const string tmp = line;
    const char *p = tmp.c_str();

    // remove leading and trailing spaces
    // condense whitespace to single whitespace
    // stop copying at "//" (comment)
    line.clear();
    for (;;)
    {
        // advance over whitespace
        while (*p > 0 && isspace(*p))
            p++;

        // a single / at the beginning of a line indicates a comment
        if (!*p || p[0] == '/') break;

        if (line.length())
            line.append(" ",1);

        // copy non-whitespace to output
        while (*p && (*p < 0 || !isspace(*p)))
        {
           if (p[0] == '/' && p[1] == '/') break; // existing behavior, maybe not ideal, but a comment can start anywhere
           line.append(p++,1);
        }
    }
    if (!line.empty() && g_debugLevel > DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] %s\n", line.c_str());
}

void ReplaceAllWith(string &output, const char *charsToReplace, const char *replacement)
{
    // replace all occurences of
    // any char in charsToReplace
    // with replacement string
    const string tmp = output;
    const char *p = tmp.c_str();
    output.clear();

    while (*p)
    {
        if (strchr(charsToReplace, *p) != NULL)
        {
            output.append(replacement);
        }
        else
            output.append(p,1);
        p++;
    }
}

void GetTokens(vector<string> &tokens, const string &line)
{
    istringstream iss(line);
    string token;
    while (iss >> quoted(token))
        tokens.push_back(token);
}

void GetTokens(vector<string> &tokens, const string &line, char delimiter)
{
    istringstream iss(line);
    string token;
    while (getline(iss, token, delimiter))
        tokens.push_back(token);
}

int strToHex(string &valueStr)
{
    return strtol(valueStr.c_str(), NULL, 16);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MidiPort
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int port, refcnt;
    void *dev;
    
    MidiPort(int portidx, void *devptr) : port(portidx), refcnt(1), dev(devptr) { };
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static WDL_TypedBuf<MidiPort> s_midiInputs;
static WDL_TypedBuf<MidiPort> s_midiOutputs;

void ReleaseMidiInput(midi_Input *input)
{
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].dev == (void*)input)
        {
            if (!--s_midiInputs.Get()[i].refcnt)
            {
                input->stop();
                delete input;
                s_midiInputs.Delete(i);
                break;
            }
        }
}

void ReleaseMidiOutput(midi_Output *output)
{
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].dev == (void*)output)
        {
            if (!--s_midiOutputs.Get()[i].refcnt)
            {
                delete output;
                s_midiOutputs.Delete(i);
                break;
            }
        }
}

static midi_Input *GetMidiInputForPort(int inputPort)
{
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].port == inputPort)
        {
            s_midiInputs.Get()[i].refcnt++;
            return (midi_Input *)s_midiInputs.Get()[i].dev;
        }
    
    midi_Input *newInput = CreateMIDIInput(inputPort);
    
    if (newInput)
    {
        newInput->start();
        MidiPort midiInputPort(inputPort, newInput);
        s_midiInputs.Add(midiInputPort);
    }
    
    return newInput;
}

static midi_Output *GetMidiOutputForPort(int outputPort)
{
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].port == outputPort)
        {
            s_midiOutputs.Get()[i].refcnt++;
            return (midi_Output *)s_midiOutputs.Get()[i].dev;
        }
    
    midi_Output *newOutput = CreateMIDIOutput(outputPort, false, NULL);
    
    if (newOutput)
    {
        MidiPort midiOutputPort(outputPort, newOutput);
        s_midiOutputs.Add(midiOutputPort);
    }
    
    return newOutput;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct OSCSurfaceSocket
////////////////////////////////7/////////////////////////////////////////////////////////////////////////////////////////
{
    string surfaceName;
    oscpkt::UdpSocket *socket;
    int refcnt;
    
    OSCSurfaceSocket(const string &name, oscpkt::UdpSocket *s)
    {
        surfaceName = name;
        socket = s;
        refcnt = 1;
    }
    ~OSCSurfaceSocket()
    {
      delete socket;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static WDL_PtrList<OSCSurfaceSocket> s_inputSockets;
static WDL_PtrList<OSCSurfaceSocket> s_outputSockets;

static oscpkt::UdpSocket *GetInputSocketForPort(string surfaceName, int inputPort)
{
    for (int i = 0; i < s_inputSockets.GetSize(); ++i)
        if (s_inputSockets.Get(i)->surfaceName == surfaceName)
        {
            s_inputSockets.Get(i)->refcnt++;
            return s_inputSockets.Get(i)->socket; // return existing
        }
    
    // otherwise make new
    oscpkt::UdpSocket *newInputSocket = new oscpkt::UdpSocket();
    
    if (newInputSocket)
    {
        newInputSocket->bindTo(inputPort);
        
        if (! newInputSocket->isOk())
        {
            //cerr << "Error opening port " << PORT_NUM << ": " << inSocket_.errorMessage() << "\n";
            return NULL;
        }
        
        s_inputSockets.Add(new OSCSurfaceSocket(surfaceName, newInputSocket));
        return newInputSocket;
    }
    
    return NULL;
}

static oscpkt::UdpSocket *GetOutputSocketForAddressAndPort(const string &surfaceName, const string &address, int outputPort)
{
    for (int i = 0; i < s_outputSockets.GetSize(); ++i)
        if (s_outputSockets.Get(i)->surfaceName == surfaceName)
        {
            s_outputSockets.Get(i)->refcnt++;
            return s_outputSockets.Get(i)->socket; // return existing
        }
    
    // otherwise make new
    oscpkt::UdpSocket *newOutputSocket = new oscpkt::UdpSocket();
    
    if (newOutputSocket)
    {
        if ( ! newOutputSocket->connectTo(address, outputPort))
        {
            //cerr << "Error connecting " << remoteDeviceIP_ << ": " << outSocket_.errorMessage() << "\n";
            return NULL;
        }
        
        if ( ! newOutputSocket->isOk())
        {
            //cerr << "Error opening port " << outPort_ << ": " << outSocket_.errorMessage() << "\n";
            return NULL;
        }

        s_outputSockets.Add(new OSCSurfaceSocket(surfaceName, newOutputSocket));
        return newOutputSocket;
    }
    
    return NULL;
}

// files
static void listFilesOfType(const string &path, vector<string> &results, const string &type)
{
    filesystem::path zonePath { path };
    
    if (filesystem::exists(path) && filesystem::is_directory(path))
        for (auto &file : filesystem::recursive_directory_iterator(path))
            if (file.path().extension() == type)
                results.push_back(file.path().string());
}

//////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurface::ProcessMidiWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens)
{
    if (in_tokens.size() < 2)
        return;

    string widgetClass;
    
    if (in_tokens.size() > 2)
        widgetClass = in_tokens[2];

    AddWidget(this, in_tokens[1].c_str());

    Widget *widget = GetWidgetByName(in_tokens[1]);
    
    if (widget == NULL)
    {
        LogToConsole(2048, "[ERROR] FAILED to ProcessMidiWidget: widget not found by name %s. Line %s\n", in_tokens[1].c_str(), lineNumber);
        return;
    }
    vector<vector<string>> tokenLines;
    
    for (string line; getline(surfaceTemplateFile, line) ; )
    {
        TrimLine(line);
        
        lineNumber++;
        
        if (line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
            continue;
        
        vector<string> tokens;
        GetTokens(tokens, line);

        if (tokens[0] == "WidgetEnd")    // Widget list complete
            break;
        
        tokenLines.push_back(tokens);
    }
    
    if (tokenLines.size() < 1)
        return;
    
    for (int i = 0; i < tokenLines.size(); ++i)
    {
        int size = (int)tokenLines[i].size();
        
        const string widgetType = tokenLines[i][0];

        MIDI_event_ex_t message1;
        MIDI_event_ex_t message2;

        string oneByteKey = "";
        string twoByteKey = "";
        string threeByteKey = "";
        string threeByteKeyMsg2 = "";
        
        if (size > 3)
        {
            message1.midi_message[0] = strToHex(tokenLines[i][1]);
            message1.midi_message[1] = strToHex(tokenLines[i][2]);
            message1.midi_message[2] = strToHex(tokenLines[i][3]);
            
            oneByteKey = to_string(message1.midi_message[0] * 0x10000);
            twoByteKey = to_string(message1.midi_message[0] * 0x10000 + message1.midi_message[1] * 0x100);
            threeByteKey = to_string(message1.midi_message[0] * 0x10000 + message1.midi_message[1] * 0x100 + message1.midi_message[2]);
        }
        if (size > 6)
        {
            message2.midi_message[0] = strToHex(tokenLines[i][4]);
            message2.midi_message[1] = strToHex(tokenLines[i][5]);
            message2.midi_message[2] = strToHex(tokenLines[i][6]);
            
            threeByteKeyMsg2 = to_string(message2.midi_message[0] * 0x10000 + message2.midi_message[1] * 0x100 + message2.midi_message[2]);
        }
        
        // Generators
        if (widgetType == "AnyPress" && (size == 4 || size == 7))
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<AnyPress_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "Press" && size == 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(threeByteKey, make_unique<PressRelease_Midi_CSIMessageGenerator>(csi_, widget, message1)));
        else if (widgetType == "Press" && size == 7)
        {
            CSIMessageGeneratorsByMessage_.insert(make_pair(threeByteKey, make_unique<PressRelease_Midi_CSIMessageGenerator>(csi_, widget, message1, message2)));
            CSIMessageGeneratorsByMessage_.insert(make_pair(threeByteKeyMsg2, make_unique<PressRelease_Midi_CSIMessageGenerator>(csi_, widget, message1, message2)));
        }
        else if (widgetType == "Fader14Bit" && size == 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(oneByteKey, make_unique<Fader14Bit_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "FaderportClassicFader14Bit" && size == 7)
            CSIMessageGeneratorsByMessage_.insert(make_pair(oneByteKey, make_unique<FaderportClassicFader14Bit_Midi_CSIMessageGenerator>(csi_, widget, message1, message2)));
        else if (widgetType == "Fader7Bit" && size== 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<Fader7Bit_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "Encoder" && widgetClass == "RotaryWidgetClass")
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "Encoder" && size == 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<Encoder_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "MFTEncoder" && size > 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<MFT_AcceleratedEncoder_Midi_CSIMessageGenerator>(csi_, widget, tokenLines[i])));
        else if (widgetType == "EncoderPlain" && size == 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<EncoderPlain_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "Encoder7Bit" && size == 4)
            CSIMessageGeneratorsByMessage_.insert(make_pair(twoByteKey, make_unique<Encoder7Bit_Midi_CSIMessageGenerator>(csi_, widget)));
        else if (widgetType == "Touch" && size == 7)
        {
            CSIMessageGeneratorsByMessage_.insert(make_pair(threeByteKey, make_unique<Touch_Midi_CSIMessageGenerator>(csi_, widget, message1, message2)));
            CSIMessageGeneratorsByMessage_.insert(make_pair(threeByteKeyMsg2, make_unique<Touch_Midi_CSIMessageGenerator>(csi_, widget, message1, message2)));
        }

        // Feedback Processors
        if (widgetType == "FB_TwoState" && size == 7)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<TwoState_Midi_FeedbackProcessor>(csi_, this, widget, message1, message2));
        }
        else if (widgetType == "FB_NovationLaunchpadMiniRGB7Bit" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_MFT_RGB" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<MFT_RGB_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_AsparionRGB" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<AsparionRGB_Midi_FeedbackProcessor>(csi_, this, widget, message1));
            AddTrackColorFeedbackProcessor(widget->GetFeedbackProcessors().back().get());
        }
        else if (widgetType == "FB_FaderportRGB" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FaderportRGB_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_FaderportTwoStateRGB" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FPTwoStateRGB_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_FaderportValueBar"  && size == 2)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FPValueBar_Midi_FeedbackProcessor>(csi_, this, widget, atoi(tokenLines[i][1].c_str())));
        }
        else if ((widgetType == "FB_FPVUMeter") && size == 2)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FPVUMeter_Midi_FeedbackProcessor>(csi_, this, widget, atoi(tokenLines[i][1].c_str())));
        }
        else if (widgetType == "FB_Fader14Bit" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<Fader14Bit_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_FaderportClassicFader14Bit" && size == 7)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FaderportClassicFader14Bit_Midi_FeedbackProcessor>(csi_, this, widget, message1, message2));
        }
        else if (widgetType == "FB_Fader7Bit" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<Fader7Bit_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_Encoder" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<Encoder_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_AsparionEncoder" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<AsparionEncoder_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_ConsoleOneVUMeter" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<ConsoleOneVUMeter_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_ConsoleOneGainReductionMeter" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<ConsoleOneGainReductionMeter_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_MCUTimeDisplay" && size == 1)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<MCU_TimeDisplay_Midi_FeedbackProcessor>(csi_, this, widget));
        }
        else if (widgetType == "FB_MCUAssignmentDisplay" && size == 1)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor>(csi_, this, widget));
        }
        else if (widgetType == "FB_QConProXMasterVUMeter" && size == 2)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<QConProXMasterVUMeter_Midi_FeedbackProcessor>(csi_, this, widget, atoi(tokenLines[i][1].c_str())));
        }
        else if ((widgetType == "FB_MCUVUMeter" || widgetType == "FB_MCUXTVUMeter") && size == 2)
        {
            int displayType = widgetType == "FB_MCUVUMeter" ? 0x14 : 0x15;
            
            widget->GetFeedbackProcessors().push_back(make_unique<MCUVUMeter_Midi_FeedbackProcessor>(csi_, this, widget, displayType, atoi(tokenLines[i][1].c_str())));
            
            SetHasMCUMeters(displayType);
        }
        else if ((widgetType == "FB_AsparionVUMeterL" || widgetType == "FB_AsparionVUMeterR") && size == 2)
        {
            bool isRight = widgetType == "FB_AsparionVUMeterR" ? true : false;
            
            widget->GetFeedbackProcessors().push_back(make_unique<AsparionVUMeter_Midi_FeedbackProcessor>(csi_, this, widget, 0x14, atoi(tokenLines[i][1].c_str()), isRight));
            
            SetHasMCUMeters(0x14);
        }
        else if (widgetType == "FB_SCE24LEDButton" && size == 4)
        {
            MIDI_event_ex_t midiEvent;
            midiEvent.midi_message[0] = strToHex(tokenLines[i][1]);
            midiEvent.midi_message[1] = strToHex(tokenLines[i][2]) + 0x60;
            midiEvent.midi_message[2] = strToHex(tokenLines[i][3]);

            widget->GetFeedbackProcessors().push_back(make_unique<SCE24TwoStateLED_Midi_FeedbackProcessor>(csi_, this, widget, midiEvent));
        }
        else if (widgetType == "FB_SCE24OLEDButton" && size == 7)
        {
            MIDI_event_ex_t midiEvent;
            midiEvent.midi_message[0] = strToHex(tokenLines[i][1]);
            midiEvent.midi_message[1] = strToHex(tokenLines[i][2]) + 0x60;
            midiEvent.midi_message[2] = strToHex(tokenLines[i][3]);
            
            widget->GetFeedbackProcessors().push_back(make_unique<SCE24OLED_Midi_FeedbackProcessor>(csi_, this, widget, midiEvent, atoi(tokenLines[i][4].c_str()), atoi(tokenLines[i][5].c_str()), atoi(tokenLines[i][6].c_str())));
        }
        else if (widgetType == "FB_SCE24Encoder" && size == 4)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<SCE24Encoder_Midi_FeedbackProcessor>(csi_, this, widget, message1));
        }
        else if (widgetType == "FB_SCE24EncoderText" && size == 7)
        {
            widget->GetFeedbackProcessors().push_back(make_unique<SCE24Text_Midi_FeedbackProcessor>(csi_, this, widget, message1, atoi(tokenLines[i][4].c_str()), atoi(tokenLines[i][5].c_str()), atoi(tokenLines[i][6].c_str())));
        }
        else if ((widgetType == "FB_MCUDisplayUpper" || widgetType == "FB_MCUDisplayLower" || widgetType == "FB_MCUXTDisplayUpper" || widgetType == "FB_MCUXTDisplayLower") && size == 2)
        {
            if (widgetType == "FB_MCUDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_MCUDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_MCUXTDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x15, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_MCUXTDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x15, 0x12, atoi(tokenLines[i][1].c_str())));
        }
        else if ((widgetType == "FB_IconDisplay1Upper" || widgetType == "FB_IconDisplay1Lower" || widgetType == "FB_IconDisplay2Upper" || widgetType == "FB_IconDisplay2Lower") && size == 2)
        {
            if (widgetType == "FB_IconDisplay1Upper")
                widget->GetFeedbackProcessors().push_back(make_unique<IconDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x14, 0x12, atoi(tokenLines[i][1].c_str()), 0x00, 0x66));
            else if (widgetType == "FB_IconDisplay1Lower")
                widget->GetFeedbackProcessors().push_back(make_unique<IconDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x14, 0x12, atoi(tokenLines[i][1].c_str()), 0x00, 0x66));
            else if (widgetType == "FB_IconDisplay2Upper")
                widget->GetFeedbackProcessors().push_back(make_unique<IconDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x15, 0x13, atoi(tokenLines[i][1].c_str()), 0x02, 0x4e));
            else if (widgetType == "FB_IconDisplay2Lower")
                widget->GetFeedbackProcessors().push_back(make_unique<IconDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x15, 0x13, atoi(tokenLines[i][1].c_str()), 0x02, 0x4e));
        }
        else if ((widgetType == "FB_AsparionDisplayUpper" || widgetType == "FB_AsparionDisplayLower" || widgetType == "FB_AsparionDisplayEncoder") && size == 2)
        {
            if (widgetType == "FB_AsparionDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<AsparionDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x01, 0x14, 0x1A, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_AsparionDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<AsparionDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, 0x14, 0x1A, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_AsparionDisplayEncoder")
                widget->GetFeedbackProcessors().push_back(make_unique<AsparionDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x03, 0x14, 0x19, atoi(tokenLines[i][1].c_str())));
        }
        else if ((widgetType == "FB_XTouchDisplayUpper" || widgetType == "FB_XTouchDisplayLower" || widgetType == "FB_XTouchXTDisplayUpper" || widgetType == "FB_XTouchXTDisplayLower") && size == 2)
        {
            if (widgetType == "FB_XTouchDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<XTouchDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_XTouchDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<XTouchDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_XTouchXTDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<XTouchDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x15, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_XTouchXTDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<XTouchDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x15, 0x12, atoi(tokenLines[i][1].c_str())));
            
            AddTrackColorFeedbackProcessor(widget->GetFeedbackProcessors().back().get());
        }
        else if ((widgetType == "FB_C4DisplayUpper" || widgetType == "FB_C4DisplayLower") && size == 3)
        {
            if (widgetType == "FB_C4DisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x17, atoi(tokenLines[i][1].c_str()) + 0x30, atoi(tokenLines[i][2].c_str())));
            else if (widgetType == "FB_C4DisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<MCUDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x17, atoi(tokenLines[i][1].c_str()) + 0x30, atoi(tokenLines[i][2].c_str())));
        }
        else if ((widgetType == "FB_FP8ScribbleLine1" || widgetType == "FB_FP16ScribbleLine1"
                 || widgetType == "FB_FP8ScribbleLine2" || widgetType == "FB_FP16ScribbleLine2"
                 || widgetType == "FB_FP8ScribbleLine3" || widgetType == "FB_FP16ScribbleLine3"
                 || widgetType == "FB_FP8ScribbleLine4" || widgetType == "FB_FP16ScribbleLine4") && size == 2)
        {
            if (widgetType == "FB_FP8ScribbleLine1")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, atoi(tokenLines[i][1].c_str()), 0x00));
            else if (widgetType == "FB_FP8ScribbleLine2")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, atoi(tokenLines[i][1].c_str()), 0x01));
            else if (widgetType == "FB_FP8ScribbleLine3")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, atoi(tokenLines[i][1].c_str()), 0x02));
            else if (widgetType == "FB_FP8ScribbleLine4")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, atoi(tokenLines[i][1].c_str()), 0x03));

            else if (widgetType == "FB_FP16ScribbleLine1")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x16, atoi(tokenLines[i][1].c_str()), 0x00));
            else if (widgetType == "FB_FP16ScribbleLine2")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x16, atoi(tokenLines[i][1].c_str()), 0x01));
            else if (widgetType == "FB_FP16ScribbleLine3")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x16, atoi(tokenLines[i][1].c_str()), 0x02));
            else if (widgetType == "FB_FP16ScribbleLine4")
                widget->GetFeedbackProcessors().push_back(make_unique<FPDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0x16, atoi(tokenLines[i][1].c_str()), 0x03));
        }
        else if ((widgetType == "FB_FP8ScribbleStripMode" || widgetType == "FB_FP16ScribbleStripMode") && size == 2)
        {
            if (widgetType == "FB_FP8ScribbleStripMode")
                widget->GetFeedbackProcessors().push_back(make_unique<FPScribbleStripMode_Midi_FeedbackProcessor>(csi_, this, widget, 0x02, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_FP16ScribbleStripMode")
                widget->GetFeedbackProcessors().push_back(make_unique<FPScribbleStripMode_Midi_FeedbackProcessor>(csi_, this, widget, 0x16, atoi(tokenLines[i][1].c_str())));
        }
        else if ((widgetType == "FB_QConLiteDisplayUpper" || widgetType == "FB_QConLiteDisplayUpperMid" || widgetType == "FB_QConLiteDisplayLowerMid" || widgetType == "FB_QConLiteDisplayLower") && size == 2)
        {
            if (widgetType == "FB_QConLiteDisplayUpper")
                widget->GetFeedbackProcessors().push_back(make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 0, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_QConLiteDisplayUpperMid")
                widget->GetFeedbackProcessors().push_back(make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 1, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_QConLiteDisplayLowerMid")
                widget->GetFeedbackProcessors().push_back(make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 2, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
            else if (widgetType == "FB_QConLiteDisplayLower")
                widget->GetFeedbackProcessors().push_back(make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(csi_, this, widget, 3, 0x14, 0x12, atoi(tokenLines[i][1].c_str())));
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void OSC_ControlSurface::ProcessOSCWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &in_tokens)
{
    if (in_tokens.size() < 2)
        return;
    
    AddWidget(this, in_tokens[1].c_str());

    Widget *widget = GetWidgetByName(in_tokens[1]);
    
    if (widget == NULL)
    {
        LogToConsole(2048, "[ERROR] FAILED to ProcessOSCWidget: widget not found by name %s. Line %s\n", in_tokens[1].c_str(), lineNumber);
        return;
    }

    vector<vector<string>> tokenLines;

    for (string line; getline(surfaceTemplateFile, line) ; )
    {
        lineNumber++;
        
        if (line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
            continue;
        
        vector<string> tokens;
        GetTokens(tokens, line);

        if (tokens[0] == "WidgetEnd")    // Widget list complete
            break;
        
        tokenLines.push_back(tokens);
    }

    for (int i = 0; i < (int)tokenLines.size(); ++i)
    {
        if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Control")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "AnyPress")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<AnyPress_CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "Touch")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<Touch_CSIMessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32Fader")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<X32_Fader_OSC_MessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "X32RotaryToEncoder")
            CSIMessageGeneratorsByMessage_.insert(make_pair(tokenLines[i][1], make_unique<X32_RotaryToEncoder_OSC_MessageGenerator>(csi_, widget)));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_Processor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_FeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_IntProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_IntFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32Processor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32FeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32IntProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32IntFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32FaderProcessor")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32FaderFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_X32RotaryToEncoder")
            widget->GetFeedbackProcessors().push_back(make_unique<OSC_X32_RotaryToEncoderFeedbackProcessor>(csi_, this, widget, tokenLines[i][1]));
    }
}

//////////////////////////////////////////////////////////////////////////////
// ControlSurface
//////////////////////////////////////////////////////////////////////////////
void ControlSurface::ProcessValues(const vector<vector<string>> &lines)
{
    bool inStepSizes = false;
    bool inAccelerationValues = false;
        
    for (int i = 0; i < (int)lines.size(); ++i)
    {
        if (lines[i].size() > 0)
        {
            if (lines[i][0] == "StepSize")
            {
                inStepSizes = true;
                continue;
            }
            else if (lines[i][0] == "StepSizeEnd")
            {
                inStepSizes = false;
                continue;
            }
            else if (lines[i][0] == "AccelerationValues")
            {
                inAccelerationValues = true;
                continue;
            }
            else if (lines[i][0] == "AccelerationValuesEnd")
            {
                inAccelerationValues = false;
                continue;
            }

            if (lines[i].size() > 1)
            {
                const string &widgetClass = lines[i][0];
                
                if (inStepSizes)
                    stepSize_[widgetClass] = atof(lines[i][1].c_str());
                else if (lines[i].size() > 2 && inAccelerationValues)
                {
                    
                    if (lines[i][1] == "Dec")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForDecrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Inc")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForIncrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Val")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValues_[widgetClass].push_back(atof(lines[i][j].c_str()));
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurface::ProcessMIDIWidgetFile(const string &filePath, Midi_ControlSurface *surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    stepSize_.clear();
    accelerationValuesForDecrement_.clear();
    accelerationValuesForIncrement_.clear();
    accelerationValues_.clear();

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] ProcessMIDIWidgetFile: %s\n", GetRelativePath(filePath.c_str()));
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if (line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line.c_str());

            if (tokens.size() > 0 && tokens[0] != "Widget")
                valueLines.push_back(tokens);
            
            if (tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                ProcessValues(valueLines);

            if (tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessMidiWidget(lineNumber, file, tokens);
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole(256, "[ERROR] FAILED to ProcessMIDIWidgetFile in %s, around line %d\n", filePath.c_str(), lineNumber);
        LogToConsole(2048, "Exception: %s\n", e.what());
    }
}

//////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
//////////////////////////////////////////////////////////////////////////////
void OSC_ControlSurface::ProcessOSCWidgetFile(const string &filePath)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    stepSize_.clear();
    accelerationValuesForDecrement_.clear();
    accelerationValuesForIncrement_.clear();
    accelerationValues_.clear();

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] ProcessOSCWidgetFile: %s\n", filePath.c_str());
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if (line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if (tokens.size() > 0 && tokens[0] != "Widget")
                valueLines.push_back(tokens);
            
            if (tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                ProcessValues(valueLines);

            if (tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessOSCWidget(lineNumber, file, tokens);
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole(256, "[ERROR] FAILED to ProcessOSCWidgetFile in %s, around line %d\n", filePath.c_str(), lineNumber);
        LogToConsole(2048, "Exception: %s\n", e.what());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSurfIntegrator
////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSurfIntegrator::InitActionsDictionary()
{
    actions_.insert(make_pair("Speak", make_unique<SpeakOSARAMessage>()));
    actions_.insert(make_pair("SendMIDIMessage", make_unique<SendMIDIMessage>()));
    actions_.insert(make_pair("SendOSCMessage", make_unique<SendOSCMessage>()));
    actions_.insert(make_pair("SaveProject", make_unique<SaveProject>()));
    actions_.insert(make_pair("Undo", make_unique<Undo>()));
    actions_.insert(make_pair("Redo", make_unique<Redo>()));
    actions_.insert(make_pair("TrackAutoMode", make_unique<TrackAutoMode>()));
    actions_.insert(make_pair("GlobalAutoMode", make_unique<GlobalAutoMode>()));
    actions_.insert(make_pair("TrackAutoModeDisplay", make_unique<TrackAutoModeDisplay>()));
    actions_.insert(make_pair("GlobalAutoModeDisplay", make_unique<GlobalAutoModeDisplay>()));
    actions_.insert(make_pair("CycleTrackInputMonitor", make_unique<CycleTrackInputMonitor>()));
    actions_.insert(make_pair("TrackInputMonitorDisplay", make_unique<TrackInputMonitorDisplay>()));
    actions_.insert(make_pair("MCUTimeDisplay", make_unique<MCUTimeDisplay>()));
    actions_.insert(make_pair("OSCTimeDisplay", make_unique<OSCTimeDisplay>()));
    actions_.insert(make_pair("NoAction", make_unique<NoAction>()));
    actions_.insert(make_pair("Reaper", make_unique<ReaperAction>()));
    actions_.insert(make_pair("FixedTextDisplay", make_unique<FixedTextDisplay>()));
    actions_.insert(make_pair("FixedRGBColorDisplay", make_unique<FixedRGBColorDisplay>()));
    actions_.insert(make_pair("MoveEditCursor", make_unique<MoveCursor>()));
    actions_.insert(make_pair("Rewind", make_unique<Rewind>()));
    actions_.insert(make_pair("FastForward", make_unique<FastForward>()));
    actions_.insert(make_pair("Play", make_unique<Play>()));
    actions_.insert(make_pair("Stop", make_unique<Stop>()));
    actions_.insert(make_pair("Record", make_unique<Record>()));
    actions_.insert(make_pair("CycleTimeline", make_unique<CycleTimeline>()));
    actions_.insert(make_pair("ToggleSynchPageBanking", make_unique<ToggleSynchPageBanking>()));
    actions_.insert(make_pair("ToggleFollowMCP", make_unique<ToggleFollowMCP>()));
    actions_.insert(make_pair("ToggleScrollLink", make_unique<ToggleScrollLink>()));
    actions_.insert(make_pair("ToggleRestrictTextLength", make_unique<ToggleRestrictTextLength>()));
    actions_.insert(make_pair("CSINameDisplay", make_unique<CSINameDisplay>()));
    actions_.insert(make_pair("CSIVersionDisplay", make_unique<CSIVersionDisplay>()));
    actions_.insert(make_pair("GlobalModeDisplay", make_unique<GlobalModeDisplay>()));
    actions_.insert(make_pair("CycleTimeDisplayModes", make_unique<CycleTimeDisplayModes>()));
    actions_.insert(make_pair("NextPage", make_unique<GoNextPage>()));
    actions_.insert(make_pair("GoPage", make_unique<GoPage>()));
    actions_.insert(make_pair("PageNameDisplay", make_unique<PageNameDisplay>()));
    actions_.insert(make_pair("GoHome", make_unique<GoHome>()));
    actions_.insert(make_pair("AllSurfacesGoHome", make_unique<AllSurfacesGoHome>()));
    actions_.insert(make_pair("GoSubZone", make_unique<GoSubZone>()));
    actions_.insert(make_pair("LeaveSubZone", make_unique<LeaveSubZone>()));
    actions_.insert(make_pair("SetXTouchDisplayColors", make_unique<SetXTouchDisplayColors>()));
    actions_.insert(make_pair("RestoreXTouchDisplayColors", make_unique<RestoreXTouchDisplayColors>()));
    actions_.insert(make_pair("GoFXSlot", make_unique<GoFXSlot>()));
    actions_.insert(make_pair("ShowFXSlot", make_unique<ShowFXSlot>()));
    actions_.insert(make_pair("HideFXSlot", make_unique<HideFXSlot>()));
    actions_.insert(make_pair("ToggleUseLocalModifiers", make_unique<ToggleUseLocalModifiers>()));
    actions_.insert(make_pair("ToggleUseLocalFXSlot", make_unique<ToggleUseLocalFXSlot>()));
    actions_.insert(make_pair("SetLatchTime", make_unique<SetLatchTime>()));
    actions_.insert(make_pair("SetHoldTime", make_unique<SetHoldTime>()));
    actions_.insert(make_pair("SetDoublePressTime", make_unique<SetDoublePressTime>()));
    actions_.insert(make_pair("ToggleEnableFocusedFXMapping", make_unique<ToggleEnableFocusedFXMapping>()));
    actions_.insert(make_pair("DisableFocusedFXMapping", make_unique<DisableFocusedFXMapping>()));
    actions_.insert(make_pair("ToggleEnableLastTouchedFXParamMapping", make_unique<ToggleEnableLastTouchedFXParamMapping>()));
    actions_.insert(make_pair("DisableLastTouchedFXParamMapping", make_unique<DisableLastTouchedFXParamMapping>()));
    actions_.insert(make_pair("LearnFocusedFX", make_unique<LearnFocusedFX>()));
    actions_.insert(make_pair("GoZone", make_unique<GoZone>()));
    actions_.insert(make_pair("ClearLastTouchedFXParam", make_unique<ClearLastTouchedFXParam>()));
    actions_.insert(make_pair("ClearFocusedFX", make_unique<ClearFocusedFX>()));
    actions_.insert(make_pair("ClearSelectedTrackFX", make_unique<ClearSelectedTrackFX>()));
    actions_.insert(make_pair("ClearFXSlot", make_unique<ClearFXSlot>()));
    actions_.insert(make_pair("Bank", make_unique<Bank>()));
    actions_.insert(make_pair("Shift", make_unique<SetShift>()));
    actions_.insert(make_pair("Option", make_unique<SetOption>()));
    actions_.insert(make_pair("Control", make_unique<SetControl>()));
    actions_.insert(make_pair("Alt", make_unique<SetAlt>()));
    actions_.insert(make_pair("Flip", make_unique<SetFlip>()));
    actions_.insert(make_pair("Global", make_unique<SetGlobal>()));
    actions_.insert(make_pair("Marker", make_unique<SetMarker>()));
    actions_.insert(make_pair("Nudge", make_unique<SetNudge>()));
    actions_.insert(make_pair("Zoom", make_unique<SetZoom>()));
    actions_.insert(make_pair("Scrub", make_unique<SetScrub>()));
    actions_.insert(make_pair("ClearModifier", make_unique<ClearModifier>()));
    actions_.insert(make_pair("ClearModifiers", make_unique<ClearModifiers>()));
    actions_.insert(make_pair("ToggleChannel", make_unique<SetToggleChannel>()));
    actions_.insert(make_pair("CycleTrackAutoMode", make_unique<CycleTrackAutoMode>()));
    actions_.insert(make_pair("TrackVolume", make_unique<TrackVolume>()));
    actions_.insert(make_pair("SoftTakeover7BitTrackVolume", make_unique<SoftTakeover7BitTrackVolume>()));
    actions_.insert(make_pair("SoftTakeover14BitTrackVolume", make_unique<SoftTakeover14BitTrackVolume>()));
    actions_.insert(make_pair("TrackVolumeDB", make_unique<TrackVolumeDB>()));
    actions_.insert(make_pair("TrackToggleVCASpill", make_unique<TrackToggleVCASpill>()));
    actions_.insert(make_pair("TrackVCALeaderDisplay", make_unique<TrackVCALeaderDisplay>()));
    actions_.insert(make_pair("TrackToggleFolderSpill", make_unique<TrackToggleFolderSpill>()));
    actions_.insert(make_pair("TrackFolderParentDisplay", make_unique<TrackFolderParentDisplay>()));
    actions_.insert(make_pair("ToggleFolderView", make_unique<ToggleFolderView>()));
    actions_.insert(make_pair("TrackEnterFolder", make_unique<TrackEnterFolder>()));
    actions_.insert(make_pair("ExitCurrentFolder", make_unique<ExitCurrentFolder>()));
    actions_.insert(make_pair("TrackSelect", make_unique<TrackSelect>()));
    actions_.insert(make_pair("TrackUniqueSelect", make_unique<TrackUniqueSelect>()));
    actions_.insert(make_pair("TrackRangeSelect", make_unique<TrackRangeSelect>()));
    actions_.insert(make_pair("TrackRecordArm", make_unique<TrackRecordArm>()));
    actions_.insert(make_pair("TrackRecordArmDisplay", make_unique<TrackRecordArmDisplay>()));
    actions_.insert(make_pair("TrackMute", make_unique<TrackMute>()));
    actions_.insert(make_pair("TrackSolo", make_unique<TrackSolo>()));
    actions_.insert(make_pair("ClearAllSolo", make_unique<ClearAllSolo>()));
    actions_.insert(make_pair("TrackInvertPolarity", make_unique<TrackInvertPolarity>()));
    actions_.insert(make_pair("TrackInvertPolarityDisplay", make_unique<TrackInvertPolarityDisplay>()));
    actions_.insert(make_pair("TrackPan", make_unique<TrackPan>()));
    actions_.insert(make_pair("TrackPanPercent", make_unique<TrackPanPercent>()));
    actions_.insert(make_pair("TrackPanWidth", make_unique<TrackPanWidth>()));
    actions_.insert(make_pair("TrackPanWidthPercent", make_unique<TrackPanWidthPercent>()));
    actions_.insert(make_pair("TrackPanL", make_unique<TrackPanL>()));
    actions_.insert(make_pair("TrackPanLPercent", make_unique<TrackPanLPercent>()));
    actions_.insert(make_pair("TrackPanR", make_unique<TrackPanR>()));
    actions_.insert(make_pair("TrackPanRPercent", make_unique<TrackPanRPercent>()));
    actions_.insert(make_pair("TrackPanAutoLeft", make_unique<TrackPanAutoLeft>()));
    actions_.insert(make_pair("TrackPanAutoRight", make_unique<TrackPanAutoRight>()));
    actions_.insert(make_pair("TrackNameDisplay", make_unique<TrackNameDisplay>()));
    actions_.insert(make_pair("TrackNumberDisplay", make_unique<TrackNumberDisplay>()));
    actions_.insert(make_pair("TrackRecordInputDisplay", make_unique<TrackRecordInputDisplay>()));
    actions_.insert(make_pair("TrackVolumeDisplay", make_unique<TrackVolumeDisplay>()));
    actions_.insert(make_pair("TrackPanDisplay", make_unique<TrackPanDisplay>()));
    actions_.insert(make_pair("TrackPanWidthDisplay", make_unique<TrackPanWidthDisplay>()));
    actions_.insert(make_pair("TrackPanLeftDisplay", make_unique<TrackPanLeftDisplay>()));
    actions_.insert(make_pair("TrackPanRightDisplay", make_unique<TrackPanRightDisplay>()));
    actions_.insert(make_pair("TrackPanAutoLeftDisplay", make_unique<TrackPanAutoLeftDisplay>()));
    actions_.insert(make_pair("TrackPanAutoRightDisplay", make_unique<TrackPanAutoRightDisplay>()));
    actions_.insert(make_pair("TrackOutputMeter", make_unique<TrackOutputMeter>()));
    actions_.insert(make_pair("TrackOutputMeterAverageLR", make_unique<TrackOutputMeterAverageLR>()));
    actions_.insert(make_pair("TrackVolumeWithMeterAverageLR", make_unique<TrackVolumeWithMeterAverageLR>()));
    actions_.insert(make_pair("TrackOutputMeterMaxPeakLR", make_unique<TrackOutputMeterMaxPeakLR>()));
    actions_.insert(make_pair("TrackVolumeWithMeterMaxPeakLR", make_unique<TrackVolumeWithMeterMaxPeakLR>()));
    actions_.insert(make_pair("LastTouchedFXParam", make_unique<LastTouchedFXParam>()));
    actions_.insert(make_pair("FXParam", make_unique<FXParam>()));
    actions_.insert(make_pair("JSFXParam", make_unique<JSFXParam>()));
    actions_.insert(make_pair("TCPFXParam", make_unique<TCPFXParam>()));
    actions_.insert(make_pair("ToggleFXBypass", make_unique<ToggleFXBypass>()));
    actions_.insert(make_pair("FXBypassDisplay", make_unique<FXBypassDisplay>()));
    actions_.insert(make_pair("ToggleFXOffline", make_unique<ToggleFXOffline>()));
    actions_.insert(make_pair("FXOfflineDisplay", make_unique<FXOfflineDisplay>()));
    actions_.insert(make_pair("FXNameDisplay", make_unique<FXNameDisplay>()));
    actions_.insert(make_pair("FXMenuNameDisplay", make_unique<FXMenuNameDisplay>()));
    actions_.insert(make_pair("SpeakFXMenuName", make_unique<SpeakFXMenuName>()));
    actions_.insert(make_pair("FXParamNameDisplay", make_unique<FXParamNameDisplay>()));
    actions_.insert(make_pair("TCPFXParamNameDisplay", make_unique<TCPFXParamNameDisplay>()));
    actions_.insert(make_pair("FXParamValueDisplay", make_unique<FXParamValueDisplay>()));
    actions_.insert(make_pair("TCPFXParamValueDisplay", make_unique<TCPFXParamValueDisplay>()));
    actions_.insert(make_pair("LastTouchedFXParamNameDisplay", make_unique<LastTouchedFXParamNameDisplay>()));
    actions_.insert(make_pair("LastTouchedFXParamValueDisplay", make_unique<LastTouchedFXParamValueDisplay>()));
    actions_.insert(make_pair("FXGainReductionMeter", make_unique<FXGainReductionMeter>()));
    actions_.insert(make_pair("TrackSendVolume", make_unique<TrackSendVolume>()));
    actions_.insert(make_pair("TrackSendVolumeDB", make_unique<TrackSendVolumeDB>()));
    actions_.insert(make_pair("TrackSendPan", make_unique<TrackSendPan>()));
    actions_.insert(make_pair("TrackSendPanPercent", make_unique<TrackSendPanPercent>()));
    actions_.insert(make_pair("TrackSendMute", make_unique<TrackSendMute>()));
    actions_.insert(make_pair("TrackSendInvertPolarity", make_unique<TrackSendInvertPolarity>()));
    actions_.insert(make_pair("TrackSendStereoMonoToggle", make_unique<TrackSendStereoMonoToggle>()));
    actions_.insert(make_pair("TrackSendPrePost", make_unique<TrackSendPrePost>()));
    actions_.insert(make_pair("TrackSendNameDisplay", make_unique<TrackSendNameDisplay>()));
    actions_.insert(make_pair("SpeakTrackSendDestination", make_unique<SpeakTrackSendDestination>()));
    actions_.insert(make_pair("TrackSendVolumeDisplay", make_unique<TrackSendVolumeDisplay>()));
    actions_.insert(make_pair("TrackSendPanDisplay", make_unique<TrackSendPanDisplay>()));
    actions_.insert(make_pair("TrackSendStereoMonoDisplay", make_unique<TrackSendStereoMonoDisplay>()));
    actions_.insert(make_pair("TrackSendPrePostDisplay", make_unique<TrackSendPrePostDisplay>()));
    actions_.insert(make_pair("TrackReceiveVolume", make_unique<TrackReceiveVolume>()));
    actions_.insert(make_pair("TrackReceiveVolumeDB", make_unique<TrackReceiveVolumeDB>()));
    actions_.insert(make_pair("TrackReceivePan", make_unique<TrackReceivePan>()));
    actions_.insert(make_pair("TrackReceivePanPercent", make_unique<TrackReceivePanPercent>()));
    actions_.insert(make_pair("TrackReceiveMute", make_unique<TrackReceiveMute>()));
    actions_.insert(make_pair("TrackReceiveInvertPolarity", make_unique<TrackReceiveInvertPolarity>()));
    actions_.insert(make_pair("TrackReceiveStereoMonoToggle", make_unique<TrackReceiveStereoMonoToggle>()));
    actions_.insert(make_pair("TrackReceivePrePost", make_unique<TrackReceivePrePost>()));
    actions_.insert(make_pair("TrackReceiveNameDisplay", make_unique<TrackReceiveNameDisplay>()));
    actions_.insert(make_pair("SpeakTrackReceiveSource", make_unique<SpeakTrackReceiveSource>()));
    actions_.insert(make_pair("TrackReceiveVolumeDisplay", make_unique<TrackReceiveVolumeDisplay>()));
    actions_.insert(make_pair("TrackReceivePanDisplay", make_unique<TrackReceivePanDisplay>()));
    actions_.insert(make_pair("TrackReceiveStereoMonoDisplay", make_unique<TrackReceiveStereoMonoDisplay>()));
    actions_.insert(make_pair("TrackReceivePrePostDisplay", make_unique<TrackReceivePrePostDisplay>()));
}

void CSurfIntegrator::Init()
{
    pages_.clear();
    
    string currentBroadcaster;
    
    Page *currentPage = NULL;
    
    string CSIFolderPath = string(GetResourcePath()) + "/CSI";
    
    if ( ! filesystem::exists(CSIFolderPath))
    {
        char tmp[MEDBUF];
        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), CSIFolderPath.c_str());
        MessageBox(g_hwnd, tmp, __LOCALIZE("Missing CSI Folder","csi_mbox"), MB_OK);

        return;
    }
    
    string iniFilePath = string(GetResourcePath()) + "/CSI/CSI.ini";
    
    if ( ! filesystem::exists(iniFilePath))
    {
        char tmp[MEDBUF];
        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), iniFilePath.c_str());
        MessageBox(g_hwnd, tmp, __LOCALIZE("Missing CSI.ini","csi_mbox"), MB_OK);

        return;
    }

    
    int lineNumber = 0;
    
    try
    {
        ifstream iniFile(iniFilePath);
               
        for (string line; getline(iniFile, line) ; )
        {
            TrimLine(line);
            
            if (lineNumber == 0)
            {
                PropertyList pList;
                vector<string> properties;
                properties.push_back(line.c_str());
                GetPropertiesFromTokens(0, 1, properties, pList);

                const char *versionProp = pList.get_prop(PropertyType_Version);
                if (versionProp)
                {
                    if (strcmp(versionProp, s_MajorVersionToken))
                    {
                        char tmp[MEDBUF];
                        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Version mismatch -- Your CSI.ini file is not %s.","csi_mbox"), s_MajorVersionToken);
                        MessageBox(g_hwnd, tmp, __LOCALIZE("CSI.ini version mismatch","csi_mbox"), MB_OK);

                        iniFile.close();
                        return;
                    }
                    else
                    {
                        lineNumber++;
                        continue;
                    }
                }
                else
                {
                    char tmp[MEDBUF];
                    snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Cannot read version %s.","csi_mbox"), "");
                    MessageBox(g_hwnd, tmp, __LOCALIZE("CSI.ini no version","csi_mbox"), MB_OK);

                    iniFile.close();
                    return;
                }
            }
            
            if (line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line.c_str());
            
            if (tokens.size() > 0) // ignore comment lines and blank lines
            {
                PropertyList pList;
                GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);
                
                if (const char *typeProp = pList.get_prop(PropertyType_SurfaceType))
                {
                    if (const char *nameProp = pList.get_prop(PropertyType_SurfaceName))
                    {
                        if (const char *channelCountProp = pList.get_prop(PropertyType_SurfaceChannelCount))
                        {
                            int channelCount = atoi(channelCountProp);
                            
                            if ( ! strcmp(typeProp, s_MidiSurfaceToken) && tokens.size() == 7)
                            {
                                if (pList.get_prop(PropertyType_MidiInput) != NULL &&
                                    pList.get_prop(PropertyType_MidiOutput) != NULL &&
                                    pList.get_prop(PropertyType_MIDISurfaceRefreshRate) != NULL &&
                                    pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun) != NULL)
                                {
                                    int midiIn = atoi(pList.get_prop(PropertyType_MidiInput));
                                    int midiOut = atoi(pList.get_prop(PropertyType_MidiOutput));
                                    int surfaceRefreshRate = atoi(pList.get_prop(PropertyType_MIDISurfaceRefreshRate));
                                    int maxMIDIMesssagesPerRun = atoi(pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun));
                                    
                                    midiSurfacesIO_.push_back(make_unique<Midi_ControlSurfaceIO>(this, nameProp, channelCount, GetMidiInputForPort(midiIn), GetMidiOutputForPort(midiOut), surfaceRefreshRate, maxMIDIMesssagesPerRun));
                                }
                            }
                            else if (( ! strcmp(typeProp, s_OSCSurfaceToken) || ! strcmp(typeProp, s_OSCX32SurfaceToken)) && tokens.size() == 7)
                            {
                                if (pList.get_prop(PropertyType_ReceiveOnPort) != NULL &&
                                    pList.get_prop(PropertyType_TransmitToPort) != NULL &&
                                    pList.get_prop(PropertyType_TransmitToIPAddress) != NULL &&
                                    pList.get_prop(PropertyType_MaxPacketsPerRun) != NULL)
                                {
                                    const char *receiveOnPort = pList.get_prop(PropertyType_ReceiveOnPort);
                                    const char *transmitToPort = pList.get_prop(PropertyType_TransmitToPort);
                                    const char *transmitToIPAddress = pList.get_prop(PropertyType_TransmitToIPAddress);
                                    int maxPacketsPerRun = atoi(pList.get_prop(PropertyType_MaxPacketsPerRun));
                                    
                                    if ( ! strcmp(typeProp, s_OSCSurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                    else if ( ! strcmp(typeProp, s_OSCX32SurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_X32ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                }
                            }
                        }
                    }
                }
                else if (const char *pageNameProp = pList.get_prop(PropertyType_PageName))
                {
                    bool followMCP = true;
                    bool synchPages = true;
                    bool isScrollLinkEnabled = false;
                    bool isScrollSynchEnabled = false;

                    currentPage = NULL;
                    
                    if (tokens.size() > 1)
                    {
                        if (const char *pageFollowsMCPProp = pList.get_prop(PropertyType_PageFollowsMCP))
                        {
                            if ( ! strcmp(pageFollowsMCPProp, "No"))
                                followMCP = false;
                        }
                        
                        if (const char *synchPagesProp = pList.get_prop(PropertyType_SynchPages))
                        {
                            if ( ! strcmp(synchPagesProp, "No"))
                                synchPages = false;
                        }
                        
                        if (const char *scrollLinkProp = pList.get_prop(PropertyType_ScrollLink))
                        {
                            if ( ! strcmp(scrollLinkProp, "Yes"))
                                isScrollLinkEnabled = true;
                        }
                        
                        if (const char *scrollSynchProp = pList.get_prop(PropertyType_ScrollSynch))
                        {
                            if ( ! strcmp(scrollSynchProp, "Yes"))
                                isScrollSynchEnabled = true;
                        }

                        pages_.push_back(make_unique<Page>(this, pageNameProp, followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled));
                        currentPage = pages_.back().get();
                    }
                }
                else if (const char *broadcasterProp = pList.get_prop(PropertyType_Broadcaster))
                {
                    currentBroadcaster = broadcasterProp;
                }
                else if (currentPage && tokens.size() > 2 && currentBroadcaster != "" && pList.get_prop(PropertyType_Listener) != NULL)
                {
                    if (currentPage && tokens.size() > 2 && currentBroadcaster != "")
                    {
                        
                        ControlSurface *broadcaster = NULL;
                        ControlSurface *listener = NULL;
                        
                        const vector<unique_ptr<ControlSurface>> &surfaces = currentPage->GetSurfaces();
                        
                        string currentSurface = string(pList.get_prop(PropertyType_Listener));
                        
                        for (int i = 0; i < surfaces.size(); ++i)
                        {
                            if (surfaces[i]->GetName() == currentBroadcaster)
                                broadcaster = surfaces[i].get();
                            if (surfaces[i]->GetName() == currentSurface)
                                 listener = surfaces[i].get();
                        }
                        
                        if (broadcaster != NULL && listener != NULL)
                        {
                            broadcaster->GetZoneManager()->AddListener(listener);
                            listener->GetZoneManager()->SetListenerCategories(pList);
                        }
                    }
                }
                else if (currentPage && tokens.size() == 5)
                {
                    if (const char *surfaceProp = pList.get_prop(PropertyType_Surface))
                    {
                        if (const char *surfaceFolderProp = pList.get_prop(PropertyType_SurfaceFolder))
                        {
                            if (const char *startChannelProp = pList.get_prop(PropertyType_StartChannel))
                            {
                                int startChannel = atoi(startChannelProp);
                                
                                string baseDir = string(GetResourcePath()) + string("/CSI/Surfaces/");
                                     
                                if ( ! filesystem::exists(baseDir))
                                {
                                    char tmp[MEDBUF];
                                    snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), baseDir.c_str());
                                    MessageBox(g_hwnd, tmp, __LOCALIZE("Missing Surfaces Folder","csi_mbox"), MB_OK);

                                    return;
                                }
                                
                                string surfaceFile = baseDir + surfaceFolderProp + "/Surface.txt";
                                
                                if ( ! filesystem::exists(surfaceFile))
                                {
                                    char tmp[MEDBUF];
                                    snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), surfaceFile.c_str());
                                    MessageBox(g_hwnd, tmp, __LOCALIZE("Missing Surface File","csi_mbox"), MB_OK);

                                    return;
                                }
                                
                                string zoneFolder = baseDir + surfaceFolderProp + "/Zones";
                                if (const char *zoneFolderProp = pList.get_prop(PropertyType_ZoneFolder))
                                    zoneFolder = baseDir + zoneFolderProp + "/Zones";
                                
                                if ( ! filesystem::exists(zoneFolder))
                                {
                                    char tmp[MEDBUF];
                                    snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), zoneFolder.c_str());
                                    MessageBox(g_hwnd, tmp, __LOCALIZE("Missing Zone Folder","csi_mbox"), MB_OK);

                                    return;
                                }
                                
                                string fxZoneFolder = baseDir + surfaceFolderProp + "/FXZones";
                                if (const char *fxZoneFolderProp = pList.get_prop(PropertyType_FXZoneFolder))
                                    fxZoneFolder = baseDir + fxZoneFolderProp + "/FXZones";

                                if ( ! filesystem::exists(fxZoneFolder))
                                {
                                    try
                                    {
                                        RecursiveCreateDirectory(fxZoneFolder.c_str(), 0);
                                    }
                                    catch (const std::exception& e)
                                    {
                                        LogToConsole(256, "[ERROR] FAILED to Init. Unable to create folder %s\n", fxZoneFolder.c_str());
                                        LogToConsole(2048, "Exception: %s\n", e.what());

                                        char tmp[MEDBUF];
                                        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find %s", "csi_mbox"), fxZoneFolder.c_str());
                                        MessageBox(g_hwnd, tmp, __LOCALIZE("Missing FX Zone Folder","csi_mbox"), MB_OK);

                                        return;
                                    }

                                }

                                bool foundIt = false;
                                
                                for (auto &io : midiSurfacesIO_)
                                {
                                    if ( ! strcmp(surfaceProp, io->GetName()))
                                    {
                                        foundIt = true;
                                        currentPage->GetSurfaces().push_back(make_unique<Midi_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                        break;
                                    }
                                }
                                
                                if ( ! foundIt)
                                {
                                    for (auto &io : oscSurfacesIO_)
                                    {
                                        if ( ! strcmp(surfaceProp, io->GetName()))
                                        {
                                            foundIt = true;
                                            currentPage->GetSurfaces().push_back(make_unique<OSC_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            lineNumber++;
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole(256, "[ERROR] FAILED to Init in %s, around line %d\n", iniFilePath.c_str(), lineNumber);
        LogToConsole(2048, "Exception: %s\n", e.what());
    }
    
    if (pages_.size() == 0)
        pages_.push_back(make_unique<Page>(this, "Home", false, false, false, false));
    
    for (auto &page : pages_)
    {
        for (auto &surface : page->GetSurfaces())
            surface->ForceClear();

        page->OnInitialization();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *TrackNavigator::GetTrack()
{
    return trackNavigationManager_->GetTrackFromChannel(channelNum_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *MasterTrackNavigator::GetTrack()
{
    return GetMasterTrack(NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectedTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *SelectedTrackNavigator::GetTrack()
{
    return page_->GetSelectedTrack();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// FocusedFXNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack *FocusedFXNavigator::GetTrack()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int takeNumber = 0;
    int fxIndex = 0;
    int paramIndex = 0;
    
    int retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxIndex, &paramIndex);

    trackNumber++;
    
    if (retVal && ! (paramIndex & 0x01)) // Track FX
    {
        if (trackNumber > 0)
            return DAW::GetTrack(trackNumber);
        else if (trackNumber == 0)
            return GetMasterTrack(NULL);
        else
            return NULL;
    }
    else
        return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ActionContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ActionContext::ActionContext(CSurfIntegrator *const csi, Action *action, Widget *widget, Zone *zone, int paramIndex, const vector<string> &paramsAndProperties) : csi_(csi), action_(action), widget_(widget), zone_(zone), paramIndex_(paramIndex)
{
    vector<string> params_wr;
    const vector<string> &params = params_wr;
    
    for (int i = 0; i < (int)(paramsAndProperties).size(); ++i)
    {
        if ((paramsAndProperties)[i].find("=") == string::npos)
            params_wr.push_back((paramsAndProperties)[i]);
    }
    GetPropertiesFromTokens(0, (int)(paramsAndProperties).size(), paramsAndProperties, widgetProperties_);

    
    const char* feedback = widgetProperties_.get_prop(PropertyType_Feedback);
    if (feedback && !strcmp(feedback, "No"))
        provideFeedback_ = false;

    const char* holdDelay = widgetProperties_.get_prop(PropertyType_HoldDelay);
    if (holdDelay)
        holdDelayMs_ = atoi(holdDelay);

    const char* holdRepeatInterval = widgetProperties_.get_prop(PropertyType_HoldRepeatInterval);
    if (holdRepeatInterval)
        holdRepeatIntervalMs_ = atoi(holdRepeatInterval);

    const char* runCount = widgetProperties_.get_prop(PropertyType_RunCount);
    if (runCount)
        runCount_ = atoi(runCount);
    if (runCount_ < 1) runCount_ = 1;

    const char * meterMode  = widgetProperties_.get_prop(PropertyType_MeterMode);
    if (meterMode  &&  strlen(meterMode) > 0)
        strncpy(meterMode_, meterMode, sizeof(meterMode_) - 1);

    for (int i = 0; i < (int)(paramsAndProperties).size(); ++i)
        if (paramsAndProperties[i] == "NoFeedback")
            provideFeedback_ = false;

    string actionName = "";
    
    if (params.size() > 0)
        actionName = params[0];
    
    // Action with int param, could include leading minus sign
    if (params.size() > 1 && (isdigit(params[1][0]) ||  params[1][0] == '-'))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        intParam_= atol(params[1].c_str());
    }
    
    if (actionName == "Bank" && (params.size() > 2 && (isdigit(params[2][0]) ||  params[2][0] == '-')))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        stringParam_ = params[1];
        intParam_= atol(params[2].c_str());
    }
        
    // Action with param index, must be positive
    if (params.size() > 1 && isdigit(params[1][0]))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    // Action with string param
    if (params.size() > 1)
        stringParam_ = params[1];
    
    if (actionName == "TrackVolumeDB" || actionName == "TrackSendVolumeDB")
    {
        rangeMinimum_ = -144.0;
        rangeMaximum_ = 24.0;
    }
    
    if (actionName == "TrackPanPercent" ||
        actionName == "TrackPanWidthPercent" ||
        actionName == "TrackPanLPercent" ||
        actionName == "TrackPanRPercent")
    {
        rangeMinimum_ = -100.0;
        rangeMaximum_ = 100.0;
    }
   
    if ((actionName == "Reaper" ||
         actionName == "ReaperDec" ||
         actionName == "ReaperInc") && params.size() > 1)
    {
        if (isdigit(params[1][0]))
        {
            commandId_ =  atol(params[1].c_str());
        }
        else // look up by string
        {
            commandId_ = NamedCommandLookup(params[1].c_str());
            
            if (commandId_ == 0) // can't find it
                commandId_ = 65535; // no-op
        }
    }
        
    if ((actionName == "FXParam" || actionName == "JSFXParam") &&
          params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if (actionName == "FXParamValueDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if (actionName == "FXParamNameDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
        
        if (params.size() > 2 && params[2] != "{" && params[2] != "[")
               fxParamDisplayName_ = params[2];
    }
    
    if (actionName == "FixedTextDisplay" && (params.size() > 2 && (isdigit(params[2][0]))))  // C++ 2003 says empty strings can be queried without catastrophe :)
    {
        stringParam_ = params[1];
        paramIndex_= atol(params[2].c_str());
    }
    
    if (params.size() > 0)
        SetColor(params, supportsColor_, supportsTrackColor_, colorValues_);
    
    GetSteppedValues(widget, action_, zone_, paramIndex_, params, widgetProperties_, deltaValue_, acceleratedDeltaValues_, rangeMinimum_, rangeMaximum_, steppedValues_, acceleratedTickValues_);

    if (acceleratedTickValues_.size() < 1)
        acceleratedTickValues_.push_back(0);
}

Page *ActionContext::GetPage()
{
    return widget_->GetSurface()->GetPage();
}

ControlSurface *ActionContext::GetSurface()
{
    return widget_->GetSurface();
}

MediaTrack *ActionContext::GetTrack()
{
    return zone_->GetNavigator()->GetTrack();
}

int ActionContext::GetSlotIndex()
{
    return zone_->GetSlotIndex();
}

const char *ActionContext::GetName()
{
    return zone_->GetAlias();
}

void ActionContext::RequestUpdate()
{
    if (provideFeedback_)
        action_->RequestUpdate(this);
}

void ActionContext::ClearWidget()
{
    UpdateWidgetValue(0.0);
    UpdateWidgetValue("");
}

void ActionContext::UpdateColorValue(double value)
{
    if (supportsColor_)
    {
        currentColorIndex_ = value == 0 ? 0 : 1;
        if (colorValues_.size() > currentColorIndex_)
            widget_->UpdateColorValue(colorValues_[currentColorIndex_]);
    }
}

void ActionContext::UpdateWidgetValue(double value)
{
    if (steppedValues_.size() > 0)
        SetSteppedValueIndex(value);

    value = isFeedbackInverted_ == false ? value : 1.0 - value;

    widget_->UpdateValue(widgetProperties_, value);

    UpdateColorValue(value);

    if (supportsTrackColor_)
        UpdateTrackColor();
}

void ActionContext::UpdateJSFXWidgetSteppedValue(double value)
{
    if (steppedValues_.size() > 0)
        SetSteppedValueIndex(value);
}

void ActionContext::UpdateTrackColor()
{
    if (MediaTrack* track = zone_->GetNavigator()->GetTrack())
    {
        rgba_color color = DAW::GetTrackColor(track);
        widget_->UpdateColorValue(color);
    }
}

void ActionContext::UpdateWidgetValue(const char* value)
{
    widget_->UpdateValue(widgetProperties_, value ? value : "");
}

void ActionContext::ForceWidgetValue(const char* value)
{
    widget_->ForceValue(widgetProperties_, value ? value : "");
}

void ActionContext::LogAction(double value)
{
    if (g_debugLevel >= DEBUG_LEVEL_INFO)
    {
        std::ostringstream oss;
        if (supportsColor_) {
            oss << " { ";
            for (size_t i = 0; i < colorValues_.size(); ++i) {
                oss << " " << colorValues_[i].r << " " << colorValues_[i].g << " " << colorValues_[i].b;
                if (i != colorValues_.size() - 1) oss << ", ";
            }
            oss << " }[" << currentColorIndex_ << "]";
        }
        if (!provideFeedback_) oss << " FeedBack=No";
        if (isValueInverted_) oss << " Invert";
        if (isFeedbackInverted_) oss << " InvertFB";
        if (holdDelayMs_ > 0) oss << " HoldDelay=" << holdDelayMs_;
        if (holdRepeatIntervalMs_ > 0) oss << " HoldRepeatInterval=" << holdRepeatIntervalMs_;

        LogToConsole(512, "[INFO] @%s/%s: [%s] %s(%s) # %s; val:%0.2f ctx:%s%s\n"
            ,this->GetSurface()->GetName()
            ,this->GetZone()->GetName()
            ,this->GetWidget()->GetName()
            ,this->GetAction()->GetName()
            ,this->GetStringParam()
            ,(this->GetCommandId() > 0) ? DAW::GetCommandName(this->GetCommandId()) : ""
            ,value
            ,this->GetName()
            ,oss.str().c_str()
        );
    }
}

// runs once button pressed/released
void ActionContext::DoAction(double value)
{
    DWORD nowTs = GetTickCount();
    int holdDelayMs = holdDelayMs_ == HOLD_DELAY_INHERIT_VALUE ? this->GetSurface()->GetHoldTime() : holdDelayMs_;
    deferredValue_ = value;
    
    if ((isDoublePress_ || GetWidget()->HasDoublePressActions()) && value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        if (doublePressStartTs_ == 0 || nowTs > doublePressStartTs_ + GetSurface()->GetDoublePressTime()) {
            doublePressStartTs_ = nowTs;
            if (isDoublePress_) return; // throttle normal press
        } else {
            doublePressStartTs_ = 0;
            if (!isDoublePress_ && holdDelayMs == 0) return; // block normal press inside double-press window
        }
    }

    if (holdRepeatIntervalMs_ > 0) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            holdRepeatActive_ = false;
        } else {
            if (holdDelayMs == 0) {
                holdRepeatActive_ = true;
                lastHoldRepeatTs_ = nowTs;
            }
        }
    }
    if (holdDelayMs > 0) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            holdActive_ = false;
        } else {
            holdActive_ = true;
            lastHoldStartTs_ = nowTs;
        }
    } else {
        PerformAction(value);
    }
}

// runs in loop to support button hold/repeat actions
void ActionContext::RunDeferredActions()
{
    int holdDelayMs = holdDelayMs_ == HOLD_DELAY_INHERIT_VALUE ? this->GetSurface()->GetHoldTime() : holdDelayMs_;

    if (holdDelayMs > 0
        && holdActive_
        && lastHoldStartTs_ > 0
        && GetTickCount() > (lastHoldStartTs_ + holdDelayMs)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(256, "[DEBUG] HOLD [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - lastHoldStartTs_);
        PerformAction(deferredValue_);
        holdActive_ = false; // to mark that this action with it's defined hold delay was performed and separate it from repeated action trigger
        if (holdRepeatIntervalMs_ > 0) {
            holdRepeatActive_ = true;
            lastHoldRepeatTs_ = GetTickCount();
        }
    }
    if (holdRepeatIntervalMs_ > 0
        && holdRepeatActive_
        && lastHoldRepeatTs_ > 0
        && GetTickCount() > (lastHoldRepeatTs_ + holdRepeatIntervalMs_)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(256, "[DEBUG] REPEAT [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - lastHoldRepeatTs_);
        lastHoldRepeatTs_ = GetTickCount();
        PerformAction(deferredValue_);
    }
}

void ActionContext::PerformAction(double value)
{
    if (!steppedValues_.empty())
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (steppedValuesIndex_ == steppedValues_.size() - 1)
        {
            if (steppedValues_[0] < steppedValues_[steppedValuesIndex_]) // GAW -- only wrap if 1st value is lower
                steppedValuesIndex_ = 0;
        }
        else steppedValuesIndex_++;

        for (int i = 0; i < runCount_; ++i)
            DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else for (int i = 0; i < runCount_; ++i)
        DoRangeBoundAction(value);
}

void ActionContext::DoRelativeAction(double delta)
{
    if (steppedValues_.size() > 0)
        DoSteppedValueAction(delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::DoRelativeAction(int accelerationIndex, double delta)
{
    if (steppedValues_.size() > 0)
        DoAcceleratedSteppedValueAction(accelerationIndex, delta);
    else if (acceleratedDeltaValues_.size() > 0)
        DoAcceleratedDeltaValueAction(accelerationIndex, delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) +  (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::DoRangeBoundAction(double value)
{
    if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE)
        this->LogAction(value);

    if (value > rangeMaximum_)
        value = rangeMaximum_;
    
    if (value < rangeMinimum_)
        value = rangeMinimum_;
    
    if (isValueInverted_)
        value = 1.0 - value;
    
    for (int i = 0; i < runCount_; ++i)
        action_->Do(this, value);
}

void ActionContext::DoSteppedValueAction(double delta)
{
    if (delta > 0)
    {
        steppedValuesIndex_++;
        
        if (steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else
    {
        steppedValuesIndex_--;
        
        if (steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedSteppedValueAction(int accelerationIndex, double delta)
{
    if (delta > 0)
    {
        accumulatedIncTicks_++;
        accumulatedDecTicks_ = accumulatedDecTicks_ - 1 < 0 ? 0 : accumulatedDecTicks_ - 1;
    }
    else if (delta < 0)
    {
        accumulatedDecTicks_++;
        accumulatedIncTicks_ = accumulatedIncTicks_ - 1 < 0 ? 0 : accumulatedIncTicks_ - 1;
    }
    
    accelerationIndex = accelerationIndex > (int)acceleratedTickValues_.size() - 1 ? (int)acceleratedTickValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if (delta > 0 && accumulatedIncTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_++;
        
        if (steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else if (delta < 0 && accumulatedDecTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_--;
        
        if (steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedDeltaValueAction(int accelerationIndex, double delta)
{
    accelerationIndex = accelerationIndex > (int)acceleratedDeltaValues_.size() - 1 ? (int)acceleratedDeltaValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if (delta > 0.0)
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + acceleratedDeltaValues_[accelerationIndex]);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) - acceleratedDeltaValues_[accelerationIndex]);
}

void ActionContext::GetColorValues(vector<rgba_color> &colorValues, const vector<string> &colors)
{
    for (int i = 0; i < (int)colors.size(); ++i)
    {
        rgba_color colorValue;
        
        if (GetColorValue(colors[i].c_str(), colorValue))
            colorValues.push_back(colorValue);
    }
}

void ActionContext::SetColor(const vector<string> &params, bool &supportsColor, bool &supportsTrackColor, vector<rgba_color> &colorValues)
{
    vector<int> rawValues;
    vector<string> hexColors;

    int openCurlyIndex = 0;
    int closeCurlyIndex = 0;
    
    for (int i = 0; i < params.size(); ++i)
        if (params[i] == "{")
        {
            openCurlyIndex = i;
            break;
        }
    
    for (int i = 0; i < params.size(); ++i)
        if (params[i] == "}")
        {
            closeCurlyIndex = i;
            break;
        }
   
    if (openCurlyIndex != 0 && closeCurlyIndex != 0)
    {
        for (int i = openCurlyIndex + 1; i < closeCurlyIndex; ++i)
        {
            const string &strVal = params[i];
            
            if (strVal[0] == '#')
            {
                hexColors.push_back(strVal);
                continue;
            }
            
            if (strVal == "Track")
            {
                supportsTrackColor = true;
                break;
            }
            else if (strVal[0])
            {
                char *ep = NULL;
                const int value = strtol(strVal.c_str(), &ep, 10);
                if (ep && !*ep)
                    rawValues.push_back(wdl_clamp(value, 0, 255));
            }
        }
        
        if (hexColors.size() > 0)
        {
            supportsColor = true;

            GetColorValues(colorValues, hexColors);
        }
        else if (rawValues.size() % 3 == 0 && rawValues.size() > 2)
        {
            supportsColor = true;
            
            for (int i = 0; i < rawValues.size(); i += 3)
            {
                rgba_color color;
                
                color.r = rawValues[i];
                color.g = rawValues[i + 1];
                color.b = rawValues[i + 2];
                
                colorValues.push_back(color);
            }
        }
    }
}

void ActionContext::GetSteppedValues(Widget *widget, Action *action,  Zone *zone, int paramNumber, const vector<string> &params, const PropertyList &widgetProperties, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues)
{
    ::GetSteppedValues(params, 0, deltaValue, acceleratedDeltaValues, rangeMinimum, rangeMaximum, steppedValues, acceleratedTickValues);
    
    if (deltaValue == 0.0 && widget->GetStepSize() != 0.0)
        deltaValue = widget->GetStepSize();
    
    if (acceleratedDeltaValues.size() == 0 && widget->GetAccelerationValues().size() != 0)
        acceleratedDeltaValues = widget->GetAccelerationValues();
         
    if (steppedValues.size() > 0 && acceleratedTickValues.size() == 0)
    {
        double stepSize = deltaValue;
        
        if (stepSize != 0.0)
        {
            stepSize *= 10000.0;

            int stepCount = (int) steppedValues.size();
            int baseTickCount = (NUM_ELEM(s_tickCounts_) > stepCount)
                ? s_tickCounts_[stepCount]
                : s_tickCounts_[NUM_ELEM(s_tickCounts_) - 1]
            ;
            int tickCount = int(baseTickCount / stepSize + 0.5);
            acceleratedTickValues.push_back(tickCount);


        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Zone
///////////////////////////////////////////////////////////////////////////////////////////////////////
void Zone::InitSubZones(const vector<string> &subZones, const char *widgetSuffix)
{
    map<const string, CSIZoneInfo> &zoneInfo = zoneManager_->GetZoneInfo();

    for (int i = 0; i < (int)subZones.size(); ++i)
    {
        if (zoneInfo.find(subZones[i]) != zoneInfo.end())
        {
            subZones_.push_back(make_unique<SubZone>(csi_, zoneManager_, GetNavigator(), GetSlotIndex(), subZones[i], zoneInfo[subZones[i]].alias, zoneInfo[subZones[i]].filePath, this));
            zoneManager_->LoadZoneFile(subZones_.back().get(), widgetSuffix);
        }
    }
}

int Zone::GetSlotIndex()
{
    if (name_ == "TrackSend")
        return zoneManager_->GetTrackSendOffset();
    if (name_ == "TrackReceive")
        return zoneManager_->GetTrackReceiveOffset();
    if (name_ == "TrackFXMenu")
        return zoneManager_->GetTrackFXMenuOffset();
    if (name_ == "SelectedTrack")
        return slotIndex_;
    if (name_ == "SelectedTrackSend")
        return slotIndex_ + zoneManager_->GetSelectedTrackSendOffset();
    if (name_ == "SelectedTrackReceive")
        return slotIndex_ + zoneManager_->GetSelectedTrackReceiveOffset();
    if (name_ == "SelectedTrackFXMenu")
        return slotIndex_ + zoneManager_->GetSelectedTrackFXMenuOffset();
    if (name_ == "MasterTrackFXMenu")
        return slotIndex_ + zoneManager_->GetMasterTrackFXMenuOffset();
    else return slotIndex_;
}

void Zone::AddWidget(Widget *widget)
{
    if (find(widgets_.begin(), widgets_.end(), widget) == widgets_.end())
        widgets_.push_back(widget);
}

void Zone::Activate()
{
    UpdateCurrentActionContextModifiers();
    
    for (auto &widget : widgets_)
    {
        if (!strcmp(widget->GetName(), "OnZoneActivation"))
            for (auto &actionContext :  GetActionContexts(widget))
                actionContext->DoAction(1.0);

        widget->Configure(GetActionContexts(widget));
    }

    isActive_ = true;
    
    if (!strcmp(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->VCAModeActivated();
    else if (!strcmp(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->FolderModeActivated();
    else if (!strcmp(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->SelectedTracksModeActivated();

    zoneManager_->GetSurface()->SendOSCMessage(GetName());

    for (auto &subZone : subZones_)
        subZone->Deactivate();

    for (auto &includedZone : includedZones_)
        includedZone->Activate();
}

void Zone::Deactivate()
{
    if (! isActive_)
        return;
    for (auto &widget : widgets_)
    {
        for (auto &actionContext : GetActionContexts(widget))
        {
            actionContext->UpdateWidgetValue(0.0);
            actionContext->UpdateWidgetValue("");

            if (!strcmp(widget->GetName(), "OnZoneDeactivation"))
                actionContext->DoAction(1.0);
        }
    }

    isActive_ = false;
    
    if (!strcmp(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->VCAModeDeactivated();
    else if (!strcmp(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->FolderModeDeactivated();
    else if (!strcmp(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->SelectedTracksModeDeactivated();
    
    for (auto &includedZone : includedZones_)
        includedZone->Deactivate();

    for (auto &subZone : subZones_)
        subZone->Deactivate();
}

void Zone::RequestUpdate()
{
    if (! isActive_)
        return;
    
    for (auto &subZone : subZones_)
        subZone->RequestUpdate();

    for (auto &includedZone : includedZones_)
        includedZone->RequestUpdate();
    
    for (auto widget : widgets_)
    {
        if ( ! widget->GetHasBeenUsedByUpdate())
        {
            widget->SetHasBeenUsedByUpdate();
            RequestUpdateWidget(widget);
        }
    }
}

void Zone::SetXTouchDisplayColors(const char *colors)
{
    for (auto &widget : widgets_)
       widget->SetXTouchDisplayColors(colors);
}

void Zone::RestoreXTouchDisplayColors()
{
    for (auto &widget : widgets_)
        widget->RestoreXTouchDisplayColors();
}

void Zone::DoAction(Widget *widget, bool &isUsed, double value)
{
    if (! isActive_ || isUsed)
        return;
        
    for (auto &subZone : subZones_)
        subZone->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;
        
        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoAction(value);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoAction(widget, isUsed, value);
    }
}

void Zone::DoRelativeAction(Widget *widget, bool &isUsed, double delta)
{
    if (! isActive_ || isUsed)
        return;
    
    for (auto &subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(delta);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, delta);
    }
}

void Zone::DoRelativeAction(Widget *widget, bool &isUsed, int accelerationIndex, double delta)
{
    if (! isActive_ || isUsed)
        return;

    for (auto &subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(accelerationIndex, delta);
    }
    else
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    }
}

void Zone::DoTouch(Widget *widget, const char *widgetName, bool &isUsed, double value)
{
    if (! isActive_ || isUsed)
        return;

    for (auto &subZone : subZones_)
        subZone->DoTouch(widget, widgetName, isUsed, value);
    
    if (isUsed)
        return;
    
    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end())
    {
        isUsed = true;

        for (auto &actionContext : GetActionContexts(widget))
            actionContext->DoTouch(value);
    }
    else 
    {
        for (auto &includedZone : includedZones_)
            includedZone->DoTouch(widget, widgetName, isUsed, value);
    }
}

void Zone::UpdateCurrentActionContextModifiers()
{
    for (auto &widget : widgets_)
    {
        UpdateCurrentActionContextModifier(widget);
        widget->Configure(GetActionContexts(widget, currentActionContextModifiers_[widget]));
    }
    
    for (auto &includedZone : includedZones_)
        includedZone->UpdateCurrentActionContextModifiers();

    for (auto &subZone : subZones_)
        subZone->UpdateCurrentActionContextModifiers();
}

void Zone::UpdateCurrentActionContextModifier(Widget *widget)
{
    for(int i = 0; i < (int)widget->GetSurface()->GetModifiers().size(); ++i)
    {
        if(actionContextDictionary_[widget].count(widget->GetSurface()->GetModifiers()[i]) > 0)
        {
            currentActionContextModifiers_[widget] = widget->GetSurface()->GetModifiers()[i];
            break;
        }
    }
}

ActionContext *Zone::AddActionContext(Widget *widget, int modifier, Zone *zone, const char *actionName, vector<string> &params)
{
    actionContextDictionary_[widget][modifier].push_back(make_unique<ActionContext>(csi_, csi_->GetAction(actionName), widget, zone, 0, params));
    
    return actionContextDictionary_[widget][modifier].back().get();
}

const vector<unique_ptr<ActionContext>> &Zone::GetActionContexts(Widget *widget)
{
    if(currentActionContextModifiers_.count(widget) == 0)
        UpdateCurrentActionContextModifier(widget);
    
    bool isTouched = false;
    bool isToggled = false;
    
    if(widget->GetSurface()->GetIsChannelTouched(widget->GetChannelNumber()))
        isTouched = true;

    if(widget->GetSurface()->GetIsChannelToggled(widget->GetChannelNumber()))
        isToggled = true;
    
    if(currentActionContextModifiers_.count(widget) > 0 && actionContextDictionary_.count(widget) > 0)
    {
        int modifier = currentActionContextModifiers_[widget];
        
        if(isTouched && isToggled && actionContextDictionary_[widget].count(modifier + 3) > 0)
            return actionContextDictionary_[widget][modifier + 3];
        else if(isTouched && actionContextDictionary_[widget].count(modifier + 1) > 0)
            return actionContextDictionary_[widget][modifier + 1];
        else if(isToggled && actionContextDictionary_[widget].count(modifier + 2) > 0)
            return actionContextDictionary_[widget][modifier + 2];
        else if(actionContextDictionary_[widget].count(modifier) > 0)
            return actionContextDictionary_[widget][modifier];
    }

    return emptyContexts_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager *Widget::GetZoneManager()
{
    return surface_->GetZoneManager();
}

void Widget::Configure(const vector<unique_ptr<ActionContext>> &contexts)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->Configure(contexts);
}

void  Widget::UpdateValue(const PropertyList &properties, double value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void  Widget::UpdateValue(const PropertyList &properties, const char * const &value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void  Widget::ForceValue(const PropertyList &properties, const char * const &value)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceValue(properties, value);
}

void Widget::RunDeferredActions()
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->RunDeferredActions();
}

void  Widget::UpdateColorValue(const rgba_color &color)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetColorValue(color);
}

void Widget::SetXTouchDisplayColors(const char *colors)
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetXTouchDisplayColors(colors);
}

void Widget::RestoreXTouchDisplayColors()
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->RestoreXTouchDisplayColors();
}

void  Widget::ForceClear()
{
    for (auto &feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceClear();
}

void Widget::LogInput(double value)
{
    if (g_surfaceInDisplay) LogToConsole(256, "IN <- %s %s %f\n", GetSurface()->GetName(), GetName(), value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_FeedbackProcessor::SendMidiSysExMessage(MIDI_event_ex_t *midiMessage)
{
    surface_->SendMidiSysExMessage(midiMessage);
}

void Midi_FeedbackProcessor::SendMidiMessage(int first, int second, int third)
{
    bool updateMeters = surface_->GetHasMCUMeters() && first == 0xd0; // MUST UPDATE METERS REGARDLESS OF LAST MESSAGE SENT AS METERS ON THE WILL DECAY TO OFF IF NOT UPDATE REGULARLY
 
    if (updateMeters  ||  first != lastMessageSent_.midi_message[0] || second != lastMessageSent_.midi_message[1] || third != lastMessageSent_.midi_message[2])
    {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02x %02x %02x", first, second, third);

        if (updateMeters)
            snprintf(buffer, sizeof(buffer), "%02x %02x", first, second);

        this->LogMessage(buffer);
        ForceMidiMessage(first, second, third);
    }
}

void Midi_FeedbackProcessor::ForceMidiMessage(int first, int second, int third)
{
    lastMessageSent_.midi_message[0] = first;
    lastMessageSent_.midi_message[1] = second;
    lastMessageSent_.midi_message[2] = third;
    surface_->SendMidiMessage(first, second, third);
}

void Midi_FeedbackProcessor::LogMessage(char* value)
{
    if (g_surfaceOutDisplay) LogToConsole(512, "@S:'%s' [W:'%s'] MIDI: %s\n", surface_->GetName(), widget_->GetName(), value);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void OSC_FeedbackProcessor::SetColorValue(const rgba_color &color)
{
    if (lastColor_ != color)
    {
        lastColor_ = color;
        char tmp[32];
        surface_->SendOSCMessage(this, (oscAddress_ + "/Color").c_str(), color.rgba_to_string(tmp));
    }
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList &properties, double value)
{
    if ((GetTickCount() - GetWidget()->GetLastIncomingMessageTime()) < 50) // adjust the 50 millisecond value to give you smooth behaviour without making updates sluggish
        return;

    lastDoubleValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), value);
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList &properties, const char * const &value)
{
    lastStringValue_ = value;
    char tmp[MEDBUF];
    surface_->SendOSCMessage(this, oscAddress_.c_str(), GetWidget()->GetSurface()->GetRestrictedLengthText(value,tmp,sizeof(tmp)));
}

void OSC_FeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), 0.0);
    
    lastStringValue_ = "";
    surface_->SendOSCMessage(this, oscAddress_.c_str(), "");
}

void OSC_IntFeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int)0);
}

void OSC_IntFeedbackProcessor::ForceValue(const PropertyList &properties, double value)
{
    lastDoubleValue_ = value;
    
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int)value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZoneManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager::ZoneManager(CSurfIntegrator *const csi, ControlSurface *surface, const string &zoneFolder, const string &fxZoneFolder) : csi_(csi), surface_(surface), zoneFolder_(zoneFolder), fxZoneFolder_(fxZoneFolder == "" ? zoneFolder : fxZoneFolder) {}

Navigator *ZoneManager::GetNavigatorForTrack(MediaTrack *track) { return surface_->GetPage()->GetNavigatorForTrack(track); }
Navigator *ZoneManager::GetMasterTrackNavigator() { return surface_->GetPage()->GetMasterTrackNavigator(); }
Navigator *ZoneManager::GetSelectedTrackNavigator() { return surface_->GetPage()->GetSelectedTrackNavigator(); }
Navigator *ZoneManager::GetFocusedFXNavigator() { return surface_->GetPage()->GetFocusedFXNavigator(); }
int ZoneManager::GetNumChannels() { return surface_->GetNumChannels(); }

void ZoneManager::Initialize()
{
    PreProcessZones();

    if (zoneInfo_.find("Home") == zoneInfo_.end())
    {
        char tmp[MEDBUF];
        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("%s needs a Home Zone to operate, please recheck your installation", "csi_mbox"), surface_->GetName());
        MessageBox(g_hwnd, tmp, __LOCALIZE("CSI Missing Home Zone", "csi_mbox"), MB_OK);
        return;
    }
            
    homeZone_ = make_unique<Zone>(csi_, this, GetSelectedTrackNavigator(), 0, "Home", "Home", zoneInfo_["Home"].filePath);
    LoadZoneFile(homeZone_.get(), "");
    
    vector<string> zoneList;
    if (zoneInfo_.find("GoZones") != zoneInfo_.end())
        LoadZoneMetadata(zoneInfo_["GoZones"].filePath.c_str(), zoneList);
    LoadZones(goZones_, zoneList);
    
    if (zoneInfo_.find("LastTouchedFXParam") != zoneInfo_.end())
    {
        lastTouchedFXParamZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), 0, "LastTouchedFXParam", "LastTouchedFXParam", zoneInfo_["LastTouchedFXParam"].filePath);
        LoadZoneFile(lastTouchedFXParamZone_.get(), "");
    }
        
    homeZone_->Activate();
}

void ZoneManager::PreProcessZoneFile(const string &filePath)
{
    try
    {
        ifstream file(filePath);
        
        CSIZoneInfo info;
        info.filePath = filePath;

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] PreProcessZoneFile: %s\n", GetRelativePath(filePath.c_str()));
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            if (line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if (tokens[0] == "Zone" && tokens.size() > 1)
            {
                info.alias = tokens.size() > 2 ? tokens[2] : tokens[1];
                AddZoneFilePath(tokens[1], info);
            }

            break;
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole(256, "[ERROR] FAILED to PreProcessZoneFile in %s\n", filePath.c_str());
        LogToConsole(2048, "Exception: %s\n", e.what());
    }
}

static ModifierManager s_modifierManager(NULL);

void ZoneManager::GetWidgetNameAndModifiers(const string &line, string &baseWidgetName, int &modifier, bool &isValueInverted, bool &isFeedbackInverted, bool &hasHoldModifier, bool & HasDoublePressPseudoModifier, bool &isDecrease, bool &isIncrease)
{
    vector<string> tokens;
    GetTokens(tokens, line, '+');
    
    baseWidgetName = tokens[tokens.size() - 1];

    if (tokens.size() > 1)
    {
        for (int i = 0; i < tokens.size() - 1; ++i)
        {
            if (tokens[i].find("Touch") != string::npos)
                modifier += 1;
            else if (tokens[i] == "Toggle")
                modifier += 2;
            else if (tokens[i] == "Invert")
                isValueInverted = true;
            else if (tokens[i] == "InvertFB")
                isFeedbackInverted = true;
            else if (tokens[i] == "Hold")
                hasHoldModifier = true;
            else if (tokens[i] == "DoublePress")
                HasDoublePressPseudoModifier = true;
            else if (tokens[i] == "Decrease")
                isDecrease = true;
            else if (tokens[i] == "Increase")
                isIncrease = true;
        }
    }
    
    tokens.erase(tokens.begin() + tokens.size() - 1);
    
    modifier += s_modifierManager.GetModifierValue(tokens);
}

void ZoneManager::GetNavigatorsForZone(const char *zoneName, const char *navigatorName, vector<Navigator *> &navigators)
{
    if (!strcmp(navigatorName, "MasterTrackNavigator") || !strcmp(zoneName, "MasterTrack"))
        navigators.push_back(GetMasterTrackNavigator());
    else if (!strcmp(zoneName, "MasterTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetMasterTrackNavigator());
    else if (!strcmp(navigatorName, "TrackNavigator") ||
             !strcmp(zoneName, "Track") ||
             !strcmp(zoneName, "VCA") ||
             !strcmp(zoneName, "Folder") ||
             !strcmp(zoneName, "SelectedTracks") ||
             !strcmp(zoneName, "TrackSend") ||
             !strcmp(zoneName, "TrackReceive") ||
             !strcmp(zoneName, "TrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
        {
            Navigator *channelNavigator = GetSurface()->GetPage()->GetNavigatorForChannel(i + GetSurface()->GetChannelOffset());
            if (channelNavigator)
                navigators.push_back(channelNavigator);
        }
    else if (!strcmp(zoneName, "SelectedTrack") ||
             !strcmp(zoneName, "SelectedTrackSend") ||
             !strcmp(zoneName, "SelectedTrackReceive") ||
             !strcmp(zoneName, "SelectedTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetSelectedTrackNavigator());
    else if (!strcmp(navigatorName, "FocusedFXNavigator"))
        navigators.push_back(GetFocusedFXNavigator());
    else
        navigators.push_back(GetSelectedTrackNavigator());
}

void ZoneManager::LoadZones(vector<unique_ptr<Zone>> &zones, vector<string> &zoneList)
{
    for (int i = 0; i < zoneList.size(); ++i)
    {
        vector<string> tokens;
        GetTokens(tokens, zoneList[i]);
        
        char zoneName[MEDBUF];
        snprintf(zoneName, sizeof(zoneName), "%s", tokens[0].c_str());
        
        char navigatorName[MEDBUF];
        navigatorName[0] = 0;
        if(tokens.size() > 1)
            snprintf(navigatorName, sizeof(navigatorName), "%s", tokens[1].c_str());
    
        if (zoneInfo_.find(zoneName) != zoneInfo_.end())
        {
            vector<Navigator *> navigators;
            GetNavigatorsForZone(zoneName, navigatorName, navigators);
            
            if (navigators.size() == 1)
            {
                zones.push_back(make_unique<Zone>(csi_, this, navigators[0], 0, string(zoneName), zoneInfo_[zoneName].alias, zoneInfo_[zoneName].filePath));
                LoadZoneFile(zones.back().get(), "");
            }
            else if (navigators.size() > 1)
            {
                for (int j = 0; j < navigators.size(); ++j)
                {
                    char buf[MEDBUF];
                    snprintf(buf, sizeof(buf), "%s%d", string(zoneName).c_str(), j + 1);
                    
                    zones.push_back(make_unique<Zone>(csi_, this, navigators[j], j, string(zoneName), string(buf), zoneInfo_[zoneName].filePath));
                    snprintf(buf, sizeof(buf), "%d", j + 1);
                    LoadZoneFile(zones.back().get(), buf);
                }
            }
        }
    }
}

void ZoneManager::LoadZoneFile(Zone *zone, const char *widgetSuffix)
{
    LoadZoneFile(zone, zone->GetSourceFilePath(), widgetSuffix);
}

void ZoneManager::LoadZoneFile(Zone *zone, const char *filePath, const char *widgetSuffix)
{
    int lineNumber = 0;
    bool isInIncludedZonesSection = false;
    vector<string> includedZonesList;
    bool isInSubZonesSection = false;
    vector<string> subZonesList;

    try
    {
        ifstream file(filePath);
        
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(2048, "[DEBUG] {Z:%s} # LoadZoneFile: %s\n", zone->GetName(), GetRelativePath(filePath));
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if (line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
            
            if (line == s_BeginAutoSection || line == s_EndAutoSection)
                continue;
            
            ReplaceAllWith(line, "|", widgetSuffix);
            
            vector<string> tokens;
            GetTokens(tokens, line);
            
            if (tokens[0] == "Zone" || tokens[0] == "ZoneEnd")
                continue;
            
            else if (tokens[0] == "SubZones")
                isInSubZonesSection = true;
            else if (tokens[0] == "SubZonesEnd")
            {
                isInSubZonesSection = false;
                zone->InitSubZones(subZonesList, widgetSuffix);
            }
            else if (isInSubZonesSection)
                subZonesList.push_back(tokens[0]);
            
            else if (tokens[0] == "IncludedZones")
                isInIncludedZonesSection = true;
            else if (tokens[0] == "IncludedZonesEnd")
            {
                isInIncludedZonesSection = false;
                LoadZones(zone->GetIncludedZones(), includedZonesList);
            }
            else if (isInIncludedZonesSection)
                includedZonesList.push_back(tokens[0]);
            
            else if (tokens.size() > 1)
            {
                string widgetName;
                int modifier = 0;
                bool isValueInverted = false;
                bool isFeedbackInverted = false;
                bool hasHoldModifier = false;
                bool HasDoublePressPseudoModifier = false;
                bool isDecrease = false;
                bool isIncrease = false;
                
                GetWidgetNameAndModifiers(tokens[0].c_str(), widgetName, modifier, isValueInverted, isFeedbackInverted, hasHoldModifier, HasDoublePressPseudoModifier, isDecrease, isIncrease);
                
                Widget *widget = GetSurface()->GetWidgetByName(widgetName);
                                            
                if (widget == NULL)
                    continue;

                zone->AddWidget(widget);

                vector<string> memberParams;

                for (int i = 1; i < tokens.size(); ++i)
                    memberParams.push_back(tokens[i]);
                
                // For legacy .zon definitions
                if (tokens[1] == "NullDisplay")
                    continue;
                
                ActionContext *context = zone->AddActionContext(widget, modifier, zone, tokens[1].c_str(), memberParams);

                if (isValueInverted)
                        context->SetIsValueInverted();
                    
                if (isFeedbackInverted)
                    context->SetIsFeedbackInverted();
                
                if (hasHoldModifier && context->GetHoldDelay() == 0)
                    context->SetHoldDelay(ActionContext::HOLD_DELAY_INHERIT_VALUE);
                
                if (HasDoublePressPseudoModifier)
                {
                    context->SetDoublePress();
                    widget->SetHasDoublePressActions();
                }

                vector<double> range;
                
                if (isDecrease)
                {
                    range.push_back(-2.0);
                    range.push_back(1.0);
                    context->SetRange(range);
                }
                else if (isIncrease)
                {
                    range.push_back(0.0);
                    range.push_back(2.0);
                    context->SetRange(range);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LogToConsole(256, "[ERROR] FAILED to LoadZoneFile in %s, around line %d\n", zone->GetSourceFilePath(), lineNumber);
        LogToConsole(2048, "Exception: %s\n", e.what());
    }
}

void ZoneManager::AddListener(ControlSurface *surface)
{
    listeners_.push_back(surface->GetZoneManager());
}

void ZoneManager::SetListenerCategories(PropertyList &pList)
{
    if (const char *property =  pList.get_prop(PropertyType_GoHome))
        if (! strcmp(property, "Yes"))
            listensToGoHome_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackSends))
        if (! strcmp(property, "Yes"))
            listensToSends_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackReceives))
        if (! strcmp(property, "Yes"))
            listensToReceives_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_FXMenu))
        if (! strcmp(property, "Yes"))
            listensToFXMenu_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_SelectedTrackFX))
        if (! strcmp(property, "Yes"))
            listensToSelectedTrackFX_ = true;
    
    if (const char *property =  pList.get_prop(PropertyType_Modifiers))
        if (! strcmp(property, "Yes"))
            surface_->SetListensToModifiers();;
}

void ZoneManager::CheckFocusedFXState()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int takeNumber = 0;
    int fxSlot = 0;
    int paramIndex = 0;
    
    bool retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxSlot, &paramIndex);
    
    if (! isFocusedFXMappingEnabled_)
        return;

    if (! retVal || (retVal && (paramIndex & 0x01)))
    {
        if (focusedFXZone_ != NULL)
            ClearFocusedFX();
        
        return;
    }
    
    if (fxSlot < 0)
        return;
    
    MediaTrack *focusedTrack = NULL;

    trackNumber++;
    
    if (retVal && ! (paramIndex & 0x01))
    {
        if (trackNumber > 0)
            focusedTrack = DAW::GetTrack(trackNumber);
        else if (trackNumber == 0)
            focusedTrack = GetMasterTrack(NULL);
    }
    
    if (focusedTrack)
    {
        char fxName[MEDBUF];
        TrackFX_GetFXName(focusedTrack, fxSlot, fxName, sizeof(fxName));
        
        if(focusedFXZone_ != NULL && focusedFXZone_->GetSlotIndex() == fxSlot && !strcmp(fxName, focusedFXZone_->GetName()))
            return;
        else
            ClearFocusedFX();
        
        if (zoneInfo_.find(fxName) != zoneInfo_.end())
        {
            focusedFXZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath.c_str());
            LoadZoneFile(focusedFXZone_.get(), "");
            focusedFXZone_->Activate();
        }            
    }
}

void ZoneManager::GoSelectedTrackFX()
{
    selectedTrackFXZones_.clear();
    
    if (MediaTrack *selectedTrack = surface_->GetPage()->GetSelectedTrack())
    {
        for (int i = 0; i < TrackFX_GetCount(selectedTrack); ++i)
        {
            char fxName[MEDBUF];
            
            TrackFX_GetFXName(selectedTrack, i, fxName, sizeof(fxName));
            
            if (zoneInfo_.find(fxName) != zoneInfo_.end())
            {
                shared_ptr<Zone> zone = make_shared<Zone>(csi_, this, GetSelectedTrackNavigator(), i, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
                LoadZoneFile(zone.get(), "");
                selectedTrackFXZones_.push_back(zone);
                zone->Activate();
            }
        }
    }
}

void ZoneManager::GoFXSlot(MediaTrack *track, Navigator *navigator, int fxSlot)
{
    if (fxSlot > TrackFX_GetCount(track) - 1)
        return;
        
    char fxName[MEDBUF];
    
    TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));

    if (zoneInfo_.find(fxName) != zoneInfo_.end())
    {
        ClearFXSlot();        
        fxSlotZone_ = make_shared<Zone>(csi_, this, navigator, fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
        LoadZoneFile(fxSlotZone_.get(), "");
        fxSlotZone_->Activate();
    }
    else
        TrackFX_SetOpen(track, fxSlot, true);
}

void ZoneManager::UpdateCurrentActionContextModifiers()
{  
    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->UpdateCurrentActionContextModifiers();

    if (lastTouchedFXParamZone_ != NULL)
        lastTouchedFXParamZone_->UpdateCurrentActionContextModifiers();
    
    if (focusedFXZone_ != NULL)
        focusedFXZone_->UpdateCurrentActionContextModifiers();
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->UpdateCurrentActionContextModifiers();
    
    if (fxSlotZone_ != NULL)
        fxSlotZone_->UpdateCurrentActionContextModifiers();
    
    if (homeZone_ != NULL)
        homeZone_->UpdateCurrentActionContextModifiers();
    
    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->UpdateCurrentActionContextModifiers();
}

void ZoneManager::PreProcessZones()
{
    if (zoneFolder_[0] == 0)
    {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your CSI.ini, cannot find Zone folder for %s in:\r\n\r\n%s/CSI/Zones/","csi_mbox"), GetSurface()->GetName(), GetResourcePath());
        MessageBox(g_hwnd, tmp, __LOCALIZE("Zone folder definiton for surface is empty","csi_mbox"), MB_OK);

        return;
    }
    
    vector<string>  zoneFilesToProcess;
    
    listFilesOfType(zoneFolder_ + "/", zoneFilesToProcess, ".zon"); // recursively find all .zon files, starting at zoneFolder
       
    if (zoneFilesToProcess.size() == 0)
    {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), __LOCALIZE_VERFMT("Please check your installation, cannot find Zone files for %s in:\r\n\r\n%s","csi_mbox"), GetSurface()->GetName(), zoneFolder_.c_str());
        MessageBox(g_hwnd, tmp, __LOCALIZE("Zone folder is missing or empty","csi_mbox"), MB_OK);

        return;
    }
          
    listFilesOfType(fxZoneFolder_ + "/", zoneFilesToProcess, ".zon"); // recursively find all .zon files, starting at fxZoneFolder
     
    for (const string &zoneFile : zoneFilesToProcess)
        PreProcessZoneFile(zoneFile);
}

void ZoneManager::DoAction(Widget *widget, double value)
{
    widget->LogInput(value);
    
    bool isUsed = false;
    
    DoAction(widget, value, isUsed);
}
    
void ZoneManager::DoAction(Widget *widget, double value, bool &isUsed)
{
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);
    
    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoAction(widget, isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoAction(widget, isUsed, value);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoAction(widget, isUsed, value);
}

void ZoneManager::DoRelativeAction(Widget *widget, double delta)
{
    widget->LogInput(delta);
    
    bool isUsed = false;
    
    DoRelativeAction(widget, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget *widget, double delta, bool &isUsed)
{
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, delta);
    
    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, delta);
}

void ZoneManager::DoRelativeAction(Widget *widget, int accelerationIndex, double delta)
{
    widget->LogInput(delta);
    
    bool isUsed = false;
    
    DoRelativeAction(widget, accelerationIndex, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget *widget, int accelerationIndex, double delta, bool &isUsed)
{
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;
    
    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
}

void ZoneManager::DoTouch(Widget *widget, double value)
{
    widget->LogInput(value);
    
    bool isUsed = false;
    
    DoTouch(widget, value, isUsed);
}

void ZoneManager::DoTouch(Widget *widget, double value, bool &isUsed)
{
    surface_->TouchChannel(widget->GetChannelNumber(), value != 0);
       
    // GAW -- temporary
    //if (surface_->GetModifiers().GetSize() > 0 && value != 0.0) // ignore touch releases for Learn mode
        //WidgetMoved(this, widget, surface_->GetModifiers().Get()[0]);

    //if (learnFocusedFXZone_ != NULL)
        //learnFocusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed)
        return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if (isUsed)
        return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed)
        return;

    if (homeZone_ != NULL)
        homeZone_->DoTouch(widget, widget->GetName(), isUsed, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModifierManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ModifierManager::RecalculateModifiers()
{
    if (surface_ == NULL && page_ == NULL)
        return;
    
    if (modifierCombinations_.ResizeOK(1,false))
      modifierCombinations_.Get()[0] = 0 ;
           
    Modifiers activeModifierIndices[MaxModifiers];
    int activeModifierIndices_cnt = 0;
    
    for (int i = 0; i < MaxModifiers; ++i)
        if (modifiers_[i].isEngaged)
            activeModifierIndices[activeModifierIndices_cnt++] = (Modifiers)i;
    
    if (activeModifierIndices_cnt>0)
    {
        GetCombinations(activeModifierIndices,activeModifierIndices_cnt, modifierCombinations_);
        qsort(modifierCombinations_.Get(), modifierCombinations_.GetSize(), sizeof(modifierCombinations_.Get()[0]), intcmp_rev);
    }
    
    modifierVector_.clear();
    
    for (int i = 0; i < modifierCombinations_.GetSize(); ++i)
        modifierVector_.push_back(modifierCombinations_.Get()[i]);
    
    if (surface_ != NULL)
        surface_->GetZoneManager()->UpdateCurrentActionContextModifiers();
    else if (page_ != NULL)
        page_->UpdateCurrentActionContextModifiers();
}

void ModifierManager::SetLatchModifier(bool value, Modifiers modifier, int latchTime)
{
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        const char* modifierName = stringFromModifier(modifier);
        DWORD keyReleasedTime = GetTickCount();
        DWORD heldTime = keyReleasedTime - modifiers_[modifier].pressedTime;
        if (heldTime >= (DWORD) latchTime) {
            if (modifiers_[modifier].isLocked == true) {
                modifiers_[modifier].isLocked = false;
                if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(256, "[DEBUG] [%s] UNLOCK\n", modifierName);
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s Unlock", modifierName);
                csi_->Speak(tmp);
            }
            modifiers_[modifier].isEngaged = false;
        } else {
            auto modifierName = stringFromModifier(modifier);
            if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole(256, "[DEBUG] [%s] [LOCK]\n", modifierName);
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s Lock", modifierName);
            csi_->Speak(tmp);
            modifiers_[modifier].isLocked = true;
        }
        modifiers_[modifier].pressedTime = 0;
    } else {
        if (modifiers_[modifier].isEngaged == false) {
            modifiers_[modifier].isEngaged = true;
            modifiers_[modifier].pressedTime = GetTickCount();
        } else {
            modifiers_[modifier].pressedTime = 0;
        }
    }
    
    RecalculateModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigationManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void TrackNavigationManager::RebuildTracks()
{
    int oldTracksSize = (int) tracks_.size();

    tracks_.clear();

    if (isFolderViewActive_)
    {
        parentOfCurrentFolderTrack_ = nullptr;
        std::vector<MediaTrack*> ancestorStack;

        // Find the parent of the current folder track, and the index of the first track in the folder
        int trackID = 1;
        if (currentFolderTrackID_ > 0 && currentFolderTrackID_ < GetNumTracks())
        {
            for (; trackID <= GetNumTracks(); trackID++)
            {
                MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
                int depthOffset = static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

                if (trackID == currentFolderTrackID_)
                {
                    if (depthOffset == 1)
                    {
                        trackID++; // The next track is the first one in the folder (a folder cannot be empty)
                    }
                    else // The currentFolderTrackID_ is not a folder actually
                    {
                        if (!ancestorStack.empty())
                        {
                            currentFolderTrackID_ = GetIdFromTrack(ancestorStack.back()); // Back to last parent folder
                            trackID = currentFolderTrackID_ + 1; // Next track is the first one in the folder
                            ancestorStack.pop_back();
                        }
                        else
                        {
                            currentFolderTrackID_ = 0; // Back to the root level
                            trackID = 1;
                        }
                    }
                    break;
                }

                if (depthOffset > 0)
                    ancestorStack.push_back(track);
                else if (depthOffset < 0 && !ancestorStack.empty())
                    ancestorStack.pop_back();
            }
        }

        // Set the parent folder ancestor stack
        if (!ancestorStack.empty())
            parentOfCurrentFolderTrack_ = ancestorStack.back();

        // List the tracks in the folder
        int relativeDepth = 0; // Where 0 is the level of the current folder content
        for (; trackID <= GetNumTracks(); trackID++)
        {
            MediaTrack* track = CSurf_TrackFromID(trackID, followMCP_);
            if (!track)
                continue;

            if (relativeDepth == 0 && IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);

            relativeDepth += static_cast<int>(GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH"));

            // Last track of the folder
            if (relativeDepth < 0)
                break;
        }
    }
    else
    {
        for (int i = 1; i <= GetNumTracks(); ++i)
        {
            MediaTrack* track = CSurf_TrackFromID(i, followMCP_);
            if (!track)
                continue;

            if (IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);
        }
    }

    if (tracks_.size() < oldTracksSize)
    {
        for (int i = oldTracksSize; i > tracks_.size(); i--)
            page_->ForceClearTrack(i - trackOffset_);
    }

    if (tracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::RebuildSelectedTracks()
{
    if (currentTrackVCAFolderMode_ != 3)
        return;

    int oldTracksSize = (int) selectedTracks_.size();
    
    selectedTracks_.clear();
    
    for (int i = 0; i < CountSelectedTracks2(NULL, false); ++i)
        selectedTracks_.push_back(DAW::GetSelectedTrack(i));

    if (selectedTracks_.size() < oldTracksSize)
    {
        for (int i = oldTracksSize; i > selectedTracks_.size(); i--)
            page_->ForceClearTrack(i - selectedTracksOffset_);
    }
    
    if (selectedTracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::AdjustSelectedTrackBank(int amount)
{
    if (MediaTrack *selectedTrack = GetSelectedTrack())
    {
        int trackNum = GetIdFromTrack(selectedTrack);
        
        trackNum += amount;
        
        if (trackNum < 1)
            trackNum = 1;
        
        if (trackNum > GetNumTracks())
            trackNum = GetNumTracks();
        
        if (MediaTrack *trackToSelect = GetTrackFromId(trackNum))
        {
            SetOnlyTrackSelected(trackToSelect);
            if (GetScrollLink())
                SetMixerScroll(trackToSelect);

            page_->OnTrackSelection(trackToSelect);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlSurface::Stop()
{
    if (isRewinding_ || isFastForwarding_) // set the cursor to the Play position
        CSurf_OnPlay();
 
    page_->SignalStop();
    CancelRewindAndFastForward();
    CSurf_OnStop();
}

void ControlSurface::Play()
{
    page_->SignalPlay();
    CancelRewindAndFastForward();
    CSurf_OnPlay();
}

void ControlSurface::Record()
{
    page_->SignalRecord();
    CancelRewindAndFastForward();
    CSurf_OnRecord();
}

void ControlSurface::OnTrackSelection(MediaTrack *track)
{
    string onTrackSelection("OnTrackSelection");
    
    if (widgetsByName_.count(onTrackSelection) > 0)
    {
        if (GetMediaTrackInfo_Value(track, "I_SELECTED"))
            zoneManager_->DoAction(widgetsByName_[onTrackSelection].get(), 1.0);
        else
            zoneManager_->OnTrackDeselection();
        
        zoneManager_->OnTrackSelection();
    }
}

void ControlSurface::ForceClearTrack(int trackNum)
{
    for (auto widget : widgets_)
        if (widget->GetChannelNumber() + channelOffset_ == trackNum)
            widget->ForceClear();
}

void ControlSurface::ForceUpdateTrackColors()
{
    for (auto trackColorFeedbackProcessor : trackColorFeedbackProcessors_)
        trackColorFeedbackProcessor->ForceUpdateTrackColors();
}

rgba_color ControlSurface::GetTrackColorForChannel(int channel)
{
    rgba_color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    if (channel < 0 || channel >= numChannels_)
        return white;
    
    if (MediaTrack *track = page_->GetNavigatorForChannel(channel + channelOffset_)->GetTrack())
        return DAW::GetTrackColor(track);
    else
        return white;
}

void ControlSurface::RequestUpdate()
{
    for (auto widget : widgets_)
        widget->ClearHasBeenUsedByUpdate();

    zoneManager_->RequestUpdate();

    const PropertyList properties;
    
    for (auto &widget : widgets_)
    {
        if ( ! widget->GetHasBeenUsedByUpdate())
        {
            widget->SetHasBeenUsedByUpdate();
            
            rgba_color color;
            widget->UpdateValue(properties, 0.0);
            widget->UpdateValue(properties, "");
            widget->UpdateColorValue(color);
        }
    }

    if (isRewinding_)
    {
        if (GetCursorPosition() == 0)
            StopRewinding();
        else
        {
            CSurf_OnRew(0);

            if (speedX5_ == true)
            {
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
            }
        }
    }
        
    else if (isFastForwarding_)
    {
        if (GetCursorPosition() > GetProjectLength(NULL))
            StopFastForwarding();
        else
        {
            CSurf_OnFwd(0);
            
            if (speedX5_ == true)
            {
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
            }
        }
    }
}

bool ControlSurface::GetShift()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetShift();
    else
        return page_->GetModifierManager()->GetShift();
}

bool ControlSurface::GetOption()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetOption();
    else
        return page_->GetModifierManager()->GetOption();
}

bool ControlSurface::GetControl()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetControl();
    else
        return page_->GetModifierManager()->GetControl();
}

bool ControlSurface::GetAlt()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetAlt();
    else
        return page_->GetModifierManager()->GetAlt();
}

bool ControlSurface::GetFlip()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetFlip();
    else
        return page_->GetModifierManager()->GetFlip();
}

bool ControlSurface::GetGlobal()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetGlobal();
    else
        return page_->GetModifierManager()->GetGlobal();
}

bool ControlSurface::GetMarker()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetMarker();
    else
        return page_->GetModifierManager()->GetMarker();
}

bool ControlSurface::GetNudge()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetNudge();
    else
        return page_->GetModifierManager()->GetNudge();
}

bool ControlSurface::GetZoom()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetZoom();
    else
        return page_->GetModifierManager()->GetZoom();
}

bool ControlSurface::GetScrub()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetScrub();
    else
        return page_->GetModifierManager()->GetScrub();
}

void ControlSurface::SetModifierValue(int value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else if (usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else
        page_->GetModifierManager()->SetModifierValue(value);
}

void ControlSurface::SetShift(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetShift(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetShift(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetShift(value, latchTime_);
    else
        page_->GetModifierManager()->SetShift(value, latchTime_);
}

void ControlSurface::SetOption(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetOption(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetOption(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetOption(value, latchTime_);
    else
        page_->GetModifierManager()->SetOption(value, latchTime_);
}

void ControlSurface::SetControl(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetControl(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetControl(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetControl(value, latchTime_);
    else
        page_->GetModifierManager()->SetControl(value, latchTime_);
}

void ControlSurface::SetAlt(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetAlt(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetAlt(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetAlt(value, latchTime_);
    else
        page_->GetModifierManager()->SetAlt(value, latchTime_);
}

void ControlSurface::SetFlip(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetFlip(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetFlip(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetFlip(value, latchTime_);
    else
        page_->GetModifierManager()->SetFlip(value, latchTime_);
}

void ControlSurface::SetGlobal(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetGlobal(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetGlobal(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetGlobal(value, latchTime_);
    else
        page_->GetModifierManager()->SetGlobal(value, latchTime_);
}

void ControlSurface::SetMarker(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetMarker(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetMarker(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetMarker(value, latchTime_);
    else
        page_->GetModifierManager()->SetMarker(value, latchTime_);
}

void ControlSurface::SetNudge(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetNudge(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetNudge(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetNudge(value, latchTime_);
    else
        page_->GetModifierManager()->SetNudge(value, latchTime_);
}

void ControlSurface::SetZoom(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetZoom(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetZoom(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetZoom(value, latchTime_);
    else
        page_->GetModifierManager()->SetZoom(value, latchTime_);
}

void ControlSurface::SetScrub(bool value)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetScrub(value, latchTime_);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetScrub(value, latchTime_);
    }
    else if (usesLocalModifiers_)
        modifierManager_->SetScrub(value, latchTime_);
    else
        page_->GetModifierManager()->SetScrub(value, latchTime_);
}

const vector<int> &ControlSurface::GetModifiers()
{
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetModifiers();
    else
        return page_->GetModifierManager()->GetModifiers();
}

void ControlSurface::ClearModifier(const char *modifier)
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifier(modifier);
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifier(modifier);
    }
    else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifier(modifier);
    else
        page_->GetModifierManager()->ClearModifier(modifier);
}

void ControlSurface::ClearModifiers()
{
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifiers();
        
        for (auto &listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && ! listener->GetSurface()->GetUsesLocalModifiers() &listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifiers();
    }
    else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifiers();
    else
        page_->GetModifierManager()->ClearModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurfaceIO
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurfaceIO::HandleExternalInput(Midi_ControlSurface *surface)
{
    if (midiInput_)
    {
        midiInput_->SwapBufsPrecise(GetTickCount(), GetTickCount());
        MIDI_eventlist *list = midiInput_->GetReadBuf();
        int bpos = 0;
        MIDI_event_t *evt;
        while ((evt = list->EnumItems(&bpos)))
            surface->ProcessMidiMessage((MIDI_event_ex_t*)evt);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
Midi_ControlSurface::Midi_ControlSurface(CSurfIntegrator *const csi, Page *page, const char *name, int channelOffset, const char *surfaceFile, const char *zoneFolder, const char *fxZoneFolder, Midi_ControlSurfaceIO *surfaceIO)
: ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset), surfaceIO_(surfaceIO)
{
    ProcessMIDIWidgetFile(surfaceFile, this);
    InitHardwiredWidgets(this);
    InitializeMeters();
    InitZoneManager(csi_, this, zoneFolder, fxZoneFolder);
}

void Midi_ControlSurface::ProcessMidiMessage(const MIDI_event_ex_t *evt)
{
    if (g_surfaceRawInDisplay)
    {
        LogToConsole(256, "IN <- %s %02x %02x %02x \n", name_.c_str(), evt->midi_message[0], evt->midi_message[1], evt->midi_message[2]);
        // LogStackTraceToConsole();
    }

    string threeByteKey = to_string(evt->midi_message[0]  * 0x10000 + evt->midi_message[1]  * 0x100 + evt->midi_message[2]);
    string twoByteKey = to_string(evt->midi_message[0]  * 0x10000 + evt->midi_message[1]  * 0x100);
    string oneByteKey = to_string(evt->midi_message[0] * 0x10000);

    // At this point we don't know how much of the message comprises the key, so try all three
    if (CSIMessageGeneratorsByMessage_.find(threeByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[threeByteKey]->ProcessMidiMessage(evt);
    else if (CSIMessageGeneratorsByMessage_.find(twoByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[twoByteKey]->ProcessMidiMessage(evt);
    else if (CSIMessageGeneratorsByMessage_.find(oneByteKey) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[oneByteKey]->ProcessMidiMessage(evt);
}

void Midi_ControlSurface::SendMidiSysExMessage(MIDI_event_ex_t *midiMessage)
{
    surfaceIO_->QueueMidiSysExMessage(midiMessage);
    
    if (g_surfaceOutDisplay)
    {
        string output = "OUT->";
        
        output += name_ + " ";

        char buf[32];

        for (int i = 0; i < midiMessage->size; ++i)
        {
            snprintf(buf, sizeof(buf), "%02x ", midiMessage->midi_message[i]);
            output += buf;
        }
        
        output += " # Midi_ControlSurface::SendMidiSysExMessage\n";

        ShowConsoleMsg(output.c_str());
    }
}

void Midi_ControlSurface::SendMidiMessage(int first, int second, int third)
{
    surfaceIO_->SendMidiMessage(first, second, third);
    
    if (g_surfaceOutDisplay)
    {
        LogToConsole(256, "%s %02x %02x %02x # Midi_ControlSurface::SendMidiMessage\n", ("OUT->" + name_).c_str(), first, second, third);
        // if (first == 144, second == 59) {
        //     // 90 = 144
        //     // 3b = 59
        //     // 7f = 127
        //     LogStackTraceToConsole();
        // }
    }
}

 ////////////////////////////////////////////////////////////////////////////////////////////////////////
 // OSC_ControlSurfaceIO
 ////////////////////////////////////////////////////////////////////////////////////////////////////////

OSC_X32ControlSurfaceIO::OSC_X32ControlSurfaceIO(CSurfIntegrator *const csi, const char *surfaceName, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun) : OSC_ControlSurfaceIO(csi, surfaceName, channelCount, receiveOnPort, transmitToPort, transmitToIpAddress, maxPacketsPerRun) {}

OSC_ControlSurfaceIO::OSC_ControlSurfaceIO(CSurfIntegrator *const csi, const char *surfaceName, int channelCount, const char *receiveOnPort, const char *transmitToPort, const char *transmitToIpAddress, int maxPacketsPerRun) : csi_(csi), name_(surfaceName), channelCount_(channelCount)
{
    // private:
    maxPacketsPerRun_ = maxPacketsPerRun < 0 ? 0 : maxPacketsPerRun;

    if (strcmp(receiveOnPort, transmitToPort))
    {
        inSocket_  = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));
        outSocket_ = GetOutputSocketForAddressAndPort(surfaceName, transmitToIpAddress, atoi(transmitToPort));
    }
    else // WHEN INPUT AND OUTPUT SOCKETS ARE THE SAME -- DO MAGIC :)
    {
        oscpkt::UdpSocket *inSocket = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));

        struct addrinfo hints;
        struct addrinfo *addressInfo;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;      // IPV4
        hints.ai_socktype = SOCK_DGRAM; // UDP
        hints.ai_flags = 0x00000001;    // socket address is intended for bind
        getaddrinfo(transmitToIpAddress, transmitToPort, &hints, &addressInfo);
        memcpy(&(inSocket->remote_addr), (void*)(addressInfo->ai_addr), addressInfo->ai_addrlen);

        inSocket_  = inSocket;
        outSocket_ = inSocket;
    }
 }

OSC_ControlSurfaceIO::~OSC_ControlSurfaceIO()
{
    Sleep(33);
    
    int count = 0;
    while (packetQueue_.GetSize()>=sizeof(int) && ++count < 100)
    {
        BeginRun();
        if (count) Sleep(33);
    }

    if (inSocket_)
    {
        for (int x = 0; x < s_inputSockets.GetSize(); ++x)
        {
            if (s_inputSockets.Get(x)->socket == inSocket_)
            {
                if (!--s_inputSockets.Get(x)->refcnt)
                    s_inputSockets.Delete(x, true);
                break;
            }
        }
    }
    if (outSocket_ && outSocket_ != inSocket_)
    {
        for (int x = 0; x < s_outputSockets.GetSize(); ++x)
        {
            if (s_outputSockets.Get(x)->socket == outSocket_)
            {
                if (!--s_outputSockets.Get(x)->refcnt)
                    s_outputSockets.Delete(x, true);
                break;
            }
        }
    }
}

void OSC_ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface *surface)
{
   if (inSocket_ != NULL && inSocket_->isOk())
   {
       while (inSocket_->receiveNextPacket(0))  // timeout, in ms
       {
           packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
           oscpkt::Message *message;
           
           while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0)
           {
               if (message->arg().isFloat())
               {
                   float value = 0;
                   message->arg().popFloat(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
               else if (message->arg().isInt32())
               {
                   int value;
                   message->arg().popInt32(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
           }
       }
   }
}

void OSC_X32ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface *surface)
{
   if (inSocket_ != NULL && inSocket_->isOk())
   {
       while (inSocket_->receiveNextPacket(0))  // timeout, in ms
       {
           packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
           oscpkt::Message *message;
           
           while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0)
           {
               if (message->arg().isFloat())
               {
                   float value = 0;
                   message->arg().popFloat(value);
                   surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
               else if (message->arg().isInt32())
               {
                   int value;
                   message->arg().popInt32(value);
                   
                   if (message->addressPattern() == "/-stat/selidx")
                   {
                       string x32Select = message->addressPattern() + "/";
                       
                       if (value < 10)
                           x32Select += "0";

                       char buf[64];
                       snprintf(buf, sizeof(buf), "%d", value);
                       x32Select += buf;
                                              
                       surface->ProcessOSCMessage(x32Select.c_str(), 1.0);
                   }
                   else
                       surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
               }
           }
       }
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
OSC_ControlSurface::OSC_ControlSurface(CSurfIntegrator *const csi, Page *page, const char *name, int channelOffset, const char *templateFilename, const char *zoneFolder, const char *fxZoneFolder, OSC_ControlSurfaceIO *surfaceIO) : ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset), surfaceIO_(surfaceIO)

{
    ProcessOSCWidgetFile(templateFilename);
    InitHardwiredWidgets(this);
    InitZoneManager(csi_, this, zoneFolder, fxZoneFolder);
}

void OSC_ControlSurface::ProcessOSCMessage(const char *message, double value)
{
    if (CSIMessageGeneratorsByMessage_.find(message) != CSIMessageGeneratorsByMessage_.end())
        CSIMessageGeneratorsByMessage_[message]->ProcessMessage(value);
    
    if (g_surfaceInDisplay) LogToConsole(MEDBUF, "IN <- %s %s %f\n", name_.c_str(), message, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *zoneName)
{
    string oscAddress(zoneName);
    ReplaceAllWith(oscAddress, s_BadFileChars, "_");
    oscAddress = "/" + oscAddress;

    surfaceIO_->SendOSCMessage(oscAddress.c_str());
        
    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "->LoadingZone---->%s\n", name_.c_str());
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %d # Surface::SendOSCMessage 1\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %f # Surface::SendOSCMessage 2\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char *oscAddress, const char *value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %s # Surface::SendOSCMessage 3\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
    
    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %f # Surface::SendOSCMessage 4\n", feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %s %d # Surface::SendOSCMessage 5\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor *feedbackProcessor, const char *oscAddress, const char *value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if (g_surfaceOutDisplay) LogToConsole(MEDBUF, "OUT->%s %s %s %s # Surface::SendOSCMessage 6\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct
{
    MIDI_event_ex_t evt;
    char data[MEDBUF];
} s_midiSysExData;

void Midi_ControlSurface::SendSysexInitData(int line[], int numElem)
{
    memset(s_midiSysExData.data, 0, sizeof(s_midiSysExData.data));

    s_midiSysExData.evt.frame_offset=0;
    s_midiSysExData.evt.size=0;
    
    for (int i = 0; i < numElem; ++i)
        s_midiSysExData.evt.midi_message[s_midiSysExData.evt.size++] = line[i];
    
    SendMidiSysExMessage(&s_midiSysExData.evt);
}

void Midi_ControlSurface::InitializeMCU()
{
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x07, 0x01, 0xF7 };

    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}

void Midi_ControlSurface::InitializeMCUXT()
{
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x07, 0x01, 0xF7 };
    
    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSurfIntegrator
////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char * const Control_Surface_Integrator = "Control Surface Integrator";

CSurfIntegrator::CSurfIntegrator()
{
    InitActionsDictionary();

    int size = 0;
    int index = projectconfig_var_getoffs("projtimemode", &size);
    timeModeOffs_ = size==4 ? index : -1;
    
    index = projectconfig_var_getoffs("projtimemode2", &size);
    timeMode2Offs_ = size==4 ? index : -1;
    
    index = projectconfig_var_getoffs("projmeasoffs", &size);
    measOffsOffs_ = size==4 ? index : - 1;
    
    index = projectconfig_var_getoffs("projtimeoffs", &size);
    timeOffsOffs_ = size==8 ? index : -1;
    
    index = projectconfig_var_getoffs("panmode", &size);
    projectPanModeOffs_ = size==4 ? index : -1;

    // these are supported by ~7.10+, previous versions we fallback to get_config_var() on-demand
    index = projectconfig_var_getoffs("projmetrov1", &size);
    projectMetronomePrimaryVolumeOffs_ = size==8 ? index : -1;

    index = projectconfig_var_getoffs("projmetrov2", &size);
    projectMetronomeSecondaryVolumeOffs_ = size==8 ? index : -1;
}

CSurfIntegrator::~CSurfIntegrator()
{
    Shutdown();

    midiSurfacesIO_.clear();
    oscSurfacesIO_.clear();
    pages_.clear();
    actions_.clear();
}

const char *CSurfIntegrator::GetTypeString()
{
    return "CSI";
}

const char *CSurfIntegrator::GetDescString()
{
    return Control_Surface_Integrator;
}

const char *CSurfIntegrator::GetConfigString() // string of configuration data
{
    snprintf(configtmp, sizeof(configtmp),"0 0");
    return configtmp;
}

int CSurfIntegrator::Extended(int call, void *parm1, void *parm2, void *parm3)
{
    if (call == CSURF_EXT_SUPPORTS_EXTENDED_TOUCH)
    {
        return 1;
    }
    
    if (call == CSURF_EXT_RESET)
    {
       Init();
    }
    
    if (call == CSURF_EXT_SETFXCHANGE)
    {
        // parm1=(MediaTrack*)track, whenever FX are added, deleted, or change order
        TrackFXListChanged((MediaTrack*)parm1);
    }
        
    if (call == CSURF_EXT_SETMIXERSCROLL)
    {
        MediaTrack *leftPtr = (MediaTrack *)parm1;
        
        int offset = CSurf_TrackToID(leftPtr, true);
        
        offset--;
        
        if (offset < 0)
            offset = 0;
            
        SetTrackOffset(offset);
    }
        
    return 1;
}

static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
    return new CSurfIntegrator();
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
    return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_CSI), parent, dlgProcMainConfig, (LPARAM)initConfigString);
}

reaper_csurf_reg_t csurf_integrator_reg =
{
    "CSI",
    Control_Surface_Integrator,
    createFunc,
    configFunc,
};


void localize_init(void * (*GetFunc)(const char *name))
{
    *(void **)&importedLocalizeFunc = GetFunc("__localizeFunc");
    *(void **)&importedLocalizeMenu = GetFunc("__localizeMenu");
    *(void **)&importedLocalizeInitializeDialog = GetFunc("__localizeInitializeDialog");
    *(void **)&importedLocalizePrepareDialog = GetFunc("__localizePrepareDialog");
}
