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

    // Scenario operations
    void AddScenario(SectionType type, const std::string& name);
    void CloneScenario(int section_idx, const std::string& new_name);
    void RenameScenario(int section_idx, const std::string& new_name);
    void DeleteScenario(int section_idx);

    // Key operations
    void AddKey(int section_idx);
    void AddConditionalKey(int section_idx);
    void DeleteKey(int section_idx, int line_idx);

    // Dialog rendering
    void RenderNewScenarioDialog();
    void RenderCloneScenarioDialog();
    void RenderRenameScenarioDialog();
    void RenderDeleteConfirmDialog();

    // State
    bool m_HasFile;
    bool m_Dirty;
    char m_FilePath[512];
    std::string m_ErrorMsg;

    // Parsed INI data
    IniFile m_Ini;

    // Selection
    int m_SelectedSection;   // Index into m_Ini.sections, -1 = none
    int m_SelectedLine;      // Index into selected section's lines, -1 = none

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

    // Dialog state — New Scenario
    bool m_ShowNewScenarioDlg;
    int m_NewScenarioType;   // Index into scenario type list
    char m_NewScenarioName[256];

    // Dialog state — Clone Scenario
    bool m_ShowCloneScenarioDlg;
    int m_CloneSourceIdx;    // Section index of source
    char m_CloneNewName[256];

    // Dialog state — Rename Scenario
    bool m_ShowRenameScenarioDlg;
    int m_RenameTargetIdx;   // Section index to rename
    char m_RenameNewName[256];

    // Dialog state — Delete Confirmation
    bool m_ShowDeleteConfirmDlg;
    int m_DeleteTargetIdx;   // Section index to delete
};

#endif // CONFIG_EDITOR_H
