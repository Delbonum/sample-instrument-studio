#include "Assets.h"

#include <BinaryData.h>

namespace sis
{
namespace
{
    const char* findResource (const char* originalFilename, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* resourceName = BinaryData::namedResourceList[i];

            if (juce::String (BinaryData::getNamedResourceOriginalFilename (resourceName)) == originalFilename)
                return BinaryData::getNamedResource (resourceName, size);
        }

        size = 0;
        return nullptr;
    }

    juce::Typeface::Ptr loadTypeface (const char* originalFilename)
    {
        int size = 0;
        if (const auto* data = findResource (originalFilename, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);

        jassertfalse; // Schrift fehlt in SisBinaryData
        return {};
    }

    juce::Font makeFont (juce::Typeface::Ptr typeface, float size, float tracking)
    {
        auto options = typeface != nullptr ? juce::FontOptions (typeface) : juce::FontOptions();
        return juce::Font (options.withPointHeight (size).withKerningFactor (tracking));
    }
} // namespace

juce::String manualHtml()
{
    int size = 0;

    if (const auto* data = findResource ("Handbuch.html", size))
        return juce::String::fromUTF8 (data, size);

    return {};
}

FontLibrary::FontLibrary()
    : sansRegular  (loadTypeface ("IBMPlexSans-Regular.ttf")),
      sansMedium   (loadTypeface ("IBMPlexSans-Medium.ttf")),
      sansSemibold (loadTypeface ("IBMPlexSans-SemiBold.ttf")),
      monoRegular  (loadTypeface ("IBMPlexMono-Regular.ttf")),
      monoMedium   (loadTypeface ("IBMPlexMono-Medium.ttf"))
{
}

juce::Typeface::Ptr FontLibrary::getSans (Weight w) const
{
    switch (w)
    {
        case Weight::medium:   return sansMedium;
        case Weight::semibold: return sansSemibold;
        case Weight::regular:  break;
    }
    return sansRegular;
}

juce::Typeface::Ptr FontLibrary::getMono (Weight w) const
{
    return w == Weight::regular ? monoRegular : monoMedium;
}

juce::Font sansFont (float size, Weight w)
{
    const juce::SharedResourcePointer<FontLibrary> fonts;
    return makeFont (fonts->getSans (w), size, 0.0f);
}

juce::Font monoFont (float size, Weight w)
{
    const juce::SharedResourcePointer<FontLibrary> fonts;
    return makeFont (fonts->getMono (w), size, 0.0f);
}

juce::Font capsFont (float size, float tracking)
{
    const juce::SharedResourcePointer<FontLibrary> fonts;
    return makeFont (fonts->getMono (Weight::regular), size, tracking);
}

float textWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox (0, -1, true).getWidth();
}

juce::Image appIcon()
{
    int size = 0;
    if (const auto* data = findResource ("sis_icon.png", size))
        return juce::ImageCache::getFromMemory (data, size);
    return {};
}
} // namespace sis
