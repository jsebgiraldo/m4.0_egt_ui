# EGT widget reference

Notes on EGT v1.12.1 widget construction, positioning, and alignment, gathered from local headers, example projects, and upstream docs. Tailored to this project's "Figma node -> exact pixel position" workflow.

Source files this is based on:

- Local headers: `/usr/local/include/egt/{widget,frame,label,button,imageholder,image,widgetflags,theme,painter,geometry,sizer}.h` and `egt/detail/alignment.h`.
- Upstream docs: `https://linux4sam.github.io/egt-docs/` (saved per-page summaries in `upstream/`).
- Examples: `/opt/egt/examples/{imagebutton,widgets,basicui,...}`.
- This project's working screens: `src/screens/screen_wifi_init.cpp`, `screen_home.cpp`, `screen_wifi_connected.cpp`, `src/ui/components.cpp`.

## Coordinate model

EGT uses **local-to-parent** coordinates for every child widget. The quote from `frame.h`:

> "Child widget coordinates have an origin at the top left of their parent frame. In other words, child widgets are drawn respective to and inside of their parent frame."

So:

- A child added to a `Frame` at `Rect(0, 0, 800, 480)` (a screen-sized container) uses display-aligned coords by coincidence, because the container's origin is `(0, 0)`.
- A child added to a wrapper `Frame` at `Rect(302, 355, 204, 61)` uses **local** coords. Writing `Rect(0, 0, 204, 61)` means "fill the wrapper", not "top-left of the screen".

Helper conversions:

| Call | Direction |
|---|---|
| `widget->local_to_display(Point)` | local -> screen |
| `widget->display_to_local(DisplayPoint)` | screen -> local |
| `widget->to_parent(Point)` | local -> parent local |
| `frame->to_child(Point)` | parent local -> child local |

### `box()` vs `content_area()`

`box()` is the full rectangle the widget occupies in parent-local coords, including margin + border + padding.

`content_area()` returns `box()` shrunk by `moat() = margin + padding + border`. It is in the **widget's own local space** (origin at the widget's top-left), so if you call it from inside `Widget::draw()` and pass the resulting point into a `Painter` call, no further translation is needed.

`local_box()` is `Rect(Point{0,0}, size())` - same as content_area but without subtracting moat.

For most widgets in this project the moat is small (border 2 px on outlined buttons, 0 elsewhere), but it is **non-zero** for any widget with a border, and that matters once you start nesting.

## Widget construction and placement

### Setting position + size

Three equivalent ways:

```cpp
auto w = std::make_shared<Label>("hello", Rect(100, 50, 200, 40));   // ctor

auto w = std::make_shared<Label>("hello");
w->move(Point(100, 50));
w->resize(Size(200, 40));

auto w = std::make_shared<Label>("hello");
w->box(Rect(100, 50, 200, 40));     // sugar for move + resize
```

From `widget.h`:

```cpp
// Change the bounding box of the widget.
// This is the same as calling move() and resize() at the same time.
void box(const Rect& rect);
```

There is a subtle difference: `move(Point)` and `resize(Size)` both update `m_user_requested_box` via the `x()/y()/width()/height()` setters **only if** the widget is not currently inside a layout pass and the parent is not laying it out. The constructor `Widget(const Rect&)` sets `m_box` directly. For free positioning in a plain `Frame`, all three are equivalent in practice.

### autoresize and the "I set Rect, widget moved" trap

Every widget has `autoresize() == true` by default. The header for this flag (`Widget::Flag::no_autoresize`) says:

> "Do not automatically resize a widget to meet the minimal size hint."

For most widgets `min_size_hint()` is just a recommended floor, but for `ImageLabel` / `ImageButton` it derives from the image's pixel size (plus moat). On a layout pass triggered by `show()`, a font change, or a parent layout, the widget can be re-sized back to its hint, overriding the rect you set.

Defence in depth:

1. Pass an explicit non-empty `Rect` in the constructor (skips the `do_set_image` auto-resize).
2. Call `widget->autoresize(false);` if the widget will be in a parent that may run layout.
3. Don't put it inside a Sizer if you want absolute pixel placement.

### Plain Frame does **not** honour `widget->align()`

`Widget::align(AlignFlags)` is consumed by **Sizer** subclasses (`HorizontalBoxSizer`, `VerticalBoxSizer`, `FlexBoxSizer`, `StaticGrid`, `ScrolledView`) which call `detail::align_algorithm()` during their `layout()`. A plain `Frame::layout()` does **not** reposition children based on `align()`. This project uses plain `Frame` everywhere, so writing `widget->align(AlignFlag::center)` has no effect.

If you want centring in a Frame, compute the pixel position yourself.

## ImageLabel / Image widget

`ImageLabel` is `using ImageLabel = ImageHolder<Label, ...>` - everything below applies equally to `ImageButton`.

### Constructor auto-resize

`ImageHolder::do_set_image()`:

```cpp
if (this->size().empty() && !image.empty())
    this->resize(image.size() + Size(this->moat() * 2, this->moat() * 2));
```

Translation: if you build the widget with no explicit rect and pass it an image, it self-resizes to `image.size() + 2*moat`. The image's `size()` is `original * (hscale, vscale)`, so pre-scaling controls the final box.

### `image_align` semantics

`image_align()` defaults to `AlignFlag::left | AlignFlag::expand`. It is consumed by `ImageHolder::default_draw()`:

```cpp
auto target = widget.image().align(widget.content_area(), widget.image_align());
painter.draw(widget.image(), target.point());
```

So:

- `AlignFlag::center` -> image is placed inside `content_area()` and centred, at its native (post-scale) size.
- `AlignFlag::center | AlignFlag::expand` -> image is scaled to fill `content_area()`, preserving ratio (default), then centred.
- `AlignFlag::left` -> top-left of image at top-left of `content_area()`.

When there is no text (empty string in the constructor or `show_label(false)`), the image is aligned inside the **widget's content area** rather than relative to text.

### Pre-scaling at load time

```cpp
const float hscale = static_cast<float>(target_w) / src_w;
const float vscale = static_cast<float>(target_h) / src_h;
Image img("file:assets/figma/images/x.png", hscale, vscale);
```

After this, `img.size() == (target_w, target_h)`. Using `image_align(AlignFlag::center)` (no `expand`) then draws the image at native size centred in the widget's content area - no further scaling happens at draw time, and `min_size_hint()` reflects the scaled size.

### Disabling auto behaviours

- `widget->auto_scale_image(false)` -> clears the `expand` bit, image is drawn at its native size.
- `widget->autoresize(false)` -> the widget's box won't grow back to `min_size_hint()` during layout.
- `widget->keep_image_ratio(false)` -> with `expand`, the image stretches non-uniformly.

## Button

`Button` inherits from `TextWidget`. Key facts:

- `border()` defaults from the theme (`Theme::default_border() == 2`). Border is drawn **inside** the widget's box() (the border stroke is at the inner edge of the box, and content_area is the box minus border + padding + margin). So a `Button` at `Rect(302, 355, 204, 61)` with border 2 has its visible outline at exactly that rect; the content_area for text is `(302+2, 355+2, 200, 57)` (plus any extra padding).
- `border_radius(float)` sets corner rounding.
- `text_align(AlignFlags)` works the same as `Label`. Default is centred.
- Button does **not** apply hidden padding that pushes the outline outside the rect. The rect you pass is the visible bounding box.

### Adding a child icon

`Button` is a `TextWidget`, not a `Frame`. You cannot `button->add(other_widget)`. To overlay an icon, you have three options:

1. Use `ImageButton` (image + text in the same widget, `image_align` controls position relative to text).
2. Wrap the Button and the icon in a parent `Frame` and position both inside. **This is what this project does** (see `screen_wifi_connected.cpp`).
3. Subclass `Button` and override `draw()` to also blit the image.

## Frame

`Frame` is the only Widget that natively contains children. It:

- Owns children via `std::shared_ptr` (or non-owning via the `Widget&` overload).
- Draws children in z-order = insertion order (first added is bottom).
- Does **not** reposition children unless they have ratios set (xratio/yratio) or it is a Sizer subclass.
- Clips children to its box by default (`Widget::Flag::no_clip` turns this off).

A screen in this project is always a `Frame(Rect(0, 0, SCREEN_W, SCREEN_H))` and children are added at the desired pixel positions.

### When to wrap in a sub-Frame

Use a wrapper Frame when:

- You want two widgets to share a local coordinate space (icon + button).
- You want to move a group together later (just `move()` the wrapper).
- You want clipping to a sub-rect.

The cost is negligible - a Frame with `fill_flags({})` draws nothing of itself.

## AlignFlag values

| Flag | Bitfield slot | Effect when honoured by parent / draw |
|---|---|---|
| `AlignFlag::center_horizontal` | horizontal=1 | Centred horizontally inside bounding box. |
| `AlignFlag::left` | horizontal=2 | Pin to left edge. |
| `AlignFlag::right` | horizontal=3 | Pin to right edge. |
| `AlignFlag::center_vertical` | vertical=1 | Centred vertically. |
| `AlignFlag::top` | vertical=2 | Pin to top edge. |
| `AlignFlag::bottom` | vertical=3 | Pin to bottom edge. |
| `AlignFlag::center` | `center_h | center_v` | Centred both axes. |
| `AlignFlag::expand_horizontal` | expand bit 0 | Stretch to fill horizontal. |
| `AlignFlag::expand_vertical` | expand bit 1 | Stretch to fill vertical. |
| `AlignFlag::expand` | both expand bits | Stretch both axes. |
| `AlignFlag::keep_ratio` | expand bit 2 | Preserve original aspect ratio while expanding. |

Common combinations:

- `AlignFlag::center | AlignFlag::expand` -> stretch-to-fit and centre.
- `AlignFlag::center` -> centre at native size.
- `AlignFlag::left | AlignFlag::expand` -> default for `ImageHolder` (image on left, stretched).

Reminder: `Widget::align(AlignFlags)` is for Sizers; `ImageHolder::image_align(AlignFlags)` is for the image-inside-widget draw step. Different APIs, same enum.

## The "icon inside Button" pattern

The canonical idiom in this project is **wrap both in a Frame**. Full code in `code-snippets/icon-over-button.cpp`. Summary:

```cpp
const Rect btn_rect(302, 355, 204, 61);
auto wrap = std::make_shared<Frame>(btn_rect);
wrap->fill_flags({});                                       // transparent

auto btn = std::make_shared<Button>("Continue",
                                    Rect(0, 0, btn_rect.width(), btn_rect.height()));
wrap->add(btn);

// Pre-scale image so its natural size matches target - no expand needed.
const float hscale = static_cast<float>(chev_w) / 35.0f;
const float vscale = static_cast<float>(chev_h) / 52.0f;
auto img = Image("file:assets/figma/images/chevron-right.png", hscale, vscale);

auto chevron = std::make_shared<ImageLabel>(img);
chevron->image_align(AlignFlag::center);
chevron->box(Rect(btn_rect.width() - chev_w - 22,
                  (btn_rect.height() - chev_h) / 2,
                  chev_w, chev_h));
wrap->add(chevron);
```

Why this works:

1. The wrapper Frame fixes a known local origin, so the chevron's coords are always relative to the button.
2. The chevron is constructed with an image that is already pre-scaled to the target pixel size, so the auto-resize-on-set path produces the size we want.
3. `box(Rect)` is called once with the final position; with `image_align(AlignFlag::center)` (no `expand`), draw() does not re-scale the image and the layout pass does not re-position the widget.

Other valid options for special cases:

- `ImageButton` if the icon is the button's primary visual content.
- A custom Widget subclass overriding `draw()` to blit both the box and the icon. Use this for one-off shapes (see `CheckCircle` in `screen_wifi_connected.cpp` and `custom-draw-widget.cpp`).

## The 30-px offset bug

### Setup

```cpp
auto btn = std::make_shared<Button>("Continue", Rect(302, 355, 204, 61));
container->add(btn);

auto chev_img = Image("file:assets/figma/images/chevron-right.png");   // NOT pre-scaled
auto chevron = std::make_shared<ImageLabel>(chev_img);
chevron->box(Rect(462, 372, 22, 39));
container->add(chevron);
```

Observed: chevron renders ~30 px higher than `y=372`. Button outline renders correctly at `y=355`.

### Candidate explanations

**A. `image_align` default is `left | expand`.** With `expand`, `ImageHolder::default_draw()` calls `image.align(content_area(), AlignFlag::left | AlignFlag::expand)`. The image gets scaled to fit `content_area()` while keeping ratio. The PNG is 35x52 (ratio 0.673). The widget's content area is 22x39 (ratio 0.564, taller than the image). Keeping ratio, the image is scaled to `22 x (22/0.673) = 22 x 32.7`, which fits horizontally. It is then "left-aligned", meaning **top-left of the content_area**, so the actual blit is at the top of the box, not centred. This shifts the visible chevron up by `(39 - 32.7) / 2 = 3.2 px`. Not 30 px on its own, but it stacks with B.

**B. `ImageHolder::do_set_image()` auto-resize.** Because `chev_img` is **not** pre-scaled, it has its natural size 35x52. `do_set_image` runs once at construction with `size().empty() == true` and **resizes the widget to `35 + 2*moat` x `52 + 2*moat`**. We then call `box(Rect(462, 372, 22, 39))` which overrides this. But the subsequent layout pass triggered by `container->add()` -> parent_layout -> `min_size_hint()` will see `min_size_hint == 52 + 2*moat = ~56 px tall` (because the image-size contribution is bigger than the user-requested size with `autoresize == true`), and **grows the widget back to fit the image**. The widget now sits at `y = 372` with `height ~= 56`, but visually the image is anchored to its content_area top -> the user perceives "icon rendered ~30 px above the button outline" because the widget itself moved up... actually it didn't move, it grew downward, but the **image inside it** is drawn from the **top** of the widget, so the visible icon is centred around `y = 372 + 26 = 398`, much higher than expected. (The 30 px is approximate; the exact number depends on theme moat and font metrics.)

**C. `min_size_hint()` is consulted during `Frame::add()` and the wrong `y` is used in a re-layout.** Less likely root cause, but if a `screen->show()` happens after `add` and the parent re-runs layout with the user-requested rect, that rect is the **first** one set, not the final one. With `autoresize=true`, layout consults `min_size_hint()` and may collapse-then-grow the widget asymmetrically (anchored from the box origin), pushing the visible content upward relative to the user-requested box. This is the same family of bug as B but a different trigger path.

**D. Sibling z-order or invisible padding from the Theme.** `Theme::default_border() == 2` is set on Buttons but not Labels/ImageLabels by default, and z-order is insertion-order. Neither of these would cause a 30 px y-shift, but they can shift things by a few px once an ImageLabel is between two outlined widgets. Probably not the primary cause here.

### Most likely root cause

**B (auto-resize via `do_set_image` + layout-time re-growth from `min_size_hint`) compounded by A (`expand` default scaling the image inside an expanded content_area).** The chevron widget is silently bigger than you asked for, and the image inside it is left-aligned by default. The user sees the image at the top of the actually-larger widget, which sits above where the user-requested box would have sat if respected.

### The fix (already applied in `src/screens/screen_wifi_connected.cpp`)

Three changes any one of which would help, all three for belt-and-braces:

1. **Wrap the button + chevron in a sub-Frame.** Both share the wrapper's local coord space, and the wrapper itself is not subject to image-driven auto-resize.
2. **Pre-scale the image at load time** (`Image(uri, hscale, vscale)`) so its natural size equals the on-screen target size. `min_size_hint()` then matches the requested rect and the layout pass is a no-op.
3. **Set `image_align(AlignFlag::center)`** (no `expand`) so the image draws at native size centred inside the content_area, with no scale step.

For maximum safety also call `chevron->autoresize(false);` on any free-floating ImageLabel.

## References

- Local headers: `/usr/local/include/egt/{widget,frame,label,button,imageholder,image,widgetflags,theme,painter,geometry,sizer}.h`, `egt/detail/alignment.h`.
- Upstream API docs (summaries in `upstream/`):
  - https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Widget.html
  - https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Frame.html
  - https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1ImageHolder.html
  - https://linux4sam.github.io/egt-docs/classegt_1_1v1_1_1Painter.html
  - https://linux4sam.github.io/egt-docs/group__alignment.html
- Examples: `/opt/egt/examples/imagebutton/imagebutton.cpp`, `/opt/egt/examples/widgets/widgets.cpp`, `/opt/egt/examples/basicui/`.
- Working screens in this project: `src/screens/screen_wifi_init.cpp`, `src/screens/screen_home.cpp`, `src/screens/screen_wifi_connected.cpp`, `src/ui/components.cpp`.
- Code snippets in `code-snippets/`: `icon-over-button.cpp`, `absolute-positioning.cpp`, `imagebutton-from-upstream.cpp`, `custom-draw-widget.cpp`.
