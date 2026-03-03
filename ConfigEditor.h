#ifndef CONFIG_EDITOR_H
#define CONFIG_EDITOR_H

#include "common.h"
#include "imgui.h"
#include "IniModel.h"
#include "IniParser.h"
#include "IniWriter.h"

class ConfigEditor
{
public:
    ConfigEditor();
    ~ConfigEditor();

    // Main render function — call each frame when Config Editor view is active.
    void Render();

private:
    void RenderSectionTree();
    void RenderKeyTable();

    // Load/unload
    bool LoadFile(const std::string& path);
    void CloseFile();

    // State
    bool m_HasFile;
    bool m_Dirty;
    char m_FilePath[512];
    std::string m_ErrorMsg;

    // Parsed INI data
    IniFile m_Ini;

    // Selection
    int m_SelectedSection;   // Index into m_Ini.sections, -1 = none

    // Filter
    char m_FilterBuf[128];
};

#endif // CONFIG_EDITOR_H
