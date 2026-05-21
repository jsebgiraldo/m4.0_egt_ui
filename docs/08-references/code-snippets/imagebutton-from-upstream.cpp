// imagebutton-from-upstream.cpp
//
// Excerpt from /opt/egt/examples/imagebutton/imagebutton.cpp.
//
// Shows how upstream demonstrates ImageButton: always inside a Sizer
// (VerticalBoxSizer, HorizontalBoxSizer), never as a free-floating widget
// at absolute coordinates. The auto_scale + auto_resize knobs are exposed
// because both interact with the rect you ask for.
//
// Lesson for this project: when you place an ImageButton/ImageLabel at
// fixed absolute coords inside a plain Frame, you are off the documented
// path. Either pre-scale the image so it matches your target size, or
// disable auto_scale_image() and autoresize().

#include <egt/ui>

class TestButton : public egt::VerticalBoxSizer
{
public:
    TestButton(egt::Frame& parent,
               const egt::Image& image,
               const std::string& text,
               const egt::Rect& rect,
               bool auto_scale,
               bool auto_resize)
        : egt::VerticalBoxSizer(parent),
          m_button(*this, image, text, rect)
    {
        // Two knobs that together control whether the rect you asked for
        // is respected. Both default to true.
        m_button.auto_scale_image(auto_scale);   // image fills the box on expand
        m_button.autoresize(auto_resize);        // widget grows to min_size_hint
    }

private:
    egt::ImageButton m_button;
};

// From /opt/egt/examples/widgets/widgets.cpp — image_align used to put the
// image to the right of the button label.
//
//   auto imagebutton4 = std::make_shared<egt::ImageButton>(image, "Calculator");
//   grid0->add(expand(imagebutton4));
//   imagebutton4->text_align(egt::AlignFlag::center_horizontal | egt::AlignFlag::bottom);
//   imagebutton4->image_align(egt::AlignFlag::right);
//
// Note this is the image-relative-to-text alignment. There is no upstream
// example of placing an ImageLabel at an exact pixel offset inside an
// unrelated Button; the idiomatic answer is ImageButton (image + text in one
// widget) or a custom widget that overrides draw().
