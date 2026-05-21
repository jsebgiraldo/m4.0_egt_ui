# Painter (upstream notes)

Source: https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Painter.html
Local header: `/usr/local/include/egt/painter.h`.

## Role

Painter wraps the cairo-style 2D drawing surface used during `Widget::draw(Painter&, const Rect&)`. You almost never construct one yourself - the event loop hands a Painter in default state to the widget's draw method, and that's where you call shape/text/image methods.

When painting a custom widget (like `CheckCircle` in `src/screens/screen_wifi_connected.cpp`), the coordinate space of the painter is the **widget's local coordinate space**, with origin at the widget's `box().point()`. So you call `content_area().center()` and pass the resulting point into `Arc(...)` without further translation.

## Common entry points used in this project

- `painter.set(Color)` -> source colour.
- `painter.line_width(float)` -> stroke width.
- `painter.draw(Arc{center, radius, start, end})`, `painter.draw(Line{a, b})`, `painter.draw(Rect{...})`.
- `painter.stroke()` / `painter.fill()` -> commit.
- `painter.draw(Image, Point)` -> blit an image at a point. The point is in widget-local coords.

## What it does NOT do

`painter.draw(Image, Point)` does **not** scale the image. To scale, pre-scale the `Image` instance via constructor (`Image(uri, hscale, vscale)`) or `Image::scale(...)`.

`painter.draw(Image, Point)` also doesn't align - it just blits at the point you pass. If you want centred-in-box semantics, call `image.align(rect, AlignFlag::center)` first and use the returned rect's `.point()`. This is what `ImageHolder::default_draw()` does:

```cpp
auto target = widget.image().align(widget.content_area(), widget.image_align());
painter.draw(widget.image(), target.point());
```

## Notes for the Figma workflow

The `draw()` Rect parameter is the **damage rect**, in this widget's coords. It is allowed to be smaller than `box()`. Many custom widgets just ignore it and redraw the whole content; that's fine for small widgets but wasteful for large ones.

## What the docs are missing

- The painter is not transformed by `Widget::box().point()`; it's already transformed by the parent's draw pipeline. Don't add the widget's `x()` / `y()` into shapes you draw inside `Widget::draw()`.
