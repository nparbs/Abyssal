#include "FileLibrary.h"

namespace abyssal
{

FileLibrary::FileLibrary (juce::String name, juce::String wc) : folderName (std::move (name)), wildcard (std::move (wc)) {}

juce::File FileLibrary::getDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Abyssal").getChildFile (folderName);
    dir.createDirectory();
    return dir;
}

juce::Array<juce::File> FileLibrary::list() const
{
    const auto root = getDirectory();
    juce::Array<juce::File> files;
    for (const auto& entry : juce::RangedDirectoryIterator (root, true, wildcard, juce::File::findFiles))
        files.add (entry.getFile());

    struct ByPath
    {
        juce::File root;
        int compareElements (const juce::File& a, const juce::File& b) const
        {
            // Root files first, then folders, each in natural order.
            const auto ra = a.getRelativePathFrom (root), rb = b.getRelativePathFrom (root);
            const bool fa = ra.containsChar (juce::File::getSeparatorChar()), fb = rb.containsChar (juce::File::getSeparatorChar());
            if (fa != fb) return fa ? 1 : -1;
            return ra.compareNatural (rb);
        }
    } sorter { root };
    files.sort (sorter);
    return files;
}

juce::File FileLibrary::copyInto (const juce::File& source, const juce::File& targetDir)
{
    targetDir.createDirectory();
    auto target = targetDir.getChildFile (source.getFileName());
    if (target.existsAsFile())
    {
        if (target.getSize() == source.getSize() && target.hasIdenticalContentTo (source))
            return target;
        target = targetDir.getNonexistentChildFile (source.getFileNameWithoutExtension(), source.getFileExtension(), false);
    }
    return source.copyFileTo (target) ? target : juce::File();
}

juce::File FileLibrary::add (const juce::File& file) const
{
    if (contains (file))
        return file;
    const auto copy = copyInto (file, getDirectory());
    return copy.existsAsFile() ? copy : file;
}

juce::Array<juce::File> FileLibrary::importFolder (const juce::File& folder) const
{
    juce::Array<juce::File> imported;
    if (! folder.isDirectory() || contains (folder) || folder == getDirectory())
        return imported;

    const auto targetRoot = getDirectory().getChildFile (juce::File::createLegalFileName (folder.getFileName()));
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, wildcard, juce::File::findFiles))
    {
        const auto src = entry.getFile();
        const auto rel = src.getParentDirectory().getRelativePathFrom (folder);
        const auto dir = rel == "." || rel.isEmpty() ? targetRoot : targetRoot.getChildFile (rel);
        const auto copy = copyInto (src, dir);
        if (copy.existsAsFile())
            imported.add (copy);
    }

    struct ByPath { static int compareElements (const juce::File& a, const juce::File& b)
                    { return a.getFullPathName().compareNatural (b.getFullPathName()); } } sorter;
    imported.sort (sorter);
    return imported;
}

bool FileLibrary::remove (const juce::File& file) const
{
    if (! contains (file) || ! file.moveToTrash())
        return false;

    // Remove now-empty sub-folders up to the library root.
    for (auto dir = file.getParentDirectory(); dir != getDirectory() && dir.isAChildOf (getDirectory()); dir = dir.getParentDirectory())
    {
        if (dir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) > 0) break;
        dir.deleteFile();
    }
    return true;
}

juce::String FileLibrary::folderOf (const juce::File& f) const
{
    const auto parent = f.getParentDirectory();
    return parent == getDirectory() ? juce::String() : parent.getRelativePathFrom (getDirectory());
}

juce::File FileLibrary::step (const juce::File& current, int delta) const
{
    const auto all = list();
    if (all.isEmpty())
        return {};

    // Step within the current file's folder (a "pack"); fall back to the whole library.
    juce::Array<juce::File> scope;
    if (contains (current))
        for (auto& f : all)
            if (f.getParentDirectory() == current.getParentDirectory())
                scope.add (f);
    if (scope.isEmpty())
        scope = all;

    const int idx = scope.indexOf (current);
    if (idx < 0)
        return delta >= 0 ? scope.getFirst() : scope.getLast();
    return scope[(idx + delta + scope.size()) % scope.size()];
}

} // namespace abyssal
