// This file is part of VSTGUI. It is subject to the license terms 
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#include "genericalertbox.h"
#include "../../lib/cframe.h"
#include "../../lib/controls/cbuttons.h"
#include "../../lib/controls/ctextlabel.h"
#include "../../uidescription/delegationcontroller.h"
#include "../../uidescription/iuidescription.h"
#include "../include/helpers/value.h"
#include "../include/helpers/valuelistener.h"
#include "../include/helpers/windowcontroller.h"

//------------------------------------------------------------------------
namespace VSTGUI {
namespace Standalone {
namespace Detail {
namespace {

//------------------------------------------------------------------------
const auto xmlText = R"(<?xml version="1.0" encoding="UTF-8"?>
<vstgui-ui-description version="1">
	<template autosize="left right top bottom " background-color="~ BlackCColor" background-color-draw-style="filled" class="CViewContainer" mouse-enabled="true" name="AlertBox" opacity="1" origin="0, 0" size="420, 110" sub-controller="ButtonController" transparent="true" wants-focus="false">
		<view autosize="left right top bottom " class="CGradientView" draw-antialiased="true" frame-color="~ BlackCColor" frame-width="-1" gradient="About Background" gradient-angle="0" gradient-style="linear" mouse-enabled="false" opacity="1" origin="0, 0" radial-center="0.5, 0.5" radial-radius="1" round-rect-radius="5" size="420, 110" transparent="false" wants-focus="false"/>
		<view autosize="right bottom " class="CTextButton" control-tag="AlertBox.firstButton" default-value="0.5" font="~ SystemFont" frame-color="~ BlackCColor" frame-color-highlighted="~ BlackCColor" frame-width="-1" gradient="Default TextButton Gradient" gradient-highlighted="Default TextButton Gradient Highlighted" icon-position="left" icon-text-margin="0" kick-style="false" max-value="1" min-value="0" mouse-enabled="true" opacity="1" origin="180, 80" round-radius="4" size="100, 20" text-alignment="center" text-color="~ BlackCColor" text-color-highlighted="~ WhiteCColor" title="OK" transparent="false" wants-focus="true" wheel-inc-value="0.1"/>
		<view autosize="right bottom " class="CTextButton" control-tag="AlertBox.secondButton" default-value="0.5" font="~ SystemFont" frame-color="~ BlackCColor" frame-color-highlighted="~ BlackCColor" frame-width="-1" gradient="Default TextButton Gradient" gradient-highlighted="Default TextButton Gradient Highlighted" icon-position="left" icon-text-margin="0" kick-style="false" max-value="1" min-value="0" mouse-enabled="true" opacity="1" origin="300, 80" round-radius="4" size="100, 20" text-alignment="center" text-color="~ BlackCColor" text-color-highlighted="~ WhiteCColor" title="Cancel" transparent="false" wants-focus="true" wheel-inc-value="0.1"/>
		<view autosize="left bottom " class="CTextButton" control-tag="AlertBox.thirdButton" default-value="0.5" font="~ SystemFont" frame-color="~ BlackCColor" frame-color-highlighted="~ BlackCColor" frame-width="-1" gradient="Default TextButton Gradient" gradient-highlighted="Default TextButton Gradient Highlighted" icon-position="left" icon-text-margin="0" kick-style="false" max-value="1" min-value="0" mouse-enabled="true" opacity="1" origin="20, 80" round-radius="4" size="100, 20" text-alignment="center" text-color="~ BlackCColor" text-color-highlighted="~ WhiteCColor" title="Third" transparent="false" wants-focus="true" wheel-inc-value="0.1"/>
		<view auto-height="false" autosize="left right top " back-color="~ BlackCColor" background-offset="0, 0" class="CMultiLineTextLabel" control-tag="AlertBox.headline" default-value="0.5" font="~ NormalFontVeryBig" font-antialias="true" font-color="~ BlackCColor" frame-color="~ BlackCColor" frame-width="1" line-layout="wrap" max-value="1" min-value="0" mouse-enabled="false" opacity="1" origin="10, 10" round-rect-radius="6" shadow-color="~ GreyCColor" size="400, 30" style-3D-in="false" style-3D-out="false" style-no-draw="false" style-no-frame="false" style-no-text="false" style-round-rect="false" style-shadow-text="false" text-alignment="center" text-inset="0, 0" text-rotation="0" text-shadow-offset="1, 1" title="This is a test headline" transparent="true" value-precision="2" wants-focus="false" wheel-inc-value="0.1"/>
		<view auto-height="false" autosize="left right top " back-color="~ BlackCColor" background-offset="0, 0" class="CMultiLineTextLabel" control-tag="AlertBox.description" default-value="0.5" font="~ SystemFont" font-antialias="true" font-color="~ BlackCColor" frame-color="~ BlackCColor" frame-width="1" line-layout="wrap" max-value="1" min-value="0" mouse-enabled="false" opacity="1" origin="10, 40" round-rect-radius="6" shadow-color="~ RedCColor" size="400, 30" style-3D-in="false" style-3D-out="false" style-no-draw="false" style-no-frame="false" style-no-text="false" style-round-rect="false" style-shadow-text="false" text-alignment="center" text-inset="5, 5" text-rotation="0" text-shadow-offset="1, 1" title="This is a test description" transparent="true" value-precision="2" wants-focus="false" wheel-inc-value="0.1"/>
	</template>
	<control-tags>
		<control-tag name="AlertBox.description" tag="4"/>
		<control-tag name="AlertBox.firstButton" tag="0"/>
		<control-tag name="AlertBox.headline" tag="3"/>
		<control-tag name="AlertBox.secondButton" tag="1"/>
		<control-tag name="AlertBox.thirdButton" tag="2"/>
	</control-tags>
	<colors>
		<color name="AlertBox.background" rgba="#ecececff"/>
	</colors>
	<gradients>
		<gradient name="About Background">
			<color-stop rgba="#dcdcdcff" start="0"/>
			<color-stop rgba="#b4b4b4ff" start="1"/>
		</gradient>
		<gradient name="Default TextButton Gradient">
			<color-stop rgba="#dcdcdcff" start="0"/>
			<color-stop rgba="#b4b4b4ff" start="1"/>
		</gradient>
		<gradient name="Default TextButton Gradient Highlighted">
			<color-stop rgba="#b4b4b4ff" start="0"/>
			<color-stop rgba="#646464ff" start="1"/>
		</gradient>
	</gradients>
</vstgui-ui-description>
)";

//------------------------------------------------------------------------
}

//------------------------------------------------------------------------
class AlertBoxController : public UIDesc::IModelBinding,
                           public UIDesc::ICustomization,
                           public ValueListenerAdapter,
                           public WindowControllerAdapter,
                           public std::enable_shared_from_this<AlertBoxController>
{
public:
	AlertBoxController (const AlertBoxConfig& config, const AlertBoxCallback& callback)
	: callback (callback)
	, firstButtonTitle (config.defaultButton)
	, secondButtonTitle (config.secondButton)
	, thirdButtonTitle (config.thirdButton)
	{
		addValue (Value::make ("AlertBox.firstButton"));
		addValue (Value::make ("AlertBox.secondButton"));
		addValue (Value::make ("AlertBox.thirdButton"));
		addValue (Value::makeStringListValue ("AlertBox.headline", {config.headline}))
		    ->setActive (false);
		addValue (Value::makeStringListValue ("AlertBox.description", {config.description}))
		    ->setActive (false);
	}

	void setWindow (const WindowPtr& w)
	{
		window = w;
		window->registerWindowListener (this);
	}

	const ValueList& getValues () const override { return values; }

	IController* createController (const UTF8StringView& name, IController* parent,
	                               const IUIDescription* uiDesc) override
	{
		if (name == "ButtonController")
		{
			return new ButtonController (*this, parent);
		}
		return nullptr;
	}

	void onEndEdit (IValue& value) override
	{
		if (value.getValue () < 0.5)
			return;
		auto self = shared_from_this ();
		if (value.getID () == "AlertBox.firstButton")
		{
			alertResult = AlertResult::DefaultButton;
		}
		else if (value.getID () == "AlertBox.secondButton")
		{
			alertResult = AlertResult::SecondButton;
		}
		else if (value.getID () == "AlertBox.thirdButton")
		{
			alertResult = AlertResult::ThirdButton;
		}
		window->close ();
	}

	void onClosed (const IWindow& window) override
	{
		if (callback)
		{
			callback (alertResult);
			callback = nullptr;
		}
	}

	void onSetContentView (IWindow& window, const SharedPointer<CFrame>& contentView) override
	{
		std::vector<CMultiLineTextLabel*> views; 
		if (contentView->getChildViewsOfType<CMultiLineTextLabel> (views, true) == 0)
			return;
		CCoord diffY = 0.;
		CCoord lastViewBottom = 0.;
		for (auto label : views)
		{
			auto prevSize = label->getViewSize ();
			label->setAutoHeight (true);
			auto newSize = label->getViewSize ();
			diffY += newSize.getHeight () - prevSize.getHeight ();
			if (lastViewBottom == 0)
			{
				lastViewBottom = newSize.bottom;
			}
			else
			{
				newSize.offset (0, lastViewBottom - newSize.top);
				label->setViewSize (newSize);
			}
		}
		if (diffY == 0.)
			return;
		auto windowSize = window.getSize ();
		windowSize.y += diffY;
		window.setSize (windowSize);
		contentView->setSize (windowSize.x, windowSize.y);
	}

private:
	struct ButtonController : DelegationController
	{
		ButtonController (AlertBoxController& alertBoxController, IController* parent)
		: DelegationController (parent), alertBoxController (alertBoxController)
		{
		}

		CView* verifyView (CView* view, const UIAttributes& attributes,
		                   const IUIDescription* description) override
		{
			if (auto button = dynamic_cast<CTextButton*> (view))
			{
				UTF8StringView tagName = description->lookupControlTagName (button->getTag ());
				if (tagName == "AlertBox.firstButton")
					button->setTitle (alertBoxController.firstButtonTitle);
				else if (tagName == "AlertBox.secondButton")
				{
					if (alertBoxController.secondButtonTitle.empty ())
					{
						view->forget ();
						return nullptr;
					}
					button->setTitle (alertBoxController.secondButtonTitle);
				}
				else if (tagName == "AlertBox.thirdButton")
				{
					if (alertBoxController.thirdButtonTitle.empty ())
					{
						view->forget ();
						return nullptr;
					}
					button->setTitle (alertBoxController.thirdButtonTitle);
				}
			}
			return controller->verifyView (view, attributes, description);
		}
		AlertBoxController& alertBoxController;
	};

	ValuePtr addValue (ValuePtr&& value)
	{
		value->registerListener (this);
		values.emplace_back (std::move (value));
		return values.back ();
	}

	WindowPtr window;
	AlertBoxCallback callback;
	ValueList values;
	UTF8String firstButtonTitle;
	UTF8String secondButtonTitle;
	UTF8String thirdButtonTitle;
	AlertResult alertResult {AlertResult::Error};
};

//------------------------------------------------------------------------
WindowPtr createAlertBox (const AlertBoxConfig& alertBoxConfig, const AlertBoxCallback& callback)
{
	vstgui_assert (callback);

	auto controller = std::make_shared<AlertBoxController> (alertBoxConfig, callback);
	UIDesc::Config config;
	config.windowConfig.type = WindowType::Document;
	config.windowConfig.style.transparent ().movableByWindowBackground ();
	config.viewName = "AlertBox";
	config.uiDescFileName = xmlText;
	config.modelBinding = staticPtrCast<UIDesc::IModelBinding> (controller);
	config.customization = staticPtrCast<UIDesc::ICustomization> (controller);
	auto window = UIDesc::makeWindow (config);
	controller->setWindow (window);
	return window;
}

//------------------------------------------------------------------------
} // Detail
} // Standalone
} // VSTGUI
