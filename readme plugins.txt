Due to a GDB bug gcc-built debug DLLs can't be used with debugger attached on DAW, it will cause following error:
Solution: Use msvc built dlls (I have not tested gcc dlls without debug info)

gdb versino 8.1.0 gives same error:

504,379 =thread-created,id="40",group-id="i1"
504,379 ~"[New Thread 20148.0x445c]\n"
504,380 &"warning: onecoreuap\\inetcore\\urlmon\\zones\\zoneidentifier.cxx(359)\\urlmon.dll!00007FFF\
41170450: (caller: 00007FFF4117020D) ReturnHr(2) tid(4e04) 80070002 Das System kann die angegebene D\
atei nicht finden.\r\n"
504,384 =thread-created,id="41",group-id="i1"
504,384 ~"[New Thread 20148.0x22f8]\n"
504,608 vst_host.cpp:67 cbPrintf: Bionic Supa Delay - ED 1_0 depr audioMasterWantMidi 0 6 1
504,608 pluginctr.cpp:660 pluginEntryDragRelease: Insert effect on guictr_plugins, parent <null>
504,608 track.cpp:328 insertEffect: Insert effect at idx 0
504,608 base_plugin.cpp:67 setTrackLink: Update audiostage of guictr_plugins which is default
504,608 plugin/../../gui/plugin.h:75 setTitle: SET TITLE Bionic Supa Delay - ED 1_0
504,608 vst_host.cpp:67 cbPrintf: Bionic Supa Delay - ED 1_0 depr audioMasterWantMidi 0 6 1
506,679 vst_host.cpp:67 cbPrintf: Bionic Supa Delay - ED 1_0 audioMasterUpdateDisplay 0 42 0
518,621 textfield.cpp:273 focusEvent: focusEvent 1 1
518,958 ~"[Thread 20148.0x4548 exited with code 0]\n"
518,958 =thread-exited,id="38",group-id="i1"
565,562 textfield.cpp:273 focusEvent: focusEvent 1 0
568,513 =library-loaded,id="C:\\PluginManager\\configs\\default\\hosts\\Ableton\\categories\\dev\\te\
stplugin_adv.dll",target-name="C:\\PluginManager\\configs\\default\\hosts\\Ableton\\categories\\dev\\
\testplugin_adv.dll",host-name="C:\\PluginManager\\configs\\default\\hosts\\Ableton\\categories\\dev\
\\testplugin_adv.dll",symbols-loaded="0",thread-group="i1",ranges=[{from="0x0000000065ec1000",to="0x\
0000000066092970"}]
568,759 ~"../../../../src/gdb-8.0.1/gdb/breakpoint.c:6384: internal-error: void print_one_breakpoint\
_location(breakpoint*, bp_location*, int, bp_location**, int): Assertion `b->loc == NULL || b->loc->\
next == NULL' failed.\nA problem internal to GDB has been detected,\nfurther debugging may prove unr\
eliable.\nQuit this debugging session? "
568,759 ~"(y or n) [answered Y; input not from terminal]\n"
568,759 &"\nThis is a bug, please report it."
568,759 &"  For instructions, see:\n<http://www.gnu.org/software/gdb/bugs/>."
568,759 &"\n\n"
568,759 ~"../../../../src/gdb-8.0.1/gdb/breakpoint.c:6384: internal-error: void print_one_breakpoint\
_location(breakpoint*, bp_location*, int, bp_location**, int): Assertion `b->loc == NULL || b->loc->\
next == NULL' failed.\nA problem internal to GDB has been detected,\nfurther debugging may prove unr\
eliable.\nCreate a core file of GDB? "
568,759 ~"(y or n) [answered Y; input not from terminal]\n"
