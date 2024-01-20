//
//  control_surface_integrator.cpp
//  reaper_control_surface_integrator
//
//

#include "control_surface_integrator.h"
#include "control_surface_midi_widgets.h"
#include "control_surface_action_contexts.h"
#include "control_surface_Reaper_actions.h"
#include "control_surface_manager_actions.h"

#include "WDL/dirscan.h"
#include "WDL/ptrlist.h"
#include "WDL/wdlcstring.h"

extern reaper_plugin_info_t *g_reaper_plugin_info;

int g_minNumParamSteps = 2;
int g_maxNumParamSteps = 30;

static double EnumSteppedValues(int numSteps, int stepNumber)
{
    return floor(stepNumber / (double)(numSteps - 1) * 100.0 + 0.5) * 0.01;
}

void GetParamStepsString(string &outputString, int numSteps)
{
    ostringstream stepStr;
    
    for(int i = 0; i < numSteps; i++)
    {
        stepStr << std::setprecision(2) << EnumSteppedValues(numSteps, i);
        stepStr <<  "  ";
    }

    outputString = stepStr.str();
}

void GetParamStepsValues(vector<double> &outputVector, int numSteps)
{
    outputVector.clear();

    for(int i = 0; i < numSteps; i++)
        outputVector.push_back(EnumSteppedValues(numSteps, i));
}

void TrimLine(string &line)
{
    line = regex_replace(line, regex(s_TabChars), " ");
    line = regex_replace(line, regex(s_CRLFChars), "");
    
    line = line.substr(0, line.find("//")); // remove trailing commewnts
    
    // Trim leading and trailing spaces
    line = regex_replace(line, regex("^\\s+|\\s+$"), "", regex_constants::format_default);
}

void GetTokens(vector<string> &tokens, string line)
{
    istringstream iss(line);
    string token;
    while (iss >> quoted(token))
        tokens.push_back(token);
}

int strToHex(const string &valueStr)
{
    return strtol(valueStr.c_str(), nullptr, 16);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MidiInputPort
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int port_ = 0;
    midi_Input* midiInput_ = nullptr;
    
    MidiInputPort(int port, midi_Input* midiInput) : port_(port), midiInput_(midiInput) {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MidiOutputPort
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int port_ = 0;
    midi_Output* midiOutput_ = nullptr;
    
    MidiOutputPort(int port, midi_Output* midiOutput) : port_(port), midiOutput_(midiOutput) {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static map<int, MidiInputPort*> s_midiInputs;
static map<int, MidiOutputPort*> s_midiOutputs;

static midi_Input* GetMidiInputForPort(int inputPort)
{
    if(s_midiInputs.count(inputPort) > 0)
        return s_midiInputs[inputPort]->midiInput_; // return existing
    
    // otherwise make new
    midi_Input* newInput = DAW::CreateMIDIInput(inputPort);
    
    if(newInput)
    {
        newInput->start();
        s_midiInputs[inputPort] = new MidiInputPort(inputPort, newInput);
        return newInput;
    }
    
    return nullptr;
}

static midi_Output* GetMidiOutputForPort(int outputPort)
{
    if(s_midiOutputs.count(outputPort) > 0)
        return s_midiOutputs[outputPort]->midiOutput_; // return existing
    
    // otherwise make new
    midi_Output* newOutput = DAW::CreateMIDIOutput(outputPort, false, NULL);
    
    if(newOutput)
    {
        s_midiOutputs[outputPort] = new MidiOutputPort(outputPort, newOutput);
        return newOutput;
    }
    
    return nullptr;
}

void ShutdownMidiIO()
{
    for(auto [index, input] : s_midiInputs)
        input->midiInput_->stop();
    
    for(auto [index, input] : s_midiInputs)
        delete input;
    
    for(auto [index, output] : s_midiOutputs)
        delete output;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC I/O Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static map<string, oscpkt::UdpSocket*> s_inputSockets;
static map<string, oscpkt::UdpSocket*> s_outputSockets;

static oscpkt::UdpSocket* GetInputSocketForPort(string surfaceName, int inputPort)
{
    if(s_inputSockets.count(surfaceName) > 0)
        return s_inputSockets[surfaceName]; // return existing
    
    // otherwise make new
    oscpkt::UdpSocket* newInputSocket = new oscpkt::UdpSocket();
    
    if(newInputSocket)
    {
        newInputSocket->bindTo(inputPort);
        
        if (! newInputSocket->isOk())
        {
            //cerr << "Error opening port " << PORT_NUM << ": " << inSocket_.errorMessage() << "\n";
            return nullptr;
        }
        
        s_inputSockets[surfaceName] = newInputSocket;
        
        return s_inputSockets[surfaceName];
    }
    
    return nullptr;
}

static oscpkt::UdpSocket* GetOutputSocketForAddressAndPort(const string &surfaceName, const string &address, int outputPort)
{
    if(s_outputSockets.count(surfaceName) > 0)
        return s_outputSockets[surfaceName]; // return existing
    
    // otherwise make new
    oscpkt::UdpSocket* newOutputSocket = new oscpkt::UdpSocket();
    
    if(newOutputSocket)
    {
        if( ! newOutputSocket->connectTo(address, outputPort))
        {
            //cerr << "Error connecting " << remoteDeviceIP_ << ": " << outSocket_.errorMessage() << "\n";
            return nullptr;
        }
        
        //newOutputSocket->bindTo(outputPort);
        
        if ( ! newOutputSocket->isOk())
        {
            //cerr << "Error opening port " << outPort_ << ": " << outSocket_.errorMessage() << "\n";
            return nullptr;
        }

        s_outputSockets[surfaceName] = newOutputSocket;
        
        return s_outputSockets[surfaceName];
    }
    
    return nullptr;
}

void ShutdownOSCIO()
{
    for(auto [index, input] : s_inputSockets)
        delete input;
    
    for(auto [index, output] : s_outputSockets)
        delete output;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// Parsing
//////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ActionTemplate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    string widgetName = "";
    int modifier = 0;
    string actionName = "";
    vector<string> params;
    bool isValueInverted = false;
    bool isFeedbackInverted = false;
    double holdDelayAmount = 0.0;
    bool isDecrease = false;
    bool isIncrease = false;
    bool provideFeedback = false;
};

static void listFilesOfType(const string &path, vector<string> &results, const char *type)
{
    WDL_PtrList<char> stack;
    WDL_FastString tmp;
    stack.Add(strdup(path.c_str()));
    
    while (stack.GetSize()>0)
    {
        const char *curpath = stack.Get(0);
        WDL_DirScan ds;
        if(!ds.First(curpath))
        {
            do
            {
                if (ds.GetCurrentFN()[0] == '.')
                {
                  // ignore dotfiles and ./..
                }
                else if (ds.GetCurrentIsDirectory())
                {
                    ds.GetCurrentFullFN(&tmp);
                    stack.Add(strdup(tmp.Get()));
                }
                else if (!stricmp(type,WDL_get_fileext(ds.GetCurrentFN())))
                {
                    ds.GetCurrentFullFN(&tmp);
                    results.push_back(string(tmp.Get()));
                }
            } while(!ds.Next());
        }
        stack.Delete(0,true,free);
    }
}

static void GetWidgetNameAndModifiers(const string &line, shared_ptr<ActionTemplate> actionTemplate)
{
    istringstream modifiersAndWidgetName(line);
    vector<string> tokens;
    string token;
    
    ModifierManager modifierManager;
    
    while (getline(modifiersAndWidgetName, token, '+'))
        tokens.push_back(token);
    
    actionTemplate->widgetName = tokens[tokens.size() - 1];
       
    if(tokens.size() > 1)
    {
        for(int i = 0; i < tokens.size() - 1; i++)
        {
            if(tokens[i].find("Touch") != string::npos)
                actionTemplate->modifier += 1;
            else if(tokens[i] == "Toggle")
                actionTemplate->modifier += 2;
                        
            else if(tokens[i] == "Invert")
                actionTemplate->isValueInverted = true;
            else if(tokens[i] == "InvertFB")
                actionTemplate->isFeedbackInverted = true;
            else if(tokens[i] == "Hold")
                actionTemplate->holdDelayAmount = 1.0;
            else if(tokens[i] == "Decrease")
                actionTemplate->isDecrease = true;
            else if(tokens[i] == "Increase")
                actionTemplate->isIncrease = true;
        }
    }
    
    actionTemplate->modifier += modifierManager.GetModifierValue(tokens);
}

static void BuildActionTemplate(const vector<string> &tokens, map<string, map<int, vector<shared_ptr<ActionTemplate>>>> &actionTemplatesDictionary)
{
    string feedbackIndicator = "";
    
    vector<string> params;
    for(int i = 1; i < tokens.size(); i++)
    {
        if(tokens[i] == "Feedback=Yes" || tokens[i] == "Feedback=No")
            feedbackIndicator = tokens[i];
        else
            params.push_back(tokens[i]);
    }

    shared_ptr<ActionTemplate> currentActionTemplate = make_shared<ActionTemplate>();
    
    currentActionTemplate->actionName = tokens[1];
    currentActionTemplate->params = params;
    
    GetWidgetNameAndModifiers(tokens[0], currentActionTemplate);

    actionTemplatesDictionary[currentActionTemplate->widgetName][currentActionTemplate->modifier].push_back(currentActionTemplate);
    
    if(actionTemplatesDictionary[currentActionTemplate->widgetName][currentActionTemplate->modifier].size() == 1)
    {
        if(feedbackIndicator == "" || feedbackIndicator == "Feedback=Yes")
            currentActionTemplate->provideFeedback = true;
    }
    else if(feedbackIndicator == "Feedback=Yes")
    {
        for(int i = 0; i < (int)actionTemplatesDictionary[currentActionTemplate->widgetName][currentActionTemplate->modifier].size(); ++i)
            actionTemplatesDictionary[currentActionTemplate->widgetName][currentActionTemplate->modifier][i]->provideFeedback = false;
        
        actionTemplatesDictionary[currentActionTemplate->widgetName][currentActionTemplate->modifier].back()->provideFeedback = true;
    }
}

static void ProcessSurfaceFXLayout(const string &filePath, vector<vector<string>> &surfaceFXLayout,  vector<vector<string>> &surfaceFXLayoutTemplate)
{
    try
    {
        ifstream file(filePath);
        
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            if(line == "") // ignore blank lines
                continue;
        
            vector<string> tokens;
            GetTokens(tokens, line);
            
            if(tokens[0] != "Zone" && tokens[0] != "ZoneEnd")
            {
                if(tokens[0][0] == '#')
                {
                    tokens[0] = tokens[0].substr(1, tokens[0].length() - 1);
                    surfaceFXLayoutTemplate.push_back(tokens);
                }
                else
                {
                    surfaceFXLayout.push_back(tokens);
                    
                    if(tokens.size() > 1 && tokens[1] == "FXParam")
                    {
                        vector<string> widgetAction;
                        
                        widgetAction.push_back("WidgetAction");
                        widgetAction.push_back(tokens[1]);

                        surfaceFXLayoutTemplate.push_back(widgetAction);
                    }
                    if(tokens.size() > 1 && tokens[1] == "FixedTextDisplay")
                    {
                        vector<string> widgetAction;
                        
                        widgetAction.push_back("AliasDisplayAction");
                        widgetAction.push_back(tokens[1]);

                        surfaceFXLayoutTemplate.push_back(widgetAction);
                    }
                    if(tokens.size() > 1 && tokens[1] == "FXParamValueDisplay")
                    {
                        vector<string> widgetAction;
                        
                        widgetAction.push_back("ValueDisplayAction");
                        widgetAction.push_back(tokens[1]);

                        surfaceFXLayoutTemplate.push_back(widgetAction);
                    }
                }
            }
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), 1);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void ProcessFXLayouts(const string &filePath, vector<CSILayoutInfo> &fxLayouts)
{
    try
    {
        ifstream file(filePath);
        
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            if(line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
        
            if(line.find("Zone") == string::npos)
            {
                vector<string> tokens;
                GetTokens(tokens, line);

                CSILayoutInfo info;

                if(tokens.size() == 3)
                {
                    info.modifiers = tokens[0];
                    info.suffix = tokens[1];
                    info.channelCount = atoi(tokens[2].c_str());
                }

                fxLayouts.push_back(info);
            }
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), 1);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void ProcessFXBoilerplate(const string &filePath, vector<string> &fxBoilerplate)
{
    try
    {
        ifstream file(filePath);
            
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            if(line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
        
            if(line.find("Zone") != 0)
                fxBoilerplate.push_back(line);
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), 1);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void PreProcessZoneFile(const string &filePath, shared_ptr<ZoneManager> zoneManager)
{
    string zoneName = "";
    
    try
    {
        ifstream file(filePath);
        
        CSIZoneInfo info;
        info.filePath = filePath;
                 
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            if(line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if(tokens[0] == "Zone" && tokens.size() > 1)
            {
                zoneName = tokens[1];
                info.alias = tokens.size() > 2 ? tokens[2] : zoneName;
                zoneManager->AddZoneFilePath(zoneName, info);
            }

            break;
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), 1);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void GetColorValues(vector<rgba_color> &colorValues, const vector<string> &colors)
{
    for(int i = 0; i < (int)colors.size(); ++i)
    {
        rgba_color colorValue;
        
        if(colors[i].length() == 7)
        {
            regex pattern("#([0-9a-fA-F]{6})");
            smatch match;
            if (regex_match(colors[i], match, pattern))
            {
                sscanf(match.str(1).c_str(), "%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b);
                colorValues.push_back(colorValue);
            }
        }
        else if(colors[i].length() == 9)
        {
            regex pattern("#([0-9a-fA-F]{8})");
            smatch match;
            if (regex_match(colors[i], match, pattern))
            {
                sscanf(match.str(1).c_str(), "%2x%2x%2x%2x", &colorValue.r, &colorValue.g, &colorValue.b, &colorValue.a);
                colorValues.push_back(colorValue);
            }
        }
    }
}

static void ProcessZoneFile(const string &filePath, shared_ptr<ZoneManager> zoneManager, const vector<shared_ptr<Navigator>> &navigators, vector<shared_ptr<Zone>> &zones, shared_ptr<Zone> enclosingZone)
{
    bool isInIncludedZonesSection = false;
    vector<string> includedZones;
    bool isInSubZonesSection = false;
    vector<string> subZones;
    bool isInAssociatedZonesSection = false;
    vector<string> associatedZones;
    
    map<string, map<int, vector<shared_ptr<ActionTemplate>>>> actionTemplatesDictionary;
    
    string zoneName = "";
    string zoneAlias = "";
    string actionName = "";
    int lineNumber = 0;
   
    try
    {
        ifstream file(filePath);
        
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if(line == "" || (line.size() > 0 && line[0] == '/')) // ignore blank lines and comment lines
                continue;
            
            if(line == s_BeginAutoSection || line == s_EndAutoSection)
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if(tokens.size() > 0)
            {
                if(tokens[0] == "Zone")
                {
                    zoneName = tokens.size() > 1 ? tokens[1] : "";
                    zoneAlias = tokens.size() > 2 ? tokens[2] : "";
                }
                else if(tokens[0] == "ZoneEnd" && zoneName != "")
                {
                    for(int i = 0; i < navigators.size(); i++)
                    {
                        string numStr = to_string(i + 1);
                        
                        map<string, string> expandedTouchIds;
                                                
                        shared_ptr<Zone> zone;
                        
                        if(enclosingZone == nullptr)
                            zone = make_shared<Zone>(zoneManager, navigators[i], i, zoneName, zoneAlias, filePath, includedZones, associatedZones);
                        else
                            zone = make_shared<SubZone>(zoneManager, navigators[i], i, zoneName, zoneAlias, filePath, includedZones, associatedZones, enclosingZone);
                                                
                        if(zoneName == "Home")
                            zoneManager->SetHomeZone(zone);
                                               
                        if(zoneName == "FocusedFXParam")
                            zoneManager->SetFocusedFXParamZone(zone);
                        
                        zones.push_back(zone);
                        
                        for(auto [widgetName, modifiedActionTemplates] : actionTemplatesDictionary)
                        {
                            string surfaceWidgetName = widgetName;
                            
                            if(navigators.size() > 1)
                                surfaceWidgetName = regex_replace(surfaceWidgetName, regex("[|]"), to_string(i + 1));
                            
                            if(enclosingZone != nullptr && enclosingZone->GetChannelNumber() != 0)
                                surfaceWidgetName = regex_replace(surfaceWidgetName, regex("[|]"), to_string(enclosingZone->GetChannelNumber()));
                            
                            Widget *widget = zoneManager->GetSurface()->GetWidgetByName(surfaceWidgetName);
                                                        
                            if(widget == NULL)
                                continue;
 
                            zone->AddWidget(widget, widget->GetName());
                                                        
                            for(auto [modifier, actionTemplates] : modifiedActionTemplates)
                            {
                                for(int j = 0; j < (int)actionTemplates.size(); ++j)
                                {
                                    string actionName = regex_replace(actionTemplates[j]->actionName, regex("[|]"), numStr);

                                    vector<string> memberParams;
                                    for(int k = 0; k < actionTemplates[j]->params.size(); k++)
                                        memberParams.push_back(regex_replace(actionTemplates[j]->params[k], regex("[|]"), numStr));
                                    
                                    shared_ptr<ActionContext> context = TheManager->GetActionContext(actionName, widget, zone, memberParams);
                                        
                                    context->SetProvideFeedback(actionTemplates[j]->provideFeedback);
                                    
                                    if(actionTemplates[j]->isValueInverted)
                                        context->SetIsValueInverted();
                                    
                                    if(actionTemplates[j]->isFeedbackInverted)
                                        context->SetIsFeedbackInverted();
                                    
                                    if(actionTemplates[j]->holdDelayAmount != 0.0)
                                        context->SetHoldDelayAmount(actionTemplates[j]->holdDelayAmount);
                                    
                                    if(actionTemplates[j]->isDecrease)
                                        context->SetRange({ -2.0, 1.0 });
                                    else if(actionTemplates[j]->isIncrease)
                                        context->SetRange({ 0.0, 2.0 });
                                   
                                    zone->AddActionContext(widget, modifier, context);
                                }
                            }
                        }
                    
                        if(enclosingZone == nullptr && subZones.size() > 0)
                            zone->InitSubZones(subZones, zone);
                    }
                                    
                    includedZones.clear();
                    subZones.clear();
                    associatedZones.clear();
                    actionTemplatesDictionary.clear();
                    
                    break;
                }
                                
                else if(tokens[0] == "IncludedZones")
                    isInIncludedZonesSection = true;
                
                else if(tokens[0] == "IncludedZonesEnd")
                    isInIncludedZonesSection = false;
                
                else if(isInIncludedZonesSection)
                    includedZones.push_back(tokens[0]);

                else if(tokens[0] == "SubZones")
                    isInSubZonesSection = true;
                
                else if(tokens[0] == "SubZonesEnd")
                    isInSubZonesSection = false;
                
                else if(isInSubZonesSection)
                    subZones.push_back(tokens[0]);
                 
                else if(tokens[0] == "AssociatedZones")
                    isInAssociatedZonesSection = true;
                
                else if(tokens[0] == "AssociatedZonesEnd")
                    isInAssociatedZonesSection = false;
                
                else if(isInAssociatedZonesSection)
                    associatedZones.push_back(tokens[0]);
                               
                else if(tokens.size() > 1)
                    BuildActionTemplate(tokens, actionTemplatesDictionary);
            }
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), lineNumber);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void SetColor(vector<string> &params, bool &supportsColor, bool &supportsTrackColor, vector<rgba_color> &colorValues)
{
    vector<int> rawValues;
    vector<string> hexColors;
    
    vector<string>::iterator openCurlyBrace = find(params.begin(), params.end(), "{");
    vector<string>::iterator closeCurlyBrace = find(params.begin(), params.end(), "}");
    
    if(openCurlyBrace != params.end() && closeCurlyBrace != params.end())
    {
        for(vector<string>::iterator it = openCurlyBrace + 1; it != closeCurlyBrace; ++it)
        {
            string strVal = *(it);
            
            if(strVal.length() > 0 && strVal[0] == '#')
            {
                hexColors.push_back(strVal);
                continue;
            }
            
            if(strVal == "Track")
            {
                supportsTrackColor = true;
                break;
            }
            else
            {
                if(regex_match(strVal, regex("[0-9]+")))
                {
                    int value = stoi(strVal);
                    value = value < 0 ? 0 : value;
                    value = value > 255 ? 255 : value;
                    
                    rawValues.push_back(value);
                }
            }
        }
        
        if(hexColors.size() > 0)
        {
            supportsColor = true;

            GetColorValues(colorValues, hexColors);
        }
        else if(rawValues.size() % 3 == 0 && rawValues.size() > 2)
        {
            supportsColor = true;
            
            for(int i = 0; i < rawValues.size(); i += 3)
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

static void GetSteppedValues(Widget *widget, shared_ptr<Action> action,  shared_ptr<Zone> zone, int paramNumber, vector<string> &params, const map<string, string> &widgetProperties, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues)
{
    vector<string>::iterator openSquareBrace = find(params.begin(), params.end(), "[");
    vector<string>::iterator closeSquareBrace = find(params.begin(), params.end(), "]");
    
    if(openSquareBrace != params.end() && closeSquareBrace != params.end())
    {
        for(vector<string>::iterator it = openSquareBrace + 1; it != closeSquareBrace; ++it)
        {
            string strVal = *(it);
            
            if(regex_match(strVal, regex("-?[0-9]+[.][0-9]+")) || regex_match(strVal, regex("-?[0-9]+")))
                steppedValues.push_back(stod(strVal));
            else if(regex_match(strVal, regex("[(]-?[0-9]+[.][0-9]+[)]")))
                deltaValue = stod(strVal.substr( 1, strVal.length() - 2 ));
            else if(regex_match(strVal, regex("[(]-?[0-9]+[)]")))
                acceleratedTickValues.push_back(stoi(strVal.substr( 1, strVal.length() - 2 )));
            else if(regex_match(strVal, regex("[(](-?[0-9]+[.][0-9]+[,])+-?[0-9]+[.][0-9]+[)]")))
            {
                istringstream acceleratedDeltaValueStream(strVal.substr( 1, strVal.length() - 2 ));
                string deltaValue;
                
                while (getline(acceleratedDeltaValueStream, deltaValue, ','))
                    acceleratedDeltaValues.push_back(stod(deltaValue));
            }
            else if(regex_match(strVal, regex("[(](-?[0-9]+[,])+-?[0-9]+[)]")))
            {
                istringstream acceleratedTickValueStream(strVal.substr( 1, strVal.length() - 2 ));
                string tickValue;
                
                while (getline(acceleratedTickValueStream, tickValue, ','))
                    acceleratedTickValues.push_back(stoi(tickValue));
            }
            else if(regex_match(strVal, regex("-?[0-9]+[.][0-9]+[>]-?[0-9]+[.][0-9]+")) || regex_match(strVal, regex("[0-9]+[-][0-9]+")))
            {
                istringstream range(strVal);
                vector<string> range_tokens;
                string range_token;
                
                while (getline(range, range_token, '>'))
                    range_tokens.push_back(range_token);
                
                if(range_tokens.size() == 2)
                {
                    double firstValue = stod(range_tokens[0]);
                    double lastValue = stod(range_tokens[1]);
                    
                    if(lastValue > firstValue)
                    {
                        rangeMinimum = firstValue;
                        rangeMaximum = lastValue;
                    }
                    else
                    {
                        rangeMinimum = lastValue;
                        rangeMaximum = firstValue;
                    }
                }
            }
        }
    }
    
    if(deltaValue == 0.0 && widget->GetStepSize() != 0.0)
        deltaValue = widget->GetStepSize();
    
    if(acceleratedDeltaValues.size() == 0 && widget->GetAccelerationValues().size() != 0)
        acceleratedDeltaValues = widget->GetAccelerationValues();
         
    if(steppedValues.size() > 0 && acceleratedTickValues.size() == 0)
    {
        double stepSize = deltaValue;
        
        if(stepSize != 0.0)
        {
            stepSize *= 10000.0;
            int baseTickCount = TheManager->GetBaseTickCount((int)steppedValues.size());
            int tickCount = int(baseTickCount / stepSize + 0.5);
            acceleratedTickValues.push_back(tickCount);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Widgets
//////////////////////////////////////////////////////////////////////////////
static void ProcessMidiWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &tokens, Midi_ControlSurface* surface, map<string, double> &stepSizes, map<string, map<int, int>> accelerationValuesForDecrement, map<string, map<int, int>> accelerationValuesForIncrement, map<string, vector<double>> accelerationValues)
{
    if(tokens.size() < 2)
        return;
    
    string widgetName = tokens[1];
    
    string widgetClass = "";
    
    if(tokens.size() > 2)
        widgetClass = tokens[2];

    Widget *widget = new Widget(surface, widgetName);
       
    surface->AddWidget(widget);

    vector<vector<string>> tokenLines;
    
    for (string line; getline(surfaceTemplateFile, line) ; )
    {
        TrimLine(line);
        
        lineNumber++;
        
        if(line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
            continue;
        
        vector<string> tokens;
        GetTokens(tokens, line);

        if(tokens[0] == "WidgetEnd")    // finito baybay - Widget list complete
            break;
        
        tokenLines.push_back(tokens);
    }
    
    if(tokenLines.size() < 1)
        return;
    
    for(int i = 0; i < tokenLines.size(); i++)
    {
        int size = (int)tokenLines[i].size();
        
        string widgetType = tokenLines[i][0];

        shared_ptr<MIDI_event_ex_t> message1 = nullptr;
        shared_ptr<MIDI_event_ex_t> message2 = nullptr;

        int twoByteKey = 0;
        
        if(size > 3)
        {
            message1 = make_shared<MIDI_event_ex_t>(strToHex(tokenLines[i][1]), strToHex(tokenLines[i][2]), strToHex(tokenLines[i][3]));
            twoByteKey = message1->midi_message[0] * 0x10000 + message1->midi_message[1] * 0x100;
        }
        if(size > 6)
            message2 = make_shared<MIDI_event_ex_t>(strToHex(tokenLines[i][4]), strToHex(tokenLines[i][5]), strToHex(tokenLines[i][6]));
        
        // Control Signal Generators
        
        if(widgetType == "AnyPress" && (size == 4 || size == 7))
            surface->AddCSIMessageGenerator(make_shared<AnyPress_Midi_CSIMessageGenerator>(widget, message1), twoByteKey);
        if(widgetType == "Press" && size == 4)
            surface->AddCSIMessageGenerator(make_shared<PressRelease_Midi_CSIMessageGenerator>(widget, message1), message1->midi_message[0] * 0x10000 + message1->midi_message[1] * 0x100 + message1->midi_message[2]);
        else if(widgetType == "Press" && size == 7)
        {
            shared_ptr<PressRelease_Midi_CSIMessageGenerator> generator = make_shared<PressRelease_Midi_CSIMessageGenerator>(widget, message1, message2);
            
            surface->AddCSIMessageGenerator(generator, message1->midi_message[0] * 0x10000 + message1->midi_message[1] * 0x100 + message1->midi_message[2]);
            surface->AddCSIMessageGenerator(generator, message2->midi_message[0] * 0x10000 + message2->midi_message[1] * 0x100 + message2->midi_message[2]);
        }
        else if(widgetType == "Fader14Bit" && size == 4)
            surface->AddCSIMessageGenerator(make_shared<Fader14Bit_Midi_CSIMessageGenerator>(widget, message1), message1->midi_message[0] * 0x10000);
        else if(widgetType == "FaderportClassicFader14Bit" && size == 7)
            surface->AddCSIMessageGenerator(make_shared<FaderportClassicFader14Bit_Midi_CSIMessageGenerator>(widget, message1, message2), message1->midi_message[0] * 0x10000);
        else if(widgetType == "Fader7Bit" && size== 4)
            surface->AddCSIMessageGenerator(make_shared<Fader7Bit_Midi_CSIMessageGenerator>(widget, message1), twoByteKey);
        else if(widgetType == "Encoder" && size == 4 && widgetClass == "RotaryWidgetClass")
        {
            if(stepSizes.count(widgetClass) > 0 && accelerationValuesForDecrement.count(widgetClass) > 0 && accelerationValuesForIncrement.count(widgetClass) > 0 && accelerationValues.count(widgetClass) > 0)
                surface->AddCSIMessageGenerator(make_shared<AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator>(widget, message1, stepSizes[widgetClass], accelerationValuesForDecrement[widgetClass], accelerationValuesForIncrement[widgetClass], accelerationValues[widgetClass]), twoByteKey);
        }
        else if(widgetType == "Encoder" && size == 4)
            surface->AddCSIMessageGenerator(make_shared<Encoder_Midi_CSIMessageGenerator>(widget, message1), twoByteKey);
        else if(widgetType == "Encoder" && size > 4)
            surface->AddCSIMessageGenerator(make_shared<AcceleratedEncoder_Midi_CSIMessageGenerator>(widget, message1, tokenLines[i]), twoByteKey);
        else if(widgetType == "MFTEncoder" && size > 4)
            surface->AddCSIMessageGenerator(make_shared<MFT_AcceleratedEncoder_Midi_CSIMessageGenerator>(widget, message1, tokenLines[i]), twoByteKey);
        else if(widgetType == "EncoderPlain" && size == 4)
            surface->AddCSIMessageGenerator(make_shared<EncoderPlain_Midi_CSIMessageGenerator>(widget, message1), twoByteKey);
        else if(widgetType == "Encoder7Bit" && size == 4)
            surface->AddCSIMessageGenerator(make_shared<Encoder7Bit_Midi_CSIMessageGenerator>(widget, message1), twoByteKey);
        else if(widgetType == "Touch" && size == 7)
        {
            shared_ptr<Touch_Midi_CSIMessageGenerator> generator = make_shared<Touch_Midi_CSIMessageGenerator>(widget, message1, message2);
            
            surface->AddCSIMessageGenerator(generator, message1->midi_message[0] * 0x10000 + message1->midi_message[1] * 0x100 + message1->midi_message[2]);
            surface->AddCSIMessageGenerator(generator, message2->midi_message[0] * 0x10000 + message2->midi_message[1] * 0x100 + message2->midi_message[2]);
        }
        
        // Feedback Processors
        FeedbackProcessor *feedbackProcessor = NULL;

        if(widgetType == "FB_TwoState" && size == 7)
        {
            feedbackProcessor = new TwoState_Midi_FeedbackProcessor(surface, widget, message1, message2);
        }
        else if(widgetType == "FB_NovationLaunchpadMiniRGB7Bit" && size == 4)
        {
            feedbackProcessor = new NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_MFT_RGB" && size == 4)
        {
            feedbackProcessor = new MFT_RGB_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_AsparionRGB" && size == 4)
        {
            feedbackProcessor = new AsparionRGB_Midi_FeedbackProcessor(surface, widget, message1);
            
            if(feedbackProcessor)
                surface->AddTrackColorFeedbackProcessor(feedbackProcessor);
        }
        else if(widgetType == "FB_FaderportRGB" && size == 4)
        {
            feedbackProcessor = new FaderportRGB_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_FaderportTwoStateRGB" && size == 4)
        {
            feedbackProcessor = new FPTwoStateRGB_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_FaderportValueBar"  && size == 2)
        {
            feedbackProcessor = new FPValueBar_Midi_FeedbackProcessor(surface, widget, stoi(tokenLines[i][1]));
        }
        else if((widgetType == "FB_FPVUMeter") && size == 2)
        {
            feedbackProcessor = new FPVUMeter_Midi_FeedbackProcessor(surface, widget, stoi(tokenLines[i][1]));
        }
        else if(widgetType == "FB_Fader14Bit" && size == 4)
        {
            feedbackProcessor = new Fader14Bit_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_FaderportClassicFader14Bit" && size == 7)
        {
            feedbackProcessor = new FaderportClassicFader14Bit_Midi_FeedbackProcessor(surface, widget, message1, message2);
        }
        else if(widgetType == "FB_Fader7Bit" && size == 4)
        {
            feedbackProcessor = new Fader7Bit_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_Encoder" && size == 4)
        {
            feedbackProcessor = new Encoder_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_AsparionEncoder" && size == 4)
        {
            feedbackProcessor = new AsparionEncoder_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_ConsoleOneVUMeter" && size == 4)
        {
            feedbackProcessor = new ConsoleOneVUMeter_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_ConsoleOneGainReductionMeter" && size == 4)
        {
            feedbackProcessor = new ConsoleOneGainReductionMeter_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_MCUTimeDisplay" && size == 1)
        {
            feedbackProcessor = new MCU_TimeDisplay_Midi_FeedbackProcessor(surface, widget);
        }
        else if(widgetType == "FB_MCUAssignmentDisplay" && size == 1)
        {
            feedbackProcessor = new FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor(surface, widget);
        }
        else if(widgetType == "FB_QConProXMasterVUMeter" && size == 2)
        {
            feedbackProcessor = new QConProXMasterVUMeter_Midi_FeedbackProcessor(surface, widget, stoi(tokenLines[i][1]));
        }
        else if((widgetType == "FB_MCUVUMeter" || widgetType == "FB_MCUXTVUMeter") && size == 2)
        {
            int displayType = widgetType == "FB_MCUVUMeter" ? 0x14 : 0x15;
            
            feedbackProcessor = new MCUVUMeter_Midi_FeedbackProcessor(surface, widget, displayType, stoi(tokenLines[i][1]));
            
            surface->SetHasMCUMeters(displayType);
        }
        else if((widgetType == "FB_AsparionVUMeterL" || widgetType == "FB_AsparionVUMeterR") && size == 2)
        {
            bool isRight = widgetType == "FB_AsparionVUMeterR" ? true : false;
            
            feedbackProcessor = new AsparionVUMeter_Midi_FeedbackProcessor(surface, widget, 0x14, stoi(tokenLines[i][1]), isRight);
            
            surface->SetHasMCUMeters(0x14);
        }
        else if(widgetType == "FB_SCE24LEDButton" && size == 4)
        {
            feedbackProcessor = new SCE24TwoStateLED_Midi_FeedbackProcessor(surface, widget, make_shared<MIDI_event_ex_t>(strToHex(tokenLines[i][1]), strToHex(tokenLines[i][2]) + 0x60, strToHex(tokenLines[i][3])));
        }
        else if(widgetType == "FB_SCE24OLEDButton" && size == 4)
        {
            feedbackProcessor = new SCE24OLED_Midi_FeedbackProcessor(surface, widget, make_shared<MIDI_event_ex_t>(strToHex(tokenLines[i][1]), strToHex(tokenLines[i][2]) + 0x60, strToHex(tokenLines[i][3])));
        }
        else if(widgetType == "FB_SCE24Encoder" && size == 4)
        {
            feedbackProcessor = new SCE24Encoder_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if(widgetType == "FB_SCE24EncoderText" && size == 4)
        {
            feedbackProcessor = new SCE24Text_Midi_FeedbackProcessor(surface, widget, message1);
        }
        else if((widgetType == "FB_MCUDisplayUpper" || widgetType == "FB_MCUDisplayLower" || widgetType == "FB_MCUXTDisplayUpper" || widgetType == "FB_MCUXTDisplayLower") && size == 2)
        {
            if(widgetType == "FB_MCUDisplayUpper")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_MCUDisplayLower")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_MCUXTDisplayUpper")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x15, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_MCUXTDisplayLower")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x15, 0x12, stoi(tokenLines[i][1]));
        }
        else if((widgetType == "FB_AsparionDisplayUpper" || widgetType == "FB_AsparionDisplayLower" || widgetType == "FB_AsparionDisplayEncoder") && size == 2)
        {
            if(widgetType == "FB_AsparionDisplayUpper")
                feedbackProcessor = new AsparionDisplay_Midi_FeedbackProcessor(surface, widget, 0x01, 0x14, 0x1A, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_AsparionDisplayLower")
                feedbackProcessor = new AsparionDisplay_Midi_FeedbackProcessor(surface, widget, 0x02, 0x14, 0x1A, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_AsparionDisplayEncoder")
                feedbackProcessor = new AsparionDisplay_Midi_FeedbackProcessor(surface, widget, 0x03, 0x14, 0x19, stoi(tokenLines[i][1]));
        }
        else if((widgetType == "FB_XTouchDisplayUpper" || widgetType == "FB_XTouchDisplayLower" || widgetType == "FB_XTouchXTDisplayUpper" || widgetType == "FB_XTouchXTDisplayLower") && size == 2)
        {
            if(widgetType == "FB_XTouchDisplayUpper")
                feedbackProcessor = new XTouchDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_XTouchDisplayLower")
                feedbackProcessor = new XTouchDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_XTouchXTDisplayUpper")
                feedbackProcessor = new XTouchDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x15, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_XTouchXTDisplayLower")
                feedbackProcessor = new XTouchDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x15, 0x12, stoi(tokenLines[i][1]));
            
            if(feedbackProcessor)
                surface->AddTrackColorFeedbackProcessor(feedbackProcessor);
        }
        else if((widgetType == "FB_C4DisplayUpper" || widgetType == "FB_C4DisplayLower") && size == 3)
        {
            if(widgetType == "FB_C4DisplayUpper")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x17, stoi(tokenLines[i][1]) + 0x30, stoi(tokenLines[i][2]));
            else if(widgetType == "FB_C4DisplayLower")
                feedbackProcessor = new MCUDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x17, stoi(tokenLines[i][1]) + 0x30, stoi(tokenLines[i][2]));
        }
        else if((widgetType == "FB_FP8ScribbleLine1" || widgetType == "FB_FP16ScribbleLine1"
                 || widgetType == "FB_FP8ScribbleLine2" || widgetType == "FB_FP16ScribbleLine2"
                 || widgetType == "FB_FP8ScribbleLine3" || widgetType == "FB_FP16ScribbleLine3"
                 || widgetType == "FB_FP8ScribbleLine4" || widgetType == "FB_FP16ScribbleLine4") && size == 2)
        {
            if(widgetType == "FB_FP8ScribbleLine1")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x02, stoi(tokenLines[i][1]), 0x00);
            else if(widgetType == "FB_FP8ScribbleLine2")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x02, stoi(tokenLines[i][1]), 0x01);
            else if(widgetType == "FB_FP8ScribbleLine3")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x02, stoi(tokenLines[i][1]), 0x02);
            else if(widgetType == "FB_FP8ScribbleLine4")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x02, stoi(tokenLines[i][1]), 0x03);

            else if(widgetType == "FB_FP16ScribbleLine1")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x16, stoi(tokenLines[i][1]), 0x00);
            else if(widgetType == "FB_FP16ScribbleLine2")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x16, stoi(tokenLines[i][1]), 0x01);
            else if(widgetType == "FB_FP16ScribbleLine3")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x16, stoi(tokenLines[i][1]), 0x02);
            else if(widgetType == "FB_FP16ScribbleLine4")
                feedbackProcessor = new FPDisplay_Midi_FeedbackProcessor(surface, widget, 0x16, stoi(tokenLines[i][1]), 0x03);
        }
        else if((widgetType == "FB_FP8ScribbleStripMode" || widgetType == "FB_FP16ScribbleStripMode") && size == 2)
        {
            if(widgetType == "FB_FP8ScribbleStripMode")
                feedbackProcessor = new FPScribbleStripMode_Midi_FeedbackProcessor(surface, widget, 0x02, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_FP16ScribbleStripMode")
                feedbackProcessor = new FPScribbleStripMode_Midi_FeedbackProcessor(surface, widget, 0x16, stoi(tokenLines[i][1]));
        }
        else if((widgetType == "FB_QConLiteDisplayUpper" || widgetType == "FB_QConLiteDisplayUpperMid" || widgetType == "FB_QConLiteDisplayLowerMid" || widgetType == "FB_QConLiteDisplayLower") && size == 2)
        {
            if(widgetType == "FB_QConLiteDisplayUpper")
                feedbackProcessor = new QConLiteDisplay_Midi_FeedbackProcessor(surface, widget, 0, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_QConLiteDisplayUpperMid")
                feedbackProcessor = new QConLiteDisplay_Midi_FeedbackProcessor(surface, widget, 1, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_QConLiteDisplayLowerMid")
                feedbackProcessor = new QConLiteDisplay_Midi_FeedbackProcessor(surface, widget, 2, 0x14, 0x12, stoi(tokenLines[i][1]));
            else if(widgetType == "FB_QConLiteDisplayLower")
                feedbackProcessor = new QConLiteDisplay_Midi_FeedbackProcessor(surface, widget, 3, 0x14, 0x12, stoi(tokenLines[i][1]));
        }

        if(feedbackProcessor != NULL)
            widget->AddFeedbackProcessor(feedbackProcessor);
    }
}

static void ProcessOSCWidget(int &lineNumber, ifstream &surfaceTemplateFile, const vector<string> &tokens,  OSC_ControlSurface* surface)
{
    if(tokens.size() < 2)
        return;
    
    Widget *widget = new Widget(surface, tokens[1]);
    
    surface->AddWidget(widget);

    vector<vector<string>> tokenLines;

    for (string line; getline(surfaceTemplateFile, line) ; )
    {
        TrimLine(line);
        
        lineNumber++;
        
        if(line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
            continue;
        
        vector<string> tokens;
        GetTokens(tokens, line);

        if(tokens[0] == "WidgetEnd")    // finito baybay - Widget list complete
            break;
        
        tokenLines.push_back(tokens);
    }

    for(int i = 0; i < (int)tokenLines.size(); ++i)
    {
        if(tokenLines[i].size() > 1 && tokenLines[i][0] == "Control")
            surface->AddCSIMessageGenerator(make_shared<CSIMessageGenerator>(widget), tokenLines[i][1]);
        else if(tokenLines[i].size() > 1 && tokenLines[i][0] == "AnyPress")
            surface->AddCSIMessageGenerator(make_shared<AnyPress_CSIMessageGenerator>(widget), tokenLines[i][1]);
        else if (tokenLines[i].size() > 1 && tokenLines[i][0] == "MotorizedFaderWithoutTouch")
            surface->AddCSIMessageGenerator(make_shared<MotorizedFaderWithoutTouch_CSIMessageGenerator>(widget), tokenLines[i][1]);
        else if(tokenLines[i].size() > 1 && tokenLines[i][0] == "Touch")
            surface->AddCSIMessageGenerator(make_shared<Touch_CSIMessageGenerator>(widget), tokenLines[i][1]);
        else if(tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_Processor")
            widget->AddFeedbackProcessor(new OSC_FeedbackProcessor(surface, widget, tokenLines[i][1]));
        else if(tokenLines[i].size() > 1 && tokenLines[i][0] == "FB_IntProcessor")
            widget->AddFeedbackProcessor(new OSC_IntFeedbackProcessor(surface, widget, tokenLines[i][1]));
    }
}

static void ProcessValues(const vector<vector<string>> &lines, map<string, double> &stepSizes, map<string, map<int, int>> &accelerationValuesForDecrement, map<string, map<int, int>> &accelerationValuesForIncrement, map<string, vector<double>> &accelerationValues)
{
    bool inStepSizes = false;
    bool inAccelerationValues = false;
        
    for(int i = 0; i < (int)lines.size(); ++i)
    {
        if(lines[i].size() > 0)
        {
            if(lines[i][0] == "StepSize")
            {
                inStepSizes = true;
                continue;
            }
            else if(lines[i][0] == "StepSizeEnd")
            {
                inStepSizes = false;
                continue;
            }
            else if(lines[i][0] == "AccelerationValues")
            {
                inAccelerationValues = true;
                continue;
            }
            else if(lines[i][0] == "AccelerationValuesEnd")
            {
                inAccelerationValues = false;
                continue;
            }

            if(lines[i].size() > 1)
            {
                if(inStepSizes)
                    stepSizes[lines[i][0]] = stod(lines[i][1]);
                else if(lines[i].size() > 2 && inAccelerationValues)
                {
                    if(lines[i][1] == "Dec")
                        for(int j = 2; j < lines[i].size(); j++)
                            accelerationValuesForDecrement[lines[i][0]][strtol(lines[i][j].c_str(), nullptr, 16)] = j - 2;
                    else if(lines[i][1] == "Inc")
                        for(int j = 2; j < lines[i].size(); j++)
                            accelerationValuesForIncrement[lines[i][0]][strtol(lines[i][j].c_str(), nullptr, 16)] = j - 2;
                    else if(lines[i][1] == "Val")
                        for(int j = 2; j < lines[i].size(); j++)
                            accelerationValues[lines[i][0]].push_back(stod(lines[i][j]));
                }
            }
        }
    }
}

static void ProcessMIDIWidgetFile(const string &filePath, Midi_ControlSurface* surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    map<string, double> stepSizes;
    map<string, map<int, int>> accelerationValuesForDecrement;
    map<string, map<int, int>> accelerationValuesForIncrement;
    map<string, vector<double>> accelerationValues;
    
    try
    {
        ifstream file(filePath);
        
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if(line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if(filePath[filePath.length() - 3] == 'm')
            {
                if(tokens.size() > 0 && tokens[0] != "Widget")
                    valueLines.push_back(tokens);
                
                if(tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                    ProcessValues(valueLines, stepSizes, accelerationValuesForDecrement, accelerationValuesForIncrement, accelerationValues);
            }

            if(tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessMidiWidget(lineNumber, file, tokens, surface, stepSizes, accelerationValuesForDecrement, accelerationValuesForIncrement, accelerationValues);
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), lineNumber);
        DAW::ShowConsoleMsg(buffer);
    }
}

static void ProcessOSCWidgetFile(const string &filePath, OSC_ControlSurface* surface)
{
    int lineNumber = 0;
    vector<vector<string>> valueLines;
    
    map<string, double> stepSizes;
    map<string, map<int, int>> accelerationValuesForDecrement;
    map<string, map<int, int>> accelerationValuesForIncrement;
    map<string, vector<double>> accelerationValues;
    
    try
    {
        ifstream file(filePath);
        
        for (string line; getline(file, line) ; )
        {
            TrimLine(line);
            
            lineNumber++;
            
            if(line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if(filePath[filePath.length() - 3] == 'm')
            {
                if(tokens.size() > 0 && tokens[0] != "Widget")
                    valueLines.push_back(tokens);
                
                if(tokens.size() > 0 && tokens[0] == "AccelerationValuesEnd")
                    ProcessValues(valueLines, stepSizes, accelerationValuesForDecrement, accelerationValuesForIncrement, accelerationValues);
            }

            if(tokens.size() > 0 && (tokens[0] == "Widget"))
                ProcessOSCWidget(lineNumber, file, tokens, surface);
        }
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", filePath.c_str(), lineNumber);
        DAW::ShowConsoleMsg(buffer);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::InitActionsDictionary()
{
    //actions_["DumpHex"] =                           make_shared<DumpHex>();
    actions_["MetronomePrimaryVolumeDisplay"] =     make_shared<MetronomePrimaryVolumeDisplay>();
    actions_["MetronomeSecondaryVolumeDisplay"] =   make_shared<MetronomeSecondaryVolumeDisplay>();
    actions_["MetronomePrimaryVolume"] =            make_shared<MetronomePrimaryVolume>();
    actions_["MetronomeSecondaryVolume"] =          make_shared<MetronomeSecondaryVolume>();
    actions_["Speak"] =                             make_shared<SpeakOSARAMessage>();
    actions_["SendMIDIMessage"] =                   make_shared<SendMIDIMessage>();
    actions_["SendOSCMessage"] =                    make_shared<SendOSCMessage>();
    actions_["SaveProject"] =                       make_shared<SaveProject>();
    actions_["Undo"] =                              make_shared<Undo>();
    actions_["Redo"] =                              make_shared<Redo>();
    actions_["TrackAutoMode"] =                     make_shared<TrackAutoMode>();
    actions_["GlobalAutoMode"] =                    make_shared<GlobalAutoMode>();
    actions_["TrackAutoModeDisplay"] =              make_shared<TrackAutoModeDisplay>();
    actions_["GlobalAutoModeDisplay"] =             make_shared<GlobalAutoModeDisplay>();
    actions_["CycleTrackInputMonitor"] =            make_shared<CycleTrackInputMonitor>();
    actions_["TrackInputMonitorDisplay"] =          make_shared<TrackInputMonitorDisplay>();
    actions_["MCUTimeDisplay"] =                    make_shared<MCUTimeDisplay>();
    actions_["OSCTimeDisplay"] =                    make_shared<OSCTimeDisplay>();
    actions_["NoAction"] =                          make_shared<NoAction>();
    actions_["Reaper"] =                            make_shared<ReaperAction>();
    actions_["FixedTextDisplay"] =                  make_shared<FixedTextDisplay>(); ;
    actions_["FixedRGBColorDisplay"] =              make_shared<FixedRGBColorDisplay>();
    actions_["Rewind"] =                            make_shared<Rewind>();
    actions_["FastForward"] =                       make_shared<FastForward>();
    actions_["Play"] =                              make_shared<Play>();
    actions_["Stop"] =                              make_shared<Stop>();
    actions_["Record"] =                            make_shared<Record>();
    actions_["CycleTimeline"] =                     make_shared<CycleTimeline>();
    actions_["ToggleSynchPageBanking"] =            make_shared<ToggleSynchPageBanking>();
    actions_["ToggleScrollLink"] =                  make_shared<ToggleScrollLink>();
    actions_["ToggleRestrictTextLength"] =          make_shared<ToggleRestrictTextLength>();
    actions_["CSINameDisplay"] =                    make_shared<CSINameDisplay>();
    actions_["CSIVersionDisplay"] =                 make_shared<CSIVersionDisplay>();
    actions_["GlobalModeDisplay"] =                 make_shared<GlobalModeDisplay>();
    actions_["CycleTimeDisplayModes"] =             make_shared<CycleTimeDisplayModes>();
    actions_["NextPage"] =                          make_shared<GoNextPage>();
    actions_["GoPage"] =                            make_shared<GoPage>();
    actions_["PageNameDisplay"] =                   make_shared<PageNameDisplay>();
    actions_["GoHome"] =                            make_shared<GoHome>();
    actions_["AllSurfacesGoHome"] =                 make_shared<AllSurfacesGoHome>();
    actions_["GoSubZone"] =                         make_shared<GoSubZone>();
    actions_["LeaveSubZone"] =                      make_shared<LeaveSubZone>();
    actions_["SetXTouchDisplayColors"] =            make_shared<SetXTouchDisplayColors>();
    actions_["RestoreXTouchDisplayColors"] =        make_shared<RestoreXTouchDisplayColors>();
    actions_["GoFXSlot"] =                          make_shared<GoFXSlot>();
    actions_["ShowFXSlot"] =                        make_shared<ShowFXSlot>();
    actions_["HideFXSlot"] =                        make_shared<HideFXSlot>();
    actions_["ToggleUseLocalModifiers"] =           make_shared<ToggleUseLocalModifiers>();
    actions_["SetLatchTime"] =                      make_shared<SetLatchTime>();
    actions_["ToggleEnableFocusedFXMapping"] =      make_shared<ToggleEnableFocusedFXMapping>();
    actions_["ToggleEnableFocusedFXParamMapping"] = make_shared<ToggleEnableFocusedFXParamMapping>();
    actions_["RemapAutoZone"] =                     make_shared<RemapAutoZone>();
    actions_["AutoMapSlotFX"] =                     make_shared<AutoMapSlotFX>();
    actions_["AutoMapFocusedFX"] =                  make_shared<AutoMapFocusedFX>();
    actions_["GoAssociatedZone"] =                  make_shared<GoAssociatedZone>();
    actions_["GoFXLayoutZone"] =                    make_shared<GoFXLayoutZone>();
    actions_["ClearFocusedFXParam"] =               make_shared<ClearFocusedFXParam>();
    actions_["ClearFocusedFX"] =                    make_shared<ClearFocusedFX>();
    actions_["ClearSelectedTrackFX"] =              make_shared<ClearSelectedTrackFX>();
    actions_["ClearFXSlot"] =                       make_shared<ClearFXSlot>();
    actions_["Bank"] =                              make_shared<Bank>();
    actions_["Shift"] =                             make_shared<SetShift>();
    actions_["Option"] =                            make_shared<SetOption>();
    actions_["Control"] =                           make_shared<SetControl>();
    actions_["Alt"] =                               make_shared<SetAlt>();
    actions_["Flip"] =                              make_shared<SetFlip>();
    actions_["Global"] =                            make_shared<SetGlobal>();
    actions_["Marker"] =                            make_shared<SetMarker>();
    actions_["Nudge"] =                             make_shared<SetNudge>();
    actions_["Zoom"] =                              make_shared<SetZoom>();
    actions_["Scrub"] =                             make_shared<SetScrub>();
    actions_["ClearModifier"] =                     make_shared<ClearModifier>();
    actions_["ClearModifiers"] =                    make_shared<ClearModifiers>();
    actions_["ToggleChannel"] =                     make_shared<SetToggleChannel>();
    actions_["CycleTrackAutoMode"] =                make_shared<CycleTrackAutoMode>();
    actions_["TrackVolume"] =                       make_shared<TrackVolume>();
    actions_["SoftTakeover7BitTrackVolume"] =       make_shared<SoftTakeover7BitTrackVolume>();
    actions_["SoftTakeover14BitTrackVolume"] =      make_shared<SoftTakeover14BitTrackVolume>();
    actions_["TrackVolumeDB"] =                     make_shared<TrackVolumeDB>();
    actions_["TrackToggleVCASpill"] =               make_shared<TrackToggleVCASpill>();
    actions_["TrackVCALeaderDisplay"] =             make_shared<TrackVCALeaderDisplay>();
    actions_["TrackToggleFolderSpill"] =            make_shared<TrackToggleFolderSpill>();
    actions_["TrackFolderParentDisplay"] =          make_shared<TrackFolderParentDisplay>();
    actions_["TrackSelect"] =                       make_shared<TrackSelect>();
    actions_["TrackUniqueSelect"] =                 make_shared<TrackUniqueSelect>();
    actions_["TrackRangeSelect"] =                  make_shared<TrackRangeSelect>();
    actions_["TrackRecordArm"] =                    make_shared<TrackRecordArm>();
    actions_["TrackMute"] =                         make_shared<TrackMute>();
    actions_["TrackSolo"] =                         make_shared<TrackSolo>();
    actions_["ClearAllSolo"] =                      make_shared<ClearAllSolo>();
    actions_["TrackInvertPolarity"] =               make_shared<TrackInvertPolarity>();
    actions_["TrackPan"] =                          make_shared<TrackPan>();
    actions_["TrackPanPercent"] =                   make_shared<TrackPanPercent>();
    actions_["TrackPanWidth"] =                     make_shared<TrackPanWidth>();
    actions_["TrackPanWidthPercent"] =              make_shared<TrackPanWidthPercent>();
    actions_["TrackPanL"] =                         make_shared<TrackPanL>();
    actions_["TrackPanLPercent"] =                  make_shared<TrackPanLPercent>();
    actions_["TrackPanR"] =                         make_shared<TrackPanR>();
    actions_["TrackPanRPercent"] =                  make_shared<TrackPanRPercent>();
    actions_["TrackPanAutoLeft"] =                  make_shared<TrackPanAutoLeft>();
    actions_["TrackPanAutoRight"] =                 make_shared<TrackPanAutoRight>();
    actions_["TrackNameDisplay"] =                  make_shared<TrackNameDisplay>();
    actions_["TrackNumberDisplay"] =                make_shared<TrackNumberDisplay>();
    actions_["TrackRecordInputDisplay"] =           make_shared<TrackRecordInputDisplay>();
    actions_["TrackVolumeDisplay"] =                make_shared<TrackVolumeDisplay>();
    actions_["TrackPanDisplay"] =                   make_shared<TrackPanDisplay>();
    actions_["TrackPanWidthDisplay"] =              make_shared<TrackPanWidthDisplay>();
    actions_["TrackPanLeftDisplay"] =               make_shared<TrackPanLeftDisplay>();
    actions_["TrackPanRightDisplay"] =              make_shared<TrackPanRightDisplay>();
    actions_["TrackPanAutoLeftDisplay"] =           make_shared<TrackPanAutoLeftDisplay>();
    actions_["TrackPanAutoRightDisplay"] =          make_shared<TrackPanAutoRightDisplay>();
    actions_["TrackOutputMeter"] =                  make_shared<TrackOutputMeter>();
    actions_["TrackOutputMeterAverageLR"] =         make_shared<TrackOutputMeterAverageLR>();
    actions_["TrackVolumeWithMeterAverageLR"] =     make_shared<TrackVolumeWithMeterAverageLR>();
    actions_["TrackOutputMeterMaxPeakLR"] =         make_shared<TrackOutputMeterMaxPeakLR>();
    actions_["TrackVolumeWithMeterMaxPeakLR"] =     make_shared<TrackVolumeWithMeterMaxPeakLR>();
    actions_["FocusedFXParam"] =                    make_shared<FocusedFXParam>();
    actions_["FXParam"] =                           make_shared<FXParam>();
    actions_["SaveLearnedFXParams"] =               make_shared<SaveLearnedFXParams>();
    actions_["SaveTemplatedFXParams"] =             make_shared<SaveTemplatedFXParams>();
    actions_["EraseLastTouchedControl"] =           make_shared<EraseLastTouchedControl>();
    actions_["JSFXParam"] =                         make_shared<JSFXParam>();
    actions_["TCPFXParam"] =                        make_shared<TCPFXParam>();
    actions_["ToggleFXBypass"] =                    make_shared<ToggleFXBypass>();
    actions_["FXBypassDisplay"] =                   make_shared<FXBypassDisplay>();
    actions_["ToggleFXOffline"] =                   make_shared<ToggleFXOffline>();
    actions_["FXOfflineDisplay"] =                  make_shared<FXOfflineDisplay>();
    actions_["FXNameDisplay"] =                     make_shared<FXNameDisplay>();
    actions_["FXMenuNameDisplay"] =                 make_shared<FXMenuNameDisplay>();
    actions_["SpeakFXMenuName"] =                   make_shared<SpeakFXMenuName>();
    actions_["FXParamNameDisplay"] =                make_shared<FXParamNameDisplay>();
    actions_["TCPFXParamNameDisplay"] =             make_shared<TCPFXParamNameDisplay>();
    actions_["FXParamValueDisplay"] =               make_shared<FXParamValueDisplay>();
    actions_["TCPFXParamValueDisplay"] =            make_shared<TCPFXParamValueDisplay>();
    actions_["FocusedFXParamNameDisplay"] =         make_shared<FocusedFXParamNameDisplay>();
    actions_["FocusedFXParamValueDisplay"] =        make_shared<FocusedFXParamValueDisplay>();
    actions_["FXGainReductionMeter"] =              make_shared<FXGainReductionMeter>();
    actions_["TrackSendVolume"] =                   make_shared<TrackSendVolume>();
    actions_["TrackSendVolumeDB"] =                 make_shared<TrackSendVolumeDB>();
    actions_["TrackSendPan"] =                      make_shared<TrackSendPan>();
    actions_["TrackSendPanPercent"] =               make_shared<TrackSendPanPercent>();
    actions_["TrackSendMute"] =                     make_shared<TrackSendMute>();
    actions_["TrackSendInvertPolarity"] =           make_shared<TrackSendInvertPolarity>();
    actions_["TrackSendStereoMonoToggle"] =         make_shared<TrackSendStereoMonoToggle>();
    actions_["TrackSendPrePost"] =                  make_shared<TrackSendPrePost>();
    actions_["TrackSendNameDisplay"] =              make_shared<TrackSendNameDisplay>();
    actions_["SpeakTrackSendDestination"] =         make_shared<SpeakTrackSendDestination>();
    actions_["TrackSendVolumeDisplay"] =            make_shared<TrackSendVolumeDisplay>();
    actions_["TrackSendPanDisplay"] =               make_shared<TrackSendPanDisplay>();
    actions_["TrackSendPrePostDisplay"] =           make_shared<TrackSendPrePostDisplay>();
    actions_["TrackReceiveVolume"] =                make_shared<TrackReceiveVolume>();
    actions_["TrackReceiveVolumeDB"] =              make_shared<TrackReceiveVolumeDB>();
    actions_["TrackReceivePan"] =                   make_shared<TrackReceivePan>();
    actions_["TrackReceivePanPercent"] =            make_shared<TrackReceivePanPercent>();
    actions_["TrackReceiveMute"] =                  make_shared<TrackReceiveMute>();
    actions_["TrackReceiveInvertPolarity"] =        make_shared<TrackReceiveInvertPolarity>();
    actions_["TrackReceiveStereoMonoToggle"] =      make_shared<TrackReceiveStereoMonoToggle>();
    actions_["TrackReceivePrePost"] =               make_shared<TrackReceivePrePost>();
    actions_["TrackReceiveNameDisplay"] =           make_shared<TrackReceiveNameDisplay>();
    actions_["SpeakTrackReceiveSource"] =           make_shared<SpeakTrackReceiveSource>();
    actions_["TrackReceiveVolumeDisplay"] =         make_shared<TrackReceiveVolumeDisplay>();
    actions_["TrackReceivePanDisplay"] =            make_shared<TrackReceivePanDisplay>();
    actions_["TrackReceivePrePostDisplay"] =        make_shared<TrackReceivePrePostDisplay>();
    
    learnFXActions_["LearnFXParam"] =               make_shared<LearnFXParam>();
    learnFXActions_["LearnFXParamNameDisplay"] =    make_shared<LearnFXParamNameDisplay>();
    learnFXActions_["LearnFXParamValueDisplay"] =   make_shared<LearnFXParamValueDisplay>();
}

void Manager::Init()
{
    pages_.clear();
    
    map<string, Midi_ControlSurfaceIO*> midiSurfaces;
    map<string, OSC_ControlSurfaceIO*> oscSurfaces;

    string currentBroadcaster = "";
    
    Page* currentPage = nullptr;
    
    string CSIFolderPath = string(DAW::GetResourcePath()) + "/CSI";
    
    WDL_DirScan ds;
    if (ds.First(CSIFolderPath.c_str()))
    {       
        MessageBox(g_hwnd, ("Please check your installation, cannot find " + CSIFolderPath).c_str(), "Missing CSI Folder", MB_OK);
        
        return;
    }
    
    string iniFilePath = string(DAW::GetResourcePath()) + "/CSI/CSI.ini";
    int lineNumber = 0;
    
    try
    {
        ifstream iniFile(iniFilePath);
               
        for (string line; getline(iniFile, line) ; )
        {
            TrimLine(line);
            
            if(lineNumber == 0)
            {
                if(line != s_MajorVersionToken)
                {
                    MessageBox(g_hwnd, ("Version mismatch -- Your CSI.ini file is not " + s_MajorVersionToken).c_str(), ("This is CSI " + s_MajorVersionToken).c_str(), MB_OK);
                    iniFile.close();
                    return;
                }
                else
                {
                    lineNumber++;
                    continue;
                }
            }
            
            if(line == "" || line[0] == '\r' || line[0] == '/') // ignore comment lines and blank lines
                continue;
            
            vector<string> tokens;
            GetTokens(tokens, line);

            if(tokens.size() > 1) // ignore comment lines and blank lines
            {
                if(tokens[0] == s_MidiSurfaceToken && tokens.size() == 4)
                    midiSurfaces[tokens[1]] = new Midi_ControlSurfaceIO(tokens[1], GetMidiInputForPort(atoi(tokens[2].c_str())), GetMidiOutputForPort(atoi(tokens[3].c_str())));
                else if(tokens[0] == s_OSCSurfaceToken && tokens.size() == 5)
                    oscSurfaces[tokens[1]] = new OSC_ControlSurfaceIO(tokens[1], tokens[2], tokens[3], tokens[4]);
                else if(tokens[0] == s_PageToken)
                {
                    bool followMCP = true;
                    bool synchPages = true;
                    bool isScrollLinkEnabled = false;
                    bool isScrollSynchEnabled = false;

                    currentPage = nullptr;
                    
                    if(tokens.size() > 1)
                    {
                        if(tokens.size() > 2)
                        {
                            for(int i = 2; i < tokens.size(); i++)
                            {
                                if(tokens[i] == "FollowTCP")
                                    followMCP = false;
                                else if(tokens[i] == "NoSynchPages")
                                    synchPages = false;
                                else if(tokens[i] == "UseScrollLink")
                                    isScrollLinkEnabled = true;
                                else if(tokens[i] == "UseScrollSynch")
                                    isScrollSynchEnabled = true;
                            }
                        }
                            
                        currentPage = new Page(tokens[1], followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled);
                        pages_.push_back(currentPage);
                    }
                }
                else if(currentPage && tokens.size() > 1 && tokens[0] == "Broadcaster")
                {
                    currentBroadcaster = tokens[1];
                }
                else if(currentPage && tokens.size() > 2 && currentBroadcaster != "" && tokens[0] == "Listener")
                {
                    ControlSurface* broadcaster = nullptr;
                    ControlSurface* listener = nullptr;

                    for(int i = 0; i < (int)currentPage->GetSurfaces().size(); ++i)
                    {
                        if(currentPage->GetSurfaces()[i]->GetName() == currentBroadcaster)
                            broadcaster = currentPage->GetSurfaces()[i];
                        if(currentPage->GetSurfaces()[i]->GetName() == tokens[1])
                            listener = currentPage->GetSurfaces()[i];
                    }
                    
                    if(broadcaster != nullptr && listener != nullptr)
                    {
                        broadcaster->GetZoneManager()->AddListener(listener);
                        listener->GetZoneManager()->SetListenerCategories(tokens[2]);
                    }
                }
                else
                {
                    if(currentPage && (tokens.size() == 6 || tokens.size() == 7))
                    {
                        string zoneFolder = tokens[4];
                        string fxZoneFolder = tokens[5];
                        
                        if(midiSurfaces.count(tokens[0]) > 0)
                            currentPage->AddSurface(new Midi_ControlSurface(currentPage, tokens[0], atoi(tokens[1].c_str()), atoi(tokens[2].c_str()), tokens[3], zoneFolder, fxZoneFolder, midiSurfaces[tokens[0]]));
                        else if(oscSurfaces.count(tokens[0]) > 0)
                            currentPage->AddSurface(new OSC_ControlSurface(currentPage, tokens[0], atoi(tokens[1].c_str()), atoi(tokens[2].c_str()), tokens[3], zoneFolder, fxZoneFolder, oscSurfaces[tokens[0]]));
                    }
                }
            }
            
            lineNumber++;
        }
        /*
        // Restore the PageIndex
        currentPageIndex_ = 0;
        
        char buf[512];
        
        int result = DAW::GetProjExtState(0, "CSI", "PageIndex", buf, sizeof(buf));
        
        if(result > 0)
        {
            currentPageIndex_ = atoi(buf);
 
            if(currentPageIndex_ > pages_.size() - 1)
                currentPageIndex_ = 0;
        }
        
        if(pages_.size() > 0)
            pages_[currentPageIndex_]->ForceClear();
        */
    }
    catch (exception &e)
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "Trouble in %s, around line %d\n", iniFilePath.c_str(), lineNumber);
        DAW::ShowConsoleMsg(buffer);
    }
    
    for(int i = 0; i < (int)pages_.size(); ++i)
        pages_[i]->OnInitialization();
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Parsing end
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack* TrackNavigator::GetTrack()
{
    return trackNavigationManager_->GetTrackFromChannel(channelNum_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack* MasterTrackNavigator::GetTrack()
{
    return DAW::GetMasterTrack();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectedTrackNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack* SelectedTrackNavigator::GetTrack()
{
    return page_->GetSelectedTrack();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// FocusedFXNavigator
////////////////////////////////////////////////////////////////////////////////////////////////////////
MediaTrack* FocusedFXNavigator::GetTrack()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int fxIndex = 0;
    
    if(DAW::GetFocusedFX2(&trackNumber, &itemNumber, &fxIndex) == 1) // Track FX
        return DAW::GetTrack(trackNumber);
    else
        return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ActionContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ActionContext::ActionContext(shared_ptr<Action> action, Widget *widget, shared_ptr<Zone> zone, const vector<string> &paramsAndProperties): action_(action), widget_(widget), zone_(zone)
{
    // private:
    intParam_ = 0;
    
    stringParam_ = "";
    
    paramIndex_ = 0;
    fxParamDisplayName_ = "";
    
    commandId_ = 0;
    
    rangeMinimum_ = 0.0;
    rangeMaximum_ = 1.0;
    
    steppedValuesIndex_ = 0;
    
    deltaValue_ = 0.0;
    accumulatedIncTicks_ = 0;
    accumulatedDecTicks_ = 0;
    
    isValueInverted_ = false;
    isFeedbackInverted_ = false;
    holdDelayAmount_ = 0.0;
    delayStartTime_ = 0.0;
    deferredValue_ = 0.0;
    
    supportsColor_ = false;
    currentColorIndex_ = 0;
    
    supportsTrackColor_ = false;
        
    provideFeedback_ = false;
    
    // For Learn
    cellAddress_ = "";
    
    vector<string> params;
    
    for(int i = 0; i < (int)paramsAndProperties.size(); ++i)
    {
        if(paramsAndProperties[i].find("=") != string::npos)
        {
            istringstream widgetProperty(paramsAndProperties[i]);
            vector<string> kvp;
            string token;
            
            while(getline(widgetProperty, token, '='))
                kvp.push_back(token);

            if(kvp.size() == 2)
                widgetProperties_[kvp[0]] = kvp[1];
        }
        else
            params.push_back(paramsAndProperties[i]);
    }
    
    for(int i = 1; i < params.size(); i++)
        parameters_.push_back(params[i]);
    
    string actionName = "";
    
    if(params.size() > 0)
        actionName = params[0];
    
    // Action with int param, could include leading minus sign
    if(params.size() > 1 && (isdigit(params[1][0]) ||  params[1][0] == '-'))  // C++ 11 says empty strings can be queried without catastrophe :)
    {
        intParam_= atol(params[1].c_str());
    }
    
    if(actionName == "Bank" && (params.size() > 2 && (isdigit(params[2][0]) ||  params[2][0] == '-')))  // C++ 11 says empty strings can be queried without catastrophe :)
    {
        stringParam_ = params[1];
        intParam_= atol(params[2].c_str());
    }
    
    // Action with param index, must be positive
    if(params.size() > 1 && isdigit(params[1][0]))  // C++ 11 says empty strings can be queried without catastrophe :)
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    // Action with string param
    if(params.size() > 1)
        stringParam_ = params[1];
    
    if(actionName == "TrackVolumeDB" || actionName == "TrackSendVolumeDB")
    {
        rangeMinimum_ = -144.0;
        rangeMaximum_ = 24.0;
    }
    
    if(actionName == "TrackPanPercent" || actionName == "TrackPanWidthPercent" || actionName == "TrackPanLPercent" || actionName == "TrackPanRPercent")
    {
        rangeMinimum_ = -100.0;
        rangeMaximum_ = 100.0;
    }
   
    if((actionName == "Reaper" || actionName == "ReaperDec" || actionName == "ReaperInc") && params.size() > 1)
    {
        if (isdigit(params[1][0]))
        {
            commandId_ =  atol(params[1].c_str());
        }
        else // look up by string
        {
            commandId_ = DAW::NamedCommandLookup(params[1].c_str());
            
            if(commandId_ == 0) // can't find it
                commandId_ = 65535; // no-op
        }
    }
        
    if((actionName == "FXParam" || actionName == "JSFXParam") && params.size() > 1 && isdigit(params[1][0])) // C++ 11 says empty strings can be queried without catastrophe :)
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if(actionName == "FXParamValueDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
    }
    
    if(actionName == "FXParamNameDisplay" && params.size() > 1 && isdigit(params[1][0]))
    {
        paramIndex_ = atol(params[1].c_str());
        
        if(params.size() > 2 && params[2] != "{" && params[2] != "[")
               fxParamDisplayName_ = params[2];
    }
    
    if(params.size() > 0)
        SetColor(params, supportsColor_, supportsTrackColor_, colorValues_);
    
    GetSteppedValues(widget, action_, zone_, paramIndex_, params, widgetProperties_, deltaValue_, acceleratedDeltaValues_, rangeMinimum_, rangeMaximum_, steppedValues_, acceleratedTickValues_);

    if(acceleratedTickValues_.size() < 1)
        acceleratedTickValues_.push_back(10);
}

Page* ActionContext::GetPage()
{
    return widget_->GetSurface()->GetPage();
}

ControlSurface* ActionContext::GetSurface()
{
    return widget_->GetSurface();
}

MediaTrack* ActionContext::GetTrack()
{
    return zone_->GetNavigator()->GetTrack();
}

int ActionContext::GetSlotIndex()
{
    return zone_->GetSlotIndex();
}

const string &ActionContext::GetName()
{
    return zone_->GetNameOrAlias();
}

void ActionContext::RunDeferredActions()
{
    if(holdDelayAmount_ != 0.0 && delayStartTime_ != 0.0 && DAW::GetCurrentNumberOfMilliseconds() > (delayStartTime_ + holdDelayAmount_))
    {
        if(steppedValues_.size() > 0)
        {
            if(deferredValue_ != 0.0) // ignore release messages
            {
                if(steppedValuesIndex_ == steppedValues_.size() - 1)
                {
                    if(steppedValues_[0] < steppedValues_[steppedValuesIndex_]) // GAW -- only wrap if 1st value is lower
                        steppedValuesIndex_ = 0;
                }
                else
                    steppedValuesIndex_++;
                
                DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
            }
        }
        else
            DoRangeBoundAction(deferredValue_);

        delayStartTime_ = 0.0;
        deferredValue_ = 0.0;
    }
}

void ActionContext::RequestUpdate()
{
    if(provideFeedback_)
        action_->RequestUpdate(this);
}

void ActionContext::RequestUpdate(int paramNum)
{
    if(provideFeedback_)
        action_->RequestUpdate(this, paramNum);
}

void ActionContext::ClearWidget()
{
    UpdateWidgetValue(0.0);
    UpdateWidgetValue("");
}

void ActionContext::UpdateColorValue(double value)
{
    if(supportsColor_)
    {
        currentColorIndex_ = value == 0 ? 0 : 1;
        if(colorValues_.size() > currentColorIndex_)
            widget_->UpdateColorValue(colorValues_[currentColorIndex_]);
    }
}

void ActionContext::UpdateWidgetValue(double value)
{
    if(steppedValues_.size() > 0)
        SetSteppedValueIndex(value);

    value = isFeedbackInverted_ == false ? value : 1.0 - value;
   
    widget_->UpdateValue(widgetProperties_, value);

    UpdateColorValue(value);
    
    if(supportsTrackColor_)
        UpdateTrackColor();
}

void ActionContext::UpdateJSFXWidgetSteppedValue(double value)
{
    if(steppedValues_.size() > 0)
        SetSteppedValueIndex(value);
}

void ActionContext::UpdateTrackColor()
{
    if(MediaTrack* track = zone_->GetNavigator()->GetTrack())
    {
        rgba_color color = DAW::GetTrackColor(track);
        widget_->UpdateColorValue(color);
    }
}

void ActionContext::UpdateWidgetValue(string value)
{
    widget_->UpdateValue(widgetProperties_, value);
}

void ActionContext::DoAction(double value)
{
    if(holdDelayAmount_ != 0.0)
    {
        if(value == 0.0)
        {
            deferredValue_ = 0.0;
            delayStartTime_ = 0.0;
        }
        else
        {
            deferredValue_ = value;
            delayStartTime_ =  DAW::GetCurrentNumberOfMilliseconds();
        }
    }
    else
    {
        if(steppedValues_.size() > 0)
        {
            if(value != 0.0) // ignore release messages
            {
                if(steppedValuesIndex_ == steppedValues_.size() - 1)
                {
                    if(steppedValues_[0] < steppedValues_[steppedValuesIndex_]) // GAW -- only wrap if 1st value is lower
                        steppedValuesIndex_ = 0;
                }
                else
                    steppedValuesIndex_++;
                
                DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
            }
        }
        else
            DoRangeBoundAction(value);
    }
}

void ActionContext::DoRelativeAction(double delta)
{
    if(steppedValues_.size() > 0)
        DoSteppedValueAction(delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::DoRelativeAction(int accelerationIndex, double delta)
{
    if(steppedValues_.size() > 0)
        DoAcceleratedSteppedValueAction(accelerationIndex, delta);
    else if(acceleratedDeltaValues_.size() > 0)
        DoAcceleratedDeltaValueAction(accelerationIndex, delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) +  (deltaValue_ != 0.0 ? (delta > 0 ? deltaValue_ : -deltaValue_) : delta));
}

void ActionContext::DoRangeBoundAction(double value)
{
    if(value > rangeMaximum_)
        value = rangeMaximum_;
    
    if(value < rangeMinimum_)
        value = rangeMinimum_;
    
    if(isValueInverted_)
        value = 1.0 - value;
    
    widget_->GetZoneManager()->WidgetMoved(this);
    
    action_->Do(this, value);
}

void ActionContext::DoSteppedValueAction(double delta)
{
    if(delta > 0)
    {
        steppedValuesIndex_++;
        
        if(steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else
    {
        steppedValuesIndex_--;
        
        if(steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedSteppedValueAction(int accelerationIndex, double delta)
{
    if(delta > 0)
    {
        accumulatedIncTicks_++;
        accumulatedDecTicks_ = accumulatedDecTicks_ - 1 < 0 ? 0 : accumulatedDecTicks_ - 1;
    }
    else if(delta < 0)
    {
        accumulatedDecTicks_++;
        accumulatedIncTicks_ = accumulatedIncTicks_ - 1 < 0 ? 0 : accumulatedIncTicks_ - 1;
    }
    
    accelerationIndex = accelerationIndex > (int)acceleratedTickValues_.size() - 1 ? (int)acceleratedTickValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if(delta > 0 && accumulatedIncTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_++;
        
        if(steppedValuesIndex_ > (int)steppedValues_.size() - 1)
            steppedValuesIndex_ = (int)steppedValues_.size() - 1;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
    else if(delta < 0 && accumulatedDecTicks_ >= acceleratedTickValues_[accelerationIndex])
    {
        accumulatedIncTicks_ = 0;
        accumulatedDecTicks_ = 0;
        
        steppedValuesIndex_--;
        
        if(steppedValuesIndex_ < 0 )
            steppedValuesIndex_ = 0;
        
        DoRangeBoundAction(steppedValues_[steppedValuesIndex_]);
    }
}

void ActionContext::DoAcceleratedDeltaValueAction(int accelerationIndex, double delta)
{
    accelerationIndex = accelerationIndex > (int)acceleratedDeltaValues_.size() - 1 ? (int)acceleratedDeltaValues_.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;
    
    if(delta > 0.0)
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + acceleratedDeltaValues_[accelerationIndex]);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) - acceleratedDeltaValues_[accelerationIndex]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Zone
////////////////////////////////////////////////////////////////////////////////////////////////////////
Zone::Zone(shared_ptr<ZoneManager> const zoneManager, shared_ptr<Navigator> navigator, int slotIndex, string name, string alias, string sourceFilePath, vector<string> includedZones, vector<string> associatedZones): zoneManager_(zoneManager), navigator_(navigator), slotIndex_(slotIndex), name_(name), alias_(alias), sourceFilePath_(sourceFilePath)
{
    //protected:
    isActive_ = false;

    if(name == "Home")
    {
        for(int i = 0; i < (int)associatedZones.size(); ++i)
        {
            if(zoneManager_->GetZoneFilePaths().count(associatedZones[i]) > 0)
            {
                vector<shared_ptr<Navigator>> navigators;
                AddNavigatorsForZone(associatedZones[i], navigators);

                associatedZones_[associatedZones[i]] = vector<shared_ptr<Zone>>();
                
                ProcessZoneFile(zoneManager_->GetZoneFilePaths()[associatedZones[i]].filePath, zoneManager_, navigators, associatedZones_[associatedZones[i]], nullptr);
            }
        }
    }
    
    for(int i = 0; i < (int)includedZones.size(); ++i)
    {
        if(zoneManager_->GetZoneFilePaths().count(includedZones[i]) > 0)
        {
            vector<shared_ptr<Navigator>> navigators;
            AddNavigatorsForZone(includedZones[i], navigators);
            
            ProcessZoneFile(zoneManager_->GetZoneFilePaths()[includedZones[i]].filePath, zoneManager_, navigators, includedZones_, nullptr);
        }
    }
}

void Zone::InitSubZones(const vector<string> &subZones, shared_ptr<Zone> enclosingZone)
{
    for(int i = 0; i < (int)subZones.size(); ++i)
    {
        if(zoneManager_->GetZoneFilePaths().count(subZones[i]) > 0)
        {
            vector<shared_ptr<Navigator>> navigators;
            navigators.push_back(GetNavigator());

            subZones_[subZones[i]] = vector<shared_ptr<Zone>>();
        
            ProcessZoneFile(zoneManager_->GetZoneFilePaths()[subZones[i]].filePath, zoneManager_, navigators, subZones_[subZones[i]], enclosingZone);
        }
    }
}

int Zone::GetSlotIndex()
{
    if(name_ == "TrackSend")
        return zoneManager_->GetTrackSendOffset();
    if(name_ == "TrackReceive")
        return zoneManager_->GetTrackReceiveOffset();
    if(name_ == "TrackFXMenu")
        return zoneManager_->GetTrackFXMenuOffset();
    if(name_ == "SelectedTrack")
        return slotIndex_;
    if(name_ == "SelectedTrackSend")
        return slotIndex_ + zoneManager_->GetSelectedTrackSendOffset();
    if(name_ == "SelectedTrackReceive")
        return slotIndex_ + zoneManager_->GetSelectedTrackReceiveOffset();
    if(name_ == "SelectedTrackFXMenu")
        return slotIndex_ + zoneManager_->GetSelectedTrackFXMenuOffset();
    if(name_ == "MasterTrackFXMenu")
        return slotIndex_ + zoneManager_->GetMasterTrackFXMenuOffset();
    else return slotIndex_;
}

int Zone::GetParamIndex(const string &widgetName)
{
    if(widgetsByName_.count(widgetName) > 0)
    {
        vector<shared_ptr<ActionContext>> contexts = GetActionContexts(widgetsByName_[widgetName]);
        
        if(contexts.size() > 0)
            return contexts[0]->GetParamIndex();
    }
    
    return -1;
}

int Zone::GetChannelNumber()
{
    int channelNumber = 0;
    
    for(auto [widget, isUsed] : widgets_)
        if(channelNumber < widget->GetChannelNumber())
            channelNumber = widget->GetChannelNumber();
    
    return channelNumber;
}

void Zone::SetFXParamNum(Widget *paramWidget, int paramIndex)
{
    for(auto [widget, isUsed] : widgets_)
    {
        if(widget == paramWidget)
        {
            for(int i = 0; i < (int)GetActionContexts(widget, currentActionContextModifiers_[widget]).size(); ++i)
                GetActionContexts(widget, currentActionContextModifiers_[widget])[i]->SetParamIndex(paramIndex);
            break;
        }
    }
}

void Zone::GoAssociatedZone(const string &zoneName)
{
    if(zoneName == "Track")
    {
        for(auto [key, zones] : associatedZones_)
            for(int i = 0; i < (int)zones.size(); ++i)
                zones[i]->Deactivate();
        
        return;
    }
    
    if(associatedZones_.count(zoneName) > 0 && associatedZones_[zoneName].size() > 0 && associatedZones_[zoneName][0]->GetIsActive())
    {
        for(int i = 0; i < (int)associatedZones_[zoneName].size(); ++i)
            associatedZones_[zoneName][i]->Deactivate();
        
        zoneManager_->GoHome();
        
        return;
    }
    
    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();

    if(associatedZones_.count(zoneName) > 0)
        for(int i = 0; i < (int)associatedZones_[zoneName].size(); ++i)
            associatedZones_[zoneName][i]->Activate();
}

void Zone::GoAssociatedZone(const string &zoneName, int slotIndex)
{
    if(zoneName == "Track")
    {
        for(auto [key, zones] : associatedZones_)
            for(int i = 0; i < (int)zones.size(); ++i)
                zones[i]->Deactivate();

        return;
    }
    
    if(associatedZones_.count(zoneName) > 0 && associatedZones_[zoneName].size() > 0 && associatedZones_[zoneName][0]->GetIsActive())
    {
        for(int i = 0; i < (int)associatedZones_[zoneName].size(); ++i)
            associatedZones_[zoneName][i]->Deactivate();
        
        zoneManager_->GoHome();
        
        return;
    }
    
    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();

    if(associatedZones_.count(zoneName) > 0)
    {
        for(int i = 0; i < (int)associatedZones_[zoneName].size(); ++i)
        {
            associatedZones_[zoneName][i]->SetSlotIndex(slotIndex);
            associatedZones_[zoneName][i]->Activate();
        }
    }
}

void Zone::ReactivateFXMenuZone()
{
    if(associatedZones_.count("TrackFXMenu") > 0 && associatedZones_["TrackFXMenu"][0]->GetIsActive())
        for(int i = 0; i < (int)associatedZones_["TrackFXMenu"].size(); ++i)
            associatedZones_["TrackFXMenu"][i]->Activate();
    else if(associatedZones_.count("SelectedTrackFXMenu") > 0 && associatedZones_["SelectedTrackFXMenu"][0]->GetIsActive())
        for(int i = 0; i < (int)associatedZones_["SelectedTrackFXMenu"].size(); ++i)
            associatedZones_["SelectedTrackFXMenu"][i]->Activate();
}

void Zone::Activate()
{
    UpdateCurrentActionContextModifiers();
    
    for(auto [widget, isUsed] : widgets_)
    {
        if(widget->GetName() == "OnZoneActivation")
            for(int i = 0; i < (int)GetActionContexts(widget).size(); ++i)
                GetActionContexts(widget)[i]->DoAction(1.0);
            
        widget->Configure(GetActionContexts(widget));
    }

    isActive_ = true;
    
    if(GetName() == "VCA")
        zoneManager_->GetSurface()->GetPage()->VCAModeActivated();
    else if(GetName() == "Folder")
        zoneManager_->GetSurface()->GetPage()->FolderModeActivated();
    else if(GetName() == "SelectedTracks")
        zoneManager_->GetSurface()->GetPage()->SelectedTracksModeActivated();

    zoneManager_->GetSurface()->SendOSCMessage(GetName());
       
    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();
    
    for(auto [key, zones] : subZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();

    for(int i = 0; i < (int)includedZones_.size(); ++i)
        includedZones_[i]->Activate();
}

void Zone::Deactivate()
{    
    for(auto [widget, isUsed] : widgets_)
    {
        for(int i = 0; i < (int)GetActionContexts(widget).size(); ++i)
        {
            GetActionContexts(widget)[i]->UpdateWidgetValue(0.0);
            GetActionContexts(widget)[i]->UpdateWidgetValue("");

            if(widget->GetName() == "OnZoneDeactivation")
                GetActionContexts(widget)[i]->DoAction(1.0);
        }
    }

    isActive_ = false;
    
    if(GetName() == "VCA")
        zoneManager_->GetSurface()->GetPage()->VCAModeDeactivated();
    else if(GetName() == "Folder")
        zoneManager_->GetSurface()->GetPage()->FolderModeDeactivated();
    else if(GetName() == "SelectedTracks")
        zoneManager_->GetSurface()->GetPage()->SelectedTracksModeDeactivated();
    
    for(int i = 0; i < (int)includedZones_.size(); ++i)
        includedZones_[i]->Deactivate();

    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();

    for(auto [key, zones] : subZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->Deactivate();
}

void Zone::RequestLearnFXUpdate(map<Widget *, bool> &usedWidgets)
{
    vector<int> modifiers = zoneManager_->GetSurface()->GetModifiers();
    
    int modifier = 0;
    
    if(modifiers.size() > 0)
        modifier = modifiers[0];
    
    if(learnFXCells_.count(modifier) > 0)
    {
        for(auto [cellAddress, cell] : learnFXCells_[modifier])
        {
            bool foundIt = false;
            
            for(int i = 0; i < (int)cell.fxParamWidgets.size(); ++i)
            {
                shared_ptr<LearnInfo> info = zoneManager_->GetLearnInfo(cell.fxParamWidgets[i], modifier);
                                
                if(info->isLearned)
                {
                    foundIt = true;

                    if(actionContextDictionary_.count(cell.fxParamNameDisplayWidget) > 0 && actionContextDictionary_[cell.fxParamNameDisplayWidget].count(modifier) > 0)
                        for(int i = 0; i < (int)actionContextDictionary_[cell.fxParamNameDisplayWidget][modifier].size(); ++i)
                            actionContextDictionary_[cell.fxParamNameDisplayWidget][modifier][i]->RequestUpdate(info->paramNumber);

                    if(actionContextDictionary_.count(cell.fxParamValueDisplayWidget) > 0 && actionContextDictionary_[cell.fxParamValueDisplayWidget].count(modifier) > 0)
                        for(int i = 0; i < (int)actionContextDictionary_[cell.fxParamValueDisplayWidget][modifier].size(); ++i)
                            actionContextDictionary_[cell.fxParamValueDisplayWidget][modifier][i]->RequestUpdate(info->paramNumber);
                }
                else
                {
                    if(actionContextDictionary_.count(cell.fxParamWidgets[i]) > 0 && actionContextDictionary_[cell.fxParamWidgets[i]].count(modifier) > 0)
                    {
                        for(int i = 0; i < (int)actionContextDictionary_[cell.fxParamWidgets[i]][modifier].size(); ++i)
                        {
                            actionContextDictionary_[cell.fxParamWidgets[i]][modifier][i]->UpdateWidgetValue(0.0);
                            actionContextDictionary_[cell.fxParamWidgets[i]][modifier][i]->UpdateWidgetValue("");
                        }
                    }
                }
                
                usedWidgets[cell.fxParamWidgets[i]] = true;
            }
            
            if(! foundIt)
            {
                if(actionContextDictionary_.count(cell.fxParamNameDisplayWidget) > 0 && actionContextDictionary_[cell.fxParamNameDisplayWidget].count(modifier) > 0)
                {
                    for(int i = 0; i < (int)actionContextDictionary_[cell.fxParamNameDisplayWidget][modifier].size(); ++i)
                    {
                        actionContextDictionary_[cell.fxParamNameDisplayWidget][modifier][i]->UpdateWidgetValue(0.0);
                        actionContextDictionary_[cell.fxParamNameDisplayWidget][modifier][i]->UpdateWidgetValue("");
                    }
                    
                    usedWidgets[cell.fxParamNameDisplayWidget] = true;
                }

                if(actionContextDictionary_.count(cell.fxParamValueDisplayWidget) > 0 && actionContextDictionary_[cell.fxParamValueDisplayWidget].count(modifier) > 0)
                {
                    for(int i = 0; i < (int)actionContextDictionary_[cell.fxParamValueDisplayWidget][modifier].size(); ++i)
                    {
                        actionContextDictionary_[cell.fxParamValueDisplayWidget][modifier][i]->UpdateWidgetValue(0.0);
                        actionContextDictionary_[cell.fxParamValueDisplayWidget][modifier][i]->UpdateWidgetValue("");
                    }
                    
                    usedWidgets[cell.fxParamValueDisplayWidget] = true;
                }
            }
        }
    }
}

void Zone::AddNavigatorsForZone(const string &zoneName, vector<shared_ptr<Navigator>> &navigators)
{
    if(zoneName == "MasterTrack")
        navigators.push_back(zoneManager_->GetMasterTrackNavigator());
    else if(zoneName == "Track" || zoneName == "VCA" || zoneName == "Folder" || zoneName == "SelectedTracks" || zoneName == "TrackSend" || zoneName == "TrackReceive" || zoneName == "TrackFXMenu")
    {
        for(int i = 0; i < zoneManager_->GetNumChannels(); i++)
        {
            shared_ptr<Navigator> channelNavigator = zoneManager_->GetSurface()->GetPage()->GetNavigatorForChannel(i + zoneManager_->GetSurface()->GetChannelOffset());
            if(channelNavigator)
                navigators.push_back(channelNavigator);
        }
    }
    else if(zoneName == "SelectedTrack" || zoneName == "SelectedTrackSend" || zoneName == "SelectedTrackReceive" || zoneName == "SelectedTrackFXMenu")
        for(int i = 0; i < zoneManager_->GetNumChannels(); i++)
            navigators.push_back(zoneManager_->GetSelectedTrackNavigator());
    else if(zoneName == "MasterTrackFXMenu")
        for(int i = 0; i < zoneManager_->GetNumChannels(); i++)
            navigators.push_back(zoneManager_->GetMasterTrackNavigator());
    else
        navigators.push_back(zoneManager_->GetSelectedTrackNavigator());
}

void Zone::SetXTouchDisplayColors(const string &color)
{
    for(auto [widget, isUsed] : widgets_)
        widget->SetXTouchDisplayColors(name_, color);
}

void Zone::RestoreXTouchDisplayColors()
{
    for(auto [widget, isUsed] : widgets_)
        widget->RestoreXTouchDisplayColors();
}

void Zone::DoAction(Widget *widget, bool &isUsed, double value)
{
    if(! isActive_ || isUsed)
        return;
    
    for(auto [key, zones] : subZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->DoAction(widget, isUsed, value);

    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->DoAction(widget, isUsed, value);

    if(isUsed)
        return;

    if(widgets_.count(widget) > 0)
    {
        if(TheManager->GetSurfaceInDisplay())
        {
            char buffer[250];
            snprintf(buffer, sizeof(buffer), "Zone -- %s\n", sourceFilePath_.c_str());
            DAW::ShowConsoleMsg(buffer);
        }

        isUsed = true;
        
        for(int i = 0; i < (int)GetActionContexts(widget).size(); ++i)
            GetActionContexts(widget)[i]->DoAction(value);
    }
    else
    {
        for(int i = 0; i < (int)includedZones_.size(); ++i)
            includedZones_[i]->DoAction(widget, isUsed, value);
    }
}

void Zone::UpdateCurrentActionContextModifiers()
{
    for(auto [widget, isUsed] : widgets_)
    {
        UpdateCurrentActionContextModifier(widget);
        widget->Configure(GetActionContexts(widget, currentActionContextModifiers_[widget]));
    }
    
    for(int i = 0; i < (int)includedZones_.size(); ++i)
        includedZones_[i]->UpdateCurrentActionContextModifiers();

    for(auto [key, zones] : subZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->UpdateCurrentActionContextModifiers();
    
    for(auto [key, zones] : associatedZones_)
        for(int i = 0; i < (int)zones.size(); ++i)
            zones[i]->UpdateCurrentActionContextModifiers();
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

vector<shared_ptr<ActionContext>> &Zone::GetActionContexts(Widget *widget)
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

    return defaultContexts_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget
////////////////////////////////////////////////////////////////////////////////////////////////////////
shared_ptr<ZoneManager> Widget::GetZoneManager()
{
    return surface_->GetZoneManager();
}

void Widget::Configure(vector<shared_ptr<ActionContext>> &contexts)
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->Configure(contexts);
}

void  Widget::UpdateValue(map<string, string> &properties, double value)
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->SetValue(properties, value);
}

void  Widget::UpdateValue(map<string, string> &properties, string value)
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->SetValue(properties, value);
}

void Widget::RunDeferredActions()
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->RunDeferredActions();
}

void  Widget::UpdateColorValue(rgba_color color)
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->SetColorValue(color);
}

void Widget::SetXTouchDisplayColors(const string &zoneName, const string &colors)
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->SetXTouchDisplayColors(zoneName, colors);
}

void Widget::RestoreXTouchDisplayColors()
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->RestoreXTouchDisplayColors();
}

void  Widget::ForceClear()
{
    for(int i = 0; i < feedbackProcessors_.GetSize(); ++i)
        feedbackProcessors_.Get(i)->ForceClear();
}

void Widget::LogInput(double value)
{
    if(TheManager->GetSurfaceInDisplay())
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "IN <- %s %s %f\n", GetSurface()->GetName().c_str(), GetName().c_str(), value);
        DAW::ShowConsoleMsg(buffer);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_FeedbackProcessor::SendMidiSysExMessage(MIDI_event_ex_t* midiMessage)
{
    surface_->SendMidiSysExMessage(midiMessage);
}

void Midi_FeedbackProcessor::SendMidiMessage(int first, int second, int third)
{
    if(first != lastMessageSent_->midi_message[0] || second != lastMessageSent_->midi_message[1] || third != lastMessageSent_->midi_message[2])
        ForceMidiMessage(first, second, third);
}

void Midi_FeedbackProcessor::ForceMidiMessage(int first, int second, int third)
{
    lastMessageSent_->midi_message[0] = first;
    lastMessageSent_->midi_message[1] = second;
    lastMessageSent_->midi_message[2] = third;
    surface_->SendMidiMessage(first, second, third);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void OSC_FeedbackProcessor::SetColorValue(rgba_color &color)
{
    if(lastColor_ != color)
    {
        if(lastColor_ != color)
        {
            lastColor_ = color;

            if (surface_->IsX32())
                X32SetColorValue(color);
            else
                surface_->SendOSCMessage(this, oscAddress_ + "/Color", color.to_string());
        }
    }
}

void OSC_FeedbackProcessor::X32SetColorValue(rgba_color &color)
{
    int surfaceColor = 0;
    int r = color.r;
    int g = color.g;
    int b = color.b;

    if (r == 64 && g == 64 && b == 64)                               surfaceColor = 8;    // BLACK
    else if (r > g && r > b)                                         surfaceColor = 1;    // RED
    else if (g > r && g > b)                                         surfaceColor = 2;    // GREEN
    else if (abs(r - g) < 30 && r > b && g > b)                      surfaceColor = 3;    // YELLOW
    else if (b > r && b > g)                                         surfaceColor = 4;    // BLUE
    else if (abs(r - b) < 30 && r > g && b > g)                      surfaceColor = 5;    // MAGENTA
    else if (abs(g - b) < 30 && g > r && b > r)                      surfaceColor = 6;    // CYAN
    else if (abs(r - g) < 30 && abs(r - b) < 30 && abs(g - b) < 30)  surfaceColor = 7;    // WHITE

    string oscAddress = "/ch/";
    if (widget_->GetChannelNumber() < 10)   oscAddress += '0';
    oscAddress += to_string(widget_->GetChannelNumber()) + "/config/color";
    surface_->SendOSCMessage(this, oscAddress, surfaceColor);
}

void OSC_FeedbackProcessor::ForceValue(map<string, string> &properties, double value)
{
    if(DAW::GetCurrentNumberOfMilliseconds() - GetWidget()->GetLastIncomingMessageTime() < 50) // adjust the 50 millisecond value to give you smooth behaviour without making updates sluggish
        return;

    lastDoubleValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_, value);
}

void OSC_FeedbackProcessor::ForceValue(map<string, string> &properties, const string &value)
{
    lastStringValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_, GetWidget()->GetSurface()->GetRestrictedLengthText(value));
}

void OSC_FeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_, 0.0);
    
    lastStringValue_ = "";
    surface_->SendOSCMessage(this, oscAddress_, "");
}

void OSC_IntFeedbackProcessor::ForceValue(map<string, string> &properties, double value)
{
    lastDoubleValue_ = value;
    
    if (surface_->IsX32() && oscAddress_.find("/-stat/selidx") != string::npos)
    {
        if (value != 0.0)
            surface_->SendOSCMessage(this, "/-stat/selidx", widget_->GetChannelNumber() -1);
    }
    else
        surface_->SendOSCMessage(this, oscAddress_, (int)value);
}

void OSC_IntFeedbackProcessor::ForceClear()
{
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_, 0.0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZoneManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ZoneManager::Initialize()
{
    PreProcessZones();

    if(zoneFilePaths_.count("Home") < 1)
    {
        MessageBox(g_hwnd, (surface_->GetName() + " needs a Home Zone to operate, please recheck your installation").c_str(), ("CSI cannot find Home Zone for " + surface_->GetName()).c_str(), MB_OK);
        return;
    }
        
    vector<shared_ptr<Navigator>> navigators;
    navigators.push_back(GetSelectedTrackNavigator());
    vector<shared_ptr<Zone>> dummy; // Needed to satisfy protcol, Home and FocusedFXParam have special Zone handling
    if(sharedThisPtr_ != nullptr)
        ProcessZoneFile(zoneFilePaths_["Home"].filePath, sharedThisPtr_, navigators, dummy, nullptr);
    if(zoneFilePaths_.count("FocusedFXParam") > 0 && sharedThisPtr_ != nullptr)
        ProcessZoneFile(zoneFilePaths_["FocusedFXParam"].filePath, sharedThisPtr_, navigators, dummy, nullptr);
    if(zoneFilePaths_.count("SurfaceFXLayout") > 0)
        ProcessSurfaceFXLayout(zoneFilePaths_["SurfaceFXLayout"].filePath, surfaceFXLayout_, surfaceFXLayoutTemplate_);
    if(zoneFilePaths_.count("FXLayouts") > 0)
        ProcessFXLayouts(zoneFilePaths_["FXLayouts"].filePath, fxLayouts_);
    if(zoneFilePaths_.count("FXPrologue") > 0)
        ProcessFXBoilerplate(zoneFilePaths_["FXPrologue"].filePath, fxPrologue_);
    if(zoneFilePaths_.count("FXEpilogue") > 0)
        ProcessFXBoilerplate(zoneFilePaths_["FXEpilogue"].filePath, fxEpilogue_);
    
    InitializeNoMapZone();
    InitializeFXParamsLearnZone();

    GoHome();
}

void ZoneManager::CheckFocusedFXState()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int fxIndex = 0;
    
    int retval = DAW::GetFocusedFX2(&trackNumber, &itemNumber, &fxIndex);

    if((retval & 1) && (fxIndex > -1))
    {
        MediaTrack* track = DAW::GetTrack(trackNumber);
        
        char fxName[BUFSZ];
        DAW::TrackFX_GetFXName(track, fxIndex, fxName, sizeof(fxName));

        if(learnFXName_ != "" && learnFXName_ != fxName)
        {
            string alias, learnalias;
            GetAlias(fxName,alias);
            GetAlias(learnFXName_.c_str(),learnalias);
            if(MessageBox(NULL, (string("You have now shifted focus to ") + alias + "\n\n" + learnalias + string(" has parameters that have not been saved\n\n Do you want to save them now ?")).c_str(), "Unsaved Learn FX Params", MB_YESNO) == IDYES)
            {
                SaveLearnedFXParams();
            }
            else
            {
                ClearLearnedFXParams();
                GoHome();
            }
        }
    }
    
    if(! isFocusedFXMappingEnabled_)
        return;
            
    if((retval & 1) && (fxIndex > -1))
    {
        int lastRetval = -1;

        if(focusedFXDictionary_.count(trackNumber) > 0 && focusedFXDictionary_[trackNumber].count(fxIndex) > 0)
            lastRetval = focusedFXDictionary_[trackNumber][fxIndex];
        
        if(lastRetval != retval)
        {
            if(retval == 1)
                GoFocusedFX();
            
            else if(retval & 4)
                focusedFXZones_.clear();
            
            if(focusedFXDictionary_[trackNumber].count(trackNumber) < 1)
                focusedFXDictionary_[trackNumber] = map<int, int>();
                               
            focusedFXDictionary_[trackNumber][fxIndex] = retval;;
        }
    }
}

void ZoneManager::AddListener(ControlSurface* surface)
{
    listeners_.push_back(surface->GetZoneManager());
}

void ZoneManager::SetListenerCategories(const string &categoryList)
{
    vector<string> categoryTokens;
    GetTokens(categoryTokens, categoryList);
    
    for(int i = 0; i < (int)categoryTokens.size(); ++i)
    {
        if(categoryTokens[i] == "GoHome")
            listensToGoHome_ = true;
        if(categoryTokens[i] == "Sends")
            listensToSends_ = true;
        if(categoryTokens[i] == "Receives")
            listensToReceives_ = true;
        if(categoryTokens[i] == "FocusedFX")
            listensToFocusedFX_ = true;
        if(categoryTokens[i] == "FocusedFXParam")
            listensToFocusedFXParam_ = true;
        if(categoryTokens[i] == "FXMenu")
            listensToFXMenu_ = true;
        if(categoryTokens[i] == "LocalFXSlot")
            listensToLocalFXSlot_ = true;
        if(categoryTokens[i] == "SelectedTrackFX")
            listensToSelectedTrackFX_ = true;
        if(categoryTokens[i] == "Custom")
            listensToCustom_ = true;
        
        if(categoryTokens[i] == "Modifiers")
            surface_->SetListensToModifiers();
    }
}


void ZoneManager::GoFocusedFX()
{
    focusedFXZones_.clear();
    
    int trackNumber = 0;
    int itemNumber = 0;
    int fxSlot = 0;
    MediaTrack* focusedTrack = nullptr;
    
    if(DAW::GetFocusedFX2(&trackNumber, &itemNumber, &fxSlot) == 1)
    {
        if(trackNumber > 0)
            focusedTrack = DAW::GetTrack(trackNumber);
        else if(trackNumber == 0)
            focusedTrack = GetMasterTrackNavigator()->GetTrack();
    }
    
    if(focusedTrack)
    {
        char FXName[BUFSZ];
        DAW::TrackFX_GetFXName(focusedTrack, fxSlot, FXName, sizeof(FXName));
        
        if(zoneFilePaths_.count(FXName) > 0)
        {
            vector<shared_ptr<Navigator>> navigators;
            navigators.push_back(GetSurface()->GetPage()->GetFocusedFXNavigator());
            
            if(sharedThisPtr_ != nullptr)
                ProcessZoneFile(zoneFilePaths_[FXName].filePath, sharedThisPtr_, navigators, focusedFXZones_, nullptr);
            
            for(int i = 0; i < (int)focusedFXZones_.size(); ++i)
            {
                focusedFXZones_[i]->SetXTouchDisplayColors("White");
                focusedFXZones_[i]->SetSlotIndex(fxSlot);
                focusedFXZones_[i]->Activate();
            }
        }
    }
    else
        for(int i = 0; i < (int)focusedFXZones_.size(); ++i)
            focusedFXZones_[i]->RestoreXTouchDisplayColors();
}

void ZoneManager::GoSelectedTrackFX()
{
    if(homeZone_ != nullptr)
    {
        ClearFXMapping();
        ResetOffsets();
                
        homeZone_->GoAssociatedZone("SelectedTrackFX");
    }

    selectedTrackFXZones_.clear();
    
    if(MediaTrack* selectedTrack = surface_->GetPage()->GetSelectedTrack())
    {
        for(int i = 0; i < DAW::TrackFX_GetCount(selectedTrack); i++)
        {
            char FXName[BUFSZ];
            
            DAW::TrackFX_GetFXName(selectedTrack, i, FXName, sizeof(FXName));
            
            if(zoneFilePaths_.count(FXName) > 0)
            {
                vector<shared_ptr<Navigator>> navigators;
                navigators.push_back(GetSurface()->GetPage()->GetSelectedTrackNavigator());
                if(sharedThisPtr_ != nullptr)
                    ProcessZoneFile(zoneFilePaths_[FXName].filePath, sharedThisPtr_, navigators, selectedTrackFXZones_, nullptr);
                
                selectedTrackFXZones_.back()->SetSlotIndex(i);
                selectedTrackFXZones_.back()->Activate();
            }
        }
    }
}

void ZoneManager::AutoMapFocusedFX()
{
    int trackNumber = 0;
    int itemNumber = 0;
    int fxSlot = 0;
    MediaTrack* track = nullptr;
    
    if(DAW::GetFocusedFX2(&trackNumber, &itemNumber, &fxSlot) == 1)
    {
        if(trackNumber > 0)
            track = DAW::GetTrack(trackNumber);
        
        if(track)
        {
            char fxName[BUFSZ];
            DAW::TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));
            if( ! TheManager->HaveFXSteppedValuesBeenCalculated(fxName))
                CalculateSteppedValues(fxName, track, fxSlot);
            AutoMapFX(fxName, track, fxSlot);
        }
    }
}

void ZoneManager::GoLearnFXParams(MediaTrack* track, int fxSlot)
{
    if(homeZone_ != nullptr)
    {
        ClearFXMapping();
        ResetOffsets();
                
        homeZone_->GoAssociatedZone("LearnFXParams");
    }
    
    if(track)
    {
        char fxName[BUFSZ];
        DAW::TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));
        
        if(zoneFilePaths_.count(fxName) > 0)
        {
            ifstream file(zoneFilePaths_[fxName].filePath);
             
            string line = "";
            
            if(getline(file, line))
            {
                vector<string> tokens;
                GetTokens(tokens, line);

                if(tokens.size() > 3 && tokens[3] == s_GeneratedByLearn)
                {
                    learnFXName_ = fxName;
                    GetExistingZoneParamsForLearn(fxName, track, fxSlot);
                    file.close();
                }
                else
                {
                    file.close();
                    
                    if(MessageBox(NULL, (zoneFilePaths_[fxName].alias + " already exists\n\n Do you want to delete it permanently and go into LearnMode ?").c_str(), "Zone Already Exists", MB_YESNO) == IDYES)
                    {
                        ClearLearnedFXParams();
                        RemoveZone(fxName);
                    }
                    else
                    {
                        return;
                    }
                }
            }
        }
    }
}

void ZoneManager::GoFXSlot(MediaTrack* track, shared_ptr<Navigator> navigator, int fxSlot)
{
    if(fxSlot > DAW::TrackFX_GetCount(track) - 1)
        return;
    
    char fxName[BUFSZ];
    
    DAW::TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));
    
    if( ! TheManager->HaveFXSteppedValuesBeenCalculated(fxName))
        CalculateSteppedValues(fxName, track, fxSlot);

    if(zoneFilePaths_.count(fxName) > 0)
    {
        vector<shared_ptr<Navigator>> navigators;
        navigators.push_back(navigator);
        
        if(sharedThisPtr_ != nullptr)
            ProcessZoneFile(zoneFilePaths_[fxName].filePath, sharedThisPtr_, navigators, fxSlotZones_, nullptr);
        
        if(fxSlotZones_.size() > 0)
        {
            fxSlotZones_.back()->SetSlotIndex(fxSlot);
            fxSlotZones_.back()->Activate();
        }
    }
    else if(noMapZone_ != nullptr)
    {
        DAW::TrackFX_SetOpen(track, fxSlot, true);
        
        noMapZone_->SetSlotIndex(fxSlot);
        noMapZone_->Activate();
    }
}

void ZoneManager::UpdateCurrentActionContextModifiers()
{    
    if(focusedFXParamZone_ != nullptr)
        focusedFXParamZone_->UpdateCurrentActionContextModifiers();
    
    for(int i = 0; i < (int)focusedFXZones_.size(); ++i)
        focusedFXZones_[i]->UpdateCurrentActionContextModifiers();
    
    for(int i = 0; i < (int)selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->UpdateCurrentActionContextModifiers();
    
    for(int i = 0; i < (int)fxSlotZones_.size(); ++i)
        fxSlotZones_[i]->UpdateCurrentActionContextModifiers();
    
    if(homeZone_ != nullptr)
        homeZone_->UpdateCurrentActionContextModifiers();
}

void ZoneManager::EraseLastTouchedControl()
{
    if(lastTouched_ != nullptr)
    {
        if(fxLayout_ != nullptr && fxLayoutFileLines_.size() > 0)
        {
            Widget *widget = lastTouched_->fxParamWidget;
            if(widget)
            {
                for(int i = 0; i < (int)fxLayout_->GetActionContexts(widget).size(); ++i)
                    SetParamNum(widget, 1);
                
                int modifier = fxLayout_->GetModifier(widget);
                
                if(controlDisplayAssociations_.count(modifier) > 0 && controlDisplayAssociations_[modifier].count(widget) > 0)
                    SetParamNum(controlDisplayAssociations_[modifier][widget], 1);
            }
        }

        lastTouched_->isLearned = false;
        lastTouched_->paramNumber = 0;
        lastTouched_->paramName = "";
        lastTouched_->params = "";
        lastTouched_->track = nullptr;
        lastTouched_->fxSlotNum = 0;
        
        lastTouched_ = nullptr;
    }
}

void ZoneManager::SaveTemplatedFXParams()
{
    if(learnFXName_ != "" && fxLayout_ != nullptr && fxLayoutFileLines_.size() > 0)
    {
        size_t pos = 0;
        while ((pos = fxLayoutFileLines_[0].find(fxLayout_->GetName(), pos)) != std::string::npos)
        {
            fxLayoutFileLines_[0].replace(pos, fxLayout_->GetName().length(), learnFXName_);
            pos += learnFXName_.length();
        }
        
        string alias;
        GetAlias(learnFXName_.c_str(), alias);

        fxLayoutFileLines_[0] += " \"" + alias + "\" \n\n";
        
        string path = "";
         
        if(zoneFilePaths_.count(learnFXName_) > 0)
        {
            path = zoneFilePaths_[learnFXName_].filePath;
            alias = zoneFilePaths_[learnFXName_].alias;
        }
        else
        {
            path = DAW::GetResourcePath() + string("/CSI/Zones/") + fxZoneFolder_ + "/TemplatedFXZones";
            
            RecursiveCreateDirectory(path.c_str(),0);

            GetAlias(learnFXName_.c_str(),alias);
            
            path += "/" + regex_replace(learnFXName_, regex(s_BadFileChars), "_") + ".zon";
            
            CSIZoneInfo info;
            info.filePath = path;
            info.alias = alias;
            
            AddZoneFilePath(learnFXName_, info);
            surface_->GetPage()->AddZoneFilePath(surface_, fxZoneFolder_, learnFXName_, info);
        }
        
        ofstream fxZone(path);

        if(fxZone.is_open())
        {
            for(int i = 0; i < (int)fxLayoutFileLines_.size(); ++i)
            {
                string ending = "";
                
                string lineEnding = "\n";

                if(fxLayoutFileLines_[i].length() >= lineEnding.length())
                    ending = fxLayoutFileLines_[i].substr(fxLayoutFileLines_[i].length() - lineEnding.length(), lineEnding.length());

                if(ending[ending.length() - 1] == '\r')
                    fxLayoutFileLines_[i] = fxLayoutFileLines_[i].substr(0, fxLayoutFileLines_[i].length() - 1);
                
                if(ending != lineEnding)
                    fxLayoutFileLines_[i] += "\n";
                
                fxZone << fxLayoutFileLines_[i];
            }
            
            fxZone.close();
        }

        ClearLearnedFXParams();
        GoHome();
    }
}

void ZoneManager::SaveLearnedFXParams()
{
    if(learnFXName_ != "")
    {
        string path = "";
        string alias = "";
        
        if(zoneFilePaths_.count(learnFXName_) > 0)
        {
            path = zoneFilePaths_[learnFXName_].filePath;
            alias = zoneFilePaths_[learnFXName_].alias;
        }
        else
        {
            path = DAW::GetResourcePath() + string("/CSI/Zones/") + fxZoneFolder_ + "/AutoGeneratedFXZones";
            
            RecursiveCreateDirectory(path.c_str(),0);

            GetAlias(learnFXName_.c_str(),alias);
            
            path += "/" + regex_replace(learnFXName_, regex(s_BadFileChars), "_") + ".zon";
            
            CSIZoneInfo info;
            info.filePath = path;
            info.alias = alias;
            
            AddZoneFilePath(learnFXName_, info);
            surface_->GetPage()->AddZoneFilePath(surface_, fxZoneFolder_, learnFXName_, info);
        }
        
        string nameDisplayParams = "";
        string valueDisplayParams = "";

        if(surfaceFXLayout_.size() > 2)
        {
            if(surfaceFXLayout_[1].size() > 2)
                for(int i = 2; i < surfaceFXLayout_[1].size(); i++)
                    nameDisplayParams += " " + surfaceFXLayout_[1][i];

            if(surfaceFXLayout_[2].size() > 2)
                for(int i = 2; i < surfaceFXLayout_[2].size(); i++)
                    valueDisplayParams += " " + surfaceFXLayout_[2][i];
        }
        
        ofstream fxZone(path);

        if(fxZone.is_open())
        {
            fxZone << "Zone \"" + learnFXName_ + "\" \"" + alias + "\" \"" + s_GeneratedByLearn + "\"\n";
            
            for(int i = 0; i < (int)fxPrologue_.size(); ++i)
                fxZone << "\t" + fxPrologue_[i] + "\n";
                   
            fxZone << "\n" + s_BeginAutoSection + "\n";

            if(homeZone_->GetLearnFXParamsZone())
            {
                for(auto [modifier, widgetCells] : homeZone_->GetLearnFXParamsZone()->GetLearnFXCells())
                {
                    string modifierStr = ModifierManager::GetModifierString(modifier);
                    
                    for(auto [address, cell] : widgetCells)
                    {
                        bool cellHasDisplayWidgetsDefined = false;
                        
                        for(int i = 0; i < cell.fxParamWidgets.size(); i++)
                        {
                            shared_ptr<LearnInfo> info = GetLearnInfo(cell.fxParamWidgets[i], modifier);
                            
                            if(info == nullptr)
                                continue;
                            
                            if(info->isLearned)
                            {
                                cellHasDisplayWidgetsDefined = true;
                                
                                fxZone << "\t" + modifierStr + cell.fxParamWidgets[i]->GetName() + "\tFXParam " + to_string(info->paramNumber) + " " + info->params + "\n";
                                fxZone << "\t" + modifierStr + cell.fxParamNameDisplayWidget->GetName() + "\tFixedTextDisplay \"" + info->paramName + "\"" + nameDisplayParams + "\n";
                                fxZone << "\t" + modifierStr + cell.fxParamValueDisplayWidget->GetName() + "\tFXParamValueDisplay " + to_string(info->paramNumber) + valueDisplayParams + "\n\n";
                            }
                            else if(i == cell.fxParamWidgets.size() - 1 && ! cellHasDisplayWidgetsDefined)
                            {
                                fxZone << "\t" + modifierStr + cell.fxParamWidgets[i]->GetName() + "\tNoAction\n";
                                fxZone << "\t" + modifierStr + cell.fxParamNameDisplayWidget->GetName() + "\tNoAction\n";
                                fxZone << "\t" + modifierStr + cell.fxParamValueDisplayWidget->GetName() + "\tNoAction\n\n";
                            }
                            else
                            {
                                fxZone << "\t" + modifierStr + cell.fxParamWidgets[i]->GetName() + "\tNoAction\n";
                                fxZone << "\tNullDisplay\tNoAction\n";
                                fxZone << "\tNullDisplay\tNoAction\n\n";
                            }
                        }
                        
                        fxZone << "\n";
                    }
                }
            }
            
            fxZone << s_EndAutoSection + "\n";
                    
            for(int i = 0; i < (int)fxEpilogue_.size(); ++i)
                fxZone << "\t" + fxEpilogue_[i] + "\n";

            fxZone << "ZoneEnd\n\n";
            
            for(int i = 0; i < (int)paramList_.size(); ++i)
                fxZone << paramList_[i] + "\n";
            
            fxZone.close();
        }
        
        ClearLearnedFXParams();
        GoHome();
    }
}

shared_ptr<LearnInfo> ZoneManager::GetLearnInfo(Widget *widget)
{
    vector<int> modifiers = surface_->GetModifiers();

    if(modifiers.size() > 0)
        return GetLearnInfo(widget, modifiers[0]);
    else
        return  shared_ptr<LearnInfo>(nullptr);
}

shared_ptr<LearnInfo> ZoneManager::GetLearnInfo(Widget *widget, int modifier)
{
    if(learnedFXParams_.count(widget) > 0 && learnedFXParams_[widget].count(modifier) > 0)
        return learnedFXParams_[widget][modifier];
    else
        return shared_ptr<LearnInfo>(nullptr);
}

void ZoneManager::GetWidgetNameAndModifiers(const string &line, int listSlotIndex, string &cell, string &paramWidgetName, string &paramWidgetFullName, vector<string> &modifiers, int &modifier, vector<FXParamLayoutTemplate> &layoutTemplates)
{
    istringstream modifiersAndWidgetName(line);
    string modifiersAndWidgetNameToken;
    
    while (getline(modifiersAndWidgetName, modifiersAndWidgetNameToken, '+'))
        modifiers.push_back(modifiersAndWidgetNameToken);
        
    modifier = GetModifierValue(modifiers);
    
    paramWidgetFullName = modifiers[modifiers.size() - 1];

    paramWidgetName = paramWidgetFullName.substr(0, paramWidgetFullName.length() - layoutTemplates[listSlotIndex].suffix.length());
    
    cell = layoutTemplates[listSlotIndex].suffix;
}

int ZoneManager::GetModifierValue(const vector<string> &modifierTokens)
{
    ModifierManager modifierManager;

    return modifierManager.GetModifierValue(modifierTokens);
}

void ZoneManager::InitializeNoMapZone()
{
    if(surfaceFXLayout_.size() != 3)
        return;
    
    if(GetZoneFilePaths().count("NoMap") > 0)
    {
        vector<shared_ptr<Navigator>> navigators;
        navigators.push_back(GetSelectedTrackNavigator());
        
        vector<shared_ptr<Zone>> zones;
        
        ProcessZoneFile(GetZoneFilePaths()["NoMap"].filePath, sharedThisPtr_, navigators, zones, nullptr);
        
        if(zones.size() > 0)
            noMapZone_ = zones[0];
        
        if(noMapZone_ != nullptr)
        {
            vector<Widget *> usedWidgets;
            
            for(auto [widget, isUsed] : noMapZone_->GetWidgets())
                usedWidgets.push_back(widget);
            
            vector<string> paramWidgets;

            for(int i = 0; i < (int)surfaceFXLayoutTemplate_.size(); ++i)
                if(surfaceFXLayoutTemplate_[i].size() > 0 && surfaceFXLayoutTemplate_[i][0] == "WidgetTypes")
                    for(int i = 1; i < surfaceFXLayoutTemplate_[i].size(); i++)
                        paramWidgets.push_back(surfaceFXLayoutTemplate_[i][i]);
            
            string nameDisplayWidget = "";
            if(surfaceFXLayout_[1].size() > 0)
                nameDisplayWidget = surfaceFXLayout_[1][0];
            
            string valueDisplayWidget = "";
            if(surfaceFXLayout_[2].size() > 0)
                valueDisplayWidget = surfaceFXLayout_[2][0];

            for(int i = 0; i < (int)fxLayouts_.size(); ++i)
            {
                int modifier = GetModifierValue(fxLayouts_[i].GetModifierTokens());
                
                if(modifier != 0)
                    continue;
                
                for(int j = 1; j <= fxLayouts_[i].channelCount; j++)
                {
                    string cellAdress = fxLayouts_[i].suffix + to_string(j);
                    
                    Widget *widget = GetSurface()->GetWidgetByName(nameDisplayWidget + cellAdress);
                    if(widget == NULL || find(usedWidgets.begin(), usedWidgets.end(), widget) != usedWidgets.end())
                        continue;
                    noMapZone_->AddWidget(widget, widget->GetName());
                    shared_ptr<ActionContext> context = TheManager->GetActionContext("NoAction", widget, noMapZone_, 0);
                    context->SetProvideFeedback(true);
                    noMapZone_->AddActionContext(widget, modifier, context);

                    widget = GetSurface()->GetWidgetByName(valueDisplayWidget + cellAdress);
                    if(widget == nullptr || find(usedWidgets.begin(), usedWidgets.end(), widget) != usedWidgets.end())
                        continue;
                    noMapZone_->AddWidget(widget, widget->GetName());
                    context = TheManager->GetActionContext("NoAction", widget, noMapZone_, 0);
                    context->SetProvideFeedback(true);
                    noMapZone_->AddActionContext(widget, modifier, context);
                    
                    for(int k = 0; k < (int)paramWidgets.size(); ++k)
                    {
                        Widget *widget = GetSurface()->GetWidgetByName(paramWidgets[k] + cellAdress);
                        if(widget == NULL || find(usedWidgets.begin(), usedWidgets.end(), widget) != usedWidgets.end())
                            continue;
                        noMapZone_->AddWidget(widget, widget->GetName());
                        context = TheManager->GetActionContext("NoAction", widget, noMapZone_, 0);
                        noMapZone_->AddActionContext(widget, modifier, context);
                    }
                }
            }
        }
    }
}

void ZoneManager::InitializeFXParamsLearnZone()
{
    if(surfaceFXLayout_.size() != 3)
        return;
    
    if(homeZone_ != nullptr)
    {
        if(shared_ptr<Zone> zone = homeZone_->GetLearnFXParamsZone())
        {
            vector<string> paramWidgets;
            vector<string> widgetParams;

            for(int i = 0; i < (int)surfaceFXLayoutTemplate_.size(); ++i)
                if(surfaceFXLayoutTemplate_[i].size() > 0 && surfaceFXLayoutTemplate_[i][0] == "WidgetTypes")
                    for(int j = 1; j < surfaceFXLayoutTemplate_[i].size(); j++)
                        paramWidgets.push_back(surfaceFXLayoutTemplate_[i][j]);

            if(surfaceFXLayout_[0].size() > 2)
                for(int i = 2; i < surfaceFXLayout_[0].size(); i++)
                    widgetParams.push_back(surfaceFXLayout_[0][i]);
            
            
            string nameDisplayWidget = "";
            vector<string> nameDisplayParams;

            if(surfaceFXLayout_[1].size() > 0)
                nameDisplayWidget = surfaceFXLayout_[1][0];

            if(surfaceFXLayout_[1].size() > 2)
                for(int i = 2; i < surfaceFXLayout_[1].size(); i++)
                    nameDisplayParams.push_back(surfaceFXLayout_[1][i]);

            
            string valueDisplayWidget = "";
            vector<string> valueDisplayParams;

            if(surfaceFXLayout_[2].size() > 0)
                valueDisplayWidget = surfaceFXLayout_[2][0];

            if(surfaceFXLayout_[2].size() > 2)
                for(int i = 2; i < surfaceFXLayout_[2].size(); i++)
                    valueDisplayParams.push_back(surfaceFXLayout_[2][i]);

            if(paramWidgets.size() > 0)
            {
                for(int i = 0; i < (int)fxLayouts_.size(); ++i)
                {
                    int modifier = GetModifierValue(fxLayouts_[i].GetModifierTokens());
                    
                    for(int j = 1; j <= fxLayouts_[i].channelCount; j++)
                    {
                        LearnFXCell cell;
                        
                        string cellAdress = fxLayouts_[i].suffix + to_string(j);
                        
                        Widget *widget = GetSurface()->GetWidgetByName(nameDisplayWidget + cellAdress);
                        if(widget == NULL)
                            continue;
                        cell.fxParamNameDisplayWidget = widget;
                        zone->AddWidget(widget, widget->GetName());
                        shared_ptr<ActionContext> context = TheManager->GetLearnFXActionContext("LearnFXParamNameDisplay", widget, zone, nameDisplayParams);
                        context->SetProvideFeedback(true);
                        context->SetCellAddress(cellAdress);
                        zone->AddActionContext(widget, modifier, context);

                        widget = GetSurface()->GetWidgetByName(valueDisplayWidget + cellAdress);
                        if(widget == NULL)
                            continue;
                        cell.fxParamValueDisplayWidget = widget;
                        zone->AddWidget(widget, widget->GetName());
                        context = TheManager->GetLearnFXActionContext("LearnFXParamValueDisplay", widget, zone, valueDisplayParams);
                        context->SetProvideFeedback(true);
                        context->SetCellAddress(cellAdress);
                        zone->AddActionContext(widget, modifier, context);
                        
                        for(int k = 0; k < (int)paramWidgets.size(); ++k)
                        {
                            Widget *widget = GetSurface()->GetWidgetByName(paramWidgets[k] + cellAdress);
                            if(widget == NULL)
                                continue;
                            cell.fxParamWidgets.push_back(widget);
                            zone->AddWidget(widget, widget->GetName());
                            context = TheManager->GetLearnFXActionContext("LearnFXParam", widget, zone, widgetParams);
                            context->SetProvideFeedback(true);
                            zone->AddActionContext(widget, modifier, context);
                            shared_ptr<LearnInfo> info = make_shared<LearnInfo>(widget, cellAdress);
                            learnedFXParams_[widget][modifier] = info;
                        }
                        
                        zone->AddLearnFXCell(modifier, cellAdress, cell);
                    }
                }
            }
        }
    }
}

void ZoneManager::GetExistingZoneParamsForLearn(const string &fxName, MediaTrack* track, int fxSlotNum)
{
    zoneDef_.fullPath = zoneFilePaths_[fxName].filePath;
    vector<FXParamLayoutTemplate> layoutTemplates = GetFXLayoutTemplates();
        
    UnpackZone(zoneDef_, layoutTemplates);
    
    for(int i = 0; i < (int)zoneDef_.paramDefs.size(); ++i)
    {
        for(int j = 0; j < (int)zoneDef_.paramDefs[i].definitions.size(); ++j)
        {
            Widget *widget = surface_->GetWidgetByName(zoneDef_.paramDefs[i].definitions[j].paramWidgetFullName);
            if (widget)
            {
                if(shared_ptr<LearnInfo> info = GetLearnInfo(widget, zoneDef_.paramDefs[i].definitions[j].modifier))
                {
                    if(zoneDef_.paramDefs[i].definitions[j].paramNumber != "" && zoneDef_.paramDefs[i].definitions[j].paramNameDisplayWidget != "NullDisplay")
                    {
                        info->isLearned = true;
                        info->paramName = zoneDef_.paramDefs[i].definitions[j].paramName;
                        info->track = track;
                        info->fxSlotNum =fxSlotNum;
                        info->paramNumber = stoi(zoneDef_.paramDefs[i].definitions[j].paramNumber);

                        if(zoneDef_.paramDefs[i].definitions[j].steps.size() > 0)
                        {
                            info->params = "[ ";
                            
                            for(int k = 0; k < (int)zoneDef_.paramDefs[i].definitions[j].steps.size(); ++k)
                                info->params += zoneDef_.paramDefs[i].definitions[j].steps[k] + "  ";
                            
                            info->params += "]";
                            
                            if(shared_ptr<Zone> learnZone = homeZone_->GetLearnFXParamsZone())
                            {
                                vector<double> steps;
                                
                                for(int k = 0; k < (int)zoneDef_.paramDefs[i].definitions[j].steps.size(); ++k)
                                    steps.push_back(stod(zoneDef_.paramDefs[i].definitions[j].steps[k]));
                                
                                for(int k = 0; k < (int)learnZone->GetActionContexts(widget, zoneDef_.paramDefs[i].definitions[j].modifier).size(); ++k)
                                    learnZone->GetActionContexts(widget, zoneDef_.paramDefs[i].definitions[j].modifier)[k]->SetStepValues(steps);
                            }
                        }
                        
                        if(zoneDef_.paramDefs[i].definitions[j].paramWidget.find("Rotary") != string::npos && zoneDef_.paramDefs[i].definitions[j].paramWidget.find("Push") == string::npos)
                        {
                            if(surfaceFXLayout_.size() > 0 && surfaceFXLayout_[0].size() > 2 && surfaceFXLayout_[0][0] == "Rotary")
                                for(int i = 2; i < surfaceFXLayout_[0].size(); i++)
                                    info->params += " " + surfaceFXLayout_[0][i];
                        }
                    }
                }
            }
        }
    }
}

void ZoneManager::GoFXLayoutZone(const string &zoneName, int slotIndex)
{
    if(noMapZone_ != nullptr)
        noMapZone_->Deactivate();

    if(homeZone_ != nullptr)
    {
        ClearFXMapping();

        fxLayoutFileLines_.clear();
        fxLayoutFileLinesOriginal_.clear();

        controlDisplayAssociations_.clear();
        
        homeZone_->GoAssociatedZone(zoneName, slotIndex);
        
        fxLayout_ = homeZone_->GetFXLayoutZone(zoneName);
        
        if(zoneFilePaths_.count(zoneName) > 0 && fxLayout_ != nullptr)
        {
            ifstream file(zoneFilePaths_[zoneName].filePath);
            
            for (string line; getline(file, line) ; )
            {
                if(line.find("|") != string::npos && fxLayoutFileLines_.size() > 0)
                {
                    vector<string> tokens;
                    GetTokens(tokens, line);

                    if(tokens.size() > 1 && tokens[1] == "FXParamValueDisplay") // This line is a display definition
                    {
                        if(fxLayoutFileLines_.back().find("|") != string::npos)
                        {
                            vector<string> previousLineTokens;
                            GetTokens(previousLineTokens, fxLayoutFileLines_.back());

                            if(previousLineTokens.size() > 1 && previousLineTokens[1] == "FXParam") // The previous line was a control Widget definition
                            {
                                istringstream ControlModifiersAndWidgetName(previousLineTokens[0]);
                                vector<string> modifierTokens;
                                string modifierToken;
                                
                                while (getline(ControlModifiersAndWidgetName, modifierToken, '+'))
                                    modifierTokens.push_back(modifierToken);
                                
                                int modifier = surface_->GetModifierManager()->GetModifierValue(modifierTokens);

                                Widget *controlWidget = surface_->GetWidgetByName(modifierTokens[modifierTokens.size() - 1]);
                                
                                istringstream displayModifiersAndWidgetName(tokens[0]);

                                modifierTokens.clear();
                                
                                while (getline(displayModifiersAndWidgetName, modifierToken, '+'))
                                    modifierTokens.push_back(modifierToken);

                                Widget *displayWidget = surface_->GetWidgetByName(modifierTokens[modifierTokens.size() - 1]);

                                if(controlWidget && displayWidget)
                                    controlDisplayAssociations_[modifier][controlWidget] = displayWidget;
                            }
                        }
                    }
                }
                
                fxLayoutFileLines_.push_back(line);
                fxLayoutFileLinesOriginal_.push_back(line);
            }
        }
    }
}

void ZoneManager::WidgetMoved(ActionContext* context)
{
    if(fxLayoutFileLines_.size() < 1)
        return;
    
    if(context->GetZone() != fxLayout_)
        return;
    
    MediaTrack* track = nullptr;
    
    shared_ptr<LearnInfo> info = GetLearnInfo(context->GetWidget());
    
    if(info == nullptr)
        return;
    
    if(! info->isLearned)
    {
        int trackNum = 0;
        int fxSlotNum = 0;
        int fxParamNum = 0;

        if(DAW::GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            track = DAW::GetTrack(trackNum);
            
            if(track == nullptr)
                return;
            
            char fxName[BUFSZ];
            DAW::TrackFX_GetFXName(track, fxSlotNum, fxName, sizeof(fxName));
            learnFXName_ = fxName;
                                                                    
            string paramStr = "";
            
            if(context->GetWidget()->GetName().find("Fader") == string::npos)
            {
                if(TheManager->GetSteppedValueCount(fxName, fxParamNum) == 0)
                    context->GetSurface()->GetZoneManager()->CalculateSteppedValue(fxName, track, fxSlotNum, fxParamNum);
                
                int numSteps = TheManager->GetSteppedValueCount(fxName, fxParamNum);
                
                if(context->GetWidget()->GetName().find("Push") != string::npos)
                {
                    if(numSteps == 0)
                        numSteps = 2;
                }
                
                if(numSteps > 1)
                {
                    vector<double> stepValues;
                    GetParamStepsValues(stepValues, numSteps);
                    context->SetStepValues(stepValues);

                    string steps;
                    GetParamStepsString(steps, numSteps);
                    paramStr = "[ " + steps + "]";
                }
            }
           
            Widget *widget = context->GetWidget();
            
            SetParamNum(widget, fxParamNum);
            
            int modifier = fxLayout_->GetModifier(widget);
            
            if(controlDisplayAssociations_.count(modifier) > 0 && controlDisplayAssociations_[modifier].count(widget) > 0)
                SetParamNum(controlDisplayAssociations_[modifier][widget], fxParamNum);

            info->isLearned = true;
            info->paramNumber = fxParamNum;
            info->paramName = DAW::TrackFX_GetParamName(DAW::GetTrack(trackNum), fxSlotNum, fxParamNum);
            info->params = paramStr;
            info->track = DAW::GetTrack(trackNum);
            info->fxSlotNum = fxSlotNum;
        }
    }
    
    lastTouched_ = info;
}

void ZoneManager::SetParamNum(Widget *widget, int fxParamNum)
{
    fxLayout_->SetFXParamNum(widget, fxParamNum);

    // Now modify the in memory file
    
    int modifier = fxLayout_->GetModifier(widget);

    int index = 0;
    
    for(string &line : fxLayoutFileLines_)
    {
        if(line.find(widget->GetName()) != string::npos)
        {
            istringstream fullLine(line);
            vector<string> tokens;
            string token;
            
            while (getline(fullLine, token, '+'))
                tokens.push_back(token);

            if(tokens.size() < 1)
                continue;
            
            istringstream modifiersAndWidgetName(tokens[0]);
            
            tokens.clear();

            while (getline(modifiersAndWidgetName, token, '+'))
                tokens.push_back(token);

            int lineModifier = surface_->GetModifierManager()->GetModifierValue(tokens);

            if(modifier == lineModifier)
            {
                if(line.find("|") == string::npos)
                {
                    line = fxLayoutFileLinesOriginal_[index];
                }
                else
                {
                    istringstream layoutLine(line);
                    vector<string> lineTokens;
                    string token;
                    
                    ModifierManager modifierManager;
                    
                    while (getline(layoutLine, token, '|'))
                        lineTokens.push_back(token);
                    
                    string replacementString = " " + to_string(fxParamNum) + " ";
                    
                    if(widget && lineTokens.size() > 1)
                    {
                        shared_ptr<LearnInfo> info = GetLearnInfo(widget);
                        
                        if(info != nullptr && info->params.length() != 0 && lineTokens[1].find("[") == string::npos)
                            replacementString += " " + info->params + " ";
                    }
                    
                    if(lineTokens.size() > 0)
                        line = lineTokens[0] + replacementString + (lineTokens.size() > 1 ? lineTokens[1] : "");
                }
            }
        }
        
        index++;
    }
}

void ZoneManager::DoLearn(ActionContext* context, double value)
{    
    if(value == 0.0)
        return;
    
    int trackNum = 0;
    int fxSlotNum = 0;
    int fxParamNum = 0;

    MediaTrack* track = nullptr;
    
    shared_ptr<LearnInfo> info = GetLearnInfo(context->GetWidget());
    
    if(info == nullptr)
        return;
    
    if(! info->isLearned)
    {
        if(DAW::GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            track = DAW::GetTrack(trackNum);
            
            char fxName[BUFSZ];
            DAW::TrackFX_GetFXName(track, fxSlotNum, fxName, sizeof(fxName));
            
            string paramName = DAW::TrackFX_GetParamName(track, fxSlotNum, fxParamNum);
            
            if(paramList_.size() == 0)
                for(int i = 0; i < DAW::TrackFX_GetNumParams(track, fxSlotNum); i++)
                    paramList_.push_back(to_string(i) + " " + DAW::TrackFX_GetParamName(track, fxSlotNum, i));
                                            
            string paramStr = "";
            
            if(context->GetWidget()->GetName().find("Fader") == string::npos)
            {
                if(TheManager->GetSteppedValueCount(fxName, fxParamNum) == 0)
                    context->GetSurface()->GetZoneManager()->CalculateSteppedValue(fxName, track, fxSlotNum, fxParamNum);
                
                int numSteps = TheManager->GetSteppedValueCount(fxName, fxParamNum);
                
                if(context->GetWidget()->GetName().find("Push") != string::npos)
                {
                    if(numSteps == 0)
                        numSteps = 2;
                }

                if(numSteps > 1)
                {
                    vector<double> stepValues;
                    GetParamStepsValues(stepValues, numSteps);
                    context->SetStepValues(stepValues);

                    string steps;
                    GetParamStepsString(steps, numSteps);
                    paramStr = "[ " + steps + "]";
                }
                
                if(context->GetWidget()->GetName().find("Rotary") != string::npos && context->GetWidget()->GetName().find("Push") == string::npos)
                {
                    if(surfaceFXLayout_.size() > 0 && surfaceFXLayout_[0].size() > 2 && surfaceFXLayout_[0][0] == "Rotary")
                        for(int i = 2; i < surfaceFXLayout_[0].size(); i++)
                            paramStr += " " + surfaceFXLayout_[0][i];
                }
            }
           
            int currentModifier = 0;
            
            if(surface_->GetModifiers().size() > 0)
                currentModifier = surface_->GetModifiers()[0];

            for(auto [widget, modifiers] : learnedFXParams_)
            {
                for(auto [modifier, widgetInfo] : modifiers)
                {
                    if(modifier == currentModifier && widgetInfo->cellAddress == info->cellAddress)
                    {
                        widgetInfo->isLearned = false;
                        widgetInfo->paramNumber = 0;
                        widgetInfo->paramName = "";
                        widgetInfo->params = "";
                        widgetInfo->track = nullptr;
                        widgetInfo->fxSlotNum = 0;
                    }
                }
            }

            info->isLearned = true;
            info->paramNumber = fxParamNum;
            info->paramName = DAW::TrackFX_GetParamName(DAW::GetTrack(trackNum), fxSlotNum, fxParamNum);
            info->params = paramStr;
            info->track = DAW::GetTrack(trackNum);
            info->fxSlotNum = fxSlotNum;
        }
    }
    else
    {
        lastTouched_ = info;
                       
        DAW::TrackFX_SetParam(info->track, info->fxSlotNum, info->paramNumber, value);
    }
}

void ZoneManager::RemapAutoZone()
{
    if(focusedFXZones_.size() == 1)
    {
        if(::RemapAutoZoneDialog(sharedThisPtr_, focusedFXZones_[0]->GetSourceFilePath()))
        {
            PreProcessZoneFile(focusedFXZones_[0]->GetSourceFilePath(), sharedThisPtr_);
            GoFocusedFX();
        }
    }
    else if(fxSlotZones_.size() == 1)
    {
        if(::RemapAutoZoneDialog(sharedThisPtr_, fxSlotZones_[0]->GetSourceFilePath()))
        {
            vector<shared_ptr<Navigator>> navigators;
            navigators.push_back(fxSlotZones_[0]->GetNavigator());
            
            string filePath = fxSlotZones_[0]->GetSourceFilePath();
            int slotNumber = fxSlotZones_[0]->GetSlotIndex();

            fxSlotZones_.clear();
            
            PreProcessZoneFile(filePath, sharedThisPtr_);
            ProcessZoneFile(filePath, sharedThisPtr_, navigators, fxSlotZones_, nullptr);
            
            fxSlotZones_.back()->SetSlotIndex(slotNumber);
            fxSlotZones_.back()->Activate();
        }
    }
}

void ZoneManager::PreProcessZones()
{
    vector<string> zoneFilesToProcess;
    listFilesOfType(DAW::GetResourcePath() + string("/CSI/Zones/") + zoneFolder_ + "/", zoneFilesToProcess, ".zon"); // recursively find all .zon files, starting at zoneFolder
       
    if(zoneFilesToProcess.size() == 0)
    {
        MessageBox(g_hwnd, (string("Please check your installation, cannot find Zone files in ") + DAW::GetResourcePath() + string("/CSI/Zones/") + zoneFolder_).c_str(), (GetSurface()->GetName() + " Zone folder is missing or empty").c_str(), MB_OK);

        return;
    }
      
    if(sharedThisPtr_ != nullptr)
        for(int i = 0; i < (int)zoneFilesToProcess.size(); ++i)
            PreProcessZoneFile(zoneFilesToProcess[i], sharedThisPtr_);
    
    if(zoneFolder_ != fxZoneFolder_)
    {
        zoneFilesToProcess.clear();
        
        listFilesOfType(DAW::GetResourcePath() + string("/CSI/Zones/") + fxZoneFolder_ + "/", zoneFilesToProcess, ".zon"); // recursively find all .zon files, starting at fxZoneFolder
         
        if(sharedThisPtr_ != nullptr)
            for(int i = 0; i < (int)zoneFilesToProcess.size(); ++i)
                PreProcessZoneFile(zoneFilesToProcess[i], sharedThisPtr_);
    }
}

void ZoneManager::CalculateSteppedValue(const string &fxName, MediaTrack* track, int fxIndex, int paramIndex)
{
    // Check for UAD / Plugin Alliance and bail if neither
    if(fxName.find("UAD") == string::npos && fxName.find("Plugin Alliance") == string::npos)
        return;
    
    bool wasMuted = false;
    DAW::GetTrackUIMute(track, &wasMuted);
    
    if( ! wasMuted)
        DAW::CSurf_SetSurfaceMute(track, DAW::CSurf_OnMuteChange(track, true), NULL);

    double minvalOut = 0.0;
    double maxvalOut = 0.0;

    double currentValue;

    currentValue = DAW::TrackFX_GetParam(track, fxIndex, paramIndex, &minvalOut, &maxvalOut);
    
        int stepCount = 1;
        double stepValue = 0.0;
        
        for(double value = 0.0; value < 1.01; value += .01)
        {
            DAW::TrackFX_SetParam(track, fxIndex, paramIndex, value);
            
            double fxValue = DAW::TrackFX_GetParam(track, fxIndex, paramIndex, &minvalOut, &maxvalOut);
            
            if(stepValue != fxValue)
            {
                stepValue = fxValue;
                stepCount++;
            }
        }
        
    if(stepCount > 1 && stepCount < 31)
        TheManager->SetSteppedValueCount(fxName, paramIndex, stepCount);

    DAW::TrackFX_SetParam(track, fxIndex, paramIndex, currentValue);
    
    if( ! wasMuted)
        DAW::CSurf_SetSurfaceMute(track, DAW::CSurf_OnMuteChange(track, false), NULL);
}

void ZoneManager::CalculateSteppedValues(const string &fxName, MediaTrack* track, int fxIndex)
{
    TheManager->SetSteppedValueCount(fxName, -1, 0); // Add dummy value to show the calculation has beeen performed, even though there may be no stepped values for this FX

    // Check for UAD / Plugin Alliance and bail if neither
    if(fxName.find("UAD") == string::npos && fxName.find("Plugin Alliance") == string::npos)
        return;
    
    int totalLayoutCount = 0;
    
    for(int i = 0; i < (int)fxLayouts_.size(); ++i)
        totalLayoutCount += fxLayouts_[i].channelCount;
    bool wasMuted = false;
    DAW::GetTrackUIMute(track, &wasMuted);
    
    if( ! wasMuted)
        DAW::CSurf_SetSurfaceMute(track, DAW::CSurf_OnMuteChange(track, true), NULL);

    double minvalOut = 0.0;
    double maxvalOut = 0.0;

    int numParams = DAW::TrackFX_GetNumParams(track, fxIndex);

    vector<double> currentValues;

    for(int i = 0; i < numParams && i <= totalLayoutCount; i++)
        currentValues.push_back(DAW::TrackFX_GetParam(track, fxIndex, i, &minvalOut, &maxvalOut));
    
    for(int i = 0; i < numParams && i <= totalLayoutCount; i++)
    {
        int stepCount = 1;
        double stepValue = 0.0;
        
        for(double value = 0.0; value < 1.01; value += .01)
        {
            DAW::TrackFX_SetParam(track, fxIndex, i, value);
            
            double fxValue = DAW::TrackFX_GetParam(track, fxIndex, i, &minvalOut, &maxvalOut);
            
            if(stepValue != fxValue)
            {
                stepValue = fxValue;
                stepCount++;
            }
        }
        
        if(stepCount > 1 && stepCount < 31)
            TheManager->SetSteppedValueCount(fxName, i, stepCount);
    }
    
    for(int i = 0; i < numParams && i <= totalLayoutCount; i++)
        DAW::TrackFX_SetParam(track, fxIndex, i, currentValues[i]);
    
    if( ! wasMuted)
        DAW::CSurf_SetSurfaceMute(track, DAW::CSurf_OnMuteChange(track, false), NULL);
}

void ZoneManager::AutoMapFX(const string &fxName, MediaTrack* track, int fxIndex)
{    
    if(fxLayouts_.size() == 0)
        return;

    if(surfaceFXLayout_.size() == 0)
        return;
            
    string path = DAW::GetResourcePath() + string("/CSI/Zones/") + fxZoneFolder_ + "/AutoGeneratedFXZones";
    
    RecursiveCreateDirectory(path.c_str(),0);
    
    path += "/" + regex_replace(fxName, regex(s_BadFileChars), "_") + ".zon";

    string alias;
    GetAlias(fxName.c_str(),alias);

    string paramAction = " FXParam ";
    
    if(fxName.find("JS:") != string::npos)
        paramAction = " JSFXParam ";
    
    CSIZoneInfo info;
    info.filePath = path;
    info.alias = alias;

    int totalAvailableChannels = 0;
    
    for(int i = 0; i < (int)fxLayouts_.size(); ++i)
        totalAvailableChannels += fxLayouts_[i].channelCount;
        
    AddZoneFilePath(fxName, info);
    surface_->GetPage()->AddZoneFilePath(surface_, fxZoneFolder_, fxName, info);

    ofstream fxZone(path);

    if(fxZone.is_open())
    {
        fxZone << "Zone \"" + fxName + "\" \"" + alias + "\"\n";
        
        for(int i = 0; i < (int)fxPrologue_.size(); ++i)
            fxZone << "\t" + fxPrologue_[i] + "\n";
               
        fxZone << "\n" + s_BeginAutoSection + "\n";
        
        int layoutIndex = 0;
        int channelIndex = 1;
             
        vector<string> actionWidgets;
        
        string actionWidget = surfaceFXLayout_[0][0];
     
        actionWidgets.push_back(actionWidget);
        
        for(int i = 0; i < (int)surfaceFXLayoutTemplate_.size(); ++i)
            if(surfaceFXLayoutTemplate_[i][0] == "WidgetTypes")
                for(int j = 1; j < surfaceFXLayoutTemplate_[i].size(); j++)
                    if(surfaceFXLayoutTemplate_[i][j] != actionWidget)
                        actionWidgets.push_back(surfaceFXLayoutTemplate_[i][j]);

        for(int paramIdx = 0; paramIdx < DAW::TrackFX_GetNumParams(track, fxIndex) && paramIdx < totalAvailableChannels; paramIdx++)
        {
            for(int widgetIdx = 0; widgetIdx < actionWidgets.size(); widgetIdx++)
            {
                for(int lineIdx = 0; lineIdx < surfaceFXLayout_.size(); lineIdx++)
                {
                    for(int tokenIdx = 0; tokenIdx < surfaceFXLayout_[lineIdx].size(); tokenIdx++)
                    {
                        if(tokenIdx == 0)
                        {
                            string modifiers = "";
                            
                            if(fxLayouts_[layoutIndex].modifiers != "")
                                modifiers = fxLayouts_[layoutIndex].modifiers + "+";
                            
                            if(widgetIdx == 0)
                                fxZone << "\t" + modifiers + surfaceFXLayout_[lineIdx][tokenIdx] + fxLayouts_[layoutIndex].suffix + to_string(channelIndex) + "\t";
                            else
                            {
                                if(lineIdx == 0)
                                    fxZone << "\t" + modifiers + actionWidgets[widgetIdx] + fxLayouts_[layoutIndex].suffix + to_string(channelIndex) + "\t";
                                else
                                    fxZone << "\t" + string("NullDisplay") + "\t";
                            }
                        }
                        else if(tokenIdx == 1)
                        {
                            if(widgetIdx == 0)
                                fxZone <<  surfaceFXLayout_[lineIdx][tokenIdx];
                            else
                                fxZone <<  string("NoAction");
                            
                            if(widgetIdx == 0 && surfaceFXLayout_[lineIdx][tokenIdx] == "FixedTextDisplay")
                                fxZone << " \"" + DAW::TrackFX_GetParamName(track, fxIndex, paramIdx) + "\"";
                            else if(widgetIdx == 0)
                                fxZone << " " + to_string(paramIdx);
                            
                            if(widgetIdx == 0 && surfaceFXLayout_[lineIdx][tokenIdx] == "FXParam")
                            {
                                int steppedValueCount =  TheManager->GetSteppedValueCount(fxName, paramIdx);
                                
                                if(steppedValueCount >= g_minNumParamSteps && steppedValueCount <= g_maxNumParamSteps)
                                {
                                    string steps;
                                    GetParamStepsString(steps, steppedValueCount);
                                    steps = " [ " + steps + "]";
                                    fxZone << steps;
                                }
                            }
                        }
                        else if(widgetIdx == 0)
                            fxZone << " " + surfaceFXLayout_[lineIdx][tokenIdx];
                    }
                    
                    fxZone << "\n";
                }
                
                fxZone << "\n";
            }
            
            channelIndex++;
            
            fxZone << "\n";
            
            if(channelIndex > fxLayouts_[layoutIndex].channelCount)
            {
                channelIndex = 1;
                
                if(layoutIndex < fxLayouts_.size() - 1)
                    layoutIndex++;
                else
                    break;
            }
        }
                
        // GAW -- pad partial rows
        if(channelIndex != 1 && channelIndex <= fxLayouts_[layoutIndex].channelCount)
        {
            while(channelIndex <= fxLayouts_[layoutIndex].channelCount)
            {
                for(int widgetIdx = 0; widgetIdx < actionWidgets.size(); widgetIdx++)
                {
                    string modifiers = "";
                    
                    if(fxLayouts_[layoutIndex].modifiers != "")
                        modifiers = fxLayouts_[layoutIndex].modifiers + "+";
                    
                    fxZone << "\t" + modifiers + actionWidgets[widgetIdx] + fxLayouts_[layoutIndex].suffix + to_string(channelIndex) + "\tNoAction\n";
                    
                    if(widgetIdx == 0 && surfaceFXLayout_.size() > 2 && surfaceFXLayout_[1].size() > 0 && surfaceFXLayout_[2].size() > 0)
                    {
                        fxZone << "\t" + modifiers + surfaceFXLayout_[1][0] + fxLayouts_[layoutIndex].suffix + to_string(channelIndex) + "\tNoAction";
                        
                        if(surfaceFXLayout_.size() > 1)
                            for(int i = 2; i < surfaceFXLayout_[1].size(); i++)
                                fxZone << " " + surfaceFXLayout_[1][i];
                        
                        fxZone << "\n";
                        
                        fxZone << "\t" + modifiers + surfaceFXLayout_[2][0] + fxLayouts_[layoutIndex].suffix + to_string(channelIndex) + "\tNoAction";
                        
                        if(surfaceFXLayout_.size() > 2)
                            for(int i = 2; i < surfaceFXLayout_[2].size(); i++)
                                fxZone << " " + surfaceFXLayout_[2][i];
                        
                        fxZone << "\n\n";
                    }
                    else
                    {
                        fxZone << "\tNullDisplay\tNoAction\n";
                        fxZone << "\tNullDisplay\tNoAction\n\n";
                    }
                }
                
                fxZone << "\n";
                
                channelIndex++;
            }
        }

        layoutIndex++;
        
        // GAW --pad the remaining rows
        while(layoutIndex < fxLayouts_.size())
        {
            for(int index = 1; index <= fxLayouts_[layoutIndex].channelCount; index++)
            {
                for(int widgetIdx = 0; widgetIdx < actionWidgets.size(); widgetIdx++)
                {
                    string modifiers = "";
                    
                    if(fxLayouts_[layoutIndex].modifiers != "")
                        modifiers = fxLayouts_[layoutIndex].modifiers + "+";
                    
                    fxZone << "\t" + modifiers + actionWidgets[widgetIdx] + fxLayouts_[layoutIndex].suffix + to_string(index) + "\tNoAction\n";
                    
                    if(widgetIdx == 0 && surfaceFXLayout_.size() > 2 && surfaceFXLayout_[1].size() > 0 && surfaceFXLayout_[2].size() > 0)
                    {
                        fxZone << "\t" + modifiers + surfaceFXLayout_[1][0] + fxLayouts_[layoutIndex].suffix + to_string(index) + "\tNoAction";
                        
                        if(surfaceFXLayout_.size() > 1)
                            for(int i = 2; i < surfaceFXLayout_[1].size(); i++)
                                fxZone << " " + surfaceFXLayout_[1][i];
                        
                        fxZone << "\n";
                        
                        fxZone << "\t" + modifiers + surfaceFXLayout_[2][0] + fxLayouts_[layoutIndex].suffix + to_string(index) + "\tNoAction";
                        
                        if(surfaceFXLayout_.size() > 2)
                            for(int i = 2; i < surfaceFXLayout_[2].size(); i++)
                                fxZone << " " + surfaceFXLayout_[2][i];
                        
                        fxZone << "\n\n";
                    }
                    else
                    {
                        fxZone << "\tNullDisplay\tNoAction\n";
                        fxZone << "\tNullDisplay\tNoAction\n\n";
                    }
                }
                
                fxZone << "\n";
            }
            
            layoutIndex++;
        }
        
        fxZone << s_EndAutoSection + "\n";
                
        for(int i = 0; i < (int)fxEpilogue_.size(); ++i)
            fxZone << "\t" + fxEpilogue_[i] + "\n";

        fxZone << "ZoneEnd\n\n";
        
        for(int i = 0; i < DAW::TrackFX_GetNumParams(track, fxIndex); i++)
            fxZone << to_string(i) + " " + DAW::TrackFX_GetParamName(track, fxIndex, i) + "\n";
        
        fxZone.close();
    }
    
    if(zoneFilePaths_.count(fxName) > 0)
    {
        vector<shared_ptr<Navigator>> navigators;
        navigators.push_back(GetSelectedTrackNavigator());
        
        if(sharedThisPtr_ != nullptr)
            ProcessZoneFile(zoneFilePaths_[fxName].filePath, sharedThisPtr_, navigators, fxSlotZones_, nullptr);
        
        if(fxSlotZones_.size() > 0)
        {
            fxSlotZones_.back()->SetSlotIndex(fxIndex);
            fxSlotZones_.back()->Activate();
        }
    }
}

void ZoneManager::DoTouch(Widget *widget, double value)
{
    surface_->TouchChannel(widget->GetChannelNumber(), value);
    
    widget->LogInput(value);
    
    bool isUsed = false;
    
    if(focusedFXParamZone_ != nullptr && isFocusedFXParamMappingEnabled_)
        focusedFXParamZone_->DoTouch(widget, widget->GetName(), isUsed, value);
    
    for(int i = 0; i < (int)focusedFXZones_.size(); ++i)
        focusedFXZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if(isUsed)
        return;

    for(int i = 0; i < (int)selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if(isUsed)
        return;

    for(int i = 0; i < (int) fxSlotZones_.size(); ++i)
        fxSlotZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);
    
    if(isUsed)
        return;

    if(homeZone_ != nullptr)
        homeZone_->DoTouch(widget, widget->GetName(), isUsed, value);
}

shared_ptr<Navigator> ZoneManager::GetMasterTrackNavigator() { return surface_->GetPage()->GetMasterTrackNavigator(); }
shared_ptr<Navigator> ZoneManager::GetSelectedTrackNavigator() { return surface_->GetPage()->GetSelectedTrackNavigator(); }
shared_ptr<Navigator> ZoneManager::GetFocusedFXNavigator() { return surface_->GetPage()->GetFocusedFXNavigator(); }
int ZoneManager::GetNumChannels() { return surface_->GetNumChannels(); }

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModifierManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ModifierManager::RecalculateModifiers()
{
    if(surface_ == nullptr && page_ == nullptr)
        return;
    
    modifierCombinations_.clear();
    modifierCombinations_.push_back(0);
           
    vector<int> activeModifierIndices;
    
    for(int i = 0; i < modifiers_.size(); i++)
        if(modifiers_[i].isEngaged)
            activeModifierIndices.push_back(i);
    
    if(activeModifierIndices.size() > 0)
    {
        for(int i = 0; i < (int)GetCombinations(activeModifierIndices).size(); ++i)
        {
            int modifier = 0;
            
            for(int j = 0; j < GetCombinations(activeModifierIndices)[i].size(); j++)
                modifier += modifiers_[GetCombinations(activeModifierIndices)[i][j]].value;

            modifierCombinations_.push_back(modifier);
        }
        
        sort(modifierCombinations_.begin(), modifierCombinations_.end(), [](const int & a, const int & b) { return a > b; });
    }
    
    if(surface_ != nullptr)
        surface_->GetZoneManager()->UpdateCurrentActionContextModifiers();
    else if(page_ != nullptr)
        page_->UpdateCurrentActionContextModifiers();
}

void ModifierManager::SetLatchModifier(bool value, Modifiers modifier, int latchTime)
{
    if(value && modifiers_[modifier].isEngaged == false)
    {
        modifiers_[modifier].isEngaged = value;
        modifiers_[modifier].pressedTime = DAW::GetCurrentNumberOfMilliseconds();
    }
    else
    {
        double keyReleasedTime = DAW::GetCurrentNumberOfMilliseconds();
        
        if(keyReleasedTime - modifiers_[modifier].pressedTime > latchTime)
        {
            if(value == 0 && modifiers_[modifier].isEngaged)
                TheManager->Speak(modifierNames_[modifier] + " Unlock");

            modifiers_[modifier].isEngaged = value;
        }
        else
            TheManager->Speak(modifierNames_[modifier] + " Lock");
    }
    
    RecalculateModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// TrackNavigationManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void TrackNavigationManager::RebuildTracks()
{
    int oldTracksSize = (int)tracks_.size();
    
    tracks_.clear();
    
    for (int i = 1; i <= GetNumTracks(); i++)
    {
        if(MediaTrack* track = DAW::CSurf_TrackFromID(i, followMCP_))
            if(DAW::IsTrackVisible(track, followMCP_))
                tracks_.push_back(track);
    }
    
    if(tracks_.size() < oldTracksSize)
    {
        for(int i = oldTracksSize; i > tracks_.size(); i--)
            page_->ForceClearTrack(i - trackOffset_);
    }
    
    if(tracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::RebuildSelectedTracks()
{
    if(currentTrackVCAFolderMode_ != 3)
        return;

    int oldTracksSize = (int)selectedTracks_.size();
    
    selectedTracks_.clear();
    
    for(int i = 0; i < DAW::CountSelectedTracks(); i++)
        selectedTracks_.push_back(DAW::GetSelectedTrack(i));

    if(selectedTracks_.size() < oldTracksSize)
    {
        for(int i = oldTracksSize; i > selectedTracks_.size(); i--)
            page_->ForceClearTrack(i - selectedTracksOffset_);
    }
    
    if(selectedTracks_.size() != oldTracksSize)
        page_->ForceUpdateTrackColors();
}

void TrackNavigationManager::AdjustSelectedTrackBank(int amount)
{
    if(MediaTrack* selectedTrack = GetSelectedTrack())
    {
        int trackNum = GetIdFromTrack(selectedTrack);
        
        trackNum += amount;
        
        if(trackNum < 1)
            trackNum = 1;
        
        if(trackNum > GetNumTracks())
            trackNum = GetNumTracks();
        
        if(MediaTrack* trackToSelect = GetTrackFromId(trackNum))
        {
            DAW::SetOnlyTrackSelected(trackToSelect);
            if(GetScrollLink())
                DAW::SetMixerScroll(trackToSelect);

            page_->OnTrackSelection(trackToSelect);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlSurface::Stop()
{
    if(isRewinding_ || isFastForwarding_) // set the cursor to the Play position
        DAW::CSurf_OnPlay();
 
    page_->SignalStop();
    CancelRewindAndFastForward();
    DAW::CSurf_OnStop();
}

void ControlSurface::Play()
{
    page_->SignalPlay();
    CancelRewindAndFastForward();
    DAW::CSurf_OnPlay();
}

void ControlSurface::Record()
{
    page_->SignalRecord();
    CancelRewindAndFastForward();
    DAW::CSurf_OnRecord();
}

void ControlSurface::OnTrackSelection(MediaTrack* track)
{
    if(widgetsByName_.count("OnTrackSelection") > 0)
    {
        if(DAW::GetMediaTrackInfo_Value(track, "I_SELECTED"))
            zoneManager_->DoAction(widgetsByName_["OnTrackSelection"], 1.0);
        else
            zoneManager_->OnTrackDeselection();
        
        zoneManager_->OnTrackSelection();
    }
}

void ControlSurface::ForceClearTrack(int trackNum)
{
    for(int i = 0; i < widgets_.GetSize(); ++i)
        if(widgets_.Get(i)->GetChannelNumber() + channelOffset_ == trackNum)
            widgets_.Get(i)->ForceClear();
}

void ControlSurface::ForceUpdateTrackColors()
{
    for(int i = 0; i < trackColorFeedbackProcessors_.GetSize(); ++i)
        trackColorFeedbackProcessors_.Get(i)->ForceUpdateTrackColors();
}

rgba_color ControlSurface::GetTrackColorForChannel(int channel)
{
    rgba_color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    if(channel < 0 || channel >= numChannels_)
        return white;
    
    if(fixedTrackColors_.size() == numChannels_)
        return fixedTrackColors_[channel];
    else
    {
        if(MediaTrack* track = page_->GetNavigatorForChannel(channel + channelOffset_)->GetTrack())
            return DAW::GetTrackColor(track);
        else
            return white;
    }
}

void ControlSurface::RequestUpdate()
{
    for(int i = 0; i < trackColorFeedbackProcessors_.GetSize(); ++i)
        trackColorFeedbackProcessors_.Get(i)->UpdateTrackColors();
    
    zoneManager_->RequestUpdate();
    
    if(isRewinding_)
    {
        if(DAW::GetCursorPosition() == 0)
            StopRewinding();
        else
        {
            DAW::CSurf_OnRew(0);

            if(speedX5_ == true)
            {
                DAW::CSurf_OnRew(0);
                DAW::CSurf_OnRew(0);
                DAW::CSurf_OnRew(0);
                DAW::CSurf_OnRew(0);
            }
        }
    }
        
    else if(isFastForwarding_)
    {
        if(DAW::GetCursorPosition() > DAW::GetProjectLength(nullptr))
            StopFastForwarding();
        else
        {
            DAW::CSurf_OnFwd(0);
            
            if(speedX5_ == true)
            {
                DAW::CSurf_OnFwd(0);
                DAW::CSurf_OnFwd(0);
                DAW::CSurf_OnFwd(0);
                DAW::CSurf_OnFwd(0);
            }
        }
    }
}

bool ControlSurface::GetShift()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetShift();
    else
        return page_->GetModifierManager()->GetShift();
}

bool ControlSurface::GetOption()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetOption();
    else
        return page_->GetModifierManager()->GetOption();
}

bool ControlSurface::GetControl()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetControl();
    else
        return page_->GetModifierManager()->GetControl();
}

bool ControlSurface::GetAlt()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetAlt();
    else
        return page_->GetModifierManager()->GetAlt();
}

bool ControlSurface::GetFlip()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetFlip();
    else
        return page_->GetModifierManager()->GetFlip();
}

bool ControlSurface::GetGlobal()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetGlobal();
    else
        return page_->GetModifierManager()->GetGlobal();
}

bool ControlSurface::GetMarker()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetMarker();
    else
        return page_->GetModifierManager()->GetMarker();
}

bool ControlSurface::GetNudge()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetNudge();
    else
        return page_->GetModifierManager()->GetNudge();
}

bool ControlSurface::GetZoom()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetZoom();
    else
        return page_->GetModifierManager()->GetZoom();
}

bool ControlSurface::GetScrub()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetScrub();
    else
        return page_->GetModifierManager()->GetScrub();
}

void ControlSurface::SetShift(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetShift(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetShift(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetShift(value, latchTime_);
    else
        page_->GetModifierManager()->SetShift(value, latchTime_);
}

void ControlSurface::SetOption(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetOption(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetOption(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetOption(value, latchTime_);
    else
        page_->GetModifierManager()->SetOption(value, latchTime_);
}

void ControlSurface::SetControl(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetControl(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetControl(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetControl(value, latchTime_);
    else
        page_->GetModifierManager()->SetControl(value, latchTime_);
}

void ControlSurface::SetAlt(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetAlt(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetAlt(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetAlt(value, latchTime_);
    else
        page_->GetModifierManager()->SetAlt(value, latchTime_);
}

void ControlSurface::SetFlip(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetFlip(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetFlip(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetFlip(value, latchTime_);
    else
        page_->GetModifierManager()->SetFlip(value, latchTime_);
}

void ControlSurface::SetGlobal(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetGlobal(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetGlobal(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetGlobal(value, latchTime_);
    else
        page_->GetModifierManager()->SetGlobal(value, latchTime_);
}

void ControlSurface::SetMarker(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetMarker(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetMarker(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetMarker(value, latchTime_);
    else
        page_->GetModifierManager()->SetMarker(value, latchTime_);
}

void ControlSurface::SetNudge(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetNudge(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetNudge(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetNudge(value, latchTime_);
    else
        page_->GetModifierManager()->SetNudge(value, latchTime_);
}

void ControlSurface::SetZoom(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetZoom(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetZoom(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetZoom(value, latchTime_);
    else
        page_->GetModifierManager()->SetZoom(value, latchTime_);
}

void ControlSurface::SetScrub(bool value)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->SetScrub(value, latchTime_);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->SetScrub(value, latchTime_);
    }
    else if(usesLocalModifiers_)
        modifierManager_->SetScrub(value, latchTime_);
    else
        page_->GetModifierManager()->SetScrub(value, latchTime_);
}

const vector<int> &ControlSurface::GetModifiers()
{
    if(usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetModifiers();
    else
        return page_->GetModifierManager()->GetModifiers();
}

void ControlSurface::ClearModifier(const string &modifier)
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifier(modifier);
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->ClearModifier(modifier);
    }
    else if(usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifier(modifier);
    else
        page_->GetModifierManager()->ClearModifier(modifier);
}

void ControlSurface::ClearModifiers()
{
    if(zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
    {
        modifierManager_->ClearModifiers();
        
        for(int i = 0; i < (int)zoneManager_->GetListeners().size(); ++i)
            if(zoneManager_->GetListeners()[i]->GetSurface()->GetListensToModifiers() && ! zoneManager_->GetListeners()[i]->GetSurface()->GetUsesLocalModifiers() && zoneManager_->GetListeners()[i]->GetSurface()->GetName() != name_)
                zoneManager_->GetListeners()[i]->GetSurface()->GetModifierManager()->ClearModifiers();
    }
    else if(usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifiers();
    else
        page_->GetModifierManager()->ClearModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurfaceIO
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurfaceIO::HandleExternalInput(Midi_ControlSurface* surface)
{
    if(midiInput_)
    {
        DAW::SwapBufsPrecise(midiInput_);
        MIDI_eventlist* list = midiInput_->GetReadBuf();
        int bpos = 0;
        MIDI_event_t* evt;
        while ((evt = list->EnumItems(&bpos)))
            surface->ProcessMidiMessage((MIDI_event_ex_t*)evt);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
Midi_ControlSurface::Midi_ControlSurface(Page* page, const string &name, int numChannels, int channelOffset, string templateFilename, string zoneFolder, string fxZoneFolder, Midi_ControlSurfaceIO* surfaceIO)
: ControlSurface(page, name, numChannels, channelOffset), templateFilename_(templateFilename), surfaceIO_(surfaceIO)
{
    // private:
    // special processing for MCU meters
    hasMCUMeters_ = false;
    displayType_ = 0x14;
    
    zoneManager_ = make_shared<ZoneManager>(this, zoneFolder, fxZoneFolder);
    zoneManager_->SetSharedThisPtr(zoneManager_);
    
    ProcessMIDIWidgetFile(string(DAW::GetResourcePath()) + "/CSI/Surfaces/Midi/" + templateFilename, this);
    InitHardwiredWidgets(this);
    InitializeMeters();
    zoneManager_->Initialize();

    modifierManager_ = make_shared<ModifierManager>(this);
}

void Midi_ControlSurface::ProcessMidiMessage(const MIDI_event_ex_t* evt)
{
    bool isMapped = false;
    
    // At this point we don't know how much of the message comprises the key, so try all three
    if(Midi_CSIMessageGeneratorsByMessage_.count(evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100 + evt->midi_message[2]) > 0)
    {
        isMapped = true;
        for(int i = 0; i < (int)Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100 + evt->midi_message[2]].size(); ++i)
            Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100 + evt->midi_message[2]][i]->ProcessMidiMessage(evt);
    }
    else if(Midi_CSIMessageGeneratorsByMessage_.count(evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100) > 0)
    {
        isMapped = true;
        for(int i = 0; i < (int)Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100].size(); ++i)
            Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100][i]->ProcessMidiMessage(evt);
    }
    else if(Midi_CSIMessageGeneratorsByMessage_.count(evt->midi_message[0] * 0x10000) > 0)
    {
        isMapped = true;
        for(int i = 0; i < (int)Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000].size(); ++i)
            Midi_CSIMessageGeneratorsByMessage_[evt->midi_message[0] * 0x10000][i]->ProcessMidiMessage(evt);
    }
    
    if(TheManager->GetSurfaceRawInDisplay() || (! isMapped && TheManager->GetSurfaceInDisplay()))
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "IN <- %s %02x  %02x  %02x \n", name_.c_str(), evt->midi_message[0], evt->midi_message[1], evt->midi_message[2]);
        DAW::ShowConsoleMsg(buffer);
    }
}

void Midi_ControlSurface::SendMidiSysExMessage(MIDI_event_ex_t* midiMessage)
{
    surfaceIO_->SendMidiMessage(midiMessage);
    
    string output = "OUT->" + name_ + " ";
    
    for(int i = 0; i < midiMessage->size; i++)
    {
        char buffer[32];
        
        snprintf(buffer, sizeof(buffer), "%02x ", midiMessage->midi_message[i]);
        
        output += buffer;
    }
    
    output += "\n";

    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(output.c_str());
}

void Midi_ControlSurface::SendMidiMessage(int first, int second, int third)
{
    surfaceIO_->SendMidiMessage(first, second, third);
    
    if(TheManager->GetSurfaceOutDisplay())
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "%s  %02x  %02x  %02x \n", ("OUT->" + name_).c_str(), first, second, third);
        DAW::ShowConsoleMsg(buffer);
    }
}

 ////////////////////////////////////////////////////////////////////////////////////////////////////////
 // OSC_ControlSurfaceIO
 ////////////////////////////////////////////////////////////////////////////////////////////////////////
OSC_ControlSurfaceIO::OSC_ControlSurfaceIO(const string &surfaceName, const string &receiveOnPort, const string &transmitToPort, const string &transmitToIpAddress) : name_(surfaceName)
{
    // private:
    inSocket_ = nullptr;
    outSocket_ = nullptr;
    X32HeartBeatRefreshInterval_ = 5000; // must be less than 10000
    X32HeartBeatLastRefreshTime_ = 0.0;

    if (receiveOnPort != transmitToPort)
    {
        inSocket_  = GetInputSocketForPort(surfaceName, stoi(receiveOnPort));;
        outSocket_ = GetOutputSocketForAddressAndPort(surfaceName, transmitToIpAddress, stoi(transmitToPort));
    }
    else // WHEN INPUT AND OUTPUT SOCKETS ARE THE SAME -- DO MAGIC :)
    {
        oscpkt::UdpSocket* inSocket = GetInputSocketForPort(surfaceName, stoi(receiveOnPort));;

        struct addrinfo hints;
        struct addrinfo* addressInfo;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;      // IPV4
        hints.ai_socktype = SOCK_DGRAM; // UDP
        hints.ai_flags = 0x00000001;    // socket address is intended for bind
        getaddrinfo(transmitToIpAddress.c_str(), transmitToPort.c_str(), &hints, &addressInfo);
        memcpy(&(inSocket->remote_addr), (void*)(addressInfo->ai_addr), addressInfo->ai_addrlen);

        inSocket_  = inSocket;
        outSocket_ = inSocket;
        s_outputSockets[surfaceName] = outSocket_;
    }
 }

 void OSC_ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface* surface)
 {
    if(inSocket_ != nullptr && inSocket_->isOk())
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
                    surface->ProcessOSCMessage(message->addressPattern(), value);
                }
                else if (message->arg().isInt32())
                {
                    int value;
                    message->arg().popInt32(value);
                    
                    if (surface->IsX32() && message->addressPattern() == "/-stat/selidx")
                    {
                        string x32Select = message->addressPattern() + '/';
                        if (value < 10)
                            x32Select += '0';
                        x32Select += to_string(value);
                        surface->ProcessOSCMessage(x32Select, 1.0);
                    }
                    else
                        surface->ProcessOSCMessage(message->addressPattern(), value);
                }
            }
        }
    }
 }

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
OSC_ControlSurface::OSC_ControlSurface(Page* page, const string &name, int numChannels, int channelOffset, string templateFilename, string zoneFolder, string fxZoneFolder, OSC_ControlSurfaceIO* surfaceIO) : ControlSurface(page, name, numChannels, channelOffset), templateFilename_(templateFilename), surfaceIO_(surfaceIO)

{
    zoneManager_ = make_shared<ZoneManager>(this, zoneFolder, fxZoneFolder);
    zoneManager_->SetSharedThisPtr(zoneManager_);

    ProcessOSCWidgetFile(string(DAW::GetResourcePath()) + "/CSI/Surfaces/OSC/" + templateFilename, this);
    InitHardwiredWidgets(this);
    zoneManager_->Initialize();
    
    modifierManager_ = make_shared<ModifierManager>(this);
}

void OSC_ControlSurface::ProcessOSCMessage(const string &message, double value)
{
    if(CSIMessageGeneratorsByMessage_.count(message) > 0)
        CSIMessageGeneratorsByMessage_[message]->ProcessMessage(value);
    
    if(TheManager->GetSurfaceInDisplay())
    {
        char buffer[250];
        snprintf(buffer, sizeof(buffer), "IN <- %s %s  %f  \n", name_.c_str(), message.c_str(), value);
        DAW::ShowConsoleMsg(buffer);
    }
}

void OSC_ControlSurface::SendOSCMessage(const string &zoneName)
{
    string oscAddress(zoneName);
    oscAddress = regex_replace(oscAddress, regex(s_BadFileChars), "_");
    oscAddress = "/" + oscAddress;

    surfaceIO_->SendOSCMessage(oscAddress);
        
    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg((zoneName + "->" + "LoadingZone---->" + name_ + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(const string &oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + oscAddress + " " + to_string(value) + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(const string &oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + oscAddress + " " + to_string(value) + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(const string &oscAddress, const string &value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
        
    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + oscAddress + " " + value + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const string &oscAddress, double value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);
    
    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + feedbackProcessor->GetWidget()->GetName() + " " + oscAddress + " " + to_string(value) + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const string &oscAddress, int value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + feedbackProcessor->GetWidget()->GetName() + " " + oscAddress + " " + to_string(value) + "\n").c_str());
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const string &oscAddress, const string &value)
{
    surfaceIO_->SendOSCMessage(oscAddress, value);

    if(TheManager->GetSurfaceOutDisplay())
        DAW::ShowConsoleMsg(("OUT->" + name_ + " " + feedbackProcessor->GetWidget()->GetName() + " " + oscAddress + " " + value + "\n").c_str());
}

void Midi_ControlSurface::InitializeMCU()
{
    vector<vector<int>> sysExLines;
    
    sysExLines.push_back({0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x21, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x00, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x01, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x02, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x03, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x04, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x05, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x06, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x07, 0x01, 0xF7});
    
    struct
    {
        MIDI_event_ex_t evt;
        char data[BUFSZ];
    } midiSysExData;
    
    for(int i = 0; i < (int)sysExLines.size(); ++i)
    {
        memset(midiSysExData.data, 0, sizeof(midiSysExData.data));
        
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;

        for(int j = 0; j < (int)sysExLines[i].size(); ++j)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExLines[i][j];
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }
}

void Midi_ControlSurface::InitializeMCUXT()
{
    vector<vector<int>> sysExLines;
    
    sysExLines.push_back({0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x00, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x21, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x00, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x01, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x02, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x03, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x04, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x05, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x06, 0x01, 0xF7});
    sysExLines.push_back({0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x07, 0x01, 0xF7});
    
    struct
    {
        MIDI_event_ex_t evt;
        char data[BUFSZ];
    } midiSysExData;
    
    for(int i = 0; i < (int)sysExLines.size(); ++i)
    {
        midiSysExData.evt.frame_offset=0;
        midiSysExData.evt.size=0;
        
        for(int j = 0; i < (int)sysExLines[i].size(); ++j)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExLines[i][j];
        
        SendMidiSysExMessage(&midiSysExData.evt);
    }

}

