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
    : juce::AudioProcessorEditor(p), proc(p),
    codeEditor (codeDocument, &codeTokeniser)
{
    // code editor
    // code editor
    codeEditor.loadContent(proc.get_source());

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

    // Create the font using modern JUCE FontOptions
    juce::Font font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(16.0f));
    g.setFont(font);

    // Split the logo into separate lines
    juce::String logoStr(juce::String::fromUTF8(LOGO));
    juce::StringArray lines;
    lines.addLines(logoStr);

    float line_height = font.getHeight();
    float total_height = lines.size() * line_height;
    float start_y = header_bounds.getY() + (header_bounds.getHeight() - total_height) / 2.0f;

    // Use a standard integer loop to completely avoid any implicit boolean casting issues
    for (int i = 0; i < lines.size(); ++i)
    {
        // Explicitly measure the string width using GlyphArrangement
        float line_width = juce::GlyphArrangement::getStringWidth(font, lines[i]);
        float start_x = header_bounds.getX() + (header_bounds.getWidth() - line_width) / 2.0f;

        g.drawText(lines[i], start_x, start_y + (i * line_height), line_width, line_height, juce::Justification::left, false);
    }

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
