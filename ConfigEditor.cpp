#include "ConfigEditor.h"
#include "ImGuiFileDialog.h"
#include "ImGuiFileDialogConfig.h"

ConfigEditor::ConfigEditor()
    : m_HasFile(false)
    , m_Dirty(false)
    , m_SelectedSection(-1)
    , m_SelectedLine(-1)
    , m_EditSection(-1)
    , m_EditLine(-1)
    , m_EditColumn(-1)
    , m_EditActive(false)
    , m_EditFocusNeeded(false)
    , m_ShowNewScenarioDlg(false)
    , m_NewScenarioType(0)
    , m_ShowCloneScenarioDlg(false)
    , m_CloneSourceIdx(-1)
    , m_ShowRenameScenarioDlg(false)
    , m_RenameTargetIdx(-1)
    , m_ShowDeleteConfirmDlg(false)
    , m_DeleteTargetIdx(-1)
{
    memset(m_FilePath, 0, sizeof(m_FilePath));
    memset(m_FilterBuf, 0, sizeof(m_FilterBuf));
    memset(m_EditBuf, 0, sizeof(m_EditBuf));
    memset(m_NewScenarioName, 0, sizeof(m_NewScenarioName));
    memset(m_CloneNewName, 0, sizeof(m_CloneNewName));
    memset(m_RenameNewName, 0, sizeof(m_RenameNewName));
}

ConfigEditor::~ConfigEditor()
{
}

bool ConfigEditor::LoadFile(const std::string& path)
{
    m_ErrorMsg.clear();
    IniFile ini;
    if (!IniParser::ParseFile(path, ini, m_ErrorMsg)) {
        return false;
    }

    m_Ini = ini;
    snprintf(m_FilePath, sizeof(m_FilePath), "%s", path.c_str());
    m_HasFile = true;
    m_Dirty = false;
    m_SelectedSection = -1;
    return true;
}

void ConfigEditor::CloseFile()
{
    m_HasFile = false;
    m_Dirty = false;
    m_SelectedSection = -1;
    m_SelectedLine = -1;
    m_EditSection = -1;
    m_EditLine = -1;
    m_EditColumn = -1;
    m_EditActive = false;
    m_EditFocusNeeded = false;
    m_ShowNewScenarioDlg = false;
    m_ShowCloneScenarioDlg = false;
    m_ShowRenameScenarioDlg = false;
    m_ShowDeleteConfirmDlg = false;
    memset(m_FilePath, 0, sizeof(m_FilePath));
    m_Ini = IniFile();
    m_ErrorMsg.clear();
}

void ConfigEditor::MarkDirty()
{
    m_Dirty = true;
}

// --- Scenario Operations ---

// Scenario type info for the "New Scenario" dialog
struct ScenarioTypeInfo {
    const char* label;
    const char* prefix;
    SectionType type;
};

static const ScenarioTypeInfo g_ScenarioTypes[] = {
    { "Producer",   "Producer_",   Section_Producer },
    { "Consumer",   "Consumer_",   Section_Consumer },
    { "GWProducer", "GWProducer_", Section_GWProducer },
    { "Reliable",   "Reliable_",   Section_Reliable },
    { "Rehydrate",  "Rehydrate_",  Section_Rehydrate },
};
static const int g_NumScenarioTypes = (int)(sizeof(g_ScenarioTypes) / sizeof(g_ScenarioTypes[0]));

void ConfigEditor::AddScenario(SectionType type, const std::string& name)
{
    // Build the section header name by finding the matching prefix
    std::string section_name;
    for (int i = 0; i < g_NumScenarioTypes; i++) {
        if (g_ScenarioTypes[i].type == type) {
            section_name = std::string(g_ScenarioTypes[i].prefix) + name;
            break;
        }
    }
    if (section_name.empty()) return;

    // Check for duplicate
    for (size_t i = 0; i < m_Ini.sections.size(); i++) {
        if (m_Ini.sections[i].name == section_name) {
            m_ErrorMsg = "A section named [" + section_name + "] already exists.";
            return;
        }
    }

    Section sec;
    sec.name = section_name;
    sec.section_type = type;
    // Add a default Enable=false key
    Line default_line;
    default_line.type = Line_KeyValue;
    default_line.key_entry.key = "Enable";
    default_line.key_entry.value = "false";
    sec.lines.push_back(default_line);

    m_Ini.sections.push_back(sec);
    m_SelectedSection = (int)m_Ini.sections.size() - 1;
    MarkDirty();
}

void ConfigEditor::CloneScenario(int section_idx, const std::string& new_name)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;

    const Section& src = m_Ini.sections[section_idx];

    // Determine prefix from source type
    std::string section_name;
    for (int i = 0; i < g_NumScenarioTypes; i++) {
        if (g_ScenarioTypes[i].type == src.section_type) {
            section_name = std::string(g_ScenarioTypes[i].prefix) + new_name;
            break;
        }
    }
    // For non-scenario types (PerfConfig, MDM, DSTS), just use the name directly
    if (section_name.empty()) {
        section_name = new_name;
    }

    // Check for duplicate
    for (size_t i = 0; i < m_Ini.sections.size(); i++) {
        if (m_Ini.sections[i].name == section_name) {
            m_ErrorMsg = "A section named [" + section_name + "] already exists.";
            return;
        }
    }

    // Deep copy
    Section cloned = src;
    cloned.name = section_name;

    // Insert right after the source section
    m_Ini.sections.insert(m_Ini.sections.begin() + section_idx + 1, cloned);
    m_SelectedSection = section_idx + 1;
    MarkDirty();
}

void ConfigEditor::RenameScenario(int section_idx, const std::string& new_name)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;

    Section& sec = m_Ini.sections[section_idx];

    // Build new full section name
    std::string section_name;
    for (int i = 0; i < g_NumScenarioTypes; i++) {
        if (g_ScenarioTypes[i].type == sec.section_type) {
            section_name = std::string(g_ScenarioTypes[i].prefix) + new_name;
            break;
        }
    }
    // For non-scenario types, just use the name directly
    if (section_name.empty()) {
        section_name = new_name;
    }

    // Check for duplicate (but not self)
    for (size_t i = 0; i < m_Ini.sections.size(); i++) {
        if ((int)i != section_idx && m_Ini.sections[i].name == section_name) {
            m_ErrorMsg = "A section named [" + section_name + "] already exists.";
            return;
        }
    }

    sec.name = section_name;
    MarkDirty();
}

void ConfigEditor::DeleteScenario(int section_idx)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;

    m_Ini.sections.erase(m_Ini.sections.begin() + section_idx);

    // Fix selection
    if (m_SelectedSection == section_idx) {
        m_SelectedSection = -1;
        m_SelectedLine = -1;
    } else if (m_SelectedSection > section_idx) {
        m_SelectedSection--;
    }

    // Cancel any active edit
    m_EditActive = false;
    MarkDirty();
}

// --- Key Operations ---

void ConfigEditor::AddKey(int section_idx)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;

    Section& sec = m_Ini.sections[section_idx];

    Line new_line;
    new_line.type = Line_KeyValue;
    new_line.key_entry.key = "NewKey";
    new_line.key_entry.value = "";

    sec.lines.push_back(new_line);
    MarkDirty();

    // Start editing the new key's key column
    int new_line_idx = (int)sec.lines.size() - 1;
    m_EditSection = section_idx;
    m_EditLine = new_line_idx;
    m_EditColumn = 1; // key column
    m_EditActive = true;
    m_EditFocusNeeded = true;
    snprintf(m_EditBuf, sizeof(m_EditBuf), "NewKey");
}

void ConfigEditor::AddConditionalKey(int section_idx)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;

    Section& sec = m_Ini.sections[section_idx];

    Line new_line;
    new_line.type = Line_KeyValue;
    new_line.key_entry.key = "NewKey";
    new_line.key_entry.value = "";
    // Add a placeholder condition
    Condition cond;
    cond.scope_type = Scope_Cluster;
    cond.scope_value = "ClusterName";
    new_line.key_entry.conditions.push_back(cond);

    sec.lines.push_back(new_line);
    MarkDirty();

    // Start editing the conditions column
    int new_line_idx = (int)sec.lines.size() - 1;
    m_EditSection = section_idx;
    m_EditLine = new_line_idx;
    m_EditColumn = 0; // conditions column
    m_EditActive = true;
    m_EditFocusNeeded = true;
    snprintf(m_EditBuf, sizeof(m_EditBuf), "Cluster:ClusterName");
}

void ConfigEditor::DeleteKey(int section_idx, int line_idx)
{
    if (section_idx < 0 || section_idx >= (int)m_Ini.sections.size()) return;
    Section& sec = m_Ini.sections[section_idx];
    if (line_idx < 0 || line_idx >= (int)sec.lines.size()) return;

    sec.lines.erase(sec.lines.begin() + line_idx);

    // Cancel any active edit on this line
    if (m_EditActive && m_EditSection == section_idx && m_EditLine == line_idx) {
        m_EditActive = false;
    }
    // Adjust edit line index if needed
    if (m_EditActive && m_EditSection == section_idx && m_EditLine > line_idx) {
        m_EditLine--;
    }

    // Adjust selected line
    if (m_SelectedLine == line_idx) {
        m_SelectedLine = -1;
    } else if (m_SelectedLine > line_idx) {
        m_SelectedLine--;
    }

    MarkDirty();
}

// --- Dialog Rendering ---

void ConfigEditor::RenderNewScenarioDialog()
{
    if (!m_ShowNewScenarioDlg) return;

    ImGui::OpenPopup("New Scenario");
    if (ImGui::BeginPopupModal("New Scenario", &m_ShowNewScenarioDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new scenario section.");
        ImGui::Separator();

        // Type combo
        ImGui::Text("Type:");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##ScenarioType", g_ScenarioTypes[m_NewScenarioType].label)) {
            for (int i = 0; i < g_NumScenarioTypes; i++) {
                bool selected = (m_NewScenarioType == i);
                if (ImGui::Selectable(g_ScenarioTypes[i].label, selected))
                    m_NewScenarioType = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Name input
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        bool enter_pressed = ImGui::InputText("##ScenarioName", m_NewScenarioName, sizeof(m_NewScenarioName),
            ImGuiInputTextFlags_EnterReturnsTrue);

        // Preview of full section name
        ImGui::TextDisabled("Section: [%s%s]", g_ScenarioTypes[m_NewScenarioType].prefix, m_NewScenarioName);

        ImGui::Separator();

        bool name_empty = (m_NewScenarioName[0] == '\0');
        if (name_empty) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0)) || (enter_pressed && !name_empty)) {
            AddScenario(g_ScenarioTypes[m_NewScenarioType].type, m_NewScenarioName);
            m_ShowNewScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }
        if (name_empty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowNewScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ConfigEditor::RenderCloneScenarioDialog()
{
    if (!m_ShowCloneScenarioDlg) return;

    ImGui::OpenPopup("Clone Scenario");
    if (ImGui::BeginPopupModal("Clone Scenario", &m_ShowCloneScenarioDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_CloneSourceIdx >= 0 && m_CloneSourceIdx < (int)m_Ini.sections.size()) {
            const Section& src = m_Ini.sections[m_CloneSourceIdx];
            ImGui::Text("Clone [%s] to a new scenario.", src.name.c_str());
        }
        ImGui::Separator();

        ImGui::Text("New name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        bool enter_pressed = ImGui::InputText("##CloneName", m_CloneNewName, sizeof(m_CloneNewName),
            ImGuiInputTextFlags_EnterReturnsTrue);

        // Preview
        if (m_CloneSourceIdx >= 0 && m_CloneSourceIdx < (int)m_Ini.sections.size()) {
            const Section& src = m_Ini.sections[m_CloneSourceIdx];
            for (int i = 0; i < g_NumScenarioTypes; i++) {
                if (g_ScenarioTypes[i].type == src.section_type) {
                    ImGui::TextDisabled("New section: [%s%s]", g_ScenarioTypes[i].prefix, m_CloneNewName);
                    break;
                }
            }
        }

        ImGui::Separator();

        bool name_empty = (m_CloneNewName[0] == '\0');
        if (name_empty) ImGui::BeginDisabled();
        if (ImGui::Button("Clone", ImVec2(120, 0)) || (enter_pressed && !name_empty)) {
            CloneScenario(m_CloneSourceIdx, m_CloneNewName);
            m_ShowCloneScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }
        if (name_empty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowCloneScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ConfigEditor::RenderRenameScenarioDialog()
{
    if (!m_ShowRenameScenarioDlg) return;

    ImGui::OpenPopup("Rename Scenario");
    if (ImGui::BeginPopupModal("Rename Scenario", &m_ShowRenameScenarioDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_RenameTargetIdx >= 0 && m_RenameTargetIdx < (int)m_Ini.sections.size()) {
            const Section& sec = m_Ini.sections[m_RenameTargetIdx];
            ImGui::Text("Rename [%s]", sec.name.c_str());
        }
        ImGui::Separator();

        ImGui::Text("New name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        bool enter_pressed = ImGui::InputText("##RenameName", m_RenameNewName, sizeof(m_RenameNewName),
            ImGuiInputTextFlags_EnterReturnsTrue);

        // Preview
        if (m_RenameTargetIdx >= 0 && m_RenameTargetIdx < (int)m_Ini.sections.size()) {
            const Section& sec = m_Ini.sections[m_RenameTargetIdx];
            for (int i = 0; i < g_NumScenarioTypes; i++) {
                if (g_ScenarioTypes[i].type == sec.section_type) {
                    ImGui::TextDisabled("New section: [%s%s]", g_ScenarioTypes[i].prefix, m_RenameNewName);
                    break;
                }
            }
        }

        ImGui::Separator();

        bool name_empty = (m_RenameNewName[0] == '\0');
        if (name_empty) ImGui::BeginDisabled();
        if (ImGui::Button("Rename", ImVec2(120, 0)) || (enter_pressed && !name_empty)) {
            RenameScenario(m_RenameTargetIdx, m_RenameNewName);
            m_ShowRenameScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }
        if (name_empty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowRenameScenarioDlg = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ConfigEditor::RenderDeleteConfirmDialog()
{
    if (!m_ShowDeleteConfirmDlg) return;

    ImGui::OpenPopup("Delete Section?");
    if (ImGui::BeginPopupModal("Delete Section?", &m_ShowDeleteConfirmDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_DeleteTargetIdx >= 0 && m_DeleteTargetIdx < (int)m_Ini.sections.size()) {
            const Section& sec = m_Ini.sections[m_DeleteTargetIdx];
            int kv_count = 0;
            for (const auto& l : sec.lines)
                if (l.type == Line_KeyValue) kv_count++;
            ImGui::Text("Delete [%s]?", sec.name.c_str());
            ImGui::Text("This section has %d key(s). This cannot be undone.", kv_count);
        }
        ImGui::Separator();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            DeleteScenario(m_DeleteTargetIdx);
            m_ShowDeleteConfirmDlg = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowDeleteConfirmDlg = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ConfigEditor::HandleKeyboardShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // Ctrl+S — Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && m_HasFile) {
        std::string err;
        if (IniWriter::Save(m_Ini, err)) {
            m_Dirty = false;
        } else {
            m_ErrorMsg = err;
        }
    }

    // Ctrl+N — New Scenario
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N) && m_HasFile) {
        m_ShowNewScenarioDlg = true;
        m_NewScenarioType = 0;
        memset(m_NewScenarioName, 0, sizeof(m_NewScenarioName));
    }

    // Ctrl+D — Clone/Duplicate Selected Scenario
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && m_HasFile && m_SelectedSection >= 0) {
        m_ShowCloneScenarioDlg = true;
        m_CloneSourceIdx = m_SelectedSection;
        memset(m_CloneNewName, 0, sizeof(m_CloneNewName));
        // Pre-fill with source scenario name + "_copy"
        std::string src_name = ExtractScenarioName(m_Ini.sections[m_SelectedSection].name);
        snprintf(m_CloneNewName, sizeof(m_CloneNewName), "%s_copy", src_name.c_str());
    }

    // Ctrl+Shift+K — Add new key
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_K) && m_HasFile && m_SelectedSection >= 0) {
        AddKey(m_SelectedSection);
    }

    // Delete — Delete selected key row
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_HasFile && !m_EditActive &&
        m_SelectedSection >= 0 && m_SelectedLine >= 0) {
        DeleteKey(m_SelectedSection, m_SelectedLine);
    }
}

void ConfigEditor::Render()
{
    HandleKeyboardShortcuts();
    // --- Menu bar ---
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open INI...")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("OpenIniDlgKey", "Open PerfConfig INI", ".ini,*", config);
            }

            if (ImGui::MenuItem("Save", "Ctrl+S", false, m_HasFile)) {
                std::string err;
                if (IniWriter::Save(m_Ini, err)) {
                    m_Dirty = false;
                } else {
                    m_ErrorMsg = err;
                }
            }

            if (ImGui::MenuItem("Save As...", nullptr, false, m_HasFile)) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("SaveIniAsDlgKey", "Save PerfConfig INI As", ".ini,*", config);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Close File", nullptr, false, m_HasFile)) {
                CloseFile();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- Handle file open dialog ---
    if (ImGuiFileDialog::Instance()->Display("OpenIniDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            if (!LoadFile(path)) {
                // m_ErrorMsg is already set
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // --- Handle save-as dialog ---
    if (ImGuiFileDialog::Instance()->Display("SaveIniAsDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string err;
            if (IniWriter::WriteToFile(m_Ini, path, err)) {
                m_Ini.file_path = path;
                snprintf(m_FilePath, sizeof(m_FilePath), "%s", path.c_str());
                m_Dirty = false;
            } else {
                m_ErrorMsg = err;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // --- Show errors if any ---
    if (!m_ErrorMsg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Error: %s", m_ErrorMsg.c_str());
        ImGui::PopStyleColor();
        if (ImGui::Button("Dismiss")) {
            m_ErrorMsg.clear();
        }
        ImGui::Separator();
    }

    // --- No file loaded ---
    if (!m_HasFile) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("Open a PerfConfig INI file to begin editing.");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) * 0.5f + ImGui::GetCursorPosX(),
            (avail.y - text_size.y) * 0.5f + ImGui::GetCursorPosY()
        ));
        ImGui::TextDisabled("Open a PerfConfig INI file to begin editing.");
        return;
    }

    // --- File header ---
    {
        const char* dirty_marker = m_Dirty ? " *" : "";
        // Extract just the filename from the path
        std::string display_path(m_FilePath);
        size_t slash = display_path.find_last_of("\\/");
        std::string filename = (slash != std::string::npos) ? display_path.substr(slash + 1) : display_path;

        ImGui::Text("%s%s  —  %s", filename.c_str(), dirty_marker, m_FilePath);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
        ImGui::Text("%d sections", (int)m_Ini.sections.size());
    }
    ImGui::Separator();

    // --- Two-panel layout ---
    RenderSectionTree();
    ImGui::SameLine();
    RenderKeyTable();

    // --- Modal dialogs ---
    RenderNewScenarioDialog();
    RenderCloneScenarioDialog();
    RenderRenameScenarioDialog();
    RenderDeleteConfirmDialog();
}

// --- Left panel: Section tree grouped by type ---

void ConfigEditor::RenderSectionTree()
{
    ImGui::BeginChild("CfgSectionTree", ImVec2(250, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

    // Filter
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SectionFilter", "Filter sections...", m_FilterBuf, sizeof(m_FilterBuf));

    std::string filter_lower;
    if (m_FilterBuf[0] != '\0') {
        filter_lower = m_FilterBuf;
        for (auto& c : filter_lower) c = (char)tolower((unsigned char)c);
    }

    ImGui::Separator();

    // Group sections by type
    struct TypeGroup {
        const char* label;
        SectionType type;
    };

    static const TypeGroup groups[] = {
        { "Global",     Section_PerfConfig },
        { "MDM",        Section_MDM },
        { "DSTS",       Section_DSTS },
        { "Producer",   Section_Producer },
        { "GWProducer", Section_GWProducer },
        { "Consumer",   Section_Consumer },
        { "Reliable",   Section_Reliable },
        { "Rehydrate",  Section_Rehydrate },
        { "Other",      Section_Unknown },
    };

    for (const auto& group : groups) {
        // Collect sections matching this type
        bool has_any = false;
        for (size_t i = 0; i < m_Ini.sections.size(); i++) {
            if (m_Ini.sections[i].section_type == group.type) {
                has_any = true;
                break;
            }
        }
        if (!has_any)
            continue;

        // For single-instance sections (PerfConfig, MDM, DSTS), show directly
        bool is_single = (group.type == Section_PerfConfig ||
                          group.type == Section_MDM ||
                          group.type == Section_DSTS);

        if (is_single) {
            for (size_t i = 0; i < m_Ini.sections.size(); i++) {
                if (m_Ini.sections[i].section_type != group.type)
                    continue;

                // Apply filter
                if (!filter_lower.empty()) {
                    std::string name_lower = m_Ini.sections[i].name;
                    for (auto& c : name_lower) c = (char)tolower((unsigned char)c);
                    if (name_lower.find(filter_lower) == std::string::npos)
                        continue;
                }

                bool selected = (m_SelectedSection == (int)i);
                if (ImGui::Selectable(m_Ini.sections[i].name.c_str(), selected)) {
                    m_SelectedSection = (int)i;
                    m_SelectedLine = -1;
                }

                // Right-click context menu for single sections
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Clone...")) {
                        m_ShowCloneScenarioDlg = true;
                        m_CloneSourceIdx = (int)i;
                        memset(m_CloneNewName, 0, sizeof(m_CloneNewName));
                        std::string src_name = m_Ini.sections[i].name;
                        snprintf(m_CloneNewName, sizeof(m_CloneNewName), "%s_copy", src_name.c_str());
                    }
                    ImGui::EndPopup();
                }
            }
        } else {
            // For scenario groups, use a tree node
            bool node_open = ImGui::TreeNodeEx(group.label, ImGuiTreeNodeFlags_DefaultOpen);
            if (node_open) {
                for (size_t i = 0; i < m_Ini.sections.size(); i++) {
                    if (m_Ini.sections[i].section_type != group.type)
                        continue;

                    std::string scenario_name = ExtractScenarioName(m_Ini.sections[i].name);

                    // Apply filter
                    if (!filter_lower.empty()) {
                        std::string name_lower = scenario_name;
                        for (auto& c : name_lower) c = (char)tolower((unsigned char)c);
                        if (name_lower.find(filter_lower) == std::string::npos)
                            continue;
                    }

                    bool selected = (m_SelectedSection == (int)i);
                    if (ImGui::Selectable(scenario_name.c_str(), selected)) {
                        m_SelectedSection = (int)i;
                        m_SelectedLine = -1;
                    }

                    // Right-click context menu for scenario sections
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Rename...")) {
                            m_ShowRenameScenarioDlg = true;
                            m_RenameTargetIdx = (int)i;
                            memset(m_RenameNewName, 0, sizeof(m_RenameNewName));
                            std::string sname = ExtractScenarioName(m_Ini.sections[i].name);
                            snprintf(m_RenameNewName, sizeof(m_RenameNewName), "%s", sname.c_str());
                        }
                        if (ImGui::MenuItem("Clone...")) {
                            m_ShowCloneScenarioDlg = true;
                            m_CloneSourceIdx = (int)i;
                            memset(m_CloneNewName, 0, sizeof(m_CloneNewName));
                            std::string sname = ExtractScenarioName(m_Ini.sections[i].name);
                            snprintf(m_CloneNewName, sizeof(m_CloneNewName), "%s_copy", sname.c_str());
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete...")) {
                            m_ShowDeleteConfirmDlg = true;
                            m_DeleteTargetIdx = (int)i;
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    ImGui::Separator();

    // --- Scenario operation buttons ---
    {
        bool has_selection = (m_SelectedSection >= 0 && m_SelectedSection < (int)m_Ini.sections.size());
        bool selection_is_scenario = false;
        if (has_selection) {
            SectionType st = m_Ini.sections[m_SelectedSection].section_type;
            selection_is_scenario = (st == Section_Producer || st == Section_GWProducer ||
                                     st == Section_Consumer || st == Section_Reliable ||
                                     st == Section_Rehydrate);
        }

        if (ImGui::Button("+ New Scenario", ImVec2(-1, 0))) {
            m_ShowNewScenarioDlg = true;
            m_NewScenarioType = 0;
            memset(m_NewScenarioName, 0, sizeof(m_NewScenarioName));
        }

        if (!has_selection) ImGui::BeginDisabled();
        if (ImGui::Button("Clone Selected", ImVec2(-1, 0))) {
            m_ShowCloneScenarioDlg = true;
            m_CloneSourceIdx = m_SelectedSection;
            memset(m_CloneNewName, 0, sizeof(m_CloneNewName));
            std::string sname = ExtractScenarioName(m_Ini.sections[m_SelectedSection].name);
            snprintf(m_CloneNewName, sizeof(m_CloneNewName), "%s_copy", sname.c_str());
        }
        if (!has_selection) ImGui::EndDisabled();

        if (!selection_is_scenario) ImGui::BeginDisabled();
        if (ImGui::Button("Rename Selected", ImVec2(-1, 0))) {
            m_ShowRenameScenarioDlg = true;
            m_RenameTargetIdx = m_SelectedSection;
            memset(m_RenameNewName, 0, sizeof(m_RenameNewName));
            std::string sname = ExtractScenarioName(m_Ini.sections[m_SelectedSection].name);
            snprintf(m_RenameNewName, sizeof(m_RenameNewName), "%s", sname.c_str());
        }
        if (!selection_is_scenario) ImGui::EndDisabled();

        if (!selection_is_scenario) ImGui::BeginDisabled();
        if (ImGui::Button("Delete Selected", ImVec2(-1, 0))) {
            m_ShowDeleteConfirmDlg = true;
            m_DeleteTargetIdx = m_SelectedSection;
        }
        if (!selection_is_scenario) ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

// --- Right panel: Key-value table for selected section ---

// Helper: format conditions as a compact display string
static std::string FormatConditions(const std::vector<Condition>& conds)
{
    if (conds.empty())
        return "";

    std::string result;
    for (size_t i = 0; i < conds.size(); i++) {
        if (i > 0) result += ", ";
        result += ScopeTypeName(conds[i].scope_type);
        result += ':';
        result += conds[i].scope_value;
    }
    return result;
}

// Helper: parse a conditions string back into a vector of Condition structs.
// Format: "Scope:Value, Scope:Value, ..."
static bool ParseConditionsFromString(const std::string& text, std::vector<Condition>& out)
{
    out.clear();
    if (text.empty())
        return true;

    // Split on ','
    std::string remaining = text;
    while (!remaining.empty()) {
        // Trim leading whitespace
        size_t start = remaining.find_first_not_of(" \t");
        if (start == std::string::npos) break;
        remaining = remaining.substr(start);

        // Find next comma
        size_t comma = remaining.find(',');
        std::string token = (comma != std::string::npos) ? remaining.substr(0, comma) : remaining;

        // Trim trailing whitespace from token
        size_t end = token.find_last_not_of(" \t");
        if (end != std::string::npos) token = token.substr(0, end + 1);

        // Parse "Scope:Value"
        size_t colon = token.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= token.size())
            return false;

        std::string scope_str = token.substr(0, colon);
        std::string value_str = token.substr(colon + 1);

        Condition cond;
        ParseScopeType(scope_str, cond.scope_type);
        cond.scope_value = value_str;
        out.push_back(cond);

        if (comma != std::string::npos)
            remaining = remaining.substr(comma + 1);
        else
            break;
    }
    return true;
}

void ConfigEditor::RenderKeyTable()
{
    ImGui::BeginChild("CfgKeyTable", ImVec2(0, 0), ImGuiChildFlags_Border);

    if (m_SelectedSection < 0 || m_SelectedSection >= (int)m_Ini.sections.size()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("Select a section from the tree.");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) * 0.5f + ImGui::GetCursorPosX(),
            (avail.y - text_size.y) * 0.5f + ImGui::GetCursorPosY()
        ));
        ImGui::TextDisabled("Select a section from the tree.");
        ImGui::EndChild();
        return;
    }

    Section& section = m_Ini.sections[m_SelectedSection];

    // Section header
    ImGui::Text("[%s]", section.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", SectionTypeName(section.section_type));
    ImGui::Separator();

    // Count key-value lines
    int kv_count = 0;
    for (const auto& line : section.lines) {
        if (line.type == Line_KeyValue) kv_count++;
    }

    // Key-value table
    ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp;

    // Reserve space at the bottom for buttons and stats
    float footer_height = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginTable("KeyValueTable", 3, table_flags, ImVec2(0, -footer_height))) {
        ImGui::TableSetupColumn("Conditions", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Track which line to delete (via right-click context menu)
        int line_to_delete = -1;

        for (size_t i = 0; i < section.lines.size(); i++) {
            Line& line = section.lines[i];

            if (line.type == Line_Comment) {
                // Show comments as a spanning row with muted color
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::TextWrapped("%s", line.comment.c_str());
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                continue;
            }

            if (line.type == Line_Blank)
                continue;

            // Line_KeyValue — editable row
            KeyEntry& entry = line.key_entry;
            bool is_selected_line = (m_SelectedLine == (int)i);

            ImGui::PushID((int)i);
            ImGui::TableNextRow();

            // --- Conditions column (editable) ---
            ImGui::TableNextColumn();
            {
                bool is_editing = (m_EditActive &&
                    m_EditSection == m_SelectedSection &&
                    m_EditLine == (int)i &&
                    m_EditColumn == 0);

                if (is_editing) {
                    if (m_EditFocusNeeded) {
                        ImGui::SetKeyboardFocusHere();
                        m_EditFocusNeeded = false;
                    }
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##cond", m_EditBuf, sizeof(m_EditBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                        ImGuiInputTextFlags_AutoSelectAll)) {
                        // Enter pressed — commit
                        std::vector<Condition> new_conds;
                        if (ParseConditionsFromString(m_EditBuf, new_conds)) {
                            entry.conditions = new_conds;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                    // Also commit on deactivation (clicking away)
                    if (ImGui::IsItemDeactivated()) {
                        std::vector<Condition> new_conds;
                        if (ParseConditionsFromString(m_EditBuf, new_conds)) {
                            entry.conditions = new_conds;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                } else {
                    std::string cond_str = FormatConditions(entry.conditions);
                    const char* display = entry.conditions.empty() ? "(none)" : cond_str.c_str();
                    if (entry.conditions.empty())
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                    ImGuiSelectableFlags sel_flags = ImGuiSelectableFlags_AllowDoubleClick |
                                                     ImGuiSelectableFlags_SpanAllColumns;
                    if (ImGui::Selectable(display, is_selected_line, sel_flags)) {
                        m_SelectedLine = (int)i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            m_EditSection = m_SelectedSection;
                            m_EditLine = (int)i;
                            m_EditColumn = 0;
                            m_EditActive = true;
                            m_EditFocusNeeded = true;
                            std::string cond_str2 = FormatConditions(entry.conditions);
                            snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", cond_str2.c_str());
                        }
                    }
                    if (entry.conditions.empty())
                        ImGui::PopStyleColor();
                }
            }

            // --- Key column (editable) ---
            ImGui::TableNextColumn();
            {
                bool is_editing = (m_EditActive &&
                    m_EditSection == m_SelectedSection &&
                    m_EditLine == (int)i &&
                    m_EditColumn == 1);

                if (is_editing) {
                    if (m_EditFocusNeeded) {
                        ImGui::SetKeyboardFocusHere();
                        m_EditFocusNeeded = false;
                    }
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##key", m_EditBuf, sizeof(m_EditBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                        ImGuiInputTextFlags_AutoSelectAll)) {
                        std::string new_key(m_EditBuf);
                        if (!new_key.empty() && new_key != entry.key) {
                            entry.key = new_key;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                    if (ImGui::IsItemDeactivated()) {
                        std::string new_key(m_EditBuf);
                        if (!new_key.empty() && new_key != entry.key) {
                            entry.key = new_key;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                } else {
                    if (ImGui::Selectable(entry.key.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_SelectedLine = (int)i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            m_EditSection = m_SelectedSection;
                            m_EditLine = (int)i;
                            m_EditColumn = 1;
                            m_EditActive = true;
                            m_EditFocusNeeded = true;
                            snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", entry.key.c_str());
                        }
                    }
                }
            }

            // --- Value column (editable) ---
            ImGui::TableNextColumn();
            {
                bool is_editing = (m_EditActive &&
                    m_EditSection == m_SelectedSection &&
                    m_EditLine == (int)i &&
                    m_EditColumn == 2);

                if (is_editing) {
                    if (m_EditFocusNeeded) {
                        ImGui::SetKeyboardFocusHere();
                        m_EditFocusNeeded = false;
                    }
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##val", m_EditBuf, sizeof(m_EditBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                        ImGuiInputTextFlags_AutoSelectAll)) {
                        std::string new_val(m_EditBuf);
                        if (new_val != entry.value) {
                            entry.value = new_val;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                    if (ImGui::IsItemDeactivated()) {
                        std::string new_val(m_EditBuf);
                        if (new_val != entry.value) {
                            entry.value = new_val;
                            MarkDirty();
                        }
                        m_EditActive = false;
                    }
                } else {
                    if (ImGui::Selectable(entry.value.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_SelectedLine = (int)i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            m_EditSection = m_SelectedSection;
                            m_EditLine = (int)i;
                            m_EditColumn = 2;
                            m_EditActive = true;
                            m_EditFocusNeeded = true;
                            snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", entry.value.c_str());
                        }
                    }
                }
            }

            // Right-click context menu for rows
            if (ImGui::BeginPopupContextItem("##rowctx")) {
                m_SelectedLine = (int)i;
                if (ImGui::MenuItem("Edit Conditions")) {
                    m_EditSection = m_SelectedSection;
                    m_EditLine = (int)i;
                    m_EditColumn = 0;
                    m_EditActive = true;
                    m_EditFocusNeeded = true;
                    std::string cond_str = FormatConditions(entry.conditions);
                    snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", cond_str.c_str());
                }
                if (ImGui::MenuItem("Edit Key")) {
                    m_EditSection = m_SelectedSection;
                    m_EditLine = (int)i;
                    m_EditColumn = 1;
                    m_EditActive = true;
                    m_EditFocusNeeded = true;
                    snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", entry.key.c_str());
                }
                if (ImGui::MenuItem("Edit Value")) {
                    m_EditSection = m_SelectedSection;
                    m_EditLine = (int)i;
                    m_EditColumn = 2;
                    m_EditActive = true;
                    m_EditFocusNeeded = true;
                    snprintf(m_EditBuf, sizeof(m_EditBuf), "%s", entry.value.c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Key")) {
                    line_to_delete = (int)i;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        // Apply deferred delete (outside the table iteration)
        if (line_to_delete >= 0) {
            DeleteKey(m_SelectedSection, line_to_delete);
        }
    }

    // --- Key operation buttons ---
    {
        if (ImGui::Button("+ Add Key")) {
            AddKey(m_SelectedSection);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Conditional")) {
            AddConditionalKey(m_SelectedSection);
        }
        ImGui::SameLine();

        bool has_line_selection = (m_SelectedLine >= 0 &&
            m_SelectedLine < (int)section.lines.size() &&
            section.lines[m_SelectedLine].type == Line_KeyValue);
        if (!has_line_selection) ImGui::BeginDisabled();
        if (ImGui::Button("Delete Key")) {
            DeleteKey(m_SelectedSection, m_SelectedLine);
        }
        if (!has_line_selection) ImGui::EndDisabled();
    }

    // Footer with stats
    ImGui::Separator();
    ImGui::Text("%d keys, %d lines total", kv_count, (int)section.lines.size());

    ImGui::EndChild();
}
