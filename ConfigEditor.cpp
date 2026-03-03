#include "ConfigEditor.h"
#include "ImGuiFileDialog.h"
#include "ImGuiFileDialogConfig.h"

ConfigEditor::ConfigEditor()
    : m_HasFile(false)
    , m_Dirty(false)
    , m_SelectedSection(-1)
    , m_EditSection(-1)
    , m_EditLine(-1)
    , m_EditColumn(-1)
    , m_EditActive(false)
    , m_EditFocusNeeded(false)
{
    memset(m_FilePath, 0, sizeof(m_FilePath));
    memset(m_FilterBuf, 0, sizeof(m_FilterBuf));
    memset(m_EditBuf, 0, sizeof(m_EditBuf));
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
    m_EditSection = -1;
    m_EditLine = -1;
    m_EditColumn = -1;
    m_EditActive = false;
    m_EditFocusNeeded = false;
    memset(m_FilePath, 0, sizeof(m_FilePath));
    m_Ini = IniFile();
    m_ErrorMsg.clear();
}

void ConfigEditor::MarkDirty()
{
    m_Dirty = true;
}

void ConfigEditor::HandleKeyboardShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && m_HasFile) {
        std::string err;
        if (IniWriter::Save(m_Ini, err)) {
            m_Dirty = false;
        } else {
            m_ErrorMsg = err;
        }
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
                    }
                }
                ImGui::TreePop();
            }
        }
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

    if (ImGui::BeginTable("KeyValueTable", 3, table_flags)) {
        ImGui::TableSetupColumn("Conditions", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

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
                    if (ImGui::Selectable(display, false, ImGuiSelectableFlags_AllowDoubleClick)) {
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

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // Footer with stats
    ImGui::Separator();
    ImGui::Text("%d keys, %d lines total", kv_count, (int)section.lines.size());

    ImGui::EndChild();
}
