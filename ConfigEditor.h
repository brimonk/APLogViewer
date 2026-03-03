#ifndef CONFIG_EDITOR_H
#define CONFIG_EDITOR_H

#include "common.h"
#include "imgui.h"

// Forward declaration of data model types (to be implemented in Phase 2)
// For now the Config Editor is a stub UI.

class ConfigEditor
{
public:
    ConfigEditor();
    ~ConfigEditor();

    // Main render function — call each frame when Config Editor view is active.
    // Renders the menu bar items, section tree, key table, and status bar.
    void Render();

private:
    // Placeholder state — will be expanded in later phases
    bool m_HasFile;
    char m_FilePath[512];
};

#endif // CONFIG_EDITOR_H
