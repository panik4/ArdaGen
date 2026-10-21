#pragma once
#include "ArdaGen.h"
#include "ArdaPrerequisiteChecker.h"
#include "ArdaUI/LayerVisualisation.h"
#include "UI/FwgUI.h"
#include <optional>
#include <string>
#include <vector>
namespace Arda {
class ArdaUI : public Fwg::UI::FwgUI {
  std::shared_ptr<Arda::ArdaGen> standaloneArdaGenerator;
  int selectedRegionId = -1;
  bool showVisualLayerToggles(UI::VisualLayerSettings &layerVisibility);
  UI::VisualLayerSettings visualLayerInfos = UI::createVisualLayerSettings();

protected:
  Arda::ArdaGen &currentGenerator() override;
  struct MapSelection {
    Fwg::UI::Drawing::ClickEvent click;
    std::shared_ptr<Fwg::Areas::Province> province;
    std::shared_ptr<Arda::ArdaRegion> region;
  };
  std::optional<MapSelection> consumeMapSelection();
  void resetMapSelection();
  std::string windowTitle() const override;
  std::string frameTitle() const override;
  void renderSettingsTabs(Fwg::Cfg &cfg) override;
  void setCivilisationDirty(bool dirty);
  bool redoDevelopment = false;
  bool redoPopulation = false;
  bool redoTopography = false;
  bool redoCulture = false;
  bool redoLocations = false;
  bool resetSelection = false;
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
};
} // namespace Arda
