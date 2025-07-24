#define D_SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include "FastWorldGenerator.h"
#include "UI/fwgUI.h"
#include "Utils/Logging.h"
#include "Utils/Utils.h"
#include <filesystem>

void dumpInfo(const std::string &error, const std::string &configSubFolder) {
  std::string dump = "";
  std::string path = configSubFolder;
  if (path.length() > 0) {
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_directory()) {
        continue; // Skip directories
      }
      dump += Fwg::Parsing::readFile(entry.path().string());
    }
  }
  dump += std::to_string(Fwg::Cfg::Values().seed);
  dump += "\n";
  for (auto layerSeed : Fwg::Cfg::Values().seeds) {
    dump += std::to_string(layerSeed);
    dump += "\n";
  }
  dump += error;
  dump += Fwg::Utils::Logging::Logger::logInstance.getFullLog();
  Fwg::Parsing::writeFile("log.txt", dump);
}

int main() {
  Fwg::Utils::Logging::logLine("Starting the config loading");
  // Short alias for this namespace
  namespace pt = boost::property_tree;
  // Create a root

  pt::ptree metaConf;
  try {
    Fwg::Utils::Logging::logLine("Starting the loading of MetaConf.json");
    std::ifstream f("MetaConf.json");
    std::stringstream buffer;
    if (!f.good()) {
      Fwg::Utils::Logging::logLine("Config could not be loaded");
    }
    buffer << f.rdbuf();
    Fwg::Parsing::replaceInStringStream(buffer, "\\", "//");
    pt::read_json(buffer, metaConf);
  } catch (std::exception e) {
    Fwg::Utils::Logging::logLine("Incorrect config \"MetaConf.json\"");
    Fwg::Utils::Logging::logLine("You can try fixing it yourself. Error is: ",
                                 e.what());
    Fwg::Utils::Logging::logLine(
        "Otherwise try running it through a json validator, e.g. "
        "\"https://jsonlint.com/\" or search for \"json validator\"");
    dumpInfo(e.what(), "");
    return -1;
  }
  std::string username = metaConf.get<std::string>("config.username");
  std::string workingDirectory =
      metaConf.get<std::string>("config.workingDirectory");
  std::string configSubFolder =
      workingDirectory + metaConf.get<std::string>("config.configSubFolder");

    auto &config = Fwg::Cfg::Values();
  config.workingDirectory = workingDirectory;
  // check if we can read the config
  try {
    Fwg::Utils::Logging::logLine("Starting the loading of ",
                                 configSubFolder + "FastWorldGenerator.json");
    config.readConfig(configSubFolder);
  } catch (std::exception e) {
    Fwg::Utils::Logging::logLine(
        "Incorrect config \"FastWorldGenerator.json\"");
    Fwg::Utils::Logging::logLine("You can try fixing it yourself. Error is: ",
                            e.what());
    Fwg::Utils::Logging::logLine(
        "Otherwise try running it through a json validator, e.g. "
        "\"https://jsonlint.com/\" or \"search for json validator\"");
    dumpInfo(e.what(), configSubFolder);
    return -1;
  }


  Fwg::FastWorldGenerator fwg;
  Fwg::fwgUI ui;
  ui.shiny(fwg);
}