#define D_SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

#include "ArdaGen.h"
#include "ArdaUI/ArdaUI.h"
#include "utils/Logging.h"
#include "utils/Startup.h"

int main() {
  Fwg::Utils::Startup::Configuration startup;
  try {
    Fwg::Utils::Startup::loadConfiguration(startup);

    auto generator = std::make_shared<Arda::ArdaGen>();
    Arda::ArdaUI ui;
    const int result = ui.shiny(generator);
    Fwg::Utils::Startup::writeDiagnosticDump("", startup.configDirectory);
    return result;
  } catch (const std::exception &e) {
    Fwg::Utils::Logging::logLine("Application startup/runtime failed: ",
                                 e.what());
    Fwg::Utils::Startup::writeDiagnosticDump(e.what(), startup.configDirectory);
    return -1;
  }
}
