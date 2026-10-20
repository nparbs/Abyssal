#pragma once

#include <juce_core/juce_core.h>

namespace abyssal
{

/** A folder under Documents/Abyssal that collects files of one kind (IRs, NAM captures).
    Files loaded from elsewhere are copied in so they stay available; whole folders can be imported
    and keep their sub-folder structure. */
class FileLibrary
{
public:
    FileLibrary (juce::String folderName, juce::String wildcard);

    juce::File getDirectory() const;
    const juce::String& getWildcard() const noexcept { return wildcard; }
    bool contains (const juce::File& f) const { return f.isAChildOf (getDirectory()); }

    /** All files, sorted by relative path. */
    juce::Array<juce::File> list() const;

    /** Copies a file in (reusing an identical copy). Returns the library copy, or the original if copying failed. */
    juce::File add (const juce::File& file) const;

    /** Copies every matching file under `folder` into a same-named sub-folder of the library.
        Returns the imported files (library copies) in sorted order. */
    juce::Array<juce::File> importFolder (const juce::File& folder) const;

    /** Moves a library file to the Recycle Bin; empty sub-folders are tidied up. */
    bool remove (const juce::File& file) const;

    /** The next / previous file in the same folder as `current` (wrapping), for quick auditioning. */
    juce::File step (const juce::File& current, int delta) const;

    /** Sub-folder name relative to the library root ("" for the root). */
    juce::String folderOf (const juce::File& f) const;

private:
    static juce::File copyInto (const juce::File& source, const juce::File& targetDir);
    juce::String folderName, wildcard;
};

} // namespace abyssal
