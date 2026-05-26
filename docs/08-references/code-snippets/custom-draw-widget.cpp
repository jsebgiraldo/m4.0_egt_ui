// custom-draw-widget.cpp
//
// When the built-in widgets don't give you the exact layout you want,
// override draw() on a plain Widget. Painter is already transformed into
// the widget's local coordinate space.
//
// Pulled from src/screens/screen_wifi_connected.cpp (CheckCircle).
//
// This is the most reliable way to render exactly at the pixels Figma
// specified: no auto-resize, no image_align, no layout passes touching
// the position. The widget draws itself into its box() and that's it.

#include <egt/ui>
#include <cmath>

using namespace egt;

class CheckCircle : public Widget
{
public:
    explicit CheckCircle(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*damage*/) override
    {
        // content_area() == box() minus moat (margin + padding + border).
        // It is in this widget's LOCAL coord space (origin at top-left
        // of the widget). So center() gives the local centre point.
        auto b = content_area();
        auto dim = static_cast<float>(std::min(b.width(), b.height()));
        auto centre = b.center();
        const float radius = dim / 2.0f - 2.0f;

        painter.line_width(3.5f);
        painter.set(Color(0xFF00C853));   // green
        painter.draw(Arc(centre, radius, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.stroke();

        // Check mark, two line segments, coordinates are still LOCAL.
        const float cx = centre.x();
        const float cy = centre.y();
        const float r  = dim * 0.30f;
        const Point p1(cx - r,         cy + r * 0.05f);
        const Point p2(cx - r * 0.30f, cy + r * 0.55f);
        const Point p3(cx + r * 0.95f, cy - r * 0.55f);

        painter.line_width(4.5f);
        painter.draw(Line(p1, p2));
        painter.stroke();
        painter.draw(Line(p2, p3));
        painter.stroke();
    }
};
