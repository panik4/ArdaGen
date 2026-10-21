#pragma once

#include "ArdaGen.h"
#include "areas/AreaGenerationContext.h"
#include <map>
#include <string>
#include <utility>

namespace Arda::UI {

enum class VisualLayerType {
  HEIGHTMAP,
  LANDFORMS,
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
  PROVINCE_BORDERS,
  REGION_BORDERS,
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

enum class VisualLayerComposition { Blend, Overwrite };

struct VisualLayerInfo {
  VisualLayerType type;
  bool toggled;
  float weight;
  std::string name;
  VisualLayerComposition composition;

  VisualLayerInfo(VisualLayerType type, bool toggled, float weight,
                  std::string name, VisualLayerComposition composition)
      : type(type), toggled(toggled), weight(weight), name(std::move(name)),
        composition(composition) {}
};

using VisualLayerSettings = std::map<VisualLayerType, VisualLayerInfo>;

enum class VisualLayerPreset {
  Countries,
  States,
  Provinces,
  Locations,
  Continents,
  StrategicRegions,
  Cultures,
  Religions,
  Localisation
};

VisualLayerSettings createVisualLayerSettings();
VisualLayerSettings createVisualLayerPreset(VisualLayerPreset preset);
Fwg::Gfx::Image renderVisualLayers(
    ArdaGen &generator, int width, int height,
    const Fwg::Cfg &config,
    const Fwg::Areas::AreaGenerationContext &areaContext,
    const VisualLayerSettings &settings,
    int selectedStrategicRegionID = -1);

} // namespace Arda::UI
