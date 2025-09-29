#pragma once
#include "ArdaGen.h"
#include "UI/fwgUI.h"
#include <d3d11.h>
#include <string>
#include <tchar.h>
#include <vector>
namespace Arda {
class ArdaUI : public Fwg::FwgUI {
  enum class VisualLayerType {
    HEIGHTMAP,
    ELEVATIONTYPES,
    TOPOGRAPHY,
    NORMALMAP,
    INCLINATION,
    RELSURROUNDINGS,
    HUMIDITY,
    TEMPERATURE,
    CLIMATE,
    TREECLIMATE,
    TREEDENSITY,
    HABITABILITY,
    SUPERSEGMENTS,
    SEGMENTS,
    PROVINCES,
    REGIONS,
    REGIONSWITHPROVINCES,
    REGIONSWITHBORDERS,
    CONTINENTS,
    POPULATION,
    DEVELOPMENT,
    ARABLELAND,
    LOCATIONS,
    WORLD_MAP,
    CIVILISATION_MAP,
    SUPERREGIONS,
    COUNTRIES,
    CULTUREGROUPS,
    CULTURES,
    RELIGIONS
  };

  struct VisualLayerInfo {
    VisualLayerType type;
    bool toggled = false;
    float weight = 1.0f;
    std::string name;
    bool overlay;
  };

  Fwg::Gfx::Bitmap regionSelectMap;
  // configuration

  bool showVisualLayerToggles(
      std::map<VisualLayerType, VisualLayerInfo> &layerVisibility);
  // generic scenario stuff
  // int showScenarioTab(Fwg::Cfg &cfg,
  //                    std::shared_ptr<Rpx::GenericModule> genericModule);
  // void countryEdit(std::shared_ptr<Arda::ArdaGen> generator);
  // int showCountryTab(Fwg::Cfg &cfg);

  // int showStrategicRegionTab(Fwg::Cfg &cfg,
  //                            std::shared_ptr<Rpx::ModGenerator> generator);

  std::map<VisualLayerType, VisualLayerInfo> visualLayerInfos = {
      {VisualLayerType::HEIGHTMAP,
       {VisualLayerType::HEIGHTMAP, false, 1.0f, "Heightmap"}},
      {VisualLayerType::ELEVATIONTYPES,
       {VisualLayerType::ELEVATIONTYPES, false, 1.0f, "Elevation Types"}},
      {VisualLayerType::TOPOGRAPHY,
       {VisualLayerType::TOPOGRAPHY, false, 1.0f, "Topography"}},
      {VisualLayerType::NORMALMAP,
       {VisualLayerType::NORMALMAP, false, 1.0f, "Normal Map"}},
      {VisualLayerType::INCLINATION,
       {VisualLayerType::INCLINATION, false, 1.0f, "Inclination"}},
      {VisualLayerType::RELSURROUNDINGS,
       {VisualLayerType::RELSURROUNDINGS, false, 1.0f,
        "Relative Surroundings"}},
      {VisualLayerType::HUMIDITY,
       {VisualLayerType::HUMIDITY, false, 1.0f, "Humidity"}},
      {VisualLayerType::TEMPERATURE,
       {VisualLayerType::TEMPERATURE, false, 1.0f, "Temperature"}},
      {VisualLayerType::CLIMATE,
       {VisualLayerType::CLIMATE, false, 1.0f, "Climate"}},
      {VisualLayerType::TREECLIMATE,
       {VisualLayerType::TREECLIMATE, false, 1.0f, "Tree Climate"}},
      {VisualLayerType::TREEDENSITY,
       {VisualLayerType::TREEDENSITY, false, 1.0f, "Tree Density"}},
      {VisualLayerType::HABITABILITY,
       {VisualLayerType::HABITABILITY, false, 1.0f, "Habitability"}},
      {VisualLayerType::ARABLELAND,
       {VisualLayerType::ARABLELAND, false, 1.0f, "Arable Land"}},
      {VisualLayerType::SUPERSEGMENTS,
       {VisualLayerType::SUPERSEGMENTS, false, 1.0f, "Supersegments"}},
      {VisualLayerType::SEGMENTS,
       {VisualLayerType::SEGMENTS, false, 1.0f, "Segments"}},
      {VisualLayerType::PROVINCES,
       {VisualLayerType::PROVINCES, false, 1.0f, "Provinces"}},
      {VisualLayerType::REGIONS,
       {VisualLayerType::REGIONS, false, 1.0f, "Regions"}},
      {VisualLayerType::REGIONSWITHPROVINCES,
       {VisualLayerType::REGIONSWITHPROVINCES, false, 1.0f,
        "Regions + Provinces"}},
      {VisualLayerType::REGIONSWITHBORDERS,
       {VisualLayerType::REGIONSWITHBORDERS, false, 1.0f,
        "Regions with Borders"}},
      {VisualLayerType::SUPERREGIONS,
       {VisualLayerType::SUPERREGIONS, false, 1.0f, "Superregions"}},
      {VisualLayerType::CONTINENTS,
       {VisualLayerType::CONTINENTS, false, 1.0f, "Continents"}},
      {VisualLayerType::POPULATION,
       {VisualLayerType::POPULATION, false, 1.0f, "Population"}},
      {VisualLayerType::DEVELOPMENT,
       {VisualLayerType::DEVELOPMENT, false, 1.0f, "Development"}},
      {VisualLayerType::LOCATIONS,
       {VisualLayerType::LOCATIONS, false, 1.0f, "Locations"}},
      {VisualLayerType::WORLD_MAP,
       {VisualLayerType::WORLD_MAP, false, 1.0f, "World Map"}},
      {VisualLayerType::CIVILISATION_MAP,
       {VisualLayerType::CIVILISATION_MAP, false, 1.0f, "Civilisation Map"}},
      {VisualLayerType::COUNTRIES,
       {VisualLayerType::COUNTRIES, false, 1.0f, "Countries"}},
      {VisualLayerType::CULTUREGROUPS,
       {VisualLayerType::CULTUREGROUPS, false, 1.0f, "Culture Groups"}},
      {VisualLayerType::CULTURES,
       {VisualLayerType::CULTURES, false, 1.0f, "Cultures"}},
      {VisualLayerType::RELIGIONS,
       {VisualLayerType::RELIGIONS, false, 1.0f, "Religions"}},
  };

protected:
  std::shared_ptr<Arda::ArdaGen> ardaGen;
  bool configuredScenarioGen = false;
  LanguageGenerator languageGenerator;

public:
  ArdaUI();
  int shiny(std::shared_ptr<Arda::ArdaGen> &ardaGen);
  void overview(std::shared_ptr<Arda::ArdaGen> &ardaGen, Fwg::Cfg &cfg);
  bool scenarioGenReady(bool printIssue);
  void showCivilizationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showDevelopmentTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showPopulationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showUrbanisationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showAgricultureTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showLocationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showNavmeshTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg);
  void showCultureTab(Fwg::Cfg &cfg);
  void showReligionTab(Fwg::Cfg &cfg);
  void showLanguageTab(Fwg::Cfg &cfg);

};
} // namespace Arda