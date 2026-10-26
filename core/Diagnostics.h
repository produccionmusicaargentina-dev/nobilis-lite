#pragma once

#include <juce_core/juce_core.h>
#include "Product.h"

//==============================================================================
// Lightweight crash breadcrumbs (same as GRAVIS). When NOB_DIAGNOSTICS=1,
// each lifecycle step (constructor, prepare, editor open...) is appended to:
//   Windows: %APPDATA%\Nobilis\Lite\<plugin>_log.txt
//   macOS:   ~/Library/Logs/Nobilis/<plugin>_log.txt
// If the host crashes, the last line shows where it happened.
// Disable for release builds with -DNOB_DIAGNOSTICS=OFF.
//==============================================================================
#ifndef NOB_DIAGNOSTICS
 #define NOB_DIAGNOSTICS 1
#endif

namespace nob::diag
{
#if NOB_DIAGNOSTICS
    inline juce::File logFile()
    {
       #if JUCE_MAC
        auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Logs/Nobilis");
       #else
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Nobilis").getChildFile (nob::packFolder);
       #endif
        dir.createDirectory();
        return dir.getChildFile (juce::String (nob::product().name).toLowerCase() + "_log.txt");
    }

    inline void step (const juce::String& what)
    {
        static juce::CriticalSection lock;
        const juce::ScopedLock sl (lock);

        auto f = logFile();
        if (f.getSize() > 256 * 1024)
            f.deleteFile();

        f.appendText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S ")
                      + juce::String (juce::Time::getMillisecondCounter() % 1000).paddedLeft ('0', 3)
                      + "  " + what + "\n", false, false, "\n");
    }

    // Logs only the first time a given call site runs (cheap to leave in paint / process)
    #define NOB_STEP_ONCE(text) \
        do { static std::atomic<bool> nobDone { false }; \
             if (! nobDone.exchange (true)) nob::diag::step (text); } while (false)
#else
    inline void step (const juce::String&) {}
    #define NOB_STEP_ONCE(text) do {} while (false)
#endif
}
