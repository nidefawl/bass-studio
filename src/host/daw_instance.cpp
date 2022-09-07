#include "mainctrl.h"

namespace DAW {
String MakeUniqueTrackName(project_t* project, const String& strNewName) {
    auto& trackCtr   = project->trackList;
    int offset       = 0;
    while (offset < 100) {
        String test = strNewName;
        if (offset > 0) {
            test += StringFormat(" %d", offset);
        }
        auto it = std::find_if(trackCtr.begin(), trackCtr.end(), [&test](const track_t* tr) {
            return tr->name == test;
        });
        if (it == trackCtr.end())
            return test;
        offset++;
    }
    return strNewName;
}
}