/*****************************/
/*           Ideas           */
/*****************************/
# Ideas 

Add optional horizontal grid steps to automation (allow offset/scale?)  
Cleanup automation stuff: setParamValue is called with inconsistent flags  
Resizing a clip can stop held notes. This has to be handled somehow  
Add a keyboard on the bottom, make it small so its not using up screenspace. Display all held notes from all tracks in different colors so I can quickly see what key the song is currently in etc.  
Exported audio files are incorrect length  
Automation sub tracks are not removed from track when moving plugin to another track  
Exp/Log slopes for automation data  
Basic +/- pitch for wave samples  
Sample database  
Categories for plugins (at least effects/instruments for now)  
Selection info (Show duration in bars/secs/samples)  
Add a way to copy clips + automation or only clips (keybind? key modifier?)  
Parallel groups for plugins  
Handle multi-channel audio effects correctly  
Make startup.cpp only compile into daw-application  
Allow sonogram to be used in a mouse over popup window (not working)  
Tool to auto process samples: run samples thru specific vst preset, run samples thru my effect chains (that I extract from ableton projects)  
"Duplicate muted" for tracks/plugins/etc: Duplicates the instance but keeps it muted/inactive  
Duplicate frozen track to flattened audio skipping the creation of plugin chain  
More specific undo, proper undo history, ability to configure it, disable it.   
Show inactive automation in a list  
Show automated parameters in a list, and on a tab in the plugin container  

Add lists/tables for debugging:
- all loaded plugins (done: list view showing type as icon, name and cpu usage)
- all tracks
- all guibase instances
- all automation

Alternative VST2 management:
- Show all *.dll files instead of using a vst scanner
- Cache results for shell plugins maybe
- Allow drag-dropping dll from windows explorer
 
Implement a drum seq into the timeline:
- editing must be possible without requiring context change
- samples should be sequenceable within the trackcontents
- placed events shall be shown as waveform as if it was a wave sample on an audio track
- there is a backing midi track having the notes and note editor accessible
- have them freely place in time, like midi clips
- a on/off alternative grid with 1/8th raster, [x][ ][ ][ ][x][ ][ ][ ]


arpeggiator as per clip setting  
midi arpeggiator freeze function (drag from arp to new clip on timeline)  
add list of all inputs to track, so one can navigate to connected track based on input routing  
add bypass all, add bypass track plugins, does override, but not deactivate automation  
make volume slider that modifies only exponent of float32  
clip transformation tool (select tool from toolbar -> mouse cursor is adjusted(?) -> click clip -> applys transformation operation)   
  i.e. +/-12 semitones  
add metadata to clips (creation date, modify date, source midi file, (references?))  
javascripted or python functions for plugin parameter automation, allowing live editing  
reset plugin state to last save-state of project file by right clicking right-click contextmenu on vst plugins  
rms/peak db info when hovering meters  
switch to audio view. replacing all tracks with their output buffer waveforms
track editor vertical drag-move zooming like note-editor  
consider handling context menu ownership with shared_ptrs. I may require to use shared from this to solve issues with call close-context-menu from click handlers  
scrubbing with beat sync  
add vertical plugin chain in mix view (like cubase mixer)  

/*****************************/
/*           Bugs           */
/*****************************/
# Bugs
Meters are buggy after sample rate or blocksize changes 
solo buttons not update after track routing changes  
Add Un-solo all button  
Fix recent file list: state should be saved to disk each load/save  
fix shift-clicking not extending selection to clicked clips boundaries  
Fix selecting regions while holding shift (i.e. when scrolling is required to make bigger selection)  
Fix undo/redo for automation data  
automation lanes are read in stopped mode  
status bar does not show text  
clipsettings labels overlap input fields  
ALT key sometimes stuck, wonderful old bug from GLFW.  
ALT key to disable grid is not allowing note-resize below grid size  
notes border rendering not respecting z-order when note rectangles overlap  

while having a selection with no time range, sitting on subtrack 0 of a midi track (first automation lane, just having a cursor sitting in an automation lane):  
if the track is dragged onto another track to put it as child the program will assert in subtrack range check with out of bounds. see screenshot 02.05.2021 22:58

/*****************************/
/*           TODOs           */
/*****************************/
# TODO code quality

Initialization and lifetime of DawInstance should be controlled top level, not by a GUI-Ctrl:
- MainCtrl::init(window, nanovg) renamed to MainCtrl::initAppWindow(window, nanovg) to avoid confusion
- refactor makeApp() to main.cpp::makeApp(argc, argv) 
- makeApp(argc, argv) generates the AppCtrl and DawInstance instances (stored in the shared pointers) as before
- the call to the AppCtrl::initApp(argc, argv) is moved inside main.cpp::makeApp(argc, argv) 
- remove all calls made MainCtrl::initApp(argc, argv) and move them into makeApp(argc, argv)
- now makeApp(argc, argv) calls DawInstance::initDaw(argc, argv) directly
- after return from makeApp all required instances in the tls are alive
- create main.cpp::startApp, replace AppCtrl::postInit with call to main.cpp::startApp
- main.cpp::startApp calls AppCtrl::postInit
- main.cpp::startApp calls DawInstance::startDaw(); DawInstance::postInit();
- main.cpp::startApp calls AppCtrl::postInit()
- move the project loading from DawInstance::startDaw to DawInstance::postInit
- Consider moving DawInstance::postInit() after AppCtrl::postInit
- Consider moving showWindow after startApp

DawInstance should be usable without a GUI-Ctrl instance  
MainCtrl::getPlayThread() is bad interface for no-GUI headless applications  
cleanup trackcontainer.cpp  
DawCtrl::filesDropBegin:
- figure out which thread the calls come from
- make sure it the code inside is fast and never blocks.  
  Right now wave files are loaded sync'd and the thread task for loading midi simply waits until completion, so both things block
- Read win32 docs on constraints of IDropTarget  
  
Fix the sign conversion warnings!
refactor into namespaces  
all interface types must have suffix _i   
make _t and no-_t type names consistent  
make struct and class types consistent:   
- structs have no functions besides ctors/dtors/copy/move operators
- classes have functions

Fix visibility of fields of all classes  
add more (unit) tests  
Test handling of exceptions and segfaults on threads and worker thread tasks (TrackBlockProcessTask)  
Test handling of tasks and threads not responding (stuck in inf loop)  
 Remove the networking source code for DAW builds until I use it (rgb-master uses UDP)  

change to initializer braces assignment for ivec2 (so we can easily change the type)  

    find: \s*=\s*ivec2\s*\(\s*([^, ]*),\s*([^, ]*)\s*\)\s*;  
    replace: = {\1, \2};

track_t::projectIdx should be constant (UID):  
- when restoring track contents using trackstate_t the resolution of tracks should be done using the UID for robustness reasons  
- Solution: Use toRef(), stageIds are (runtime?) constant

# TODO high priority
custom block length processing  
per track samplerate / blocksize   
test doing multiblock processing in tight loop to increase automation samplerate/resolution   

Parameter filtering:
- Host outgoing data will probably stay unfiltered
- Filtering for send automation
- Internal Plugins incoming data must be filtered. 
- Where possible we should access automation lanes to have latency free look-ahead. This allows more accurate filtering.  
  This will be some work when taking latency compensation and looping into account


# TODO low priority
render group contents into folded group track background  
global midi pitch offset (and per track global offset bypass)  
randomize note velocity + offset (shuffle/humanize/groove)  
per track velocity scaling (min max curve?!)  
Render drop shadow on popups or use other background color or outline to make them stand outline  
height track title should be equal height clip title, at least on folded tracks  



remove height limitation from automation tracks  
Allow reordering automation tracks  

plugin preset indexing, plugin preset search
quantize midi 
legato button in midi editor
consolidate clips
plugin preset store/load
group+plugin snapshots
add keybind constant
keybind editor/presets
multiselection of tracks
Mixing desk view
Compressed binary file format
stress-test synchronization
use templates + template specialization for the 3 window classes in window.cpp
scale factor for UI font sizes
support waves shell plugins 
add a toggle button for current tracks complete effect chain. This should probably be the function of the power/enable button of a track. Adding a mute button that keeps audio processing enabled
midi export
freeze track
don't fully close vst-windows, just hide them and defer destruction so they reopen quickly 
try rendering vst-windows into gui at fixed location (kind of a bookmark)
add a view of a list of all thirdparty+internal plugin instances 


/*****************************/
/*           NOTES           */
/*****************************/


Testing VST2 plugins
====================
## Ohmforce Frohmage: 
Spawns its own thread and calls the host-callback from it (endEdit): Currently the opCode is not handled. 
 
## Avenger:
Breaks hosts opengl context
 
## Spire 1.5
Stepped parameters cannot be changed (Synced delay times )

## volume shaper 
does not run in debug and fucks up debug sessions

## mpowersynth
 is blocking the main window render refresh timer if in foreground  
 (have project with 1 track mpowersynth, playing, have plugin window active in foreground, look playhead position, fps is not constant, window refreshs are not triggered reliably)  
 I reduced its framerate for now
## test_vstplugins
### Implemented
- load unload show hide onTick
### Todo
- show/hide multiple times
- load multiple instances
- simulate audio processing as in DAW (UI and audio thread) set parameter, dispatch events, set automation, process block
- write down communication with timings (opCodes in and out, called dispatch functions) and write to file
- write down i/o and parameter info and write to file
- check memory usage
- test supported samplerates and blocksizes
- keep a list of "seen" hostCanDo requests
- track VST API statistics per plugin:
  - plugin to host and host to plugin
  - total number of requests per opcode
  - min/max timings of dispatches

Performance
===========
Find a way to get a list of all used classes and template instantiation  
reduce sizeof(clip_t), sizeof(note_t)  
Analyze sizeof() all gui classes  
add more detailed timings: time spent in own code/third party code/windows code(possible ?)   
histogram of timings of processPlayback invocations	  

Thread safety
=============
automation editing is not synchronized: can cause a race condition

vst host mastercallback
=======================
look into unused opcodes:
 - VstProcessLevels/audioMasterGetCurrentProcessLevel etc

thread safety
-------------
TODO: Find out what exact thread we got called from  
This could be one of: the playthread, an audio workerthread, the UI thread  
If the thread is not known (plugin created it) we can't guarantee  
proper lockfree synchronization. So, depending on the opcode the call gets ignored if  
it is the wrong thread.  
audioMasterSizeWindow: Ignored if not from the UI thread (Should be rare from non UI-threads)  
audioMasterUpdateDisplay: Already handled by the onTick handler (20ms interval)  
audioMasterUpdateDisplay: Update the parameter list and program name  

(infinite) reentrant calls
--------------------------
TODO: Detect reentrance and guard against it.  
Any outgoing call into 3rd party or windows code might end up here again in a reentrant scenario.  
i.e. audioMasterSizeWindow calls updateWindowSize. That could trigger a message box that spawns
a win32 event-pump, causing a render of the plugin UI. Before rendering, the plugin dispatches
a audioMasterGetTime call and we end up here again.  
In the common case plugin developers are aware of this and take care of it.  
So this must be done in a lock free manner to avoid unnecessary locks and synchronization to fix a
really rare problem.  

Entrance counting has to be done per plugin and thread-id.

### Reentrant detection implementation
Limit the number of allowed threads to enter the callback to UI + playback + n audio workers (1+1+32 max)  
each plugin instance has bool array of len 34  
each thread gets an index assinged  
```cpp
//pseudo
#def MAX_NUM_OF_THREADS 35
class plugin {
 bool isInCallback[MAX_NUM_OF_THREADS];
}
threadId = get_my_thread_idx() //get thread IDX from TLS.

if (threadId == 0)
  callFromUnknownThread = true; // be worried, its a thread the plugin fired up, do limited stuff!
else
if (threadId > 0) {
  if (plugin->isInCallback[threadId] > 0)
    return;//reentrant
  plugin->isInCallback[threadId]++
}
...
...
//on function leave with help of RAII 
plugin->isInCallback[threadId]--
```


Memory usage
============
It is quite memory intense:
```
sr = 44100
channels = 2
bytesPerSample = 4
seconds = 600
sizeInMB = (bytesPerSample * channels * sr * seconds) / ( 1000**2 )
21 mb per minute of audio per track
so for a 10 track 10 minute project its 2.1gb
```

C++ Tooling
===========

Warnings
--------
Project warnings are set to:
    
    clang
    -Wall -Wno-inconsistent-missing-override -Wno-unused-parameter 

    msvc
    /external:anglebrackets /external:W0
    /w2 
    disabled
    /wd4067 unexpected tokens following preprocessor directive - expected a newline: triggers on semicolon after macro function invocation
    /wd4267 'var' : conversion from 'size_t' to 'type', possible loss of data
    /wd4244
    'argument' : conversion from 'type1' to 'type2', possible loss of data
    A floating point type was converted to an integer type. A possible loss of data may have occurred.

Testcase warnings

    clang (not sure about them)
    -Wall -Wextra -pedantic -Wnon-virtual-dtor -Woverloaded-virtual -Wconversion
    msvc
    /external:anglebrackets /external:W0
    /W3 /Wall

Testcases warnings on MSVC




COMPLETED
=========
clip loop gets notes stuck  
select all muted notes  
make hostinfo plugin a synth and detect stuck notes, (test converted projects for stuck notes by replacing all synths with hostinfo)  
Load/Save current view quickbuttons: F1-F12 keys, hold ctrl to save  
latency compensated automation (write a simple test case for this)  
further optimize waveform rendering  
dynamic scaling for all of the UI (50%-200%)  
seperate internal samplerate / external (audiointerface) samplerate  
Make the backing audio track on midi tracks optional, only active when subtrack waveview is shown  
Performance regression: midi clip processing runtime scales linear to number of loops (expected to scale linearly with loop length)  
audio track routing options  
group tracks  
audio settings dialog  
track snapshots  
plugin gui preset select   
multiselection of plugins (only single consectuive range based)  
make splitter ctr  
implement basic arpeggiator  
optimize waveform rendering  
Missing undo/redo for adding/moving/removing plugins  
Missing undo/redo for plugin parameters  
reordering tracks   
plugin groups  
vu-meters on plugins  
Fold piano roll  
Catch key presses from plugin windows  
implemented tooltips for object inspection (using template class)  
duplicate plugin/plugin groups  
Sends/Return tracks  
UI settings for internal/external samplerate/blocksize settings  
mousewheel scrolling on noteeditor not working  
