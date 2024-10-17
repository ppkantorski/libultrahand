#include <tesla.hpp>


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

extern "C" {
    void __assert_func(const char *_file, int _line, const char *_func, const char *_expr) {
        abort();
    }
}

//u8 TeslaFPS = 60;
u8 alphabackground = 0xD;
bool FullMode = true;
//PadState pad;
bool deactivateOriginalFooter = false;

u16 DefaultFramebufferWidth = 448;
u16 DefaultFramebufferHeight = 720;


namespace tsl {


    Color GradientColor(float temperature) {
        // Ensure temperature is within the range [0, 100]
        temperature = std::max(0.0f, std::min(100.0f, temperature)); // Celsius
        
        // this is where colors are at their full
        float blueStart = 35.0f;
        float greenStart = 45.0f;
        float yellowStart = 55.0f;
        float redStart = 65.0f;
        
        // Initialize RGB values
        uint8_t r, g, b, a = 0xFF;
        float t;

        if (temperature < blueStart) { // rgb 7, 7, 15 at blueStart
            r = 7;
            g = 7;
            b = 15;
        } else if (temperature >= blueStart && temperature < greenStart) {
            // Smooth color blending from (7 7 15) to (0 15 0)
            t = (temperature - blueStart) / (greenStart - blueStart);
            r = static_cast<uint8_t>(7 - 7 * t);
            g = static_cast<uint8_t>(7 + 8 * t);
            b = static_cast<uint8_t>(15 - 15 * t);
        } else if (temperature >= greenStart && temperature < yellowStart) {
            // Smooth color blending from (0 15 0) to (15 15 0)
            t = (temperature - greenStart) / (yellowStart - greenStart);
            r = static_cast<uint8_t>(15 * t);
            g = static_cast<uint8_t>(15);
            b = static_cast<uint8_t>(0);
        } else if (temperature >= yellowStart && temperature < redStart) {
            // Smooth color blending from (15 15 0) to (15 0 0)
            t = (temperature - yellowStart) / (redStart - yellowStart);
            r = static_cast<uint8_t>(15);
            g = static_cast<uint8_t>(15 - 15 * t);
            b = static_cast<uint8_t>(0);
        } else {
            // Full red
            r = 15;
            g = 0;
            b = 0;
        }
        
        return Color(r, g, b, a);
    }


    Color RGB888(const std::string& hexColor, size_t alpha, const std::string& defaultHexColor) {
        std::string validHex = hexColor.empty() || hexColor[0] != '#' ? hexColor : hexColor.substr(1);
        
        if (!isValidHexColor(validHex)) {
            validHex = defaultHexColor;
        }
        
        // Convert hex to RGB values
        uint8_t redValue = (hexMap[static_cast<unsigned char>(validHex[0])] << 4) | hexMap[static_cast<unsigned char>(validHex[1])];
        uint8_t greenValue = (hexMap[static_cast<unsigned char>(validHex[2])] << 4) | hexMap[static_cast<unsigned char>(validHex[3])];
        uint8_t blueValue = (hexMap[static_cast<unsigned char>(validHex[4])] << 4) | hexMap[static_cast<unsigned char>(validHex[5])];
        
        return Color(redValue >> 4, greenValue >> 4, blueValue >> 4, alpha);
    }


    std::tuple<uint8_t, uint8_t, uint8_t> hexToRGB444Floats(const std::string& hexColor, const std::string& defaultHexColor) {
        const char* validHex = hexColor.c_str();
        if (validHex[0] == '#') validHex++;
        
        if (!isValidHexColor(validHex)) {
            validHex = defaultHexColor.c_str();
            if (validHex[0] == '#') validHex++;
        }
        
        // Manually parse the hex string to an integer value
        unsigned int hexValue = (hexMap[static_cast<unsigned char>(validHex[0])] << 20) |
                                (hexMap[static_cast<unsigned char>(validHex[1])] << 16) |
                                (hexMap[static_cast<unsigned char>(validHex[2])] << 12) |
                                (hexMap[static_cast<unsigned char>(validHex[3])] << 8)  |
                                (hexMap[static_cast<unsigned char>(validHex[4])] << 4)  |
                                hexMap[static_cast<unsigned char>(validHex[5])];
        
        // Extract and scale the RGB components from 8-bit (0-255) to 4-bit float scale (0-15)
        uint8_t red = ((hexValue >> 16) & 0xFF) / 255.0f * 15.0f;
        uint8_t green = ((hexValue >> 8) & 0xFF) / 255.0f * 15.0f;
        uint8_t blue = (hexValue & 0xFF) / 255.0f * 15.0f;
        
        return std::make_tuple(red, green, blue);
    }

    namespace gfx {
		// Helper function to calculate string width
        float calculateStringWidth(const std::string& originalString, const float fontSize, const bool fixedWidthNumbers) {
            if (originalString.empty()) {
                return 0.0f;
            }
            
            static const stbtt_fontinfo& font = tsl::gfx::Renderer::get().getStandardFont();

            float totalWidth = 0.0f;
            ssize_t codepointWidth;
            u32 prevCharacter = 0;
            u32 currCharacter = 0;
        
            static std::unordered_map<u64, Renderer::Glyph> s_glyphCache;
        
            auto itStrEnd = originalString.cend();
            auto itStr = originalString.cbegin();
        
            u64 key = 0;
            Renderer::Glyph* glyph = nullptr;
            auto it = s_glyphCache.end();
        
            while (itStr != itStrEnd) {
                // Decode UTF-8 codepoint
                codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
                if (codepointWidth <= 0) {
                    break;
                }
        
                // Move the iterator forward by the width of the current codepoint
                itStr += codepointWidth;
        
                if (currCharacter == '\n') {
                    continue;
                }
        
                // Calculate glyph key
                key = (static_cast<u64>(currCharacter) << 32) | (static_cast<u64>(fixedWidthNumbers) << 31) | (static_cast<u64>(std::bit_cast<u32>(fontSize)));
        
                // Check cache for the glyph
                it = s_glyphCache.find(key);
        
                // If glyph not found, create and cache it
                if (it == s_glyphCache.end()) {
                    glyph = &s_glyphCache.emplace(key, Renderer::Glyph()).first->second;
        
                    // Use const_cast to handle const font
                    glyph->currFont = const_cast<stbtt_fontinfo*>(&font);
                    glyph->currFontSize = stbtt_ScaleForPixelHeight(glyph->currFont, fontSize);
                    stbtt_GetCodepointHMetrics(glyph->currFont, currCharacter, &glyph->xAdvance, nullptr);
                } else {
                    glyph = &it->second;
                }
        
                if (prevCharacter) {
                    float kernAdvance = stbtt_GetCodepointKernAdvance(glyph->currFont, prevCharacter, currCharacter);
                    totalWidth += kernAdvance * glyph->currFontSize;
                }
        
                totalWidth += int(glyph->xAdvance * glyph->currFontSize);
                prevCharacter = currCharacter;
            }
        
            return totalWidth;
        }
    }
}