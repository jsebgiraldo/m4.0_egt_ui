# AlignFlag, AlignFlags, image_align (upstream notes)

Sources:
- https://linux4sam.github.io/egt-docs/group__alignment.html
- Local header: `/usr/local/include/egt/widgetflags.h`.

## What an AlignFlag actually is

`AlignFlag` is a tagged bitfield with three slots:
- horizontal: 2 bits at offset 0 -> `center_horizontal=1`, `left=2`, `right=3`.
- vertical: 2 bits at offset 2 -> `center_vertical=1`, `top=2`, `bottom=3`.
- expand: bits at offset 4+ -> `expand_horizontal`, `expand_vertical`, `expand = both`, `keep_ratio`.

`center` is just `center_horizontal | center_vertical`. So `AlignFlag::center` is a *single* HV value, not a magic flag.

## Table of meanings

| Flag | Effect |
|---|---|
| `AlignFlag::left` | Pin to the left edge of the bounding box (uses widget padding as inset). |
| `AlignFlag::right` | Pin to the right edge. |
| `AlignFlag::center_horizontal` | Centred horizontally. |
| `AlignFlag::top` | Pin to the top edge. |
| `AlignFlag::bottom` | Pin to the bottom edge. |
| `AlignFlag::center_vertical` | Centred vertically. |
| `AlignFlag::center` | Both centres (== `center_h | center_v`). |
| `AlignFlag::expand_horizontal` | Stretch to fill horizontal axis of the bounding box. |
| `AlignFlag::expand_vertical` | Stretch to fill vertical axis. |
| `AlignFlag::expand` | Both stretches. |
| `AlignFlag::keep_ratio` | When expanding, preserve the original aspect ratio (image only). |

## Widget::align(AlignFlags) vs ImageHolder::image_align(AlignFlags)

These are two separate alignment systems and they get confused all the time.

**`Widget::align(AlignFlags)`** is consumed by **the parent's layout()**, via `detail::align_algorithm(orig, bounding, align, padding, ...)`. It only does anything if:

1. the widget is inside a Sizer / something that performs layout, **and**
2. the widget has not been marked `no_layout(true)`, **and**
3. the parent calls `layout()` after the widget's `align()` is set.

A plain `Frame` does **not** call `align_algorithm` on its children, so setting `Widget::align()` inside a screen-sized Frame in this project does **nothing**. You have to use Sizers (`HorizontalBoxSizer`, `VerticalBoxSizer`, etc.) to make `align()` matter.

**`ImageHolder::image_align(AlignFlags)`** is consumed by `ImageHolder::default_draw()` and aligns the image inside the widget's `content_area()`. This is what the "icon over button" use case actually wants.

## The `align_algorithm` (used by Sizers)

In `egt/detail/alignment.h`:

```cpp
Rect align_algorithm(const Rect& orig,
                     const Rect& bounding,
                     const AlignFlags& align,
                     DefaultDim padding = 0,
                     DefaultDim horizontal_ratio = 0,
                     DefaultDim vertical_ratio = 0,
                     DefaultDim xratio = 0,
                     DefaultDim yratio = 0);
```

This is the function Sizers call. It returns a new rect that places `orig` inside `bounding` according to `align`. Plain `Frame` doesn't call this on its children.

## What the docs are missing

- The fact that `align()` is silently ignored by plain `Frame` is not stated anywhere on the docs page; you have to read the source for `Frame::layout()` and the Sizer subclasses to see it.
