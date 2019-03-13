#include "exceptions.h"
#include "str_util.h"
#include "msgbox.h"

#define EXC_CATCH_DIALOG \
	} catch (std::exception& e) { \
		String excDesc = StringFormat("Fatal error: %s", e.what()); \
		ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK); \
		throw; \
	} catch (...) { \
		ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK); \
		throw; \
	}
#define EXC_CATCH_NO_THROW_DIALOG \
	} catch (std::exception& e) { \
		String excDesc = StringFormat("Fatal error: %s", e.what()); \
		ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK); \
	} catch (...) { \
		ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK); \
	}
