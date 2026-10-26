#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"
#include "NobLookAndFeel.h"

//==============================================================================
// Small hardware-style button used in the preset bar.
class BarButton : public juce::Button
{
public:
    enum Kind { Prev, Next, Save, Undo, Redo, Copy };
    BarButton (Kind k) : juce::Button ("bar"), kind (k) {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;

private:
    Kind kind;
};

//==============================================================================
// [<] [>]  [   Preset Name *   v ]  [save]
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);

    // Called when the user asks to save under a new name.
    std::function<void()> onSaveAs;
    // Called when the name field is clicked (opens the preset browser).
    std::function<void()> onBrowse;

    void refresh();                 // call periodically (timer)
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void saveCurrent();

    PresetManager& pm;
    BarButton prev { BarButton::Prev }, next { BarButton::Next }, save { BarButton::Save };
    juce::Rectangle<float> nameArea;
    juce::String shownName;
    bool shownModified = false, hoverName = false;
};

//==============================================================================
// Segmented A | B switch for quick comparisons.
class ABSwitch : public juce::Button
{
public:
    ABSwitch() : juce::Button ("ab") {}
    bool onB = false;
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

//==============================================================================
// In-editor dialog for naming a preset (no separate OS window, safe in every host).
class SaveOverlay : public juce::Component
{
public:
    SaveOverlay();

    std::function<void (const juce::String&)> onConfirm;
    std::function<bool (const juce::String&)> existsCheck;

    void show (const juce::String& initialName);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    class DialogButton : public juce::Button
    {
    public:
        DialogButton (const juce::String& n, bool primary) : juce::Button (n), isPrimary (primary) {}
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
        bool isPrimary;
    };

    void confirm();
    void close();
    juce::Rectangle<float> card() const;

    juce::TextEditor nameEditor;
    DialogButton ok { "SAVE", true }, cancel { "CANCEL", false };
};
