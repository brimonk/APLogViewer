#include "ConfigEditor.h"
#include "ImGuiFileDialog.h"
#include "ImGuiFileDialogConfig.h"

ConfigEditor::ConfigEditor()
    : m_HasFile(false)
{
    memset(m_FilePath, 0, sizeof(m_FilePath));
}

ConfigEditor::~ConfigEditor()
{
}

void ConfigEditor::Render()
{
    // --- Menu bar (rendered inside the parent window's menu bar) ---
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open INI...")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("OpenIniDlgKey", "Open PerfConfig INI", ".ini,*", config);
            }

            if (ImGui::MenuItem("Save", "Ctrl+S", false, m_HasFile)) {
                // TODO Phase 3: write file back
            }

            if (ImGui::MenuItem("Save As...")) {
                // TODO Phase 3: save-as dialog
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Close File", nullptr, false, m_HasFile)) {
                m_HasFile = false;
                memset(m_FilePath, 0, sizeof(m_FilePath));
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- Handle file open dialog result ---
    if (ImGuiFileDialog::Instance()->Display("OpenIniDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            snprintf(m_FilePath, sizeof(m_FilePath), "%s", path.c_str());
            m_HasFile = true;
            // TODO Phase 2: parse the INI file here
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // --- Stub content ---
    if (!m_HasFile) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("Open a PerfConfig INI file to begin editing.");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) * 0.5f + ImGui::GetCursorPosX(),
            (avail.y - text_size.y) * 0.5f + ImGui::GetCursorPosY()
        ));
        ImGui::TextDisabled("Open a PerfConfig INI file to begin editing.");
    } else {
        // File is loaded — show placeholder for section tree + key table
        ImGui::Text("Loaded: %s", m_FilePath);
        ImGui::Separator();

        // Left panel placeholder
        ImGui::BeginChild("CfgSectionTree", ImVec2(200, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
        ImGui::TextDisabled("Section tree (Phase 2)");
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel placeholder
        ImGui::BeginChild("CfgKeyTable", ImVec2(0, 0), ImGuiChildFlags_Border);
        ImGui::TextDisabled("Key/value editor (Phase 2)");
        ImGui::EndChild();
    }
}
