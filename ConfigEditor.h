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
    void HandleKeyboardShortcuts();

    // Load/unload
    bool LoadFile(const std::string& path);
    void CloseFile();

    // Editing helpers
    void MarkDirty();

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

    // Editing state — temporary buffers for the currently-edited cell
    // We use a (section_idx, line_idx, column) triple to identify what's being edited
    int m_EditSection;       // Which section is being edited (-1 = none)
    int m_EditLine;          // Which line within the section
    int m_EditColumn;        // 0=conditions, 1=key, 2=value
    char m_EditBuf[2048];    // Edit buffer
    bool m_EditActive;       // Whether an edit is in progress
    bool m_EditFocusNeeded;  // Set keyboard focus on next frame
};

#endif // CONFIG_EDITOR_H
