#include "ArdaUI/LayerVisualisation.h"

namespace Arda::UI {

VisualLayerSettings createVisualLayerSettings() {
  VisualLayerSettings settings;
  const auto add = [&settings](VisualLayerType type, const char *name,
                               VisualLayerComposition composition =
                                   VisualLayerComposition::Blend) {
    settings.emplace(type,
                     VisualLayerInfo(type, false, 1.0f, name, composition));
  };

  add(VisualLayerType::HEIGHTMAP, "Heightmap");
  add(VisualLayerType::LANDFORMS, "Elevation Types");
  add(VisualLayerType::TOPOGRAPHY, "Topography");
  add(VisualLayerType::NORMALMAP, "Normal Map");
  add(VisualLayerType::INCLINATION, "Inclination");
  add(VisualLayerType::RELSURROUNDINGS, "Relative Surroundings");
  add(VisualLayerType::HUMIDITY, "Humidity");
  add(VisualLayerType::TEMPERATURE, "Temperature");
  add(VisualLayerType::CLIMATE, "Climate");
  add(VisualLayerType::TREECLIMATE, "Tree Climate");
  add(VisualLayerType::TREEDENSITY, "Tree Density");
  add(VisualLayerType::HABITABILITY, "Habitability");
  add(VisualLayerType::ARABLELAND, "Arable Land");
  add(VisualLayerType::SUPERSEGMENTS, "Supersegments");
  add(VisualLayerType::SEGMENTS, "Segments");
  add(VisualLayerType::PROVINCES, "Provinces");
  add(VisualLayerType::REGIONS, "Regions");
  add(VisualLayerType::PROVINCE_BORDERS, "Province Borders",
      VisualLayerComposition::Overwrite);
  add(VisualLayerType::REGION_BORDERS, "Region Borders",
      VisualLayerComposition::Overwrite);
  add(VisualLayerType::SUPERREGIONS, "Superregions");
  add(VisualLayerType::CONTINENTS, "Continents");
  add(VisualLayerType::POPULATION, "Population");
  add(VisualLayerType::DEVELOPMENT, "Development");
  add(VisualLayerType::LOCATIONS, "Locations");
  add(VisualLayerType::WORLD_MAP, "World Map");
  add(VisualLayerType::CIVILISATION_MAP, "Civilisation Map");
  add(VisualLayerType::COUNTRIES, "Countries");
  add(VisualLayerType::CULTUREGROUPS, "Culture Groups");
  add(VisualLayerType::CULTURES, "Cultures");
  add(VisualLayerType::RELIGIONS, "Religions");
  return settings;
}

VisualLayerSettings createVisualLayerPreset(const VisualLayerPreset preset) {
  auto settings = createVisualLayerSettings();
  const auto enable = [&settings](const VisualLayerType type,
                                  const float weight = 1.0f) {
    auto &layer = settings.at(type);
    layer.toggled = true;
    layer.weight = weight;
  };

  switch (preset) {
  case VisualLayerPreset::Countries:
    enable(VisualLayerType::COUNTRIES);
    enable(VisualLayerType::REGION_BORDERS);
    break;
  case VisualLayerPreset::States:
    enable(VisualLayerType::COUNTRIES);
    enable(VisualLayerType::PROVINCE_BORDERS);
    enable(VisualLayerType::REGION_BORDERS);
    break;
  case VisualLayerPreset::Provinces:
    enable(VisualLayerType::PROVINCES);
    enable(VisualLayerType::PROVINCE_BORDERS);
    break;
  case VisualLayerPreset::Locations:
    enable(VisualLayerType::WORLD_MAP);
    enable(VisualLayerType::LOCATIONS);
    break;
  case VisualLayerPreset::Continents:
    enable(VisualLayerType::CONTINENTS);
    enable(VisualLayerType::PROVINCE_BORDERS);
    break;
  case VisualLayerPreset::StrategicRegions:
    enable(VisualLayerType::SUPERREGIONS);
    enable(VisualLayerType::REGION_BORDERS);
    break;
  case VisualLayerPreset::Cultures:
    enable(VisualLayerType::CULTURES);
    break;
  case VisualLayerPreset::Religions:
    enable(VisualLayerType::RELIGIONS);
    break;
  case VisualLayerPreset::Localisation:
    enable(VisualLayerType::CULTUREGROUPS);
    break;
  }
  return settings;
}

Fwg::Gfx::Image renderVisualLayers(
    ArdaGen &generator, const int width, const int height,
    const Fwg::Areas::AreaGenerationContext &areaContext,
    const VisualLayerSettings &settings,
    const int selectedStrategicRegionID) {
  std::vector<Fwg::Gfx::WeightedImage> layers;
  for (const auto &[type, info] : settings) {
    if (!info.toggled || info.composition != VisualLayerComposition::Blend) {
      continue;
    }

    switch (type) {
    case VisualLayerType::HEIGHTMAP:
      layers.push_back(
          {Fwg::Gfx::displayHeightMap(generator.terrainData.detailedHeightMap),
           info.weight});
      break;
    case VisualLayerType::LANDFORMS:
      layers.push_back(
          {Fwg::Gfx::landFormMap(generator.terrainData), info.weight});
      break;
    case VisualLayerType::TOPOGRAPHY:
      layers.push_back(
          {Fwg::Gfx::topographyMap(generator.terrainData.altitudes),
           info.weight});
      break;
    case VisualLayerType::NORMALMAP:
      layers.push_back(
          {Fwg::Gfx::displaySobelMap(generator.terrainData.sobelData),
           info.weight});
      break;
    case VisualLayerType::INCLINATION:
      layers.push_back(
          {Fwg::Gfx::displayHeightMap(generator.terrainData.inclination),
           info.weight});
      break;
    case VisualLayerType::RELSURROUNDINGS:
      layers.push_back(
          {Fwg::Gfx::displayHeightMap(generator.terrainData.distance),
           info.weight});
      break;
    case VisualLayerType::HUMIDITY:
      layers.push_back(
          {Fwg::Gfx::Climate::displayHumidity(generator.climateData),
           info.weight});
      break;
    case VisualLayerType::TEMPERATURE:
      layers.push_back(
          {Fwg::Gfx::Climate::displayTemperature(generator.climateData),
           info.weight});
      break;
    case VisualLayerType::CLIMATE:
      layers.push_back(
          {Fwg::Gfx::Climate::displayClimate(generator.climateData, false),
           info.weight});
      break;
    case VisualLayerType::TREECLIMATE:
      layers.push_back(
          {Fwg::Gfx::Climate::displayClimate(generator.climateData, true),
           info.weight});
      break;
    case VisualLayerType::TREEDENSITY:
      layers.push_back(
          {Fwg::Gfx::Climate::displayTreeDensity(generator.climateData),
           info.weight});
      break;
    case VisualLayerType::HABITABILITY:
      layers.push_back(
          {Fwg::Gfx::displayHabitability(generator.climateData.habitabilities),
           info.weight});
      break;
    case VisualLayerType::ARABLELAND:
      layers.push_back(
          {Fwg::Gfx::Climate::displayArableLand(generator.climateData),
           info.weight});
      break;
    case VisualLayerType::SUPERSEGMENTS:
      layers.push_back({Fwg::Gfx::Segments::displaySuperSegments(
                            generator.areaData.superSegments, areaContext),
                        info.weight});
      break;
    case VisualLayerType::SEGMENTS:
      layers.push_back(
          {Fwg::Gfx::Segments::displaySegments(generator.areaData.segments,
                                               areaContext),
           info.weight});
      break;
    case VisualLayerType::PROVINCES:
      layers.push_back({generator.provinceMap, info.weight});
      break;
    case VisualLayerType::REGIONS:
      layers.push_back({generator.regionMap, info.weight});
      break;
    case VisualLayerType::PROVINCE_BORDERS:
    case VisualLayerType::REGION_BORDERS:
      break;
    case VisualLayerType::SUPERREGIONS:
      layers.push_back({Arda::Gfx::visualiseStrategicRegions(
                            generator.superRegionMap, generator.superRegions,
                            selectedStrategicRegionID),
                        info.weight});
      break;
    case VisualLayerType::CONTINENTS:
      layers.push_back(
          {Fwg::Gfx::simpleContinents(generator.areaData.continents,
                                      generator.areaData.seaBodies,
                                      areaContext),
           info.weight});
      break;
    case VisualLayerType::POPULATION:
      layers.push_back(
          {Arda::Gfx::displayPopulation(generator.ardaProvinces), info.weight});
      break;
    case VisualLayerType::DEVELOPMENT:
      layers.push_back({Arda::Gfx::displayDevelopment(generator.ardaProvinces),
                        info.weight});
      break;
    case VisualLayerType::LOCATIONS:
      layers.push_back({Arda::Gfx::displayLocations(generator.areaData.regions,
                                                    generator.worldMap),
                        info.weight});
      break;
    case VisualLayerType::WORLD_MAP:
      layers.push_back({generator.worldMap, info.weight});
      break;
    case VisualLayerType::CIVILISATION_MAP:
      layers.push_back({Fwg::Gfx::displayWorldCivilisationMap(
                            generator.climateData, generator.provinceMap,
                            generator.worldMap, generator.regionMap, ""),
                        info.weight});
      break;
    case VisualLayerType::COUNTRIES:
      layers.push_back({generator.countryMap, info.weight});
      break;
    case VisualLayerType::CULTUREGROUPS:
      layers.push_back(
          {Arda::Gfx::displayCultureGroups(generator.ardaProvinces),
           info.weight});
      break;
    case VisualLayerType::CULTURES:
      layers.push_back(
          {Arda::Gfx::displayCultures(generator.ardaProvinces), info.weight});
      break;
    case VisualLayerType::RELIGIONS:
      layers.push_back(
          {Arda::Gfx::displayReligions(generator.ardaProvinces), info.weight});
      break;
    }
  }
  auto composedImage = Fwg::Gfx::mergeWeightedImages(width, height, layers);
  for (const auto &[type, info] : settings) {
    if (!info.toggled ||
        info.composition != VisualLayerComposition::Overwrite) {
      continue;
    }

    switch (type) {
    case VisualLayerType::PROVINCE_BORDERS:
      Fwg::Gfx::applyAreaBorders(composedImage, generator.areaData.provinces);
      break;
    case VisualLayerType::REGION_BORDERS:
      Fwg::Gfx::applyAreaBorders(composedImage, generator.ardaRegions);
      break;
    default:
      break;
    }
  }
  return composedImage;
}

} // namespace Arda::UI
