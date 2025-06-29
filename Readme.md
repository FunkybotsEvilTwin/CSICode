# CSI - Control Surface Integrator

Welcome to the primary code repository for the **CSI (Control Surface Integrator)** project!

CSI is a powerful and flexible system for integrating a wide range of hardware control surfaces with [REAPER](https://www.reaper.fm/), providing advanced mapping, feedback, and workflow customization capabilities. Whether you're using a basic MIDI controller or a complex multi-surface studio setup including MIDI and OSC devices, CSI enables deep control over your DAW environment.

CSI is a fully open-source project. Anyone is welcome to explore, contribute to, or fork the codebase to create customized builds that meet their specific needs.

---

## Resources and Documentation

- 📚 **[CSI Wiki - User Guide and Troubleshooting](https://github.com/FunkybotsEvilTwin/CSIUserGuide/wiki)**  
  The official CSI User Guide with setup instructions, feature documentation, and troubleshooting tips.

- 📦 **[CSI Install Repository](https://github.com/FunkybotsEvilTwin/CSI_Install)**  
  Contains required installation files, surface definitions, and support scripts needed to run CSI.

- 🧪 **[CSI Experimental Repository](https://github.com/FunkybotsEvilTwin/CSI_Install)**  
  A place to explore experimental and beta versions of CSI.

---

## Issue Reporting

If you encounter a bug or problem with CSI, we recommend first reporting it in the **main CSI thread** on the [REAPER Forum](https://forum.cockos.com/showthread.php?t=183143).  
This allows for community discussion and quick troubleshooting.

If community discussions and quick troubleshooting fail to resolve the issue, feel free to open an Issue here on GitHub to help track and resolve the problem.

When submitting an Issue, please provide:
- A clear description of the problem
- Steps to reproduce
- Expected results
- Actual results
- Surface(s) involved
- CSI version
- Operating system and REAPER version

---

## Contributing Code

**Code contributions are welcome and encouraged!**

We kindly ask that contributors follow these guidelines, as well as review the CSI Code Style Guide and Authoring Tips in the next section, to help keep the project organized and moving forward smoothly:

- **Introduce yourself!**  
  Feel free to reach out to the team before beginning work — either by posting in the **main CSI thread** on the [REAPER Forum](https://forum.cockos.com/showthread.php?t=183143) or by sending a private message to `funkybot` on the forum.  
  Contacting us helps avoid conflicts with project goals or other contributors who may already be working on related tasks.

- **Align with project goals.**  
  Before making major changes, please check in to make sure your ideas fit with the long-term direction of the project.

- **Issues list.**  
  If you see an open Issue you would like to tackle, please post a quick comment calling "dibs" before you start working.  
  This helps avoid duplicate efforts where multiple contributors work on the same thing.

- **Keep Pull Requests (PRs) focused and small.**  
  PRs should ideally cover a **single feature or fix**.  
  This makes reviewing and testing changes much easier.

- **Include clear explanations.**  
  When submitting a PR, please describe:
  - The purpose of the change
  - Why the change is necessary
  - How the change is used (especially if it relates to a specific surface)
  - Any relevant zone or syntax information

- **Do not remove project files** without prior discussion.  
  Project files (such as Visual Studio solutions, WDL components, etc.) are important for cross-platform builds and ongoing development. Please avoid deleting or replacing these files in a PR unless the change has been discussed and agreed upon by the team.

- **PR review process.**  
  Regular CSI contributors will review and approve pull requests to ensure consistency, compatibility, and project integrity.

---

## CSI Code Style Guide and Authoring Tips

### 1. Avoid Shorthand  
Use full, descriptive names instead of abbreviations. This makes code self-documenting and easier to read.

**Bad:**  
```
    #elif CONDITION  
    // ...  
    #endif  

    int oldTracksSize = (int)t;
```

**Good:**  
```
    #else  
    #if CONDITION  
    // ...  
    #endif  

    int oldTracksSize = static_cast<int>(tracks);
```

### 2. Bracket Usage  
Place opening braces on their own line and align closing braces vertically. Never leave an opening brace at the end of a line. This helps visually align code in various IDE's.

**Bad:**  
```
    class Something : public Action {  
    public:  
        const char* GetName() override { return "Something"; }  
    };
```

**Good:**  
```
    class Something : public Action  
    {  
    public:  
        const char* GetName() override  
        {  
            return "Something";  
        }  
    };
```

### 3. Lowercase Variables  
Variable names should be lowercase and spelled out and use a trailing underscore for private members. 

**Bad:**  
```
    if (!T) continue;
```

**Good:**  
```
    if (!track) continue;
```   
 
 **Bad:**  
```
    bool IsInitialized_ = false;
```

**Good:**  
```
    bool isInitialized_ = false;
```

### 4. Spacing and Comparisons  
Use consistent spacing around operators and align related expressions to improve readability.

**Not Recommended:**  
```
    if (tracks_[i] !=track||colors_[i] !=GetTrackColor(track))
```

**Preferred:** Adds a few extra spaces to make comparisons a little easier to spot.  
```
    if (tracks_[i] != track   ||  colors_[i] != GetTrackColor(track))
```

### 5. Avoid Timers  
CSI's Run method is called approximately every 30 ms. Do not introduce timers. Keep all logic within Run.

### 6. Embrace Object-Oriented Design Principles  
CSI is built around object-oriented design. Before implementing a solution, review how existing classes interact to ensure your changes integrate seamlessly.

### 7. Keep Code Self-Contained  
When adding things like new actions or feedback processors, encapsulate logic within the class. If you find yourself scattering helper functions across files, reconsider your design or stop and ask for guidance.

---

## Forks and Custom Versions

Because CSI is open source, anyone is free to clone this repository and create their own builds incorporating any changes, large or small.

If you have ideas that significantly diverge from the current project goals, you're welcome to maintain your own fork!

---

## Thank You!

We appreciate your contributions and interest in helping CSI continue to grow and improve. Your support makes this project possible!
