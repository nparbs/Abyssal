// Custom standalone application for Abyssal (enabled with JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP).
// Identical to JUCE's default standalone app except that, on first run, the audio input is NOT muted:
// this is a guitar amp, so hearing the input through it is the whole point. Your choice in the
// audio settings is remembered afterwards.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{

class AbyssalStandaloneApp final : public juce::JUCEApplication
{
public:
    AbyssalStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = JucePlugin_Name;
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "";
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override           { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                 { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        auto* settings = appProperties.getUserSettings();
        if (! settings->containsKey ("shouldMuteInput"))
            settings->setValue ("shouldMuteInput", false);

        auto holder = std::make_unique<juce::StandalonePluginHolder> (settings, false, juce::String{}, nullptr,
                                                                      juce::Array<juce::StandalonePluginHolder::PluginInOuts>{},
                                                                      false);

        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
            std::move (holder));

        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] { if (auto* app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
};

} // namespace

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new AbyssalStandaloneApp(); }
