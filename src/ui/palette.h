#pragma once
/// ╔══════════════════════════════════════════════════════════════════════════╗
/// ║  MASTER COLOR PALETTE — M4 EGT Application                            ║
/// ║  All UI components MUST reference colors from this file.               ║
/// ║  Do NOT hardcode Color(r,g,b) values elsewhere.                        ║
/// ╚══════════════════════════════════════════════════════════════════════════╝

#include <egt/ui>

namespace palette {

// ── Brand ───────────────────────────────────────────────────────────────────
inline const egt::Color kGreen         { 91, 197,   0};  // Primary brand green
inline const egt::Color kGreenDark     { 70, 155,   0};  // Pressed / active state
inline const egt::Color kGreenLight    {200, 235, 170};  // Scrollbar, hover highlights
inline const egt::Color kCyan          { 48, 163, 196};  // Secondary accent (demo, links)

// ── Neutrals ────────────────────────────────────────────────────────────────
inline const egt::Color kWhite         {255, 255, 255};
inline const egt::Color kBlack         {  0,   0,   0};
inline const egt::Color kGray50        {245, 245, 245};  // Card backgrounds
inline const egt::Color kGray100       {228, 229, 232};  // Special key fill
inline const egt::Color kGray200       {217, 217, 217};  // Borders, separators
inline const egt::Color kGray300       {205, 205, 205};  // Inactive tracks
inline const egt::Color kGray400       {170, 170, 170};  // Muted borders
inline const egt::Color kGray500       {150, 150, 150};  // Placeholder text
inline const egt::Color kGray600       {120, 121, 125};  // Hints, secondary text
inline const egt::Color kGray700       {100, 101, 105};  // Primary body text
inline const egt::Color kGray900       { 17,  24,  39};  // Input text (near-black)

// ── Semantic ────────────────────────────────────────────────────────────────
inline const egt::Color kSuccess       = kGreen;
inline const egt::Color kError         {220,  53,  69};
inline const egt::Color kWarning       {255, 165,   0};
inline const egt::Color kInfo          = kCyan;

// ── Overlays / Shadows ──────────────────────────────────────────────────────
inline const egt::Color kTransparent   {  0,   0,   0, 0};
inline const egt::Color kOverlay       { 80,  80,  80, 220}; // Modal backdrop
inline const egt::Color kShadow        {  0,   0,   0,  40}; // Subtle shadow

// ── Slider ──────────────────────────────────────────────────────────────────
inline const egt::Color kSliderHandle        = kGreen;
inline const egt::Color kSliderHandlePressed = kGreenDark;
inline const egt::Color kSliderHandleBorder  {  0,   0,   0,  30}; // Soft shadow ring
inline const egt::Color kSliderTrackActive   = kGreen;
inline const egt::Color kSliderTrackInactive = kGray300;

} // namespace palette
