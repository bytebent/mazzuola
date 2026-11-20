#pragma once
// Where a reference image sits in a 2D view (ARCHITECTURE §5): one per view direction,
// shared by the views facing that way, never saved. All in the view's plane coordinates
// (cm; u right, v up as the view shows them). Pure C++: the picture itself lives on the
// GPU (app side).
#include <string>

namespace mazz {

struct RefPlacement {
    double cu = 0, cv = 0; // centre
    double width = 100;    // cm; the height follows the picture's proportions
    double aspect = 1;     // picture height / width
    double rot = 0;        // degrees, counter-clockwise as seen in the view
    double alpha = 0.5;    // 0 = invisible, 1 = opaque
    bool front = false;    // drawn over the model (else behind it)

    double height() const { return width * aspect; }
    // Corners in plane coords, counter-clockwise from the picture's bottom-left
    // (bottom-left, bottom-right, top-right, top-left).
    void corners(double u[4], double v[4]) const;
    // Plane point -> picture coords (0..1 across, 0..1 up); true if inside.
    bool toPicture(double u, double v, double& s, double& t) const;
    // Fits a picture of `aspect` inside the rectangle u0..u1, v0..v1, centred
    // (a new image fills the visible part of the view).
    void fit(double u0, double v0, double u1, double v1, double aspect);
    // Scales about corner `fixed` (0..3, see corners()) so that the opposite corner
    // lands as near as possible to (u, v), proportions kept. Width stays >= minWidth;
    // with a grid step > 0 the width is a whole number of steps (at least one).
    void dragCorner(int fixed, double u, double v, double minWidth, double step = 0);
};

} // namespace mazz
