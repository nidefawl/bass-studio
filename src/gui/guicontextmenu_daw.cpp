#include "str_util.h"
#include "automation.h"
#include "automatable.h"
#include "../host/plugin/base_plugin.h"
#include "guicontextmenu_daw.h"
#include "track_snapshot.h"
#include "logging.h"

guictxtmenu_at_param::guictxtmenu_at_param(automatable_t* _atl, int32_t _paramIdx)
	: atl(_atl), paramIdx(_paramIdx)
{
	dbgassert(_atl);
	this->size.x = 240;
	addContextEntriesAutomation(this, _atl, paramIdx);
}
void guictxtmenu_at_param::clicked(int _id) {
	handleAutomatbleContextMenu(atl, paramIdx, _id);
	closeContextMenu();
}

void guictxtmenu_notrack::clicked(int _id) {
		if (_id >= idxImport) {
			auto window = parentCtrl->window;
			// promptUserFilePath initiates a native dialog that would close this context menu
			// so we do it ourself controlled here
			closeContextMenu(); // deletes this
			// now we make sure not to access heap (this) after this point
			String path;
			if (promptUserFilePath(window, 0, vFILE_TYPES_TRACKSNAPSHOT, path)) {
	        	std::shared_ptr<trackcontainer_snapshot_t> ctr = loadTrackContainer(path);
	        	dbgassert(ctr);
	        	if (ctr) {
	        		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	        		for (track_snapshot_t& ts : ctr->tracks) {
	        			track_t* tr = new track_t(ts);
	        			ts.trackLoaded = tr;
	        			DawInstance::get()->addTrackImpl(-1, tr, 0);
	            		log_printf("add track %s\n", StringAsCStr(tr->name));
	        		}

	    			vsthost* host = vsthost::getInstance();
	        		//load plugins
	        		for (track_snapshot_t& ts : ctr->tracks) {
	            		log_printf("track '%s' loading %d plugins\n", StringAsCStr(ts.trackLoaded->name), ts.plugins.pluginSnapshots.size());
	            		ts.stageIds = track_id_snapshot_t{};
	        			ts.trackLoaded->loadSnapshot(ts);
		    			std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
		    			for (auto effect : effects) {
		    				host->activateDeferred(effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
		    			}
	        		}
	        		dbgassert(dawCtrl);
	        		for (track_snapshot_t& ts : ctr->tracks) {
	        			ts.trackLoaded->getStage()->pluginsChanged();
	        		}
	        		host->onTrackLayoutChange();
	        		if (DawInstance::get()) DawInstance::get()->onPluginsChanged();

	        	}
			}
			return;
		} else {
			DawInstance::get()->insertNewTrack(-1, _id);
		}
		closeContextMenu();
	}
