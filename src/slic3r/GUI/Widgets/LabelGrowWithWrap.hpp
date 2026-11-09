#ifndef slic3r_GUI_LabelGrowWithWrap_hpp_
#define slic3r_GUI_LabelGrowWithWrap_hpp_

class Label;

// Let an auto-wrapping label raise its own minimum height once it has wrapped.
//
// A sizer lays a wrapping label out before the label knows its width, so it
// gets the height of the unwrapped text and the extra lines are cut. After each
// wrap this raises the minimum height to the wrapped height (it never lowers
// it, so it cannot oscillate) and lays out the nearest scrolled ancestor again.
void GrowWithWrap(Label *label);

#endif // slic3r_GUI_LabelGrowWithWrap_hpp_
