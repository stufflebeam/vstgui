﻿// This file is part of VSTGUI. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#include "x11platform.h"
#include "../../cfileselector.h"
#include "../../cframe.h"
#include "../../cstring.h"
#include "x11frame.h"
#include "cairobitmap.h"
#include <cassert>
#include <chrono>
#include <array>
#include <dlfcn.h>
#include <iostream>
#include <link.h>
#include <unordered_map>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_keysyms.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <X11/Xlib.h>

// c++11 compile error workaround
#define explicit _explicit
#include <xcb/xkb.h>
#undef explicit

//------------------------------------------------------------------------
namespace VSTGUI {

struct X11FileSelector : CNewFileSelector
{
	X11FileSelector (CFrame* parent, Style style) : CNewFileSelector (parent), style (style) {}

	bool runInternal (CBaseObject* delegate) override
	{
		this->delegate = delegate;
		return false;
	}

	void cancelInternal () override {}

	bool runModalInternal () override { return false; }

	Style style;
	SharedPointer<CBaseObject> delegate;
};

//------------------------------------------------------------------------
CNewFileSelector* CNewFileSelector::create (CFrame* parent, Style style)
{
	return new X11FileSelector (parent, style);
}

//------------------------------------------------------------------------
namespace X11 {

//------------------------------------------------------------------------
namespace {

using VirtMap = std::unordered_map<xkb_keysym_t, uint16_t>;
const VirtMap keyMap = {{XKB_KEY_BackSpace, VKEY_BACK},
						{XKB_KEY_Tab, VKEY_TAB},
						{XKB_KEY_Clear, VKEY_CLEAR},
						{XKB_KEY_Return, VKEY_RETURN},
						{XKB_KEY_Pause, VKEY_PAUSE},
						{XKB_KEY_Escape, VKEY_ESCAPE},
						{XKB_KEY_space, VKEY_SPACE},
						{XKB_KEY_KP_Next, VKEY_NEXT},
						{XKB_KEY_End, VKEY_END},
						{XKB_KEY_Home, VKEY_HOME},

						{XKB_KEY_Left, VKEY_LEFT},
						{XKB_KEY_Up, VKEY_UP},
						{XKB_KEY_Right, VKEY_RIGHT},
						{XKB_KEY_Down, VKEY_DOWN},
						{XKB_KEY_Page_Up, VKEY_PAGEUP},
						{XKB_KEY_Page_Down, VKEY_PAGEDOWN},
						{XKB_KEY_KP_Page_Up, VKEY_PAGEUP},
						{XKB_KEY_KP_Page_Down, VKEY_PAGEDOWN},

						{XKB_KEY_Select, VKEY_SELECT},
						{XKB_KEY_Print, VKEY_PRINT},
						{XKB_KEY_KP_Enter, VKEY_ENTER},
						{XKB_KEY_Insert, VKEY_INSERT},
						{XKB_KEY_Delete, VKEY_DELETE},
						{XKB_KEY_Help, VKEY_HELP},
						// Numpads ???
						{XKB_KEY_KP_Multiply, VKEY_MULTIPLY},
						{XKB_KEY_KP_Add, VKEY_ADD},
						{XKB_KEY_KP_Separator, VKEY_SEPARATOR},
						{XKB_KEY_KP_Subtract, VKEY_SUBTRACT},
						{XKB_KEY_KP_Decimal, VKEY_DECIMAL},
						{XKB_KEY_KP_Divide, VKEY_DIVIDE},
						{XKB_KEY_F1, VKEY_F1},
						{XKB_KEY_F2, VKEY_F2},
						{XKB_KEY_F3, VKEY_F3},
						{XKB_KEY_F4, VKEY_F4},
						{XKB_KEY_F5, VKEY_F5},
						{XKB_KEY_F6, VKEY_F6},
						{XKB_KEY_F7, VKEY_F7},
						{XKB_KEY_F8, VKEY_F8},
						{XKB_KEY_F9, VKEY_F9},
						{XKB_KEY_F10, VKEY_F10},
						{XKB_KEY_F11, VKEY_F11},
						{XKB_KEY_F12, VKEY_F12},
						{XKB_KEY_Num_Lock, VKEY_NUMLOCK},
						{XKB_KEY_Scroll_Lock, VKEY_SCROLL}, // correct ?
						{XKB_KEY_Shift_L, VKEY_SHIFT},
						{XKB_KEY_Shift_R, VKEY_SHIFT},
						{XKB_KEY_Control_L, VKEY_CONTROL},
						{XKB_KEY_Control_R, VKEY_CONTROL},
						{XKB_KEY_Alt_L, VKEY_ALT},
						{XKB_KEY_Alt_R, VKEY_ALT},
						{XKB_KEY_VoidSymbol, 0}};

//------------------------------------------------------------------------
VstKeyCode internalMakeKeyCode (xkb_state* xkbState, xcb_keycode_t detail, uint16_t state)
{
	if (!xkbState)
		return {};

	VstKeyCode code{};

	if (state & XCB_MOD_MASK_SHIFT)
		code.modifier |= MODIFIER_SHIFT;
	if (state & XCB_MOD_MASK_CONTROL)
		code.modifier |= MODIFIER_CONTROL;
	if (state & (XCB_MOD_MASK_1 | XCB_MOD_MASK_5))
		code.modifier |= MODIFIER_ALTERNATE;

	auto ksym = xkb_state_key_get_one_sym (xkbState, detail);

	auto it = keyMap.find (ksym);
	if (it != keyMap.end ())
		code.virt = it->second;
	else
		code.character = xkb_state_key_get_utf32 (xkbState, detail);

	return code;
}

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
struct Platform::Impl
{
	std::string path;
};

//------------------------------------------------------------------------
Platform& Platform::getInstance ()
{
	static Platform gInstance;
	return gInstance;
}

//------------------------------------------------------------------------
Platform::Platform ()
{
	impl = std::unique_ptr<Impl> (new Impl);

	Cairo::Bitmap::setGetResourcePathFunc ([this]() {
		auto path = getPath ();
		path += "/Contents/Resources/";
		return path;
	});
}

//------------------------------------------------------------------------
Platform::~Platform ()
{
	Cairo::Bitmap::setGetResourcePathFunc ([]() { return std::string (); });
}

//------------------------------------------------------------------------
uint64_t Platform::getCurrentTimeMs ()
{
	using namespace std::chrono;
	return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count ();
}

//------------------------------------------------------------------------
std::string Platform::getPath ()
{
	if (impl->path.empty () && soHandle)
	{
		struct link_map* map;
		if (dlinfo (soHandle, RTLD_DI_LINKMAP, &map) == 0)
		{
			auto path = std::string (map->l_name);
			for (int i = 0; i < 3; i++)
			{
				int delPos = path.find_last_of ('/');
				if (delPos == -1)
				{
					fprintf (stderr, "Could not determine bundle location.\n");
					return {}; // unexpected
				}
				path.erase (delPos, path.length () - delPos);
			}
			auto rp = realpath (path.data (), nullptr);
			path = rp;
			free (rp);
			std::swap (impl->path, path);
		}
	}
	return impl->path;
}

//------------------------------------------------------------------------
struct RunLoop::Impl : IEventHandler
{
	using WindowEventHandlerMap = std::unordered_map<uint32_t, IFrameEventHandler*>;

	SharedPointer<IRunLoop> runLoop;
	std::atomic<uint32_t> useCount{0};
	xcb_connection_t* xcbConnection{nullptr};
	xcb_cursor_context_t* cursorContext{nullptr};
	xkb_context* xkbContext{nullptr};
	xkb_state* xkbState{nullptr};
	xkb_keymap* xkbKeymap{nullptr};
	WindowEventHandlerMap windowEventHandlerMap;
	std::array<xcb_cursor_t, CCursorType::kCursorHand> cursors{XCB_CURSOR_NONE};

	void init (const SharedPointer<IRunLoop>& inRunLoop)
	{
		if (++useCount != 1)
			return;
		runLoop = inRunLoop;
		int screenNo;
		xcbConnection = xcb_connect (nullptr, &screenNo);
		runLoop->registerEventHandler (xcb_get_file_descriptor (xcbConnection), this);
		auto screen = xcb_aux_get_screen (xcbConnection, screenNo);
		xcb_cursor_context_new (xcbConnection, screen, &cursorContext);

		xcb_xkb_use_extension (xcbConnection, XKB_X11_MIN_MAJOR_XKB_VERSION,
							   XKB_X11_MIN_MINOR_XKB_VERSION);
		xkbContext = xkb_context_new (XKB_CONTEXT_NO_FLAGS);

		int32_t deviceId = xkb_x11_get_core_keyboard_device_id (xcbConnection);
		if (deviceId > -1)
		{
			xkbKeymap = xkb_x11_keymap_new_from_device (xkbContext, xcbConnection, deviceId,
														XKB_KEYMAP_COMPILE_NO_FLAGS);
			xkbState = xkb_state_new (xkbKeymap);
		}
	}

	void exit ()
	{
		if (--useCount != 0)
			return;
		if (xcbConnection)
		{
			if (xkbState)
				xkb_state_unref (xkbState);
			if (xkbKeymap)
				xkb_keymap_unref (xkbKeymap);
			if (xkbContext)
				xkb_context_unref (xkbContext);
			if (cursorContext)
			{
				for (auto c : cursors)
				{
					if (c != XCB_CURSOR_NONE)
						xcb_free_cursor (xcbConnection, c);
				}
				xcb_cursor_context_free (cursorContext);
			}

			xcb_disconnect (xcbConnection);
		}
		runLoop->unregisterEventHandler (this);
		runLoop = nullptr;
	}

	template<typename T>
	void dispatchEvent (T& event, xcb_window_t windowId)
	{
		auto it = windowEventHandlerMap.find (windowId);
		if (it == windowEventHandlerMap.end ())
			return;
		it->second->onEvent (event);
	}

	void onEvent () override
	{
		while (auto event = xcb_poll_for_event (xcbConnection))
		{
			auto type = event->response_type & ~0x80;
			switch (type)
			{
				case XCB_KEY_PRESS:
				{
					auto ev = reinterpret_cast<xcb_key_press_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_KEY_RELEASE:
				{
					auto ev = reinterpret_cast<xcb_key_release_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_BUTTON_PRESS:
				{
					auto ev = reinterpret_cast<xcb_button_press_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_BUTTON_RELEASE:
				{
					auto ev = reinterpret_cast<xcb_button_release_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_MOTION_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_motion_notify_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_ENTER_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_enter_notify_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_LEAVE_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_leave_notify_event_t*> (event);
					dispatchEvent (*ev, ev->event);
					break;
				}
				case XCB_EXPOSE:
				{
					auto ev = reinterpret_cast<xcb_expose_event_t*> (event);
					dispatchEvent (*ev, ev->window);
					break;
				}
				case XCB_UNMAP_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_unmap_notify_event_t*> (event);
					break;
				}
				case XCB_MAP_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_map_notify_event_t*> (event);
					dispatchEvent (*ev, ev->window);
					break;
				}
				case XCB_CONFIGURE_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_configure_notify_event_t*> (event);
					break;
				}
				case XCB_PROPERTY_NOTIFY:
				{
					auto ev = reinterpret_cast<xcb_property_notify_event_t*> (event);
					dispatchEvent (*ev, ev->window);
					break;
				}
				case XCB_CLIENT_MESSAGE:
				{
					auto ev = reinterpret_cast<xcb_client_message_event_t*> (event);
					dispatchEvent (*ev, ev->window);
					break;
				}
			}
			std::free (event);
		}
		xcb_flush (xcbConnection);
	}
};

//------------------------------------------------------------------------
RunLoop& RunLoop::instance ()
{
	static RunLoop gInstance;
	return gInstance;
}

//------------------------------------------------------------------------
void RunLoop::init (const SharedPointer<IRunLoop>& runLoop)
{
	instance ().impl->init (runLoop);
}

//------------------------------------------------------------------------
void RunLoop::exit ()
{
	instance ().impl->exit ();
}

//------------------------------------------------------------------------
const SharedPointer<IRunLoop> RunLoop::get ()
{
	return instance ().impl->runLoop;
}

//------------------------------------------------------------------------
RunLoop::RunLoop ()
{
	impl = std::unique_ptr<Impl> (new Impl);
}

//------------------------------------------------------------------------
RunLoop::~RunLoop () noexcept = default;

//------------------------------------------------------------------------
void RunLoop::registerWindowEventHandler (uint32_t windowId, IFrameEventHandler* handler)
{
	impl->windowEventHandlerMap.emplace (windowId, handler);
}

//------------------------------------------------------------------------
void RunLoop::unregisterWindowEventHandler (uint32_t windowId)
{
	auto it = impl->windowEventHandlerMap.find (windowId);
	if (it == impl->windowEventHandlerMap.end ())
		return;
	impl->windowEventHandlerMap.erase (it);
}

//------------------------------------------------------------------------
xcb_connection_t* RunLoop::getXcbConnection () const
{
	return impl->xcbConnection;
}

//------------------------------------------------------------------------
uint32_t RunLoop::getCursorID (CCursorType cursor)
{
	if (impl->cursors[cursor] == XCB_CURSOR_NONE && impl->cursorContext)
	{
		const char* name = nullptr;
		switch (cursor)
		{
			case kCursorDefault:
				name = "arrow";
				break;
			case kCursorWait:
				name = "watch";
				break;
			case kCursorHSize:
				name = "sb_h_double_arrow";
				break;
			case kCursorVSize:
				name = "sb_v_double_arrow";
				break;
			case kCursorNESWSize:
				name = "cross";
				break;
			case kCursorNWSESize:
				name = "cross";
				break;
			case kCursorSizeAll:
				name = "cross";
				break;
			case kCursorCopy:
				name = "copy";
				break;
			case kCursorNotAllowed:
				name = "forbidden";
				break;
			case kCursorHand:
				name = "openhand";
				break;
		}
		if (name)
		{
			impl->cursors[cursor] = xcb_cursor_load_cursor (impl->cursorContext, name);
		}
	}
	return impl->cursors[cursor];
}

//------------------------------------------------------------------------
VstKeyCode RunLoop::makeKeyCode (uint8_t detail, uint16_t state) const
{
	return internalMakeKeyCode (impl->xkbState, detail, state);
}

//------------------------------------------------------------------------
} // X11
} // VSTGUI
