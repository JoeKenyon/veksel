#include "Editor.h"
#include "Log.h"

namespace
{
    const char* LOGO =
        "██╗   ██╗███████╗██╗  ██╗███████╗███████╗██╗     \n"
        "██║   ██║██╔════╝██║ ██╔╝██╔════╝██╔════╝██║     \n"
        "██║   ██║█████╗  █████╔╝ ███████╗█████╗  ██║     \n"
        "╚██╗ ██╔╝██╔══╝  ██╔═██╗ ╚════██║██╔══╝  ██║     \n"
        " ╚████╔╝ ███████╗██║  ██╗███████║███████╗███████╗\n"
        "  ╚═══╝  ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝";

    //float params[6] = {};
}

VekselEditor::VekselEditor(VekselProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    // code editor
    codeEditor.loadContent(proc.get_source());
    codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId,     juce::Colours::black);
    codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId,    juce::Colours::white);
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberTextId,       juce::Colour(0xff888888));
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xff111111));

    // the tokeniser's default colours are for light backgrounds
    auto scheme = codeEditor.getColourScheme();
    for (auto& t : scheme.types)
    {
        if      (t.name == "Error")                        t.colour = juce::Colours::red;
        else if (t.name == "Comment")                      t.colour = juce::Colour(0xff888888);
        else if (t.name == "Keyword")                      t.colour = juce::Colour(0xff6cb6ff);
        else if (t.name == "Integer" || t.name == "Float") t.colour = juce::Colour(0xffe0a458);
        else if (t.name == "String")                       t.colour = juce::Colour(0xff98c379);
        else if (t.name == "Preprocessor Text")            t.colour = juce::Colour(0xffc678dd);
        else                                               t.colour = juce::Colours::white;
    }
    codeEditor.setColourScheme(scheme);
    addAndMakeVisible(codeEditor);

    // compile button
    compileButton.setButtonText("Compile & Run");
    compileButton.onClick = [this]
    {
        proc.compile(codeEditor.getDocument().getAllContent());
        push_log("[info] compilation triggered via button");
    };
    addAndMakeVisible(compileButton);

    // log
    logText.setMultiLine(true);
    logText.setReadOnly(true);
    logText.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    addAndMakeVisible(logText);

    setSize(900, 800);
    setResizable(true, true);
    startTimerHz(10);
}

VekselEditor::~VekselEditor()
{
    stopTimer();
}

void VekselEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    g.setColour(juce::Colours::white);
    //g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));
    g.drawFittedText(juce::String::fromUTF8(LOGO), header_bounds, 6, 1.0f);

    g.setColour(juce::Colours::darkgrey);
}

void VekselEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    header_bounds = area.removeFromTop(70);
    area.removeFromTop(8);

    logText.setBounds(area.removeFromBottom(160));
    area.removeFromBottom(8);

    // editor + compile button
    compileButton.setBounds(area.removeFromBottom(25));
    codeEditor.setBounds(area);
}

void VekselEditor::timerCallback()
{
    if (auto text = take_log_text())
    {
        logText.setText(*text, false);
        logText.moveCaretToEnd();
    }
}
