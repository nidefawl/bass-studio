
namespace DAW::Host::PluginScanner {
int mainPluginScanner(int argc, char* argv[]);
} // namespace DAW::Host::PluginScanner

int main(int argc, char* argv[]) {
    return DAW::Host::PluginScanner::mainPluginScanner(argc, argv);
}
