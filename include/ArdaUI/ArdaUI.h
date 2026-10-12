#pragma once
#include "ArdaGen.h"
#include "ArdaPrerequisiteChecker.h"
#include "ArdaUI/LayerVisualisation.h"
#include "UI/FwgUI.h"
#include <string>
#include <vector>
namespace Arda {
class ArdaUI : public Fwg::UI::FwgUI {
  Fwg::Gfx::Image regionSelectMap;
  bool showVisualLayerToggles(UI::VisualLayerSettings &layerVisibility);
  UI::VisualLayerSettings visualLayerInfos = UI::createVisualLayerSettings();

protected:
  std::shared_ptr<Arda::ArdaGen> ardaGen;
  bool redoDevelopment = false;
  bool redoPopulation = false;
  bool redoTopography = false;
  bool redoCulture = false;
  bool redoLocations = false;
  bool resetSelection = false;
  std::string simulationStatus;
  LanguageGenerator languageGenerator;
  std::shared_ptr<Arda::ArdaRegion> getSelectedRegion();

public:
  ArdaUI();
  int shiny(std::shared_ptr<Arda::ArdaGen> &ardaGen);
  void overview(Fwg::Cfg &cfg);
  void automapAreas();
  bool scenarioGenReady(bool printIssue);
  void showCivilizationTab(Fwg::Cfg &cfg);
  void showDevelopmentTab(Fwg::Cfg &cfg);
  void showPopulationTab(Fwg::Cfg &cfg);
  void showTopographyTab(Fwg::Cfg &cfg);
  void showLocationTab(Fwg::Cfg &cfg);
  void showNavmeshTab(Fwg::Cfg &cfg);
  void showCultureTab(Fwg::Cfg &cfg);
  void showReligionTab(Fwg::Cfg &cfg);
  void showLanguageTab(Fwg::Cfg &cfg);
};
} // namespace Arda