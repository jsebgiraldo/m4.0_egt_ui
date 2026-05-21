# ImageLabel / ImageHolder (upstream notes)

Sources:
- https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1ImageHolder.html
- https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Label.html
- Local header: `/usr/local/include/egt/imageholder.h`, `/usr/local/include/egt/label.h`.

## What ImageLabel is

`ImageLabel` is a `using` alias:

```cpp
using ImageLabel = ImageHolder<Label,
                               Palette::ColorId::label_bg,
                               Palette::ColorId::border,
                               Palette::ColorId::label_text>;
```

So everything described under `ImageHolder` applies to `ImageLabel`. Same for `ImageButton = ImageHolder<Button, ...>`.

## Auto-resize gotcha (the load-bearing one)

`ImageHolder::do_set_image()` (called from the constructor) has this:

```cpp
if (this->size().empty() && !image.empty())
    this->resize(image.size() + Size(this->moat() * 2, this->moat() * 2));
```

In other words: **if you construct an `ImageLabel` without an explicit `Rect`, it auto-resizes itself to the image's natural pixel size + 2*moat.** This is fine when you construct with no rect. It also doesn't fire if you give a non-empty rect.

But when you construct an `ImageLabel` with **no rect** and then call `box(Rect(...))` later, the widget has already self-resized once. The `box(Rect)` call overrides it, but `Widget::autoresize()` is still true by default. A later `layout()` (triggered by `show()`, parent layout, font change, etc.) **can** re-resize the widget back to its `min_size_hint()`, which derives from the image. This is the silent re-layout that causes "I set Rect(...) and the widget moved" bugs.

Defense: call `widget->autoresize(false);` after construction, **or** always pass an explicit non-empty rect to the constructor.

## image_align semantics

`m_image_align` defaults to `AlignFlag::left | AlignFlag::expand`. The header comment on the setter says:

> "Only left, right, top, and bottom alignments are supported."

That comment refers to alignment **relative to the text**. When the label has no text (`show_label(false)`, set automatically if you construct with empty text), the image is aligned inside the widget's `content_area()`:

```cpp
auto target = widget.image().align(widget.content_area(), widget.image_align());
painter.draw(widget.image(), target.point());
```

So:
- `AlignFlag::center` -> image is centred inside `content_area()`.
- `AlignFlag::center | AlignFlag::expand` -> image is scaled to fill the content area (keeping ratio if `keep_image_ratio()` is true, which is the default), then centred.
- `AlignFlag::left` -> image's top-left aligned to content_area's top-left (within the moat).

The `expand` flag is the difference between "draw the image at its native size, positioned via alignment" and "scale it to fit the box". `auto_scale_image(bool)` is the named API that flips the `expand` bit.

## Scaling at load time

`Image(uri, hscale, vscale)` lets you pre-scale the image when loading. This is the cleanest way to control on-screen size. Once loaded with a scale, the image's `size()` returns the scaled size, and ImageHolder uses that for `min_size_hint()` and the auto-resize-on-set behaviour.

## What the docs are missing

- The interaction between `box(Rect)` set after construction and the auto-resize-to-image-size in `do_set_image()`.
- The fact that `min_size_hint()` of an ImageLabel **includes the image size** unless `show_label(false) && auto_scale_image()` is both true, which means an ImageLabel placed in a Sizer / parent that respects min_size_hint will get bigger than the rect you set.
