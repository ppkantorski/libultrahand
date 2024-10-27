#include <tesla.hpp>


namespace gfx {

    extern "C" u64 __nx_vi_layer_id;

    Renderer& Renderer::get() {
        static Renderer renderer;
        return renderer;
    }

    Renderer::Renderer() {
        updateDrawFunction();
    }

    void Renderer::init() {
        auto [horizontalUnderscanPixels, verticalUnderscanPixels] = getUnderscanPixels();
        if (this->m_initialized) return;

        tsl::hlp::doWithSmSession([this, horizontalUnderscanPixels] {
            viInitialize(ViServiceType_Manager);
            viOpenDefaultDisplay(&this->m_display);
            viGetDisplayVsyncEvent(&this->m_display, &this->m_vsyncEvent);
            viCreateManagedLayer(&this->m_display, static_cast<ViLayerFlags>(0), 0, &__nx_vi_layer_id);
            viCreateLayer(&this->m_display, &this->m_layer);
            viSetLayerScalingMode(&this->m_layer, ViScalingMode_FitToLayer);

            if (horizontalUnderscanPixels == 0) {
                s32 layerZ = 0;
                if (R_SUCCEEDED(viGetZOrderCountMax(&this->m_display, &layerZ)) && layerZ > 0) {
                    viSetLayerZ(&this->m_layer, layerZ);
                } else {
                    viSetLayerZ(&this->m_layer, 255); // Max fallback
                }
            } else {
                viSetLayerZ(&this->m_layer, 34); // For underscanning
            }

            viAddToLayerStack(&this->m_layer, ViLayerStack_Default);
            viSetLayerSize(&this->m_layer, cfg::LayerWidth, cfg::LayerHeight);
            viSetLayerPosition(&this->m_layer, cfg::LayerPosX, cfg::LayerPosY);
            nwindowCreateFromLayer(&this->m_window, &this->m_layer);
            framebufferCreate(&this->m_framebuffer, &this->m_window, cfg::FramebufferWidth, cfg::FramebufferHeight, PIXEL_FORMAT_RGBA_4444, 2);
            setInitialize();
            initFonts();
            setExit();
        });

        this->m_initialized = true;
    }

    void Renderer::exit() {
        if (!this->m_initialized) return;
        framebufferClose(&this->m_framebuffer);
        nwindowClose(&this->m_window);
        viDestroyManagedLayer(&this->m_layer);
        viCloseDisplay(&this->m_display);
        eventClose(&this->m_vsyncEvent);
        viExit();
    }

    Result Renderer::initFonts() {
        static PlFontData stdFontData, localFontData, extFontData;

        TSL_R_TRY(plGetSharedFontByType(&stdFontData, PlSharedFontType_Standard));
        u8 *fontBuffer = reinterpret_cast<u8*>(stdFontData.address);
        stbtt_InitFont(&this->m_stdFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

        u64 languageCode;
        if (R_SUCCEEDED(setGetSystemLanguage(&languageCode))) {
            SetLanguage setLanguage;
            TSL_R_TRY(setMakeLanguage(languageCode, &setLanguage));
            this->m_hasLocalFont = true;
            switch (setLanguage) {
                case SetLanguage_ZHCN:
                case SetLanguage_ZHHANS:
                    TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseSimplified));
                    break;
                case SetLanguage_KO:
                    TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_KO));
                    break;
                case SetLanguage_ZHTW:
                case SetLanguage_ZHHANT:
                    TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseTraditional));
                    break;
                default:
                    this->m_hasLocalFont = false;
                    break;
            }

            if (this->m_hasLocalFont) {
                fontBuffer = reinterpret_cast<u8*>(localFontData.address);
                stbtt_InitFont(&this->m_localFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
            }
        }

        TSL_R_TRY(plGetSharedFontByType(&extFontData, PlSharedFontType_NintendoExt));
        fontBuffer = reinterpret_cast<u8*>(extFontData.address);
        stbtt_InitFont(&this->m_extFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

        return 0;
    }

    void Renderer::setOpacity(float opacity) {
        s_opacity = std::clamp(opacity, 0.0F, 1.0F);
    }

    void Renderer::enableScissoring(const s32 x, const s32 y, const s32 w, const s32 h) {
        this->m_scissoringStack.emplace(x, y, w, h);
    }

    void Renderer::disableScissoring() {
        if (!this->m_scissoringStack.empty())
            this->m_scissoringStack.pop();
    }

    void Renderer::setPixel(const s32 x, const s32 y, const Color& color, const u32 offset) {
        if (x < cfg::FramebufferWidth && y < cfg::FramebufferHeight && offset != UINT32_MAX) {
            Color* framebuffer = static_cast<Color*>(getCurrentFramebuffer());
            framebuffer[offset] = color;
        }
    }

    u8 Renderer::blendColor(const u8 src, const u8 dst, const u8 alpha) {
        return (dst * alpha + src * (0x0F - alpha)) >> 4;
    }

    void Renderer::setPixelBlendSrc(const s32 x, const s32 y, const Color& color) {
        u32 offset = getPixelOffset(x, y);
        if (offset == UINT32_MAX) return;

        u16* framebuffer = static_cast<u16*>(getCurrentFramebuffer());
        Color src(framebuffer[offset]);

        Color end(0);
        end.r = blendColor(src.r, color.r, color.a);
        end.g = blendColor(src.g, color.g, color.a);
        end.b = blendColor(src.b, color.b, color.a);
        end.a = src.a;

        setPixel(x, y, end, offset);
    }

    void Renderer::setPixelBlendDst(const s32 x, const s32 y, const Color& color) {
        u32 offset = getPixelOffset(x, y);
        if (offset == UINT32_MAX) return;

        u16* framebuffer = static_cast<u16*>(getCurrentFramebuffer());
        Color src(framebuffer[offset]);

        Color end(0);
        end.r = blendColor(src.r, color.r, color.a);
        end.g = blendColor(src.g, color.g, color.a);
        end.b = blendColor(src.b, color.b, color.a);
        end.a = color.a + (src.a * (0xF - color.a) / 0xF);

        setPixel(x, y, end, offset);
    }

    void Renderer::drawRect(const s32 x, const s32 y, const s32 w, const s32 h, const Color& color) {
        s32 x_end = x + w;
        s32 y_end = y + h;

        for (s32 yi = y; yi < y_end; ++yi) {
            for (s32 xi = x; xi < x_end; ++xi) {
                setPixelBlendDst(xi, yi, color);
            }
        }
    }

    void Renderer::drawEmptyRect(s16 x, s16 y, s16 w, s16 h, Color color) {
        if (x < 0 || y < 0 || x >= cfg::FramebufferWidth || y >= cfg::FramebufferHeight)
            return;

        for (s16 x1 = x; x1 <= (x + w); x1++) {
            for (s16 y1 = y; y1 <= (y + h); y1++) {
                if (y1 == y || x1 == x || y1 == y + h || x1 == x + w)
                    setPixelBlendDst(x1, y1, color);
            }
        }
    }

    void Renderer::drawLine(s16 x0, s16 y0, s16 x1, s16 y1, Color color) {
        if ((x0 == x1) && (y0 == y1)) {
            this->setPixelBlendDst(x0, y0, color);
            return;
        }

        s16 x_max = std::max(x0, x1);
        s16 y_max = std::max(y0, y1);
        s16 x_min = std::min(x0, x1);
        s16 y_min = std::min(y0, y1);

        if (x_min < 0 || y_min < 0 || x_min >= cfg::FramebufferWidth || y_min >= cfg::FramebufferHeight)
            return;

        // y = mx + b
        s16 dy = y_max - y_min;
        s16 dx = x_max - x_min;

        if (dx == 0) {
            for (s16 y = y_min; y <= y_max; y++) {
                this->setPixelBlendDst(x_min, y, color);
            }
            return;
        }

        float m = (float)dy / float(dx);
        float b = y_min - (m * x_min);

        for (s16 x = x_min; x <= x_max; x++) {
            s16 y = std::lround((m * (float)x) + b);
            s16 y_end = std::lround((m * (float)(x+1)) + b);
            if (y == y_end) {
                if (x <= x_max && y <= y_max)
                    this->setPixelBlendDst(x, y, color);
            }
            else while (y < y_end) {
                if (x <= x_max && y <= y_max)
                    this->setPixelBlendDst(x, y, color);
                y += 1;
            }
        }
                    
    }

    void Renderer::drawDashedLine(s16 x0, s16 y0, s16 x1, s16 y1, s16 line_width, Color color) {
        // Source of formula: https://www.cc.gatech.edu/grads/m/Aaron.E.McClennen/Bresenham/code.html
        s16 x_min = std::min(x0, x1);
        s16 x_max = std::max(x0, x1);
        s16 y_min = std::min(y0, y1);
        s16 y_max = std::max(y0, y1);
        if (x_min < 0 || y_min < 0 || x_min >= cfg::FramebufferWidth || y_min >= cfg::FramebufferHeight)
            return;
        s16 dx = x_max - x_min;
        s16 dy = y_max - y_min;
        s16 d = 2 * dy - dx;
        s16 incrE = 2*dy;
        s16 incrNE = 2*(dy - dx);
        this->setPixelBlendDst(x_min, y_min, color);
        s16 x = x_min;
        s16 y = y_min;
        s16 rendered = 0;
        while(x < x1) {
            if (d <= 0) {
                d += incrE;
                x++;
            }
            else {
                d += incrNE;
                x++;
                y++;
            }
            rendered++;
            if (x < 0 || y < 0 || x >= cfg::FramebufferWidth || y >= cfg::FramebufferHeight)
                continue;
            if (x <= x_max && y <= y_max) {
                if (rendered > 0 && rendered < line_width) {
                    this->setPixelBlendDst(x, y, color);
                }
                else if (rendered > 0 && rendered >= line_width) {
                    rendered *= -1;
                }
            }
        } 
    }

    void Renderer::drawCircle(const s32 centerX, const s32 centerY, const u16 radius, const bool filled, const Color& color) {
        s32 x = radius;
        s32 y = 0;
        s32 radiusError = 0;
        s32 xChange = 1 - (radius << 1);
        s32 yChange = 0;
        
        while (x >= y) {
            if (filled) {
                for (s32 i = centerX - x; i <= centerX + x; i++) {
                    this->setPixelBlendDst(i, centerY + y, color);
                    this->setPixelBlendDst(i, centerY - y, color);
                }
                
                for (s32 i = centerX - y; i <= centerX + y; i++) {
                    this->setPixelBlendDst(i, centerY + x, color);
                    this->setPixelBlendDst(i, centerY - x, color);
                }
            } else {
                this->setPixelBlendDst(centerX + x, centerY + y, color);
                this->setPixelBlendDst(centerX + y, centerY + x, color);
                this->setPixelBlendDst(centerX - y, centerY + x, color);
                this->setPixelBlendDst(centerX - x, centerY + y, color);
                this->setPixelBlendDst(centerX - x, centerY - y, color);
                this->setPixelBlendDst(centerX - y, centerY - x, color);
                this->setPixelBlendDst(centerX + y, centerY - x, color);
                this->setPixelBlendDst(centerX + x, centerY - y, color);
            }
            
            y++;
            radiusError += yChange;
            yChange += 2;
            
            if (((radiusError << 1) + xChange) > 0) {
                x--;
                radiusError += xChange;
                xChange += 2;
            }
        }
    }

    void Renderer::drawQuarterCircle(s32 centerX, s32 centerY, u16 radius, bool filled, const Color& color, int quadrant) {
        s32 x = radius;
        s32 y = 0;
        s32 radiusError = 0;
        s32 xChange = 1 - (radius << 1);
        s32 yChange = 0;
        
        while (x >= y) {
            if (filled) {
                switch (quadrant) {
                    case 1: // Top-right
                        for (s32 i = centerX; i <= centerX + x; i++) {
                            this->setPixelBlendDst(i, centerY - y, color);
                        }
                        for (s32 i = centerX; i <= centerX + y; i++) {
                            this->setPixelBlendDst(i, centerY - x, color);
                        }
                        break;
                    case 2: // Top-left
                        for (s32 i = centerX - x; i <= centerX; i++) {
                            this->setPixelBlendDst(i, centerY - y, color);
                        }
                        for (s32 i = centerX - y; i <= centerX; i++) {
                            this->setPixelBlendDst(i, centerY - x, color);
                        }
                        break;
                    case 3: // Bottom-left
                        for (s32 i = centerX - x; i <= centerX; i++) {
                            this->setPixelBlendDst(i, centerY + y, color);
                        }
                        for (s32 i = centerX - y; i <= centerX; i++) {
                            this->setPixelBlendDst(i, centerY + x, color);
                        }
                        break;
                    case 4: // Bottom-right
                        for (s32 i = centerX; i <= centerX + x; i++) {
                            this->setPixelBlendDst(i, centerY + y, color);
                        }
                        for (s32 i = centerX; i <= centerX + y; i++) {
                            this->setPixelBlendDst(i, centerY + x, color);
                        }
                        break;
                }
            } else {
                switch (quadrant) {
                    case 1: // Top-right
                        this->setPixelBlendDst(centerX + x, centerY - y, color);
                        this->setPixelBlendDst(centerX + y, centerY - x, color);
                        break;
                    case 2: // Top-left
                        this->setPixelBlendDst(centerX - x, centerY - y, color);
                        this->setPixelBlendDst(centerX - y, centerY - x, color);
                        break;
                    case 3: // Bottom-left
                        this->setPixelBlendDst(centerX - x, centerY + y, color);
                        this->setPixelBlendDst(centerX - y, centerY + x, color);
                        break;
                    case 4: // Bottom-right
                        this->setPixelBlendDst(centerX + x, centerY + y, color);
                        this->setPixelBlendDst(centerX + y, centerY + x, color);
                        break;
                }
            }
            
            y++;
            radiusError += yChange;
            yChange += 2;
            
            if (((radiusError << 1) + xChange) > 0) {
                x--;
                radiusError += xChange;
                xChange += 2;
            }
        }
    }

    void Renderer::drawBorderedRoundedRect(const s32 x, const s32 y, const s32 width, const s32 height, const s32 thickness, const s32 radius, const Color& highlightColor) {
        s32 startX = x + 4;
        s32 startY = y;
        s32 adjustedWidth = width - 12;
        s32 adjustedHeight = height + 1;
        
        // Draw borders
        this->drawRect(startX, startY - thickness, adjustedWidth, thickness, highlightColor); // Top border
        this->drawRect(startX, startY + adjustedHeight, adjustedWidth, thickness, highlightColor); // Bottom border
        this->drawRect(startX - thickness, startY, thickness, adjustedHeight, highlightColor); // Left border
        this->drawRect(startX + adjustedWidth, startY, thickness, adjustedHeight, highlightColor); // Right border
        
        // Draw corners
        this->drawQuarterCircle(startX, startY, radius, true, highlightColor, 2); // Upper-left
        this->drawQuarterCircle(startX, startY + height, radius, true, highlightColor, 3); // Lower-left
        this->drawQuarterCircle(x + width - 9, startY, radius, true, highlightColor, 1); // Upper-right
        this->drawQuarterCircle(x + width - 9, startY + height, radius, true, highlightColor, 4); // Lower-right
    }

    void Renderer::processRoundedRectChunk(Renderer* self, const s32 x, const s32 y, const s32 x_end, const s32 y_end, const s32 r2, const s32 radius, const Color& color, const s32 startRow, const s32 endRow) {
        s32 x_left = x + radius;
        s32 x_right = x_end - radius;
        s32 y_top = y + radius;
        s32 y_bottom = y_end - radius;
        
        // Process the rectangle in chunks
        for (s32 y1 = startRow; y1 < endRow; ++y1) {
            for (s32 x1 = x; x1 < x_end; ++x1) {
                //bool isInCorner = false;
    
                if (x1 < x_left) {
                    // Left side
                    if (y1 < y_top) {
                        // Top-left corner
                        s32 dx = x_left - x1;
                        s32 dy = y_top - y1;
                        if (dx * dx + dy * dy <= r2) {
                            self->setPixelBlendDst(x1, y1, color);
                        }
                    } else if (y1 >= y_bottom) {
                        // Bottom-left corner
                        s32 dx = x_left - x1;
                        s32 dy = y1 - y_bottom;
                        if (dx * dx + dy * dy <= r2) {
                            self->setPixelBlendDst(x1, y1, color);
                        }
                    } else {
                        // Left side of the rectangle
                        self->setPixelBlendDst(x1, y1, color);
                    }
                } else if (x1 >= x_right) {
                    // Right side
                    if (y1 < y_top) {
                        // Top-right corner
                        s32 dx = x1 - x_right;
                        s32 dy = y_top - y1;
                        if (dx * dx + dy * dy <= r2) {
                            self->setPixelBlendDst(x1, y1, color);
                        }
                    } else if (y1 >= y_bottom) {
                        // Bottom-right corner
                        s32 dx = x1 - x_right;
                        s32 dy = y1 - y_bottom;
                        if (dx * dx + dy * dy <= r2) {
                            self->setPixelBlendDst(x1, y1, color);
                        }
                    } else {
                        // Right side of the rectangle
                        self->setPixelBlendDst(x1, y1, color);
                    }
                } else {
                    self->setPixelBlendDst(x1, y1, color);
                }
            }
        }
    }

    void Renderer::drawRoundedRectMultiThreaded(const s32 x, const s32 y, const s32 w, const s32 h, const s32 radius, const Color& color) {
        s32 x_end = x + w;
        s32 y_end = y + h;
        s32 r2 = radius * radius;
    
        // Reuse ult::threads and implement work-stealing
        ult::currentRow = y;
    
        auto threadTask = [this, x, y, x_end, y_end, r2, radius, color]() {
            s32 startRow;
            while (true) {
                startRow = ult::currentRow.fetch_add(4);
                if (startRow >= y_end) break;
                //s32 endRow = std::min(startRow + 4,y_end); // Process one row at a time
                processRoundedRectChunk(this, x, y, x_end, y_end, r2, radius, color, startRow, std::min(startRow + 4,y_end));
            }
        };
    
        // Launch ult::threads
        for (unsigned i = 0; i < ult::numThreads; ++i) {
            ult::threads[i] = std::thread(threadTask);
        }
    
        // Join ult::threads
        for (auto& t : ult::threads) {
            if (t.joinable()) t.join();
        }
    }

    void Renderer::drawRoundedRectSingleThreaded(const s32 x, const s32 y, const s32 w, const s32 h, const s32 radius, const Color& color) {
        s32 x_end = x + w;
        s32 y_end = y + h;
        s32 r2 = radius * radius;
        
        // Call the processRoundedRectChunk function directly for the entire rectangle
        processRoundedRectChunk(this, x, y, x_end, y_end, r2, radius, color, y, y_end);
    }

    void Renderer::updateDrawFunction() {
        if (ult::expandedMemory) {
            drawRoundedRect = [this](s32 x, s32 y, s32 w, s32 h, s32 radius, Color color) {
                drawRoundedRectMultiThreaded(x, y, w, h, radius, color);
            };
        } else {
            drawRoundedRect = [this](s32 x, s32 y, s32 w, s32 h, s32 radius, Color color) {
                drawRoundedRectSingleThreaded(x, y, w, h, radius, color);
            };
        }
    }

    void Renderer::drawUniformRoundedRect(const s32 x, const s32 y, const s32 w, const s32 h, const Color& color) {
        // Radius is half of height to create perfect half circles on each side
        s32 radius = h / 2;
        s32 x_start = x + radius;
        s32 x_end = x + w - radius;
    
        // Draw the central rectangle excluding the corners
        for (s32 y1 = y; y1 < y + h; ++y1) {
            for (s32 x1 = x_start; x1 < x_end; ++x1) {
                this->setPixelBlendDst(x1, y1, color);
            }
        }
    
        // Draw the rounded corners using trigonometric functions for smoothness
        for (s32 x1 = 0; x1 < radius; ++x1) {
            for (s32 y1 = 0; y1 < h; ++y1) {
                s32 dy = y1 - radius; // Offset from center of the circle
                if (x1 * x1 + dy * dy <= radius * radius) {
                    // Left half-circle
                    this->setPixelBlendDst(x + radius - x1, y + y1, color);
                    // Right half-circle
                    this->setPixelBlendDst(x + w - radius + x1, y + y1, color);
                }
            }
        }
    }

    void Renderer::processBMPChunk(const s32 x, const s32 y, const s32 screenW, const u8 *preprocessedData, const s32 startRow, const s32 endRow) {
        s32 bytesPerRow = screenW * 2; // 2 bytes per pixel row due to RGBA4444
        const s32 endX = screenW - 16;
        
        alignas(16) u8 redArray[16], greenArray[16], blueArray[16], alphaArray[16];
        for (s32 y1 = startRow; y1 < endRow; ++y1) {
            const u8 *rowPtr = preprocessedData + (y1 * bytesPerRow);
            s32 x1 = 0;
    
            // Process in chunks of 16 pixels using SIMD
            for (; x1 <= endX; x1 += 16) {
                // Load 32 bytes for 16 pixels (2 bytes per pixel)
                uint8x16x2_t packed = vld2q_u8(rowPtr + (x1 * 2));
    
                // Unpack high and low 4-bit nibbles
                uint8x16_t red = vshrq_n_u8(packed.val[0], 4);
                uint8x16_t green = vandq_u8(packed.val[0], vdupq_n_u8(0x0F));
                uint8x16_t blue = vshrq_n_u8(packed.val[1], 4);
                uint8x16_t alpha = vandq_u8(packed.val[1], vdupq_n_u8(0x0F));
    
                // Scale 4-bit values to 8-bit by multiplying by 17
                uint8x16_t scale = vdupq_n_u8(17);
                red = vmulq_u8(red, scale);
                green = vmulq_u8(green, scale);
                blue = vmulq_u8(blue, scale);
                alpha = vmulq_u8(alpha, scale);
    
                vst1q_u8(redArray, red);
                vst1q_u8(greenArray, green);
                vst1q_u8(blueArray, blue);
                vst1q_u8(alphaArray, alpha);
    
                for (s32 i = 0; i < 16; ++i) {
                    setPixelBlendSrc(x + x1 + i, y + y1, {
                        redArray[i],
                        greenArray[i],
                        blueArray[i],
                        alphaArray[i]
                    });
                }
            }
    
            // Handle the remaining pixels (less than 16)
            for (s32 xRem = x1; xRem < screenW; ++xRem) {
                u8 packedValue1 = rowPtr[xRem * 2];
                u8 packedValue2 = rowPtr[xRem * 2 + 1];
    
                // Unpack and scale
                u8 red = ((packedValue1 >> 4) & 0x0F) * 17;
                u8 green = (packedValue1 & 0x0F) * 17;
                u8 blue = ((packedValue2 >> 4) & 0x0F) * 17;
                u8 alpha = (packedValue2 & 0x0F) * 17;
    
                setPixelBlendSrc(x + xRem, y + y1, {red, green, blue, alpha});
            }
        }
    
        ult::inPlotBarrier.arrive_and_wait(); // Wait for all ult::threads to reach this point
    }

    void Renderer::drawBitmapRGBA4444(const s32 x, const s32 y, const s32 screenW, const s32 screenH, const u8 *preprocessedData) {

        // Divide rows among ult::threads
        //s32 chunkSize = (screenH + ult::numThreads - 1) / ult::numThreads;
        for (unsigned i = 0; i < ult::numThreads; ++i) {
            s32 startRow = i * ult::bmpChunkSize;
            s32 endRow = std::min(startRow + ult::bmpChunkSize, screenH);
            
            // Bind the member function and create the thread
            ult::threads[i] = std::thread(std::bind(&tsl::gfx::Renderer::processBMPChunk, this, x, y, screenW, preprocessedData, startRow, endRow));
        }
    	
        // Join all ult::threads
        for (auto& t : ult::threads) {
            t.join();
        }
    }

    void Renderer::drawWallpaper() {
        if (ult::expandedMemory && !ult::refreshWallpaper.load(std::memory_order_acquire)) {
            //ult::inPlot = true;
            ult::inPlot.store(true, std::memory_order_release);
            //std::lock_guard<std::mutex> lock(wallpaperMutex);
            if (!ult::wallpaperData.empty()) {
                // Draw the bitmap at position (0, 0) on the screen
                //static bool ult::correctFrameSize = (cfg::FramebufferWidth == 448 && cfg::FramebufferHeight == 720);
                if (!ult::refreshWallpaper.load(std::memory_order_acquire) && ult::correctFrameSize) { // hard coded width and height for consistency
                    drawBitmapRGBA4444(0, 0, cfg::FramebufferWidth, cfg::FramebufferHeight, ult::wallpaperData.data());
                } else
                    ult::inPlot.store(false, std::memory_order_release);
            } else {
                ult::inPlot.store(false, std::memory_order_release);
            }
            //ult::inPlot = false;
        }
    }

    void Renderer::drawBitmap(s32 x, s32 y, s32 w, s32 h, const u8 *bmp) {
        for (s32 y1 = 0; y1 < h; y1++) {
            for (s32 x1 = 0; x1 < w; x1++) {
                const Color color = { static_cast<u8>(bmp[0] >> 4), static_cast<u8>(bmp[1] >> 4), static_cast<u8>(bmp[2] >> 4), static_cast<u8>(bmp[3] >> 4) };
                setPixelBlendSrc(x + x1, y + y1, a(color));
                bmp += 4;
            }
        }
    }

    void Renderer::fillScreen(const Color& color) {
        std::fill_n(static_cast<Color*>(this->getCurrentFramebuffer()), this->getFramebufferSize() / sizeof(Color), color);
    }

    void Renderer::clearScreen() {
        this->fillScreen({ 0x00, 0x00, 0x00, 0x00 });
    }

    const stbtt_fontinfo& Renderer::getStandardFont() const {
        return m_stdFont;
    }

    std::pair<u32, u32> Renderer::drawString(const std::string& originalString, bool monospace, const s32 x, const s32 y, const s32 fontSize, const Color& color, const ssize_t maxWidth) {
                
        #ifdef UI_OVERRIDE_PATH
        // Check for translation in the cache
        auto translatedIt = ult::translationCache.find(originalString);
        std::string translatedString = (translatedIt != ult::translationCache.end()) ? translatedIt->second : originalString;

        // Cache the translation if it wasn't already present
        if (translatedIt == ult::translationCache.end()) {
            ult::translationCache[originalString] = translatedString; // You would normally use some translation function here
        }
        const std::string* stringPtr = &translatedString;

        #else
        //std::string translatedString = originalString;
        //ult::applyLangReplacements(translatedString);
        //ult::applyLangReplacements(translatedString, true);
        //ult::convertComboToUnicode(translatedString);

        const std::string* stringPtr = &originalString;
        #endif


        float maxX = x;
        float currX = x;
        float currY = y;
        
        
        // Avoid copying the original string
        //const std::string* stringPtr = &originalString;
        
        // Cache the end iterator for efficiency
        auto itStrEnd = stringPtr->cend();
        auto itStr = stringPtr->cbegin();
        
        // Move variable declarations outside of the loop
        u32 currCharacter = 0;
        ssize_t codepointWidth = 0;
        u64 key = 0;
        Glyph* glyph = nullptr;
        
        float xPos = 0;
        float yPos = 0;
        u32 rowOffset = 0;
        uint8_t bmpColor = 0;
        Color tmpColor(0);
        
        // Static glyph cache
        static std::unordered_map<u64, Glyph> s_glyphCache; // may cause leak? will investigate later.
        auto it = s_glyphCache.end();

        float scaledFontSize;

        // Loop through each character in the string
        while (itStr != itStrEnd) {
            if (maxWidth > 0 && (currX - x) >= maxWidth)
                break;
     	
            // Decode UTF-8 codepoint
            codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
            if (codepointWidth <= 0)
                break;
     	
            // Move the iterator forward by the width of the current codepoint
            itStr += codepointWidth;
     	
            if (currCharacter == '\n') {
                maxX = std::max(currX, maxX);
                currX = x;
                currY += fontSize;
                continue;
            }
     	
            // Calculate glyph key
            key = (static_cast<u64>(currCharacter) << 32) | (static_cast<u64>(monospace) << 31) | (static_cast<u64>(std::bit_cast<u32>(fontSize)));
     	
            // Check cache for the glyph
            it = s_glyphCache.find(key);
     	
            // If glyph not found, create and cache it
            if (it == s_glyphCache.end()) {
                glyph = &s_glyphCache.emplace(key, Glyph()).first->second;
     		
                // Determine the appropriate font for the character
                if (stbtt_FindGlyphIndex(&this->m_extFont, currCharacter)) {
                    glyph->currFont = &this->m_extFont;
                } else if (this->m_hasLocalFont && stbtt_FindGlyphIndex(&this->m_stdFont, currCharacter) == 0) {
                    glyph->currFont = &this->m_localFont;
                } else {
                    glyph->currFont = &this->m_stdFont;
                }
     		
                scaledFontSize = stbtt_ScaleForPixelHeight(glyph->currFont, fontSize);
                glyph->currFontSize = scaledFontSize;
     		
                // Get glyph bitmap and metrics
                stbtt_GetCodepointBitmapBoxSubpixel(glyph->currFont, currCharacter, scaledFontSize, scaledFontSize,
                                                    0, 0, &glyph->bounds[0], &glyph->bounds[1], &glyph->bounds[2], &glyph->bounds[3]);
     		
                s32 yAdvance = 0;
                stbtt_GetCodepointHMetrics(glyph->currFont, monospace ? 'W' : currCharacter, &glyph->xAdvance, &yAdvance);
     		
                glyph->glyphBmp = stbtt_GetCodepointBitmap(glyph->currFont, scaledFontSize, scaledFontSize, currCharacter, &glyph->width, &glyph->height, nullptr, nullptr);
            } else {
                glyph = &it->second;
            }
     	
            if (glyph->glyphBmp != nullptr && !std::iswspace(currCharacter) && fontSize > 0 && color.a != 0x0) {
                xPos = currX + glyph->bounds[0];
                yPos = currY + glyph->bounds[1];
     		
                // Optimized pixel processing
                for (s32 bmpY = 0; bmpY < glyph->height; ++bmpY) {
                    rowOffset = bmpY * glyph->width;
                    for (s32 bmpX = 0; bmpX < glyph->width; ++bmpX) {
                        bmpColor = glyph->glyphBmp[rowOffset + bmpX] >> 4;
                        if (bmpColor == 0xF) {
                            // Direct pixel manipulation
                            this->setPixel(xPos + bmpX, yPos + bmpY, color, this->getPixelOffset(xPos + bmpX, yPos + bmpY));
                        } else if (bmpColor != 0x0) {
                            tmpColor = color;
                            tmpColor.a = bmpColor;
                            this->setPixelBlendDst(xPos + bmpX, yPos + bmpY, tmpColor);
                        }
                    }
                }
            }
     	
            // Advance the cursor for the next glyph
            currX += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
        }
     
        maxX = std::max(currX, maxX);
        return { static_cast<u32>(maxX - x), static_cast<u32>(currY - y) };
    }

    void Renderer::drawStringWithColoredSections(const std::string& text, const std::vector<std::string>& specialSymbols, s32 x, const s32 y, const u32 fontSize, const Color& defaultColor, const Color& specialColor) {
        size_t startPos = 0;
        size_t textLength = text.length();
        u32 segmentWidth, segmentHeight;
        
        // Create a set for fast symbol lookup
        std::unordered_set<std::string> specialSymbolSet(specialSymbols.begin(), specialSymbols.end());
        
        // Variables initialized outside the loop
        size_t specialPos = std::string::npos;
        size_t foundLength = 0;
        std::string_view foundSymbol;
        std::string normalTextStr; // To hold the text before the special symbol
        std::string specialSymbolStr; // To hold the special symbol text
        size_t pos; // To store position of the special symbol in the text
        
        while (startPos < textLength) {
            specialPos = std::string::npos;
            foundLength = 0;
            foundSymbol = std::string_view(); // Reset the foundSymbol
            
            // Find the nearest special symbol
            for (const auto& symbol : specialSymbolSet) {
                pos = text.find(symbol, startPos);
                if (pos != std::string::npos && (specialPos == std::string::npos || pos < specialPos)) {
                    specialPos = pos;
                    foundLength = symbol.length();
                    foundSymbol = symbol;
                }
            }
            
            // If no special symbol is found, draw the rest of the text
            if (specialPos == std::string::npos) {
                drawString(text.substr(startPos), false, x, y, fontSize, defaultColor);
                break;
            }
            
            // Draw the segment before the special symbol
            if (specialPos > startPos) {
                normalTextStr = text.substr(startPos, specialPos - startPos);
                std::tie(segmentWidth, segmentHeight) = drawString(normalTextStr, false, x, y, fontSize, defaultColor);
                //segmentWidth = calculateStringWidth(normalTextStr, fontSize);
                x += segmentWidth;
            }
    
            // Draw the special symbol
            specialSymbolStr = foundSymbol; // Convert std::string_view to std::string
            std::tie(segmentWidth, segmentHeight) = drawString(specialSymbolStr, false, x, y, fontSize, specialColor);
            //segmentWidth = calculateStringWidth(specialSymbolStr, fontSize);
            x += segmentWidth;
    
            // Move startPos past the special symbol
            startPos = specialPos + foundLength;
        }
    
        // Draw any remaining text after the last special symbol
        if (startPos < textLength) {
            drawString(text.substr(startPos), false, x, y, fontSize, defaultColor);
        }
    }

    std::string Renderer::limitStringLength(const std::string& originalString, const bool monospace, const s32 fontSize, const s32 maxLength) {
        #ifdef UI_OVERRIDE_PATH
        // Check for translation in the cache
        auto translatedIt = ult::translationCache.find(originalString);
        std::string translatedString = (translatedIt != ult::translationCache.end()) ? translatedIt->second : originalString;
        
        // Cache the translation if it wasn't already present
        if (translatedIt == ult::translationCache.end()) {
            ult::translationCache[originalString] = translatedString; // You would normally use some translation function here
        }
        const std::string* stringPtr = &translatedString;
        #else
        const std::string* stringPtr = &originalString;
        #endif
        
        if (stringPtr->size() < 2) {
            return *stringPtr;
        }
        
        s32 currX = 0;
        ssize_t strPos = 0;
        ssize_t codepointWidth;
        u32 ellipsisCharacter = 0x2026;  // Unicode code point for '…'
        s32 ellipsisWidth;
    
        // Calculate the width of the ellipsis
        stbtt_fontinfo* ellipsisFont = &this->m_stdFont;
        if (stbtt_FindGlyphIndex(&this->m_extFont, ellipsisCharacter)) {
            ellipsisFont = &this->m_extFont;
        } else if (this->m_hasLocalFont && stbtt_FindGlyphIndex(&this->m_stdFont, ellipsisCharacter) == 0) {
            ellipsisFont = &this->m_localFont;
        }
        float ellipsisFontSize = stbtt_ScaleForPixelHeight(ellipsisFont, fontSize);
        int ellipsisXAdvance = 0, ellipsisYAdvance = 0;
        stbtt_GetCodepointHMetrics(ellipsisFont, ellipsisCharacter, &ellipsisXAdvance, &ellipsisYAdvance);
        ellipsisWidth = static_cast<s32>(ellipsisXAdvance * ellipsisFontSize);
    
        u32 currCharacter;
        std::string substr;
    
        while (static_cast<std::string::size_type>(strPos) < stringPtr->size() && currX + ellipsisWidth < maxLength) {
            codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*stringPtr)[strPos]));
    
            if (codepointWidth <= 0) {
                break;
            }
    
            // Calculate the width of the current substring plus the ellipsis
            substr = stringPtr->substr(0, strPos + codepointWidth);
            currX = calculateStringWidth(substr, fontSize, monospace);
    
            if (currX + ellipsisWidth >= maxLength) {
                return substr + "…";
            }
    
            strPos += codepointWidth;
        }
    
        return *stringPtr;
    }

    void Renderer::setLayerPos(u32 x, u32 y) {
        float ratio = 1.5;
        u32 maxX = cfg::ScreenWidth - (int)(ratio * cfg::FramebufferWidth);
        u32 maxY = cfg::ScreenHeight - (int)(ratio * cfg::FramebufferHeight);
        if (x > maxX || y > maxY) {
            return;
        }
        setLayerPosImpl(x, y);
    }

    void Renderer::setLayerPosImpl(u32 x, u32 y) {
        // Get the underscan pixel values for both horizontal and vertical borders
        auto [horizontalUnderscanPixels, verticalUnderscanPixels] = getUnderscanPixels();
        //int horizontalUnderscanPixels = 0;
        //ult::useRightAlignment = (ult::parseValueFromIniSection(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, "right_alignment") == ult::TRUE_STR);
        //cfg::LayerPosX = 1280-32;
        cfg::LayerPosX = 0;
        cfg::LayerPosY = 0;
        cfg::FramebufferWidth  = ult::DefaultFramebufferWidth;
        cfg::FramebufferHeight = ult::DefaultFramebufferHeight;
        //ult::correctFrameSize = (cfg::FramebufferWidth == 448 && cfg::FramebufferHeight == 720); // for detecting the correct Overlay display size
        if (ult::useRightAlignment && ult::correctFrameSize) {
            cfg::LayerPosX = 1280-32 - horizontalUnderscanPixels;
            ult::layerEdge = (1280-448);
        }
        cfg::LayerPosX += x;
        cfg::LayerPosY += y;
        ASSERT_FATAL(viSetLayerPosition(&this->m_layer, cfg::LayerPosX, cfg::LayerPosY));
    }

    void Renderer::startFrame() {
        this->m_currentFramebuffer = framebufferBegin(&this->m_framebuffer, nullptr);
    }

    void Renderer::endFrame() {
        this->waitForVSync();
        framebufferEnd(&this->m_framebuffer);
        this->m_currentFramebuffer = nullptr;
    }

    void Renderer::waitForVSync() {
        eventWait(&this->m_vsyncEvent, UINT64_MAX);
    }

    u32 Renderer::getPixelOffset(const s32 x, const s32 y) {
        if (!this->m_scissoringStack.empty()) {
            const auto& currScissorConfig = this->m_scissoringStack.top();
            if (x < currScissorConfig.x || y < currScissorConfig.y || 
                x >= currScissorConfig.x + currScissorConfig.w || 
                y >= currScissorConfig.y + currScissorConfig.h) {
                return UINT32_MAX;
            }
        }
        return (((y & 127) / 16) + ((x / 32) * 8) + ((y / 128) * 112))*512 +
                ((y % 16) / 8) * 256 + 
                ((x % 32) / 16) * 128 + 
                ((y % 8) / 2) * 32 + 
                ((x % 16) / 8) * 16 + 
                (y % 2) * 8 + 
                (x % 8);
    }

    std::pair<int, int> Renderer::getUnderscanPixels() {
        // Implementation logic for retrieving underscan pixels, already provided in the header
    }

} // namespace gfx
