/********************************************************************************
 * File: glass.hpp
 * Author: ppkantorski
 * Description: 
 *   'glass.hpp' declares the frosted glass backdrop for the Ultrahand Overlay
 *   project: a blurred, tinted view of whatever is behind the overlay frame,
 *   live in games (read from the game's own frames) and from the system capture
 *   service elsewhere (HOME, applets). This header exposes the small interface
 *   the renderer (tesla.hpp) uses: the published blur strips, session and
 *   frame state, the screenshot-stack handshake and the worker controls.
 *   The implementation lives in 'glass.cpp'.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be 
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2026 ppkantorski
 ********************************************************************************/

#pragma once

#include <switch.h>
#include <atomic>

namespace ult::glass {
    // A small request to a service through one copy of libnx's request code (inlined at every
    // serviceDispatch* otherwise, a few hundred bytes each). At most one out buffer (mapped
    // alias) and one copied out handle.
    Result request(Service* srv, u32 cmd, const void* in, u32 inSize, void* out = nullptr, u32 outSize = 0,
                   void* buf = nullptr, size_t bufSize = 0, Handle* outHandle = nullptr);

    // Geometry: the overlay framebuffer is 448x720; at 1/8 scale that's 56x90.
    inline constexpr u32 kScale   = 8;
    inline constexpr u32 kStripW  = 448 / kScale;      // 56
    inline constexpr u32 kStripH  = 720 / kScale;      // 90
    inline constexpr u32 kStripCells = kStripW * kStripH;

// Panel glass (Renderer::drawGlassPanel): Status Monitor's, or an overlay that asks for it with
// USING_GLASS_PANEL_DIRECTIVE (and GLASS_PANEL_MAX_W / _H, its largest panel in cells, when
// that is more than the menus' column). Nothing of it is built otherwise.
#if IS_STATUS_MONITOR_DIRECTIVE || USING_GLASS_PANEL_DIRECTIVE
    #define ULT_GLASS_REGIONS 1
#else
    #define ULT_GLASS_REGIONS 0
#endif
// One buffer for the capture service's screenshot and the live game reads (480 KB less; the
// live reads' tile-row cache is dropped after each screenshot): Status Monitor's, or an overlay
// that asks for it with USING_GLASS_ONE_BUFFER_DIRECTIVE.
#if IS_STATUS_MONITOR_DIRECTIVE || USING_GLASS_ONE_BUFFER_DIRECTIVE
    #define ULT_GLASS_ONE_BUFFER 1
#else
    #define ULT_GLASS_ONE_BUFFER 0
#endif
#if ULT_GLASS_REGIONS
    // A panel's glass is the area behind it: any place and size up to kRegionCells cells (a
    // region, see regionKey). Status Monitor's Full mode draws the menus' column, so its panels
    // (Mini, Micro, FPS Counter / Graph, Resolutions) get the menus' strip size too.
  #if defined(GLASS_PANEL_MAX_W) && defined(GLASS_PANEL_MAX_H)
    inline constexpr u32 kPanelMaxW = GLASS_PANEL_MAX_W, kPanelMaxH = GLASS_PANEL_MAX_H;
  #else
    inline constexpr u32 kPanelMaxW = 0, kPanelMaxH = 0;
  #endif
    inline constexpr u32 kRegionCells = kPanelMaxW * kPanelMaxH > kStripCells ? kPanelMaxW * kPanelMaxH : kStripCells;
    // A strip holds the largest of them, rounded up to 8 cells (the colour mix works in 8s)
    inline constexpr u32 kStripCap = (kRegionCells + 7) & ~7u;
#else
    inline constexpr u32 kStripCap = kStripCells;
#endif
    // One blurred strip (the area behind the frame at 1/8 scale), RGB888, row-major.
    struct Strip { u8 rgb[kStripCap * 3]; };

#if ULT_GLASS_REGIONS
    // ── Regions (Status Monitor) ─────────────────────────────────────────────────
    // The area of the screen a strip covers: x, y in 1280x720 screen pixels, w, h in cells
    // (8x8 screen px). A menu's strip is the 448x720 column at the layer (s_stripX = its x);
    // a monitoring mode's panel is a region, keyed with bit 31 set: cell x (0..160), cell y
    // (0..90), width (1..160) and height (1..90) in cells, width x height <= kRegionCells.
    struct Geo { u32 x, y, w, h; };
    // A region is blurred with kRegionMargin cells of the screen around it on all four sides;
    // it and its margins must fit kRegionWorkCells: the menus' column work size (76x90; a
    // full-width bar, as Micro's: up to 18 cells = 144 px tall), or the largest panel's.
    inline constexpr u32 kRegionMargin    = 10;
    inline constexpr u32 kColumnWorkCells = (kStripW + 2 * kRegionMargin) * kStripH;
    inline constexpr u32 kPanelWorkCells  = kPanelMaxW ? (kPanelMaxW + 2 * kRegionMargin) * (kPanelMaxH + 2 * kRegionMargin) : 0;
    inline constexpr u32 kRegionWorkCells = kPanelWorkCells > kColumnWorkCells ? kPanelWorkCells : kColumnWorkCells;
    inline constexpr bool regionFits(u32 cw, u32 ch) {
        return cw >= 1 && ch >= 1 && cw <= 160 && ch <= 90 && cw * ch <= kRegionCells &&
               (cw + 2 * kRegionMargin) * (ch + 2 * kRegionMargin) <= kRegionWorkCells;
    }
    inline constexpr u32 regionKey(u32 cx, u32 cy, u32 cw, u32 ch) {
        return (1u << 31) | cx | (cy << 8) | (cw << 15) | (ch << 23);
    }
    inline constexpr bool isRegion(u32 key) { return (key >> 31) != 0; }
    inline constexpr Geo geoOf(u32 key) {
        return isRegion(key) ? Geo{ (key & 255u) * kScale, ((key >> 8) & 127u) * kScale, (key >> 15) & 255u, (key >> 23) & 127u }
                             : Geo{ key, 0, kStripW, kStripH };
    }
    // Set by the panel draw (Renderer::drawGlassPanel) on every frame it draws glass: the
    // overlay loop asks the worker for this region instead of the layer's column while it is
    // recent (s_regionNs, 0.5 s).
    extern std::atomic<u32> s_regionKey;
    extern std::atomic<u64> s_regionNs;
#endif
    // The overlay set its own glass settings (a monitoring mode, a player): the worker doesn't
    // re-read Ultrahand's from config.ini (pollLiveSettings) meanwhile.
    extern std::atomic<bool> s_keepSettings;

    // 64x64 blue-noise ranks (0..4095, each once): frozen sub-step dither.
    extern const u16 kBlueNoise64[64 * 64];

    // ── State shared with the renderer ───────────────────────────────────────────
    extern std::atomic<bool> s_running;           // worker thread alive
    extern std::atomic<bool> s_failed;            // capture service unavailable
    extern std::atomic<bool> s_wantCapture;       // set by the render loop: visible + enabled
    extern std::atomic<bool> s_needExclude;       // worker needs our layer out of the screenshot stacks
    extern std::atomic<int>  s_exclState;         // 0 in stacks, 1 removal issued, 2 confirmed
    extern std::atomic<u32>  s_stripX;            // screen x of the overlay layer
    extern std::atomic<u32>  s_session;           // bumps on every show
    extern std::atomic<u64>  s_periodNs;          // smoothed time between new frames
    extern std::atomic<bool> s_curLive;           // newest frame was read live (game, or the HOME menu via vi)
    extern std::atomic<bool> s_drewLastSession;   // previous showing drew glass (open-hold)
    extern std::atomic<bool> s_gameInFront;       // a game is in front this showing (else HOME / applet)
    // No game running, or it is out of focus (HOME / an applet in front): pm:dmnt + the
    // play-event log, as the worker checks. A few IPC calls; false while the worker runs.
    bool gameOutOfFocus();
    // What is in front: the AppletId of the newest "in focus" applet event of the play-event log
    // (AppletId_application, AppletId_SystemAppletMenu for HOME, or a library applet), -1 unknown.
    int frontApplet();
    extern std::atomic<bool> s_holding;           // overlay held invisible (fully transparent) for the first frame

    // ── Frame usage ──────────────────────────────────────────────────────────────
    void markUsed(u64 nowNs);                      // a frame draws (or waits for) glass now
    void markDrawn();                              // a frame drew glass this showing
    bool inUse(u64 nowNs);                         // a frame drew glass in the last 0.5 s

    // ── Worker control / frames ──────────────────────────────────────────────────
    void ensureRunning();
    bool reserve();                                // allocate all glass memory now (kept until release())
    bool reserved();                               // ... already allocated
    // Stops the worker and frees all glass memory (glass off): a wallpaper about to load takes
    // the shared block. reserve() / ensureRunning() allocate it again when glass is back.
    void release();
    extern void (*s_onRelease)();                  // renderer hook: frees its glass drawing buffers

    // ── Shared block ─────────────────────────────────────────────────────────────
    // The glass memory and the wallpaper (picture + animation ring) never coexist, so they take
    // turns in one block, allocated on first use and kept: switching between them allocates and
    // frees nothing, and can't fragment the heap. Overlay loop thread only.
    enum class PoolUser : u8 { None, Glass, Wallpaper };
    // From the block when it can serve this user now, else malloc (4 MB, Status Monitor, the
    // other user still holding it, too large). nullptr: no memory.
    void* sharedAlloc(PoolUser user, size_t bytes);
    void  sharedFree(void* p);                     // anything from sharedAlloc (nullptr is fine)
    extern size_t s_drawBytes;                     // renderer's glass drawing memory (sizes the block)
    // false: no block, sharedAlloc is plain malloc. For an overlay that never switches between
    // glass and wallpaper and needs its heap back when either is freed (set before either is used).
    extern bool useSharedBlock;
    void shutdown();
    // Before a reboot or power off (any thread): stops the worker for good, so no debugger or
    // capture of ours is open while the system shuts down.
    void stopForPowerOff();
    void newSession();                             // call on every show
    void dropFrames();                             // glass switched on mid-showing: no old frames
    void refreshFrames();   // keep the frames, take a fresh one now (glass back within a showing)
    // A Frosted Glass setting slider moved (Theme Settings): ult::glassBlurRadius / glassDarkness
    // were set directly. Wakes the worker for a fresh blur, and for 1 s its config.ini re-read
    // leaves those two keys alone (the file holds the last saved value, not the one sliding).
    void noteSettingEdit();
    void wakeForShow(u64 nowNs);                   // start capturing at once (glass will be drawn)
    void setWantCapture(bool on);                  // render loop, every frame (wakes the worker on false -> true)
    void screenChanged();                          // HOME button pressed: check what's on screen soon
    void screenChanged(bool staysShown);           // the same, and whether the overlay stays on screen
    bool hasFrame();                               // this showing has a glass frame
    bool unavailable();                            // no glass for now (normal background): nothing to wait for
    // The game in front (or vi at HOME) is held by another debugger, so the glass can't show it
    // (Switch Frame Tap streaming; the cheat engine with "With Cheats" off). The renderer then
    // releases the glass and shows the wallpaper (ult::glassFallback) until heldCheckFree().
    bool heldByOther();
    // Glass being switched on with the wallpaper up (render thread, worker stopped): true if the
    // game is held from it, marked held so the wallpaper simply stays (nothing reloaded).
    bool heldNow();
    // Glass released for the wallpaper (worker stopped): is the process free again (the game, or
    // vi with HOME / an applet in front)? Checked on a small thread that sleeps until asked (render
    // thread: start / stop it, request a check on events, poll heldCheckFree()).
    void startHeldCheck();
    void stopHeldCheck();                          // also done by shutdown()
    void requestHeldCheck();
    bool heldCheckFree();                          // true once free (the thread then ends)
    u64 lastHomePressNs();                         // last HOME press (screenChanged), 0 if none
    bool hasStandIn();                             // a previous showing's frame can stand in until then
    // Copies the latest strips if they changed since `seenGen`; false if none.
    bool fetch(u32& seenGen, Strip& cur, Strip& prev, bool& hasPrev, u64& curTick, u32& curX);

#if USING_LIVE_LOGGING_DIRECTIVE
    namespace live {
        void log(const char* fmt, ...);            // live_logging.log (only with live_logging on)
        void flushLog();                           // write buffered lines now (overlay exit)
    }
#endif

} // namespace ult::glass
