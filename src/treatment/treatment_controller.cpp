#include "treatment_controller.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <egt/svgimage.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>

using namespace egt;
using namespace std;

// House glyph for the "Back to Home" button (white fill so it reads on the
// cyan button). Matches the Figma completed/ended screen.
static const char* kHomeSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z" fill="#FFFFFF"/>
</svg>)svg";

static Image load_home_icon(int size)
{
    // SvgImage kept alive in a static cache (libegt 1.10 frees the raster
    // buffer when a local SvgImage dies, dangling the sliced Image).
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        static bool written = false;
        const string path = "/tmp/egt-icon-home.svg";
        if (!written) { ofstream f(path); f << kHomeSvg; written = f.good(); }
        auto svg = std::make_shared<SvgImage>("file:" + path, SizeF(size, size));
        s_cache.push_back(svg);
        return static_cast<Image>(*svg);
    } catch (...) { return {}; }
}

// Cyan chevrons flanking the paused-screen tip (Figma 67:773). Stroked, not
// unicode glyphs (those render blank in the device font).
static const char* kChevronTipLeftSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M16 4 L8 12 L16 20" fill="none" stroke="#30A3C4"
        stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg";

static const char* kChevronTipRightSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M8 4 L16 12 L8 20" fill="none" stroke="#30A3C4"
        stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg";

static Image load_chevron(bool left, int size)
{
    // SvgImage kept alive in a static cache (see load_home_icon).
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        const string path = left ? "/tmp/egt-icon-tipchevL.svg"
                                 : "/tmp/egt-icon-tipchevR.svg";
        // Scope the write so the stream flushes/closes before SvgImage reads.
        { ofstream f(path); f << (left ? kChevronTipLeftSvg
                                       : kChevronTipRightSvg); }
        auto svg = std::make_shared<SvgImage>("file:" + path, SizeF(size, size));
        s_cache.push_back(svg);
        return static_cast<Image>(*svg);
    } catch (...) { return {}; }
}

// Green edge glow for the nearly-finished screen (Figma 67:578/67:723). Like
// the age-wheel backdrop but applied to the whole screen: a green gradient
// fades inward from all four edges to transparent in the centre, drawn BEHIND
// the content. m_intensity (0..1) scales the alpha so the glow can "breathe"
// smoothly instead of a hard on/off border.
class GreenGlow : public egt::Widget {
public:
    explicit GreenGlow(const egt::Rect& r) : egt::Widget(r) {
        fill_flags({});            // fully custom-drawn, transparent base
        border(0);
        readonly(true);            // never interactive
    }
    void set_intensity(float i) {
        i = std::max(0.0f, std::min(1.0f, i));
        if (i != m_intensity) { m_intensity = i; damage(); }
    }
    void draw(egt::Painter& p, const egt::Rect&) override {
        const auto b = content_area();
        const int x = b.x(), y = b.y(), w = b.width(), h = b.height();
        const int d = 90;          // how far the glow reaches inward
        const int peak = static_cast<int>(190 * m_intensity);
        const egt::Color on(dt::kGreen.red(), dt::kGreen.green(),
                            dt::kGreen.blue(), peak);
        const egt::Color off(dt::kGreen.red(), dt::kGreen.green(),
                             dt::kGreen.blue(), 0);
        using SA = egt::Pattern::StepArray;
        // Top and bottom edges only (Figma 67:578: the green reflects the
        // lamp glow from above/below, not the left/right sides). libegt 1.10
        // has no Painter::draw(Pattern, RectF); use the set→draw→fill idiom
        // that works on both 1.10 (target) and 1.12 (host simulator).
        egt::Pattern top_grad(SA{{0.f, on}, {1.f, off}},
                              egt::Point(x, y), egt::Point(x, y + d));
        p.set(top_grad);
        p.draw(egt::Rect(x, y, w, d));
        p.fill();
        egt::Pattern bot_grad(SA{{0.f, off}, {1.f, on}},
                              egt::Point(x, y + h - d), egt::Point(x, y + h));
        p.set(bot_grad);
        p.draw(egt::Rect(x, y + h - d, w, d));
        p.fill();
    }
private:
    // Start at the breathing animation's floor (matches phase 0 of the cosine
    // in start_green_breathing), NOT full bright. make_treatment_container
    // builds the glow and on_show_screen paints it one frame BEFORE the
    // breathing timer's first tick (+50 ms) runs, so a 1.0 default showed a
    // full-bright green flash on entry that then snapped down to ~0.12. Being
    // born at the floor makes the entry seamless; the breath pulses up from here.
    float m_intensity = 0.12f;
};

// ── Shared state across treatment screens ───────────────────────────────────
struct TreatmentState {
    TreatmentConfig config;
    TreatmentCallbacks callbacks;

    int cumulative_seconds = 0;    // total treatment time accumulated
    int current_cycle = 0;         // which cycle we're on (0-based)
    int cycles_completed = 0;      // how many full cycles done

    bool is_paused = false;

    // Hold mode for figma-vs-sim screenshots. When true, the per-screen
    // countdown timers are not started so the screen stays put for a capture.
    // Set by start_treatment_flow when EGT_MOCK_TREATMENT is in the env.
    bool freeze = false;

    // Process-limit warnings fire once each as cumulative time crosses the
    // 5-min and 1-min-remaining thresholds. Tracked here so they don't
    // re-fire when a screen rebuilds between cycles.
    bool first_warning_fired  = false;
    bool second_warning_fired = false;

    // Timer references (kept alive via shared_ptr)
    shared_ptr<PeriodicTimer> active_timer;
    shared_ptr<PeriodicTimer> flash_timer;   // green-glow breathing (nearly-done)

    // Green edge glow of the current green-mode screen, so the nearly-done
    // screen can breathe it (the Figma "flashing light" effect, node 67:723).
    shared_ptr<GreenGlow> green_glow;

    // One-shot guard: once the user exits (Exit / End Treatment / Complete)
    // every in-flight timer must early-out so it doesn't yank the user back
    // into the treatment flow seconds after they navigated away.
    shared_ptr<bool> alive = make_shared<bool>(true);

    // Helper: format seconds as mm:ss
    static string format_time(int seconds) {
        int m = seconds / 60;
        int s = seconds % 60;
        stringstream ss;
        ss << setfill('0') << setw(2) << m << ":" << setw(2) << s;
        return ss.str();
    }

    // How many total cycles to reach the process limit
    int total_cycles() const {
        if (config.cycle_seconds <= 0) return 1;
        return max(1, (config.process_limit_seconds + config.cycle_seconds - 1) / config.cycle_seconds);
    }

    // Has the process hit its hard time limit?
    bool is_complete() const {
        return cumulative_seconds >= config.process_limit_seconds;
    }
};

// Fire the process-limit advance-notice alerts (tone + LED) once each as
// cumulative time crosses the configured thresholds. Safe to call from any
// per-second tick — the state flags guarantee single-fire.
static void check_and_fire_warnings(shared_ptr<TreatmentState> state)
{
    const int remaining =
        state->config.process_limit_seconds - state->cumulative_seconds;

    if (!state->first_warning_fired &&
        remaining <= state->config.first_warning_remaining) {
        state->first_warning_fired = true;
        printf("[PROCESS] First warning — %d s remaining (1 tone, 1 LED flash)\n",
               remaining); fflush(stdout);
        if (state->callbacks.on_alert_tone)    state->callbacks.on_alert_tone(1);
        if (state->callbacks.on_tip_led_flash) state->callbacks.on_tip_led_flash(1);
    }
    if (!state->second_warning_fired &&
        remaining <= state->config.second_warning_remaining) {
        state->second_warning_fired = true;
        printf("[PROCESS] Second warning — %d s remaining (2 tones, 2 LED flashes)\n",
               remaining); fflush(stdout);
        if (state->callbacks.on_alert_tone)    state->callbacks.on_alert_tone(2);
        if (state->callbacks.on_tip_led_flash) state->callbacks.on_tip_led_flash(2);
    }
}

// Which warning window the current cumulative time falls in.
enum class WarnWindow { None, FiveMin, OneMin };
static WarnWindow current_warn_window(const shared_ptr<TreatmentState>& state)
{
    const int remaining =
        state->config.process_limit_seconds - state->cumulative_seconds;
    if (remaining <= state->config.second_warning_remaining) return WarnWindow::OneMin;
    if (remaining <= state->config.first_warning_remaining)  return WarnWindow::FiveMin;
    return WarnWindow::None;
}

// ── Forward declarations ────────────────────────────────────────────────────
// `resuming` toggles the post-warming target: false (initial start) → Ready
// screen with the "Begin Treatment" button; true (after Pause → Resume) →
// position-tip directly so the operator never has to press Begin again to
// keep going.
static void show_warming(shared_ptr<TreatmentState> state, bool resuming = false);
static void show_ready(shared_ptr<TreatmentState> state);
static void show_position_tip(shared_ptr<TreatmentState> state);
static void show_treatment_active(shared_ptr<TreatmentState> state);
static void show_treatment_paused(shared_ptr<TreatmentState> state);
static void show_end_confirmation(shared_ptr<TreatmentState> state);
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early);
static void show_treatment_nearly_done(shared_ptr<TreatmentState> state, int remaining_seconds);
static void show_treatment_zero(shared_ptr<TreatmentState> state);

// ── Container result with dynamic labels ────────────────────────────────────
struct TreatmentScreen {
    shared_ptr<Frame> container;
    shared_ptr<Label> cumulative_time_label;  // for dynamic updates
};

// ── Figma-matched layout constants (432×261 → 800×480) ─────────────────────
// Cumulative time header area (Figma: Group 159 at y=23, line at y=56)
static constexpr int CUM_TIME_Y     = 15;   // y for time labels
static constexpr int CUM_SEP_Y      = 110;  // y for separator line (below logo at y=5+103=108)
static constexpr int CUM_LEFT_X     = 180;  // x start (right of logo at x=10+166=176)
static constexpr int CUM_WIDTH      = 400;  // width of cumulative area (up to demo badge at x=580)

// Main content area
// Vertical rhythm tuned so the countdown, status, dots, the process-limit
// warning banner, and the bottom buttons all fit without overlap now that
// the buttons sit higher (32 px bottom margin → BTN_Y=368).
static constexpr int CONTENT_Y      = 110;  // y for large number/percentage
static constexpr int CONTENT_H      = 130;  // height of large number area
static constexpr int STATUS_Y       = 240;  // Figma 130px * SCALE = 241
static constexpr int DOTS_Y         = 285;  // Figma 154px * SCALE = 285
// Standard bottom margin for action buttons across screens (≈32 px). All
// bottom buttons share the same bottom edge (SCREEN_H - BTN_BOTTOM_MARGIN).
static constexpr int BTN_BOTTOM_MARGIN = 21;  // Figma button bottom y≈459
static constexpr int BTN_W          = 244;  // Figma 131px * SCALE
static constexpr int BTN_H          = 100;  // Figma 54px  * SCALE
static constexpr int BTN_Y          = 480 - BTN_BOTTOM_MARGIN - BTN_H;  // = 368
static constexpr int BTN_LEFT_X     = 39;   // Figma 21px  * SCALE
static constexpr int BTN_RIGHT_X    = 537;  // Figma 290px * SCALE

// Shared bottom edge so non-treatment screens can align their (shorter)
// buttons to the same line. (e.g. y = STD_BTN_BOTTOM - height)
static constexpr int STD_BTN_BOTTOM = 480 - BTN_BOTTOM_MARGIN;  // = 448

// ── Common helpers ──────────────────────────────────────────────────────────
static TreatmentScreen make_treatment_container(
    shared_ptr<TreatmentState> state,
    bool show_cumulative,
    bool green_mode = false)
{
    // green_mode now means "nearly-finished frame" — same dark text and
    // white bg as normal screens, just with a green band around the edges.
    // (The old behaviour was a full-screen green wash with white text;
    // see Figma 67:578 for the updated treatment.)
    const Color text_color  = dt::kTextPrimary;
    const Color sep_color   = dt::kGrayLight;
    const Color bg_color    = dt::kBgWhite;

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, bg_color);

    state->green_glow.reset();      // cleared unless this is a green-mode screen

    if (green_mode) {
        // Green edge glow (Figma 67:578): a gradient fading inward from every
        // edge, drawn behind the content. The intensity breathes via a timer
        // in show_treatment_nearly_done. Added first -> sits under everything.
        auto glow = make_shared<GreenGlow>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
        container->add(glow);
        state->green_glow = glow;
    }

    // Logo (top-left, full Figma size)
    auto logo = ui::create_logo(10, 5, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Demo mode badge (top-right, vertical: DEMO MODE label + Exit below)
    if (state->config.demo_mode) {
        // Figma 66:524: DEMO MODE text centred ~x=727, top y≈17 (scaled).
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 155, 14, state->callbacks.on_leave_to_home);
        container->add(badge.frame);
    }

    shared_ptr<Label> cum_time_lbl;

    // Cumulative time header (ToDo master task 6): a two-line label
    // "CUMULATIVE / TREATMENT TIME" next to the big time value, the whole
    // pair centred horizontally on the screen and nudged lower than before
    // (it used to hug the very top). Layout is a centred group:
    //   [ right-aligned 2-line label ] gap [ big time value ]
    if (show_cumulative) {
        // Figma header geometry (81:1036 / 154:931): label right edge x=229
        // -> 424 device, header top y=23 -> 42, divider y=56 -> 103.
        // time_w 150 (not the Figma-tight 120): the interim fallback font
        // (DejaVu) renders "00:00" at 44pt ~125px wide, which clipped the
        // last digit at 120. The label is left-aligned so the extra width
        // only extends the right edge (to x=590, clear of the DEMO badge).
        const int desc_w = 210, time_w = 150, gap = 16;
        const int group_x = 214;   // 424 - desc_w
        const int hdr_y   = 42;
        const int sep_y   = 103;

        // Two-line description, right-aligned so it reads tight against the
        // time value.
        auto desc1 = make_shared<Label>("CUMULATIVE",
            Rect(group_x, hdr_y, desc_w, 24),
            AlignFlag::right | AlignFlag::center_vertical);
        desc1->font(Font(18, Font::Weight::normal));  // Figma 10pt * SCALE
        desc1->color(Palette::ColorId::label_text, text_color);
        container->add(desc1);

        auto desc2 = make_shared<Label>("TREATMENT TIME",
            Rect(group_x, hdr_y + 22, desc_w, 24),
            AlignFlag::right | AlignFlag::center_vertical);
        desc2->font(Font(18, Font::Weight::normal));  // Figma 10pt * SCALE
        desc2->color(Palette::ColorId::label_text, text_color);
        container->add(desc2);

        // Big time value, vertically centred against the 2-line label.
        cum_time_lbl = make_shared<Label>(
            TreatmentState::format_time(state->cumulative_seconds),
            Rect(group_x + desc_w + gap, hdr_y, time_w, 46),
            AlignFlag::left | AlignFlag::center_vertical);
        cum_time_lbl->font(Font(44, Font::Weight::normal));  // Figma 24px * 1.852
        cum_time_lbl->color(Palette::ColorId::label_text, text_color);
        container->add(cum_time_lbl);

        // Separator line under the group (Figma Line 14: x=113 -> 209,
        // width 202 -> 374)
        auto sep = make_shared<Frame>(Rect(209, sep_y, 374, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, sep_color);
        sep->border(0);
        container->add(sep);
    }

    return {container, cum_time_lbl};
}

// Two-tier action button used across treatment screens: the verb
// ("Pause" / "End" / "Resume") is the dominant text, "Treatment" below it
// is small and secondary. The previous `create_outlined_button("Pause\n…")`
// rendered both lines at the same size — the verb (the actually useful
// affordance) got lost in the wall of text. Style is passed in so the
// same shape works for outlined, green-filled, cyan-filled, and white-on-
// green variants the treatment screens use.
struct ActionBtnStyle {
    Color bg;
    Color fg;
    Color border;
    int   border_width;
};

// Figma "bt new" (69:885) is a borderless white card — only the drop shadow
// separates it from the background (no #D9D9D9 outline).
static const ActionBtnStyle BTN_OUTLINED        = { dt::kWhite,      dt::kTextPrimary, dt::kWhite,     0 };
static const ActionBtnStyle BTN_GREEN_FILLED    = { dt::kGreen,      dt::kWhite,       dt::kGreen,     0 };
static const ActionBtnStyle BTN_CYAN_FILLED     = { dt::kAccentCyan, dt::kWhite,       dt::kAccentCyan,0 };
static const ActionBtnStyle BTN_WHITE_GREEN_FG  = { dt::kWhite,      dt::kGreen,       dt::kWhite,     0 };

static shared_ptr<Frame> make_action_button(
    const string& big_text,
    const string& small_text,
    const Rect& rect,
    const ActionBtnStyle& style,
    function<void()> on_click)
{
    // Wrapper so the soft drop shadow has room to render outside the card
    // (EGT clips painting to a widget box). The card sits PAD in from the
    // wrapper edges; the shadow is a few translucent black rounded rects
    // nudged down ~2px (Figma effect_UDY3OL: boxShadow 0 2 2 rgba(0,0,0,0.2)).
    constexpr int PAD = 10;
    auto wrap = make_shared<Frame>(
        Rect(rect.x() - PAD, rect.y() - PAD,
             rect.width() + 2 * PAD, rect.height() + 2 * PAD));
    wrap->fill_flags({});  // transparent

    // Figma shadow is tight: 0 2px 1px rgba(0,0,0,0.2) -> device ~0 4px 2px.
    // Two thin light layers instead of the old 3-layer ~10%/layer stack.
    for (int i = 1; i >= 0; --i) {
        auto sh = make_shared<Frame>(
            Rect(PAD - i, PAD + 3 + i, rect.width() + 2 * i, rect.height() + 2 * i));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, 18));  // ~7% black / layer
        sh->border_radius(dt::RADIUS_MD + i);
        sh->border(0);
        wrap->add(sh);
    }

    auto frame = make_shared<Frame>(Rect(PAD, PAD, rect.width(), rect.height()));
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, style.bg);
    frame->color(Palette::ColorId::border, style.border);
    frame->border(style.border_width);
    frame->border_radius(dt::RADIUS_MD);
    wrap->add(frame);

    // Figma 66:524 "bt new": verb 18pt→33 bold over qualifier 11pt→20
    // regular. Centre the two-line group vertically so it stays balanced for
    // any button height (Figma button is 54px→100px tall).
    const int grp_h   = 64;
    const int grp_top = (rect.height() - grp_h) / 2;

    auto big = make_shared<Label>(big_text,
        Rect(0, grp_top, rect.width(), 40),
        AlignFlag::center);
    big->font(Font(33, Font::Weight::bold));
    big->color(Palette::ColorId::label_text, style.fg);
    frame->add(big);

    auto small = make_shared<Label>(small_text,
        Rect(0, grp_top + 40, rect.width(), 24),
        AlignFlag::center);
    small->font(Font(20, Font::Weight::normal));
    small->color(Palette::ColorId::label_text, style.fg);
    frame->add(small);

    if (on_click) {
        frame->on_event([on_click](Event& e) {
            if (e.id() == EventId::pointer_click) on_click();
        }, {EventId::pointer_click});
    }
    return wrap;
}

// Overall treatment progress as a 0..1 fraction of the process time limit.
static float treatment_progress(const shared_ptr<TreatmentState>& state)
{
    if (state->config.process_limit_seconds <= 0) return 0.0f;
    return static_cast<float>(state->cumulative_seconds) /
           static_cast<float>(state->config.process_limit_seconds);
}

// Add the segmented progress dots and return the bar so callers can update
// it live. The fill tracks cumulative time vs the process limit (not the
// cycle count) so it advances smoothly throughout the session instead of
// staying empty for many short cycles.
static shared_ptr<Frame> add_segmented_progress(
    shared_ptr<Frame> container,
    shared_ptr<TreatmentState> state,
    int y = DOTS_Y)
{
    const int seg_bar_w = 373;   // Figma 202px * SCALE (see create_segmented_progress)
    auto seg_bar = ui::create_segmented_progress(
        (dt::SCREEN_W - seg_bar_w) / 2, y, 0);
    // Figma 81:1041..1070: each 5px dot has rounded-[1px] corners -> device ~2.
    // The shared component draws square dots; round them locally here.
    for (auto& child : seg_bar->children())
        if (auto* sq = dynamic_cast<Frame*>(child.get()))
            sq->border_radius(2);
    ui::update_segmented_progress_fraction(seg_bar, treatment_progress(state));
    container->add(seg_bar);
    return seg_bar;
}

// ── Entry point ─────────────────────────────────────────────────────────────
void start_treatment_flow(
    const TreatmentConfig& config,
    const TreatmentCallbacks& callbacks)
{
    auto state = make_shared<TreatmentState>();
    state->config = config;
    state->callbacks = callbacks;

    // Wrap every exit callback so it (1) trips the one-shot guard, (2) cancels
    // the current phase timer, (3) only then calls the host's handler. Without
    // this, the user could tap Exit / Complete / End Treatment and a pending
    // timer tick fired ~1 s later would re-call show_treatment_active and yank
    // them back into the treatment flow on top of whatever screen they're on.
    auto wrap_exit = [state](function<void()> cb) -> function<void()> {
        if (!cb) return cb;
        return [state, cb]() {
            if (!*state->alive) return;       // already exited — ignore re-entry
            *state->alive = false;
            if (state->active_timer) state->active_timer->cancel();
            cb();
        };
    };
    state->callbacks.on_leave_to_home        = wrap_exit(state->callbacks.on_leave_to_home);
    state->callbacks.on_treatment_completed  = wrap_exit(state->callbacks.on_treatment_completed);
    state->callbacks.on_treatment_ended_early= wrap_exit(state->callbacks.on_treatment_ended_early);

    // Hold mode for the figma-vs-sim loop: EGT_MOCK_TREATMENT=<screen> boots
    // straight onto one treatment screen with its countdown frozen, so the
    // capture step gets a stable frame. Values:
    //   warming | ready | position | reposition | active | nearly
    //   paused  | zero | end-confirm | completed | ended
    if (const char* hold = std::getenv("EGT_MOCK_TREATMENT")) {
        const string s = hold;
        state->freeze = true;
        // Use sane non-demo timings so demo mode's tiny limits don't break the
        // progress math while a frame is held for a screenshot. Values chosen
        // to mirror the Figma mockups (00:05 header, 25 s countdown).
        state->config.process_limit_seconds    = 2700;
        state->config.cycle_seconds            = 25;
        state->config.first_warning_remaining  = 300;
        state->config.second_warning_remaining = 60;
        state->cumulative_seconds = 5;     // Figma header shows 00:05
        state->cycles_completed   = 0;
        state->current_cycle      = 0;

        if      (s == "warming")     show_warming(state);
        else if (s == "ready")       show_ready(state);
        else if (s == "position")    show_position_tip(state);
        else if (s == "reposition") { state->cycles_completed = 2; state->current_cycle = 2;
                                       state->cumulative_seconds = 30;  // Figma 67:733 header 00:30
                                       show_position_tip(state); }
        else if (s == "active")      show_treatment_active(state);
        else if (s == "nearly")    { state->config.process_limit_seconds = 30;  // Figma header 00:25
                                      state->cumulative_seconds = 25;            // dots ~full green
                                      show_treatment_nearly_done(state, 5); }
        else if (s == "paused")      show_treatment_paused(state);
        else if (s == "zero")      { state->config.process_limit_seconds = 30;  // Figma header 00:30
                                      state->cumulative_seconds = 30;            // == limit -> full green bar
                                      show_treatment_zero(state); }
        else if (s == "end-confirm") show_end_confirmation(state);
        else if (s == "completed") { state->config.process_limit_seconds = 30;  // Figma header 00:30
                                      state->cumulative_seconds = 30;            // == limit -> full green bar
                                      state->cycles_completed = state->total_cycles();
                                      show_treatment_completed(state, false); }
        else if (s == "ended")     { state->cumulative_seconds = 30;  // Figma 81:406 header 00:30
                                      show_treatment_completed(state, true); }
        else                         show_warming(state);
        return;
    }

    show_warming(state);
}

// ── Percentage layout helper ────────────────────────────────────────────────
// Lays out a big "NN%" composite (large bold number + smaller superscript %)
// centred horizontally on the screen, regardless of digit count. Until we
// did this, the number's rect was fixed and right-aligned at x=430 — fine
// at 1 digit but the whole "10%" / "100%" group drifted left as more digits
// appeared, throwing the visual centre off.
//
// Big "NN%" display drawn entirely with the Painter, measuring the real
// glyph widths via text_size at draw time. This replaces the old approach
// of two Labels positioned by hard-coded width estimates — those estimates
// were calibrated on the host font and left the "%" floating far from the
// number on the target (different font metrics). Measuring at runtime keeps
// the % tucked right against the number on any font/host.
class PercentDisplay : public Widget {
public:
    PercentDisplay(const Rect& rect, int value, const Color& color)
        : Widget(rect), m_value(value), m_color(color) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void set_value(int v) { if (v != m_value) { m_value = v; damage(); } }
    void set_color(const Color& c) { m_color = c; damage(); }

    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const string num = to_string(m_value);
        const Font num_font(118, Font::Weight::normal);   // Figma 64pt * SCALE
        const Font pct_font(30, Font::Weight::bold);       // Figma 16pt * SCALE
        const int gap = 6;

        painter.set(num_font);
        const auto ns = painter.text_size(num);
        painter.set(pct_font);
        const auto ps = painter.text_size("%");

        // Centre the NUMBER on the widget; the small % is a unit suffix that
        // hangs off the lower-right, bottom-aligned with the number (Figma
        // 37:1634 — % baseline sits at the digits' baseline).
        const float num_x = b.x() + (b.width() - ns.width()) / 2.0f;
        const float num_top = b.y() + (b.height() - ns.height()) / 2.0f;
        // Align the % visible BOTTOM with the digits' baseline. The number's
        // em-box extends ~20% below the visible digits (descender space), so
        // scale by 0.78 instead of aligning the em-box bottoms (which drops
        // the % below the digits).
        const float pct_top = num_top + 0.65f * (ns.height() - ps.height());

        painter.set(num_font);
        painter.set(m_color);
        painter.draw(PointF(num_x, num_top));
        painter.draw(num);

        painter.set(pct_font);
        painter.set(m_color);
        painter.draw(PointF(num_x + ns.width() + gap, pct_top));
        painter.draw(std::string("%"));
    }
private:
    int m_value;
    Color m_color;
};

// ── WARMING SCREEN ──────────────────────────────────────────────────────────
// Figma Group 179: large % number + superscript %, two-line status, progress bar.
// No cumulative time, no segmented dots, no buttons.
static void show_warming(shared_ptr<TreatmentState> state, bool resuming)
{
    // Warming-screen-local layout (independent from other treatment screens)
    // Number font 120px → rendered height ~145px
    // Figma 37:1626 (Spinner frame @114,20). Dual ratio: x*1.852, y*1.836.
    // Content is centred on the spinner: cx = (114+107)*1.852 = 409.
    const int W_CX         = 409;  // content centre x (not screen-centre 400)
    const int W_BOX_W      = 2 * std::min(W_CX, dt::SCREEN_W - W_CX);  // centred box
    const int W_BOX_X      = W_CX - W_BOX_W / 2;
    const int W_NUM_Y      = 122;  // number top ~ y=70*1.836=128 (centred in box)
    const int W_NUM_H      = 145;  // height of number rect (118px font)
    const int W_PCT_H      = 80;   // height of % rect
    const int W_PCT_Y      = W_NUM_Y + W_NUM_H - W_PCT_H;  // bottom-aligned with number
    const int W_STATUS1_Y  = 252;  // "Warming up"     (Figma y=136*1.836=250)
    const int W_STATUS2_Y  = 290;  // "for Treatment"  (~38px line spacing for 30px font)
    const int W_BAR_Y      = 349;  // progress bar     (Figma y=190*1.836=349)

    auto [container, _cum_lbl] = make_treatment_container(state, false);

    // ── Large "NN%" display (Painter-measured, % tucked to the number) ────
    (void)W_PCT_H; (void)W_PCT_Y;
    auto pct_display = make_shared<PercentDisplay>(
        Rect(W_BOX_X, W_NUM_Y, W_BOX_W, W_NUM_H), 0, dt::kTextPrimary);
    container->add(pct_display);

    // ── Two-line status text (Figma 16pt -> 30, centred on the content) ────
    auto status1 = make_shared<Label>("Warming up",
        Rect(W_BOX_X, W_STATUS1_Y, W_BOX_W, 36));
    status1->font(Font(30, Font::Weight::normal));
    status1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status1);

    auto status2 = make_shared<Label>("for Treatment",
        Rect(W_BOX_X, W_STATUS2_Y, W_BOX_W, 36));
    status2->font(Font(30, Font::Weight::normal));
    status2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status2);

    // ── Linear progress bar (Figma 66:494: 405x5 -> 750x8, thin, near-full
    // width with a thin gray outline) ─────────────────────────────────────
    const int bar_w = 750;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2,
        W_BAR_Y,
        bar_w, 8);
    container->add(progress_bar);

    state->callbacks.on_show_screen(container);

    // Hold mode: freeze at a representative mid-warming value and skip the
    // animation timer so the screen holds for a screenshot.
    if (state->freeze) {
        pct_display->set_value(70);
        ui::update_linear_progress(progress_bar, 70.0f);
        return;
    }

    // ── Animate warming over configured seconds ───────────────────────────
    auto progress_val = make_shared<float>(0.0f);
    auto elapsed_ms = make_shared<int>(0);
    const int total_ms = state->config.warming_seconds * 1000;

    auto timer = make_shared<PeriodicTimer>(chrono::milliseconds(50));
    state->active_timer = timer;

    weak_ptr<PercentDisplay> w_pct_disp = pct_display;
    weak_ptr<Frame> w_bar = progress_bar;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        *elapsed_ms += 50;
        *progress_val = min(100.0f, (*elapsed_ms * 100.0f) / total_ms);

        const int iv = static_cast<int>(*progress_val);
        if (auto pd = w_pct_disp.lock())
            pd->set_value(iv);

        if (auto bar = w_bar.lock())
            ui::update_linear_progress(bar, *progress_val);

        if (*elapsed_ms >= total_ms) {
            timer->cancel();
            // After a pause→resume re-warm, skip the Ready / "Begin Treatment"
            // gate (initial-start affordance) and continue the in-progress
            // session at the position-tip step so cumulative time is kept.
            if (resuming) show_position_tip(state);
            else          show_ready(state);
        }
    });
    if (!state->freeze) timer->start();
}

// ── READY SCREEN ────────────────────────────────────────────────────────────
// Figma REG_READY (168:810): "Ready for Treatment" + 100% + "Begin Treatment" button.
// User must press Begin to proceed — no auto-advance.
static void show_ready(shared_ptr<TreatmentState> state)
{
    // Figma 168:812: 100% in GRAY (like the warming %), "Ready for / Treatment"
    // tucked under the number, full green bar, and a green-gradient Begin
    // button (NOT a flat fill). Dual ratio: x*1.852, y*1.836.
    const int W_NUM_Y      = 136;   // number visible top ~130 (Figma y71 ink)
    const int W_NUM_H      = 150;
    const int W_STATUS1_Y  = 232;   // "Ready for"  (Figma ink y129 -> 237)
    const int W_STATUS2_Y  = 268;   // "Treatment"  (Figma ink y149 -> 274)
    const int W_BAR_Y      = 330;   // Figma y180 * 1.836

    // Figma 168:810 includes the cumulative-time header (00:49) + divider
    // (nodes 155:974..977), so the ready screen shows it too.
    auto [container, _cum_lbl] = make_treatment_container(state, true);

    // ── Large "100%" — gray, matching the warming % (Figma 154:929 #646569) ─
    auto pct_display = make_shared<PercentDisplay>(
        Rect(0, W_NUM_Y, dt::SCREEN_W, W_NUM_H), 100, dt::kTextPrimary);
    container->add(pct_display);

    // ── "Ready for" / "Treatment" (Figma wrap), tucked under the number ────
    auto status1 = make_shared<Label>("Ready for",
        Rect(0, W_STATUS1_Y, dt::SCREEN_W, 32));
    status1->font(Font(28, Font::Weight::normal));
    status1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status1);

    auto status2 = make_shared<Label>("Treatment",
        Rect(0, W_STATUS2_Y, dt::SCREEN_W, 32));
    status2->font(Font(28, Font::Weight::normal));
    status2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status2);

    // ── Full green progress bar (Figma 154:926: 405x5 -> 750, near-full) ───
    const int bar_w = 750;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2, W_BAR_Y, bar_w, 9);
    container->add(progress_bar);
    ui::update_linear_progress(progress_bar, 100.0f);

    // ── "Begin Treatment" button (Figma bt begin 166:1000): green vertical
    //    gradient #5BC500 -> #408A00, "Begin" large + "Treatment" small, white
    //    bold. Demo flow keeps a flat cyan fill. device rect (285,354,230,99).
    const Rect begin_r(285, 354, 230, 99);
    // Tight shadow (Figma 0 2px 1px rgba(0,0,0,0.2) -> device ~0 4px 2px):
    // two thin light layers, matching make_action_button.
    for (int i = 1; i >= 0; --i) {
        auto sh = make_shared<Frame>(Rect(begin_r.x() - i, begin_r.y() + 3 + i,
                                          begin_r.width() + 2 * i,
                                          begin_r.height() + 2 * i));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, 18));
        sh->border(0);
        sh->border_radius(dt::RADIUS_XS + i);
        container->add(sh);
    }
    auto begin_btn = make_shared<Frame>(begin_r);
    begin_btn->fill_flags({Theme::FillFlag::blend});
    begin_btn->border(0);
    begin_btn->border_radius(dt::RADIUS_XS);
    if (state->config.demo_mode) {
        begin_btn->color(Palette::ColorId::bg, dt::kAccentCyan);
    } else {
        Pattern grad(Pattern::StepArray{{0.0f, Color(91, 197, 0)},
                                        {1.0f, Color(64, 138, 0)}},
                     Point(begin_r.x(), begin_r.y()),
                     Point(begin_r.x(), begin_r.y() + begin_r.height()));
        begin_btn->color(Palette::ColorId::bg, grad);
    }
    begin_btn->on_event([=](Event& e) {
        if (e.id() == EventId::pointer_click) show_position_tip(state);
    }, {EventId::pointer_click});

    auto lbl_begin = make_shared<Label>("Begin",
        Rect(0, 16, begin_r.width(), 44), AlignFlag::center);
    lbl_begin->fill_flags({});
    lbl_begin->font(Font(33, Font::Weight::bold));
    lbl_begin->color(Palette::ColorId::label_text, dt::kWhite);
    begin_btn->add(lbl_begin);

    auto lbl_treat = make_shared<Label>("Treatment",
        Rect(0, 58, begin_r.width(), 28), AlignFlag::center);
    lbl_treat->fill_flags({});
    lbl_treat->font(Font(20, Font::Weight::bold));
    lbl_treat->color(Palette::ColorId::label_text, dt::kWhite);
    begin_btn->add(lbl_treat);

    container->add(begin_btn);

    state->callbacks.on_show_screen(container);
}

// ── POSITION TIP SCREEN ────────────────────────────────────────────────────
// Figma Group 209: Logo, DEMO MODE, cumulative time header, countdown,
// status text, Pause/End buttons. The cumulative header was previously
// suppressed which made the user feel the timer "disappeared" between
// cycles — keep it visible so the running total stays anchored.
static void show_position_tip(shared_ptr<TreatmentState> state)
{
    bool is_reposition = state->cycles_completed > 0;

    // Figma 143:859 (initial position, fetched 2026-06-10) shows the
    // cumulative-time header (00:00, node 154:931) just like the reposition
    // screen (67:733), so the header is always on here.
    auto [container, _cum_lbl2] = make_treatment_container(state, true);

    string title = is_reposition
        ? "Reposition the Applicator Tip"
        : "Position the Applicator Tip";

    // Countdown as M:SS with a single minute digit ("0:05"), Figma style.
    auto fmt_mss = [](int s) {
        return to_string(s / 60) + ":" +
               (s % 60 < 10 ? "0" : "") + to_string(s % 60);
    };

    // Large countdown display (Figma: thin/regular, 64pt * SCALE). The frozen
    // reposition capture matches Figma 67:733, which shows 0:00.
    auto countdown_val = make_shared<int>(
        (state->freeze && is_reposition) ? 0 : state->config.position_tip_seconds);
    auto countdown_label = make_shared<Label>(
        fmt_mss(*countdown_val),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(Font(116, Font::Weight::normal));
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text below countdown, two lines (Figma 100:772: 16pt * SCALE,
    // gray, "Position the" / "Applicator Tip").
    const string line1 = is_reposition ? "Re-position the" : "Position the";
    auto status1 = make_shared<Label>(line1,
        Rect(0, STATUS_Y, dt::SCREEN_W, 32));
    status1->font(Font(28, Font::Weight::normal));
    status1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status1);

    auto status2 = make_shared<Label>("Applicator Tip",
        Rect(0, STATUS_Y + 34, dt::SCREEN_W, 32));
    status2->font(Font(28, Font::Weight::normal));
    status2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status2);

    // Pause / End buttons (Figma: y=194, left=21, right=290)
    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    // Figma 67:733: on the reposition screen End is the cyan-filled call to
    // action, Pause stays white. The initial position screen keeps both white.
    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        is_reposition ? BTN_CYAN_FILLED : BTN_OUTLINED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    // Countdown timer
    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    state->active_timer = timer;

    weak_ptr<Label> w_label = countdown_label;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*countdown_val)--;
        if (auto lb = w_label.lock())
            lb->text(fmt_mss(*countdown_val));

        if (*countdown_val <= 0) {
            timer->cancel();
            show_treatment_active(state);
        }
    });
    if (!state->freeze) timer->start();
}

// ── TREATMENT ACTIVE SCREEN ────────────────────────────────────────────────
// ── TREATMENT ACTIVE SCREEN ────────────────────────────────────────────────
// Figma Group 150: Logo, DEMO MODE, cumulative time (header), countdown,
// status text, segmented dots, Pause/End buttons.
static void show_treatment_active(shared_ptr<TreatmentState> state)
{
    auto [container, cum_time_lbl] = make_treatment_container(state, true);

    // Large countdown (Figma: y=60, 64px)
    int cycle_remaining = state->config.cycle_seconds;
    int remaining_total = state->config.process_limit_seconds - state->cumulative_seconds;
    cycle_remaining = min(cycle_remaining, remaining_total);

    auto remaining = make_shared<int>(cycle_remaining);
    auto countdown_label = make_shared<Label>(
        to_string(*remaining),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(Font(116, Font::Weight::normal));  // Figma 64pt * SCALE, regular
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text below countdown (Figma: y=130)
    auto status = make_shared<Label>("Treatment started",
        Rect(0, STATUS_Y, dt::SCREEN_W, 34));
    status->font(Font(28, Font::Weight::normal));  // Figma 16pt * SCALE
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Segmented progress dots (Figma: y=154) — tracks cumulative time
    auto seg_bar = add_segmented_progress(container, state);
    if (state->freeze)   // representative fill so the held frame shows progress
        ui::update_segmented_progress_fraction(seg_bar, 0.16f);

    // ── MVP process-limit warning banner ──────────────────────────────
    // Hidden until cumulative time enters a warning window; amber at the
    // 5-min mark, red at the 1-min mark, with a live "limit in M:SS"
    // countdown. Placeholder styling for client review — Figma hasn't
    // defined these notification screens yet.
    const int wb_w = 460, wb_h = 36;
    // Sits between the dots and the bottom buttons (dots end ~306, buttons
    // start at 368) — clear of both.
    auto warn_banner = make_shared<Frame>(
        Rect((dt::SCREEN_W - wb_w) / 2, 322, wb_w, wb_h));
    warn_banner->fill_flags({Theme::FillFlag::blend});
    warn_banner->border(0);
    warn_banner->border_radius(dt::RADIUS_SM);
    warn_banner->hide();
    container->add(warn_banner);

    auto warn_lbl = make_shared<Label>("",
        Rect(0, 0, wb_w, wb_h), AlignFlag::center);
    warn_lbl->font(Font(18, Font::Weight::bold));
    warn_lbl->color(Palette::ColorId::label_text, dt::kWhite);
    warn_banner->add(warn_lbl);

    auto refresh_banner = [state](shared_ptr<Frame> banner,
                                  shared_ptr<Label> lbl) {
        const auto win = current_warn_window(state);
        if (win == WarnWindow::None) { banner->hide(); return; }
        const int rem = std::max(0,
            state->config.process_limit_seconds - state->cumulative_seconds);
        banner->color(Palette::ColorId::bg,
            win == WarnWindow::OneMin ? dt::kRed : dt::kOrange);
        lbl->text("Process limit in " + TreatmentState::format_time(rem));
        banner->show();
    };
    refresh_banner(warn_banner, warn_lbl);  // reflect current state on build

    // Pause / End buttons (Figma: y=194, left=21, right=290)
    auto timer_ref = make_shared<shared_ptr<PeriodicTimer>>(nullptr);

    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    // Figma 66:524: End matches Pause — white card, gray text, soft shadow.
    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    // Active treatment timer — 1 second ticks
    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    *timer_ref = timer;
    state->active_timer = timer;

    weak_ptr<Label> w_countdown = countdown_label;
    weak_ptr<Label> w_cum_time = cum_time_lbl;
    weak_ptr<Frame> w_warn_banner = warn_banner;
    weak_ptr<Label> w_warn_lbl = warn_lbl;
    weak_ptr<Frame> w_seg_bar = seg_bar;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock())
            lb->text(to_string(*remaining));

        // Update cumulative time dynamically
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));

        // Update overall progress dots (cumulative vs process limit)
        if (auto sb = w_seg_bar.lock())
            ui::update_segmented_progress_fraction(sb, treatment_progress(state));

        // Process-limit alerts (tone + LED) + banner refresh
        check_and_fire_warnings(state);
        if (auto wb = w_warn_banner.lock())
            if (auto wl = w_warn_lbl.lock())
                refresh_banner(wb, wl);

        // Transition to green near-end screen at threshold
        if (*remaining <= state->config.nearly_finished_threshold && *remaining > 0) {
            timer->cancel();
            show_treatment_nearly_done(state, *remaining);
            return;
        }

        // Check if treatment target reached (e.g. last cycle short)
        if (state->is_complete()) {
            timer->cancel();
            state->cycles_completed++;
            show_treatment_zero(state);   // "0 + check" screen, then auto-advance
            return;
        }

        // Check if cycle ended
        if (*remaining <= 0) {
            timer->cancel();
            state->cycles_completed++;
            state->current_cycle++;
            show_position_tip(state);
        }
    });
    if (!state->freeze) timer->start();
}

// Breathing green glow used by the nearly-done and "0" screens (Figma 67:723:
// "the green is to reflect the effect of flashing light"). Eases the glow
// intensity up and down on a cosine (~2.6 s per breath). If hold_ticks > 0 the
// breath runs for that many 50 ms ticks then fires on_done (used by the "0"
// screen to hold ~3 s and auto-advance). Frozen screenshots hold a mid-bright
// glow; EGT_FLASH_TEST forces the breathing while held so it can be captured.
static void start_green_breathing(shared_ptr<TreatmentState> state,
                                  int hold_ticks = 0,
                                  function<void()> on_done = nullptr)
{
    if (state->flash_timer) state->flash_timer->cancel();
    auto glow_timer = make_shared<PeriodicTimer>(chrono::milliseconds(50));
    state->flash_timer = glow_timer;
    weak_ptr<GreenGlow> w_glow = state->green_glow;
    auto phase = make_shared<float>(0.0f);
    auto ticks = make_shared<int>(0);
    glow_timer->on_timeout([=]() {
        if (!*state->alive) { glow_timer->cancel(); return; }
        auto g = w_glow.lock();
        if (!g) { glow_timer->cancel(); return; }
        *phase += 0.12f;   // 0.12 rad/tick * 20 ticks/s -> ~2.6 s per breath
        g->set_intensity(0.12f + 0.88f * (0.5f - 0.5f * std::cos(*phase)));
        if (hold_ticks > 0 && ++*ticks >= hold_ticks) {
            glow_timer->cancel();
            if (on_done) on_done();
        }
    });
    if (!state->freeze || std::getenv("EGT_FLASH_TEST"))
        glow_timer->start();
    else if (auto g = state->green_glow)
        g->set_intensity(0.85f);
}

// ── TREATMENT NEARLY DONE SCREEN ───────────────────────────────────────────
// Figma 67:578: the last few seconds of a cycle are signalled as an ALERT - a
// breathing green glow around the otherwise-normal cycle screen. The overall
// progress dots keep filling (they are NOT replaced by a draining bar) so the
// indicator never runs backwards.
static void show_treatment_nearly_done(shared_ptr<TreatmentState> state, int remaining_seconds)
{
    auto [container, cum_time_lbl] = make_treatment_container(state, true, true);

    auto remaining = make_shared<int>(remaining_seconds);

    // Large countdown — thin/regular, dark (the frame carries the green cue)
    auto countdown_label = make_shared<Label>(
        to_string(*remaining),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(Font(116, Font::Weight::normal));
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text — dark (same size as the Active "Treatment started" line)
    auto status = make_shared<Label>("Treatment nearly finished",
        Rect(0, STATUS_Y, dt::SCREEN_W, 32));
    status->font(Font(28, Font::Weight::normal));
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Same overall progress dots as the active screen — they keep advancing
    // with cumulative time so the indicator never reverses during the alert.
    auto seg_bar = add_segmented_progress(container, state);

    // Buttons — standard outlined (dark text on white bg)
    auto timer_ref = make_shared<shared_ptr<PeriodicTimer>>(nullptr);

    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            if (state->flash_timer) state->flash_timer->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            if (state->flash_timer) state->flash_timer->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    start_green_breathing(state);   // breathes until Pause/End or cycle change

    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    *timer_ref = timer;
    state->active_timer = timer;

    weak_ptr<Label> w_countdown = countdown_label;
    weak_ptr<Label> w_cum_time  = cum_time_lbl;
    weak_ptr<Frame> w_seg_bar   = seg_bar;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock())
            lb->text(to_string(*remaining));
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));
        // Progress dots keep filling with cumulative time (no reversal)
        if (auto sb = w_seg_bar.lock())
            ui::update_segmented_progress_fraction(sb, treatment_progress(state));

        // Process-limit alerts (tone + LED) can still cross here
        check_and_fire_warnings(state);

        // Treatment target reached?
        if (state->is_complete()) {
            timer->cancel();
            if (state->flash_timer) state->flash_timer->cancel();
            state->cycles_completed++;
            show_treatment_zero(state);   // "0 + check" screen, then auto-advance
            return;
        }

        // Cycle ended (multi-cycle scenario)
        if (*remaining <= 0) {
            timer->cancel();
            if (state->flash_timer) state->flash_timer->cancel();
            state->cycles_completed++;
            state->current_cycle++;
            show_position_tip(state);
        }
    });
    if (!state->freeze) timer->start();
}

// ── TREATMENT "0" SCREEN ────────────────────────────────────────────────────
// Figma 67:628: the moment the countdown reaches 0. Same breathing-glow screen
// as nearly-finished but with a big "0", a full green progress bar and a green
// check. Per the 67:728 note, it breathes + holds ~3 s to signal completion,
// then auto-advances to the Completed (Back to Home) screen.
static void show_treatment_zero(shared_ptr<TreatmentState> state)
{
    auto [container, _cz] = make_treatment_container(state, true, /*green=*/true);

    // Big "0" — Figma Group 42 (64pt regular, thin).
    auto zero = make_shared<Label>("0",
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    zero->font(Font(116, Font::Weight::normal));
    zero->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(zero);

    // Full green progress bar (treatment_progress == 1.0 at completion).
    add_segmented_progress(container, state);

    // Green circle check — Figma Group 175 35x35 @(196,176) -> 65x65 @(363,326).
    // Downloaded PNG, never drawn.
    const int chk = 65, chk_x = 363, chk_y = 326;
    auto chk_wrap = make_shared<Frame>(Rect(chk_x, chk_y, chk, chk));
    chk_wrap->fill_flags({});
    try {
        const std::string path = "assets/figma/images/treatment-check-green.png";
        auto probe = Image(("file:" + path).c_str());
        const float hs = static_cast<float>(chk) / probe.width();
        const float vs = static_cast<float>(chk) / probe.height();
        auto img = Image(("file:" + path).c_str(), hs, vs);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, chk, chk));
        chk_wrap->add(lbl);
    } catch (const std::exception& e) {
        printf("[TREATMENT] check icon missing: %s\n", e.what()); fflush(stdout);
    }
    container->add(chk_wrap);

    state->callbacks.on_show_screen(container);

    // Breathe, hold ~3 s (60 ticks @ 50 ms), then auto-advance to Completed.
    start_green_breathing(state, /*hold_ticks=*/60, [=]() {
        show_treatment_completed(state, false);
    });
}

// ── TREATMENT PAUSED SCREEN ────────────────────────────────────────────────
// Figma Group 160: Logo, DEMO MODE, cumulative time (header), large paused time,
// status text, tip message, Resume/End buttons. No segmented dots.
static void show_treatment_paused(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl3] = make_treatment_container(state, true);
    state->is_paused = true;

    // Large paused time display (Figma: thin/regular, 64pt * SCALE)
    auto time_display = make_shared<Label>(
        TreatmentState::format_time(state->cumulative_seconds),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    time_display->font(Font(116, Font::Weight::normal));
    time_display->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(time_display);

    // "Treatment Paused" text below (Figma 67:777: y=130, 16pt -> 30)
    auto status = make_shared<Label>("Treatment Paused",
        Rect(0, STATUS_Y - 6, dt::SCREEN_W, 38));
    status->font(Font(30, Font::Weight::normal));
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Tip message (Figma 67:773): two centred cyan lines, flanked by cyan
    // chevrons left and right.
    const int tip_y = STATUS_Y + 34;
    auto tip1 = make_shared<Label>(
        "Tip : Keep pauses short to quickly rewarm and get back to",
        Rect(0, tip_y, dt::SCREEN_W, 26));
    tip1->font(Font(18, Font::Weight::bold));
    tip1->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(tip1);

    auto tip2 = make_shared<Label>(
        "treatment faster - every second counts!",
        Rect(0, tip_y + 26, dt::SCREEN_W, 26));
    tip2->font(Font(18, Font::Weight::bold));
    tip2->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(tip2);

    const int chev_sz = 46;
    auto chevL = make_shared<ImageLabel>(load_chevron(true, chev_sz));
    chevL->autoresize(false); chevL->border(0); chevL->fill_flags({});
    chevL->image_align(AlignFlag::center);
    chevL->box(Rect(96, tip_y + 6, chev_sz, chev_sz));
    container->add(chevL);

    auto chevR = make_shared<ImageLabel>(load_chevron(false, chev_sz));
    chevR->autoresize(false); chevR->border(0); chevR->fill_flags({});
    chevR->image_align(AlignFlag::center);
    chevR->box(Rect(dt::SCREEN_W - 96 - chev_sz, tip_y + 6, chev_sz, chev_sz));
    container->add(chevR);

    // Resume / End buttons (Figma: y=198, Resume=filled left, End=outlined right)
    auto btn_resume = make_action_button("Resume", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_CYAN_FILLED,
        [=]() {
            state->is_paused = false;
            // resuming=true → after the re-warm we jump straight back into
            // the active treatment via position-tip (skip Ready/Begin).
            show_warming(state, /*resuming=*/true);
        });
    container->add(btn_resume);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);
}

// ── END CONFIRMATION DIALOG ────────────────────────────────────────────────
// Figma Group 51: Logo, DEMO MODE, card with "End Treatment" title + question,
// Cancel/End buttons at bottom. No cumulative, no dots.
static void show_end_confirmation(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl4] = make_treatment_container(state, false);

    // Card frame (Figma 81:389 Rectangle 38 @ x=94,y=53, 232x116 -> device
    // ~430x213). White fill with a faint light-gray rounded border (the Figma
    // 217->255 gradient stroke; EGT can't gradient a border so a solid
    // #D9D9D9 1px approximates it).
    const int card_w = 430;
    const int card_h = 213;
    const int card_x = (dt::SCREEN_W - card_w) / 2;
    const int card_y = 97;
    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->color(Palette::ColorId::border, Color(0xD9, 0xD9, 0xD9));
    card->border(1);
    card->border_radius(30);   // Figma 16px * SCALE
    container->add(card);

    // "End Treatment" title (Figma 81:397: 16pt bold, cool-blue gradient ->
    // solid blue midpoint), centred near the top of the card.
    const Color title_blue(48, 129, 196);
    auto title = make_shared<Label>("End Treatment",
        Rect(0, 44, card_w, 38), AlignFlag::center);
    title->font(Font(30, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, title_blue);
    card->add(title);

    // Question text (Figma 81:393: 16pt gray, two centred lines).
    auto question = make_shared<Label>(
        "Are you sure you want to\nend treatment?",
        Rect(0, 92, card_w, 80), AlignFlag::center);
    question->font(Font(30, Font::Weight::normal));
    question->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(question);

    // Cancel (left, outlined) / End Treatment (right, filled)
    // Figma: Cancel at left (21,194), End at right (297,194)
    auto btn_cancel = ui::create_outlined_button("Cancel",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        [=]() {
            if (state->is_paused)
                show_treatment_paused(state);
            else
                show_treatment_active(state);
        });
    container->add(btn_cancel);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_CYAN_FILLED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_completed(state, true);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);
}

// "Back to Home" button (Figma "bt new home"): cyan card, white house icon on
// the left, two-line "Back to / Home" text. Centred at the standard button row.
static shared_ptr<Frame> make_back_home_button(function<void()> on_click)
{
    // Figma "bt new home": 124x54 -> 230x100. House icon 25x19 -> ~47 wide,
    // "Back to" font 11 -> 20, "Home" font 18 -> 33 (two sizes, not one).
    const int home_w = 230, home_h = BTN_H;
    const int home_x = (dt::SCREEN_W - home_w) / 2;

    // Wrapper so the soft drop shadow has room to render outside the card
    // (Figma "bt new home" effect 0 2 2 rgba(0,0,0,0.2)). Same treatment as
    // the Pause/End buttons (make_action_button) so all treatment buttons sit
    // on a consistent shadow.
    constexpr int PAD = 10;
    auto wrap = make_shared<Frame>(
        Rect(home_x - PAD, BTN_Y - PAD, home_w + 2 * PAD, home_h + 2 * PAD));
    wrap->fill_flags({});
    // Tight Figma shadow (0 2px 1px rgba(0,0,0,0.2) -> device ~0 4px 2px),
    // same two-layer treatment as make_action_button.
    for (int i = 1; i >= 0; --i) {
        auto sh = make_shared<Frame>(
            Rect(PAD - i, PAD + 3 + i, home_w + 2 * i, home_h + 2 * i));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, 18));
        sh->border_radius(dt::RADIUS_MD + i);
        sh->border(0);
        wrap->add(sh);
    }

    auto btn_home = make_shared<Frame>(Rect(PAD, PAD, home_w, home_h));
    btn_home->fill_flags({Theme::FillFlag::blend});
    btn_home->color(Palette::ColorId::bg, dt::kAccentCyan);
    btn_home->color(Palette::ColorId::border, dt::kAccentCyan);
    btn_home->border(0);
    btn_home->border_radius(dt::RADIUS_MD);

    const int icon_sz2 = 50;          // was 40 - matches the bigger Figma house
    auto home_icon = load_home_icon(icon_sz2);
    if (!home_icon.empty()) {
        auto hi = make_shared<ImageLabel>(home_icon);
        hi->fill_flags({});
        hi->color(Palette::ColorId::bg, dt::kAccentCyan);
        hi->image_align(AlignFlag::center);
        hi->move(Point(33, (home_h - icon_sz2) / 2));
        hi->resize(Size(icon_sz2, icon_sz2));
        btn_home->add(hi);
    }

    const int txt_x = 100, txt_w = 120;
    auto back_lbl = make_shared<Label>("Back to",
        Rect(txt_x, 18, txt_w, 34), AlignFlag::left | AlignFlag::center_vertical);
    back_lbl->font(Font(20, Font::Weight::bold));
    back_lbl->color(Palette::ColorId::label_text, dt::kWhite);
    btn_home->add(back_lbl);

    auto home_lbl = make_shared<Label>("Home",
        Rect(txt_x, 48, txt_w, 40), AlignFlag::left | AlignFlag::center_vertical);
    home_lbl->font(Font(33, Font::Weight::bold));
    home_lbl->color(Palette::ColorId::label_text, dt::kWhite);
    btn_home->add(home_lbl);

    btn_home->on_event([on_click](Event& e) {
        if (e.id() == EventId::pointer_click) on_click();
    }, {EventId::pointer_click});
    wrap->add(btn_home);
    return wrap;
}

// ── TREATMENT COMPLETED / ENDED SCREEN ─────────────────────────────────────
// Both screens share one layout (Figma 75:320 "Treatment Completed" and
// 81:406 "Treatment Ended"): cumulative header, a centred [blue check + title]
// row, and a cyan "Back to Home" button. They differ only in the title text
// and behaviour — the ended screen auto-returns to Home after a short delay
// (client note 70:886: return home after a pre-set 15-30 s), while the
// completed screen waits for the technician to tap.
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early)
{
    auto [container, _cdone] = make_treatment_container(state, true);

    // Figma "cool blue" text/ring is a cyan->blue gradient; EGT can't
    // gradient-fill text, so use the gradient midpoint as a solid blue.
    const Color complete_blue(48, 129, 196);

    const string title = early ? "Treatment Ended" : "Treatment Completed";
    // Measured rendered width of each title @37pt bold, so the [check + gap +
    // title] block centres on the real text (a fixed box left it off-centre).
    const int title_w = early ? 320 : 398;

    // Centred check + title row (Figma Group 211 @ y=106 -> 196).
    const int row_y = 196;
    const int chk = 65, gap = 16;
    const int group_w = chk + gap + title_w;
    const int group_x = (dt::SCREEN_W - group_w) / 2;

    // Blue check (Figma Group 175, 35x35 -> 65x65). Downloaded PNG, not drawn.
    auto chk_wrap = make_shared<Frame>(Rect(group_x, row_y, chk, chk));
    chk_wrap->fill_flags({});
    try {
        const std::string path = "assets/figma/images/treatment-check-blue.png";
        auto probe = Image(("file:" + path).c_str());
        const float hs = static_cast<float>(chk) / probe.width();
        const float vs = static_cast<float>(chk) / probe.height();
        auto lbl = make_shared<ImageLabel>(
            Image(("file:" + path).c_str(), hs, vs));
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, chk, chk));
        chk_wrap->add(lbl);
    } catch (const std::exception& e) {
        printf("[TREATMENT] check icon missing: %s\n", e.what()); fflush(stdout);
    }
    container->add(chk_wrap);

    auto title_lbl = make_shared<Label>(title,
        Rect(group_x + chk + gap, row_y - 6, title_w, chk),
        AlignFlag::center);
    title_lbl->font(Font(37, Font::Weight::bold));
    title_lbl->color(Palette::ColorId::label_text, complete_blue);
    container->add(title_lbl);

    container->add(make_back_home_button([=]() {
        if (state->active_timer) state->active_timer->cancel();
        if (early) { if (state->callbacks.on_treatment_ended_early)
                         state->callbacks.on_treatment_ended_early(); }
        else       { if (state->callbacks.on_treatment_completed)
                         state->callbacks.on_treatment_completed(); }
    }));

    state->callbacks.on_show_screen(container);

    // Ended screen auto-returns to Home after AUTO_RETURN_SECONDS (client note
    // 70:886). on_treatment_ended_early is already guarded by wrap_exit, so a
    // tap before the timer fires (or vice-versa) only triggers the exit once.
    if (early && !state->freeze) {
        constexpr int AUTO_RETURN_SECONDS = 20;   // within the client's 15-30 s
        auto timer = make_shared<PeriodicTimer>(
            chrono::seconds(AUTO_RETURN_SECONDS));
        state->active_timer = timer;
        timer->on_timeout([=]() {
            timer->cancel();
            printf("[TREATMENT] ended screen auto-return to home\n"); fflush(stdout);
            if (state->callbacks.on_treatment_ended_early)
                state->callbacks.on_treatment_ended_early();
        });
        timer->start();
    }
}
