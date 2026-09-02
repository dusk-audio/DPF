/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2024 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "tests.hpp"

#define DAF_TEST_POINT_CPP
#define DAF_TEST_WINDOW_CPP
#include "dgl/src/pugl.cpp"
#include "dgl/src/Application.cpp"
#include "dgl/src/ApplicationPrivateData.cpp"
#include "dgl/src/Geometry.cpp"
#include "dgl/src/Widget.cpp"
#include "dgl/src/WidgetPrivateData.cpp"
#include "dgl/src/Window.cpp"
#include "dgl/src/WindowPrivateData.cpp"

// --------------------------------------------------------------------------------------------------------------------

int main()
{
    using DGL_NAMESPACE::Application;
    using DGL_NAMESPACE::ApplicationQuitter;
    using DGL_NAMESPACE::Window;

    // creating and destroying simple window
    {
        Application app(true);
        Window win(app);
    }

    // creating and destroying simple window, with a delay
    {
        Application app(true);
        ApplicationQuitter appQuitter(app);
        Window win(app);
        app.exec();
    }

    // showing and closing simple window, MUST be visible on screen
    {
        Application app(true);
        ApplicationQuitter appQuitter(app);
        Window win(app);
        win.show();
        app.exec();
    }

    // auto-scaling must apply the scale factor exactly once, whatever size the window starts at
    {
        Application app(true);

        // as a plugin UI does it: the window is created at the already-scaled size, then
        // auto-scaling is switched on with the unscaled design size as the minimum
        Window win(app, 0, 360, 360, 1.8, true);
        win.setGeometryConstraints(200, 200, true, true, true);
        DAF_ASSERT_EQUAL(win.getWidth(), 360u, "pre-scaled window keeps its size");
        DAF_ASSERT_EQUAL(win.getHeight(), 360u, "pre-scaled window keeps its size");

        // and a second call leaves it alone rather than scaling again
        win.setGeometryConstraints(200, 200, true, true, true);
        DAF_ASSERT_EQUAL(win.getWidth(), 360u, "repeated call does not scale again");
        DAF_ASSERT_EQUAL(win.getHeight(), 360u, "repeated call does not scale again");
    }

    // a window still at its unscaled design size grows to the scaled minimum instead
    {
        Application app(true);

        Window win(app, 0, 200, 200, 1.8, true);
        win.setGeometryConstraints(200, 200, true, true, true);
        DAF_ASSERT_EQUAL(win.getWidth(), 360u, "unscaled window grows to the scaled minimum");
        DAF_ASSERT_EQUAL(win.getHeight(), 360u, "unscaled window grows to the scaled minimum");
    }

    // TODO

    return 0;
}

// --------------------------------------------------------------------------------------------------------------------
