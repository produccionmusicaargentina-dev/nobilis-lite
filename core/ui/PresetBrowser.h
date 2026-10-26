#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"
#include "NobLookAndFeel.h"

//==============================================================================
// In-plugin preset browser: search field, category column, preset list and
// actions. Clicking a preset auditions it (the panel stays open); double-click
// loads and closes. Click outside or ESC to close.
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (PresetManager&);

    std::function<void()> onSaveAs;
    std::function<void()> onChanged;

    void open (juce::Rectangle<float> panelBoundsInParent);
    void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class ActionButton : public juce::Button
    {
    public:
        ActionButton (const juce::String& n, bool primary = false) : juce::Button (n), isPrimary (primary) {}
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
        bool isPrimary;
    };

    void rebuildList();
    void ensureCurrentVisible();
    juce::Rectangle<float> categoryArea() const;
    juce::Rectangle<float> listArea() const;
    float rowHeight() const;
    int categoryAt (juce::Point<float>) const;
    int rowAt (juce::Point<float>) const;
    void updateButtons();

    PresetManager& pm;
    juce::Rectangle<float> panel;
    juce::TextEditor search;
    ActionButton saveBtn { "SAVE" }, saveAsBtn { "SAVE AS", true }, deleteBtn { "DELETE" }, folderBtn { "FOLDER" };

    juce::StringArray categories;     // "All", factory categories..., "User"
    int selectedCategory = 0;
    std::vector<int> rows;            // indices into pm.getPresets()
    float scroll = 0.0f;
    int hoverRow = -1, hoverCategory = -1;
    juce::uint32 deleteArmedAt = 0;
};
