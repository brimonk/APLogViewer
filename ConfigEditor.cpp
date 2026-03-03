#include "ConfigEditor.h"
#include "ImGuiFileDialog.h"
#include "ImGuiFileDialogConfig.h"

ConfigEditor::ConfigEditor()
    : m_HasFile(false)
    , m_Dirty(false)
    , m_SelectedSection(-1)
{
    memset(m_FilePath, 0, sizeof(m_FilePath));
    memset(m_FilterBuf, 0, sizeof(m_FilterBuf));
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
    memset(m_FilePath, 0, sizeof(m_FilePath));
    m_Ini = IniFile();
    m_ErrorMsg.clear();
}

void ConfigEditor::Render()
{
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
        return "(none)";

    std::string result;
    for (size_t i = 0; i < conds.size(); i++) {
        if (i > 0) result += ", ";
        result += ScopeTypeName(conds[i].scope_type);
        result += ':';
        result += conds[i].scope_value;
    }
    return result;
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

        int row_idx = 0;
        for (size_t i = 0; i < section.lines.size(); i++) {
            const Line& line = section.lines[i];

            if (line.type == Line_Comment) {
                // Show comments as a spanning row with muted color
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::TextWrapped("%s", line.comment.c_str());
                ImGui::PopStyleColor();
                // Skip the other columns (they'll be empty)
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                continue;
            }

            if (line.type == Line_Blank)
                continue;

            // Line_KeyValue
            const KeyEntry& entry = line.key_entry;

            ImGui::PushID((int)i);
            ImGui::TableNextRow();

            // Conditions column
            ImGui::TableNextColumn();
            {
                std::string cond_str = FormatConditions(entry.conditions);
                if (entry.conditions.empty()) {
                    ImGui::TextDisabled("%s", cond_str.c_str());
                } else {
                    ImGui::TextWrapped("%s", cond_str.c_str());
                }
            }

            // Key column
            ImGui::TableNextColumn();
            ImGui::Text("%s", entry.key.c_str());

            // Value column
            ImGui::TableNextColumn();
            ImGui::Text("%s", entry.value.c_str());

            ImGui::PopID();
            row_idx++;
        }

        ImGui::EndTable();
    }

    // Footer with stats
    ImGui::Separator();
    ImGui::Text("%d keys, %d lines total", kv_count, (int)section.lines.size());

    ImGui::EndChild();
}
