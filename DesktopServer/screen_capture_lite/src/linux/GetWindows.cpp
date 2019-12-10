#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>


namespace
{
    class UniqueTextProperty
    {
    public:
        UniqueTextProperty()
        {
            init(p);
        }

        UniqueTextProperty(const UniqueTextProperty&) = delete;
        UniqueTextProperty& operator=(const UniqueTextProperty&) = delete;

        UniqueTextProperty(UniqueTextProperty&& other)
        {
            move(other);
        }

        UniqueTextProperty& operator=(UniqueTextProperty&& other)
        {
            move(other);
            return *this;
        }

        friend void swap(UniqueTextProperty& lhs, UniqueTextProperty& rhs)
        {
            std::swap(lhs.p, rhs.p);
        }

        ~UniqueTextProperty()
        {
            if (p.value)
                XFree(p.value);
        }

        static UniqueTextProperty GetWMName(Display* display, Window window)
        {
            UniqueTextProperty x;
            XGetWMName(display, window, &x.p);
            return x;
        }

        auto TextPropertyToStrings(Display* dpy) const
        {
            std::vector<std::string> result;

            char **list = nullptr;
            int n_strings = 0;
            auto status = XmbTextPropertyToTextList(dpy, &p, &list, &n_strings);

            if (status < Success or not n_strings or not * list)
                return result;

            result.reserve(n_strings);
            for (int i = 0; i < n_strings; ++i)
                result.emplace_back(list[i]);

            XFreeStringList(list);

            return result;
        }

    private:
        void move(UniqueTextProperty& other)
        {
            p = std::move(other.p);
            init(other.p);
        }
        void static init(XTextProperty& p)
        {
            memset(&p, 0, sizeof(XTextProperty));
        }
        XTextProperty p;
    };
}

namespace SL
{
    namespace Screen_Capture
    {

        void AddWindow(Display* display, XID& window, std::vector<Window>& wnd)
        {
            using namespace std::string_literals;

            auto wm_name{UniqueTextProperty::GetWMName(display, window)};
            auto candidates = wm_name.TextPropertyToStrings(display);
            Window w = {};
            w.Handle = reinterpret_cast<size_t>(window);

            XWindowAttributes wndattr;
            XGetWindowAttributes(display, window, &wndattr);

            w.Position = Point{ wndattr.x, wndattr.y };
            w.Size = Point{ wndattr.width, wndattr.height };

            w.Name = (candidates.empty()) ? "" : std::move(candidates.front());
            std::transform(w.Name.begin(), w.Name.end(), w.Name.begin(), ::tolower);
            wnd.push_back(w);
        }

        std::vector<Window> GetWindows()
        {
            auto* display = XOpenDisplay(NULL);
            const Atom a = XInternAtom(display, "_NET_CLIENT_LIST", true);
            Atom actualType;
            int format;
            unsigned long numItems = 0, bytesAfter = 0;
            unsigned char* data = 0;
            int status = XGetWindowProperty(display,
                                            XDefaultRootWindow(display),
                                            a,
                                            0L,
                                            (~0L),
                                            false,
                                            XA_WINDOW,
                                            &actualType,
                                            &format,
                                            &numItems,
                                            &bytesAfter,
                                            &data);
            std::vector<Window> ret;
            if (status == Success && numItems)
            {
                auto array = reinterpret_cast<::Window*>(data);
                for (decltype(numItems) k = 0; k < numItems; k++)
                {
                    auto w = array[k];
                    AddWindow(display, w, ret);
                }
                XFree(data);
            }
            XCloseDisplay(display);
            return ret;
        }
    }
}
