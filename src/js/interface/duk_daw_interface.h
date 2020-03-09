#pragma once
#include "duktape.h"
class DawInstance;
namespace NU {
namespace SCRIPTING {
	class DawInterface {
	public:
		enum enum_state_t {
			state_stop, state_idle, state_play
		};
		int play() {
			return 12345;
		}

		enum_state_t getState() {
			return this->state;
		}
		void setState(enum_state_t s) {
			this->state = s;
		}
	private:
		enum_state_t state;
	};

	void registerInterfaceToContext(duk_context* ctx);
	void setGlobalInstance(duk_context* ctx, DawInstance*);
}

}
