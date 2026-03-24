#pragma once
/// Design tokens extracted from Figma "Jason M4.0" design file.
/// Figma canvas: 432×261  →  Target display: 800×480  (scale ≈ 1.852)

#include <egt/ui>

namespace dt {

// ── Screen ──────────────────────────────────────────────────────────────────
inline constexpr int SCREEN_W = 800;
inline constexpr int SCREEN_H = 480;

// ── Scale helper (Figma 432×261 → 800×480) ─────────────────────────────────
inline constexpr double SCALE = 800.0 / 432.0;  // ≈ 1.852

// ── Colors (from Figma fills) ───────────────────────────────────────────────
inline const egt::Color kBgWhite       {255, 255, 255};
inline const egt::Color kTextPrimary   {100, 101, 105};  // rgb(100,101,105)
inline const egt::Color kAccentCyan    { 48, 163, 196};  // rgb(48,163,196)  – Demo mode, filled buttons
inline const egt::Color kGreen         { 91, 197,   0};  // rgb(91,197,0)    – Progress, success
inline const egt::Color kGreenLight    {200, 235, 170};  // light green – scrollbar, hover highlights
inline const egt::Color kGrayLight     {217, 217, 217};  // rgb(217,217,217) – Inactive segments, borders
inline const egt::Color kGrayBg        {245, 245, 245};  // background for cards
inline const egt::Color kWhite         {255, 255, 255};
inline const egt::Color kBlack         {  0,   0,   0};
inline const egt::Color kRed           {220,  53,  69};  // errors, critical
inline const egt::Color kOrange        {255, 165,   0};  // warnings
inline const egt::Color kTransparent   {  0,   0,   0, 0};

// Error severity colors
inline const egt::Color kErrorInfoBanner   = kAccentCyan;
inline const egt::Color kErrorWarnBanner   = kOrange;
inline const egt::Color kErrorCritBanner   = kRed;

// ── Fonts ───────────────────────────────────────────────────────────────────
// Font size constants
inline constexpr int FONT_HUGE     = 72;
inline constexpr int FONT_LARGE    = 42;
inline constexpr int FONT_TITLE    = 28;
inline constexpr int FONT_SUBTITLE = 20;
inline constexpr int FONT_BODY     = 16;
inline constexpr int FONT_BUTTON   = 18;
inline constexpr int FONT_SMALL    = 14;
inline constexpr int FONT_TINY     = 12;

// Headers / large countdown
inline egt::Font fontHuge()    { return egt::Font(FONT_HUGE, egt::Font::Weight::bold); }
inline egt::Font fontLarge()   { return egt::Font(FONT_LARGE, egt::Font::Weight::bold); }
inline egt::Font fontTitle()   { return egt::Font(FONT_TITLE, egt::Font::Weight::bold); }
inline egt::Font fontSubtitle(){ return egt::Font(FONT_SUBTITLE, egt::Font::Weight::normal); }
inline egt::Font fontBody()    { return egt::Font(FONT_BODY, egt::Font::Weight::normal); }
inline egt::Font fontButton()  { return egt::Font(FONT_BUTTON, egt::Font::Weight::bold); }
inline egt::Font fontSmall()   { return egt::Font(FONT_SMALL, egt::Font::Weight::normal); }
inline egt::Font fontTiny()    { return egt::Font(FONT_TINY, egt::Font::Weight::normal); }

// ── Dimensions (scaled from Figma) ─────────────────────────────────────────
// Logo in Figma: 90×56  →  166×103
inline constexpr int LOGO_W = 166;
inline constexpr int LOGO_H = 103;

// Header bar
inline constexpr int HEADER_H = 60;

// Main buttons in Figma: 131×54 outlined, 124×54 filled  →  ~242×100, ~230×100
inline constexpr int BTN_W       = 242;
inline constexpr int BTN_H       = 100;
inline constexpr int BTN_FILL_W  = 230;
inline constexpr int BTN_FILL_H  = 100;
inline constexpr int BTN_SMALL_W = 120;
inline constexpr int BTN_SMALL_H = 50;

// Segmented progress bar
inline constexpr int SEGMENT_COUNT  = 6;
inline constexpr int SEGMENT_W      = 60;   // each segment ~32px in Figma * scale
inline constexpr int SEGMENT_H      = 9;
inline constexpr int SEGMENT_GAP    = 8;
inline constexpr int SEGMENT_DOT_SZ = 9;

// Border radius
inline constexpr int RADIUS_SM =  8;
inline constexpr int RADIUS_MD = 12;
inline constexpr int RADIUS_LG = 16;

} // namespace dt
