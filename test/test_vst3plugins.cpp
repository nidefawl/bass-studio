#include "rand.hpp"
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/samples/vst-hosting/editorhost/source/platform/iplatform.h>
#include <public.sdk/samples/vst-hosting/editorhost/source/platform/linux/window.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <base/source/fcommandline.h>
#include <base/source/fdebug.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/vsttypes.h>
#include <cstdio>
#include <set>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <public.sdk/samples/vst-hosting/editorhost/source/platform/linux/runloop.h>
#endif

enum OpenFlags
{
    kSetComponentHandler = 1 << 0,
    kSecondWindow = 1 << 1,
};

using namespace VST3::Hosting;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace Steinberg::Vst::EditorHost;
namespace Steinberg {

//------------------------------------------------------------------------
inline bool operator== (const ViewRect& r1, const ViewRect& r2)
{
	return memcmp (&r1, &r2, sizeof (ViewRect)) == 0;
}

//------------------------------------------------------------------------
inline bool operator!= (const ViewRect& r1, const ViewRect& r2)
{
	return !(r1 == r2);
}
}

class ComponentHandler : public IComponentHandler
{
public:
	tresult PLUGIN_API beginEdit (ParamID id) override
	{
		SMTG_DBPRT1 ("beginEdit called (%d)\n", id);
		return kNotImplemented;
	}
	tresult PLUGIN_API performEdit (ParamID id, ParamValue valueNormalized) override
	{
		SMTG_DBPRT2 ("performEdit called (%d, %f)\n", id, valueNormalized);
		return kNotImplemented;
	}
	tresult PLUGIN_API endEdit (ParamID id) override
	{
		SMTG_DBPRT1 ("endEdit called (%d)\n", id);
		return kNotImplemented;
	}
	tresult PLUGIN_API restartComponent (int32 flags) override
	{
		SMTG_DBPRT1 ("restartComponent called (%d)\n", flags);
		return kNotImplemented;
	}

private:
	tresult PLUGIN_API queryInterface (const TUID /*_iid*/, void** /*obj*/) override
	{
		return kNoInterface;
	}
	// we do not care here of the ref-counting. A plug-in call of release should not destroy this
	// class!
	uint32 PLUGIN_API addRef () override { return 1000; }
	uint32 PLUGIN_API release () override { return 1000; }
};

static ComponentHandler gComponentHandler;
class WindowController : public IWindowController, public IPlugFrame
{
public:
	WindowController (const IPtr<IPlugView>& plugView);
	~WindowController () noexcept override;

	void onShow (IWindow& w) override;
	void onClose (IWindow& w) override;
	void onResize (IWindow& w, Size newSize) override;
	Size constrainSize (IWindow& w, Size requestedSize) override;
	void onContentScaleFactorChanged (IWindow& window, float newScaleFactor) override;

	// IPlugFrame
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* newSize) override;

	void closePlugView ();

private:
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		if (FUnknownPrivate::iidEqual (_iid, IPlugFrame::iid) ||
		    FUnknownPrivate::iidEqual (_iid, FUnknown::iid))
		{
			*obj = this;
			addRef ();
			return kResultTrue;
		}
		if (window)
			return window->queryInterface (_iid, obj);
		return kNoInterface;
	}
	// we do not care here of the ref-counting. A plug-in call of release should not destroy this
	// class!
	uint32 PLUGIN_API addRef () override { return 1000; }
	uint32 PLUGIN_API release () override { return 1000; }

	IPtr<IPlugView> plugView;
	IWindow* window {nullptr};
	bool resizeViewRecursionGard {false};
};

WindowPtr window;
std::shared_ptr<WindowController> windowController;

#ifdef __linux__
namespace Steinberg::Vst::EditorHost {
  void setPlatformLinuxXDisplay (IPlatform* iplatform, Display* display);
  void runEventLoopLinux (IPlatform* iplatform);
}
#endif

//------------------------------------------------------------------------
void createViewAndShow (IEditController* controller)
{
	auto view = owned (controller->createView (ViewType::kEditor));
	if (!view)
	{
		IPlatform::instance ().kill (-1, "EditController does not provide its own editor");
	}

	ViewRect plugViewSize {};
	auto result = view->getSize (&plugViewSize);
	if (result != kResultTrue)
	{
		IPlatform::instance ().kill (-1, "Could not get editor view size");
	}

	auto viewRect = ViewRectToRect (plugViewSize);

#if __linux__
	// Connect to X server
	std::string displayName (getenv ("DISPLAY"));
	if (displayName.empty ())
		displayName = ":0.0";
	Display* xDisplay {nullptr};
	if ((xDisplay = XOpenDisplay (displayName.data ())) == nullptr)
	{
		return;
	}
    Steinberg::Vst::EditorHost::setPlatformLinuxXDisplay (&IPlatform::instance (), xDisplay);

	RunLoop::instance ().setDisplay (xDisplay);
#endif

	windowController = std::make_shared<WindowController> (view);
	window = IPlatform::instance ().createWindow (
	    "Editor", viewRect.size, view->canResize () == kResultTrue, windowController);
	if (!window)
	{
		IPlatform::instance ().kill (-1, "Could not create window");
	}

	window->show ();
#ifdef _WIN32
	MSG msg;
	while (GetMessage (&msg, nullptr, 0, 0))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
#elif defined(__linux__)

	RunLoop::instance ().start ();

	XCloseDisplay (xDisplay);
#endif
}

bool testVst3(const std::string& path, const VST3::Optional<VST3::UID>& effectID, uint32_t flags) {

    std::string error;
    auto module = Module::create (path, error);
    if (!module)
    {
        std::string reason = "Could not create Module for file:";
        reason += path;
        reason += "\nError: ";
        reason += error;
        return false;
    }

	auto factory = module->getFactory ();
	if (auto factoryHostContext = IPlatform::instance ().getPluginFactoryContext ())
		factory.setHostContext (factoryHostContext);
	IPtr<PlugProvider> plugProvider {nullptr};
	for (auto& classInfo : factory.classInfos ())
	{
		if (classInfo.category () == kVstAudioEffectClass)
		{
			if (effectID)
			{
				if (*effectID != classInfo.ID ())
					continue;
			}
            std::printf("VST3 %s %s %s\n",
                classInfo.name().c_str(),
                classInfo.category().c_str(), 
                classInfo.subCategoriesString().c_str() 
            );
			plugProvider = owned (new PlugProvider (factory, classInfo, true));
			if (plugProvider->initialize () == false)
				plugProvider = nullptr;
			break;
		}
	}
	if (!plugProvider)
	{
		if (effectID)
			error =
			    "No VST3 Audio Module Class with UID " + effectID->toString () + " found in file ";
		else
			error = "No VST3 Audio Module Class found in file ";
		error += path;
		IPlatform::instance ().kill (-1, error);
	}

	auto editController = plugProvider->getController ();
	if (!editController)
	{
		error = "No EditController found (needed for allowing editor) in file " + path;
		IPlatform::instance ().kill (-1, error);
	}
	editController->release (); // plugProvider does an addRef

	if (flags & kSetComponentHandler)
	{
		SMTG_DBPRT0 ("setComponentHandler is used\n");
		editController->setComponentHandler (&gComponentHandler);
	}

	SMTG_DBPRT1 ("Open Editor for %s...\n", path.c_str ());
	createViewAndShow (editController);

	if (flags & kSecondWindow)
	{
		SMTG_DBPRT0 ("Open 2cd Editor...\n");
		createViewAndShow (editController);
	}
    return true;
}

//------------------------------------------------------------------------
WindowController::WindowController (const IPtr<IPlugView>& plugView) : plugView (plugView)
{
}

//------------------------------------------------------------------------
WindowController::~WindowController () noexcept
{
}

//------------------------------------------------------------------------
void WindowController::onShow (IWindow& w)
{
	SMTG_DBPRT1 ("onShow called (%p)\n", (void*)&w);

	window = &w;
	if (!plugView)
		return;

	auto platformWindow = window->getNativePlatformWindow ();
	if (plugView->isPlatformTypeSupported (platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, std::string ("PlugView does not support platform type:") +
		                                     platformWindow.type);
	}

	plugView->setFrame (this);

	if (plugView->attached (platformWindow.ptr, platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, "Attaching PlugView failed");
	}
}

//------------------------------------------------------------------------
void WindowController::closePlugView ()
{
	if (plugView)
	{
		plugView->setFrame (nullptr);
		if (plugView->removed () != kResultTrue)
		{
			IPlatform::instance ().kill (-1, "Removing PlugView failed");
		}
		plugView = nullptr;
	}
	window = nullptr;
}

//------------------------------------------------------------------------
void WindowController::onClose (IWindow& w)
{
	SMTG_DBPRT1 ("onClose called (%p)\n", (void*)&w);

	closePlugView ();

	// TODO maybe quit only when the last window is closed
	IPlatform::instance ().quit ();
}

//------------------------------------------------------------------------
void WindowController::onResize (IWindow& w, Size newSize)
{
	SMTG_DBPRT1 ("onResize called (%p)\n", (void*)&w);

	if (plugView)
	{
		ViewRect r {};
		r.right = newSize.width;
		r.bottom = newSize.height;
		ViewRect r2 {};
		if (plugView->getSize (&r2) == kResultTrue && r != r2)
			plugView->onSize (&r);
	}
}

//------------------------------------------------------------------------
Size WindowController::constrainSize (IWindow& w, Size requestedSize)
{
	SMTG_DBPRT1 ("constrainSize called (%p)\n", (void*)&w);

	ViewRect r {};
	r.right = requestedSize.width;
	r.bottom = requestedSize.height;
	if (plugView && plugView->checkSizeConstraint (&r) != kResultTrue)
	{
		plugView->getSize (&r);
	}
	requestedSize.width = r.right - r.left;
	requestedSize.height = r.bottom - r.top;
	return requestedSize;
}

//------------------------------------------------------------------------
void WindowController::onContentScaleFactorChanged (IWindow& w, float newScaleFactor)
{
	SMTG_DBPRT1 ("onContentScaleFactorChanged called (%p)\n", (void*)&w);

	FUnknownPtr<IPlugViewContentScaleSupport> css (plugView);
	if (css)
	{
		css->setContentScaleFactor (newScaleFactor);
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API WindowController::resizeView (IPlugView* view, ViewRect* newSize)
{
	SMTG_DBPRT1 ("resizeView called (%p)\n", (void*)view);

	if (newSize == nullptr || view == nullptr || view != plugView)
		return kInvalidArgument;
	if (!window)
		return kInternalError;
	if (resizeViewRecursionGard)
		return kResultFalse;
	ViewRect r;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r == *newSize)
		return kResultTrue;

	resizeViewRecursionGard = true;
	Size size {newSize->right - newSize->left, newSize->bottom - newSize->top};
	window->resize (size);
	resizeViewRecursionGard = false;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r != *newSize)
		plugView->onSize (newSize);
	return kResultTrue;
}

std::shared_ptr<Vst::HostApplication> pluginContext;
int main(int, char*[]) {
    pluginContext = std::make_shared<Vst::HostApplication>();
	PluginContextFactory::instance ().setPluginContext (pluginContext.get());
    uint32_t flags {};
    VST3::Optional<VST3::UID> uid;
    auto pList = VST3::Hosting::Module::getModulePaths();
    std::set<std::string> uniqueClasses;
    std::set<std::string> uniqueCategories;
    seq_rand rand;
    auto randomIndex = rand.rng_bits(32) % pList.size();
    for (size_t i = 0; i < pList.size(); i++) {
        auto& path = pList[i];
        std::string error;
        auto module = Module::create (path, error);
        if (!module)
        {
            std::string reason = "Could not create Module for file:";
            reason += path;
            reason += "\nError: ";
            reason += error;
        } else {
            for (auto& classInfo : module->getFactory().classInfos()) {
                uniqueClasses.insert(classInfo.name());
                if (classInfo.name().find("Diva") != std::string::npos) {
                    testVst3(path, uid, flags);
                    return 0;
                }
                for (auto& subCategory : classInfo.subCategories()) {
                    uniqueCategories.insert(subCategory);
                }
            }
            if (i == randomIndex) {
                // testVst3(path, uid, flags);
            }
        }
        std::printf("Scanning VST3 plugins: %zu/%zu\n", i, pList.size());
    }
    pluginContext = nullptr;
    std::printf("%zu unique classes, %zu unique categories\n",
        uniqueClasses.size(), uniqueCategories.size());
    for (auto& className : uniqueClasses) {
        std::printf("Class: %s\n", className.c_str());
    }
    for (auto& categoryName : uniqueCategories) {
        std::printf("Category: %s\n", categoryName.c_str());
    }
    return 0;
}
