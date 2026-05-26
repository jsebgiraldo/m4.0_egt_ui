# Frame (upstream notes)

Source: https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Frame.html
Also: `/usr/local/include/egt/frame.h` (v1.12.1).

## Coordinate system for children

Child widget coordinates have an origin at the **top left of the parent frame**, not at the display origin. Quote from the header:

> "Child widget coordinates have an origin at the top left of their parent frame. In other words, child widgets are drawn respective to and inside of their parent frame."

This is critical for the Figma -> pixel workflow: if you wrap a group of widgets in a sub-Frame at `Rect(302, 355, 204, 61)`, every child inside that wrapper uses `(0, 0)` as the top-left of the wrapper, not `(302, 355)`.

## add() and ownership

`Frame::add(std::shared_ptr<Widget>)` takes ownership through the shared pointer. There is also an overload that takes `Widget&` and stores a non-owning shared pointer (deleter is a no-op). The deleter trick makes the API uniform but does not change lifetime semantics.

## Z-order

> "First in is bottom, or zorder 0."

So calling `frame->add(btn); frame->add(chevron);` draws the chevron on top of the button. `add_at(widget, pos)` lets you pin a specific z-index. `zorder_top()`, `zorder_bottom()`, `zorder_up()`, `zorder_down()` are also available.

## Clipping

Children are clipped to the parent frame's box by default. The `Widget::Flag::no_clip` flag turns this off ("Use this with caution, it's probably not what you want.").

## Layout

`Frame::show()` calls `layout()`. The default layout for Frame walks children but does not move them unless an alignment / ratio / autoresize hint forces a change. With autoresize on (default) and an `ImageLabel`, the widget can be resized at layout time to its `min_size_hint()` even after you set an explicit `box(Rect)`.

## to_child / to_subordinate

`to_child(Point)` translates a point in this frame's local coords into the child's local origin (subtracts the child's position). Use when you need to map a hit-test display point into a child's local space.

## What the docs are missing

- Behaviour of `move()` / `resize()` versus `box()` when the widget has `autoresize` enabled (see `egt-widget-reference.md` §"The 30-px offset bug").
- Order of operations between `box()` set in the constructor and `image()` being assigned later via `do_set_image()` (matters for `ImageHolder`).
