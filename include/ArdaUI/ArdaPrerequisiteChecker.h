#pragma once
#include "ArdaGen.h"
#include "UI/Prerequisites.h"

namespace Arda::UI {

// Inherit all base prerequisites and add Arda-specific ones
class ArdaPrerequisiteChecker : public Fwg::UI::PrerequisiteChecker {
public:
  // =========================================================================
  // Arda-Specific Prerequisites
  // =========================================================================

  static Fwg::UI::Prerequisite ardaProvinces(const ArdaGen &ardaGen) {
    return {"Arda Provinces", "Map provinces to Arda data first",
            [&]() { return !ardaGen.ardaProvinces.empty(); }};
  }

  static Fwg::UI::Prerequisite ardaRegions(const ArdaGen &ardaGen) {
    return {"Arda Regions", "Map regions to Arda data first",
            [&]() { return !ardaGen.ardaRegions.empty(); }};
  }

  static Fwg::UI::Prerequisite ardaContinents(const ArdaGen &ardaGen) {
    return {"Arda Continents", "Map continents to Arda data first",
            [&]() { return !ardaGen.ardaContinents.empty(); }};
  }

  static Fwg::UI::Prerequisite development(const ArdaGen &ardaGen) {
    return {"Development Data", "Generate development data first", [&]() {
              return !ardaGen.ardaProvinces.empty() &&
                     ardaGen.ardaProvinces[0]->averageDevelopment >= 0;
            }};
  }

  static Fwg::UI::Prerequisite population(const ArdaGen &ardaGen) {
    return {"Population Data", "Generate population data first", [&]() {
              return !ardaGen.ardaProvinces.empty() &&
                     ardaGen.ardaProvinces[0]->populationDensity >= 0;
            }};
  }

  static Fwg::UI::Prerequisite cultures(const ArdaGen &ardaGen) {
    return {"Culture Data", "Generate culture data first", [&]() {
              return !ardaGen.ardaRegions.empty() &&
                     ardaGen.ardaRegions[0]->getPrimaryCulture() != nullptr;
            }};
  }

  static Fwg::UI::Prerequisite locations(const ArdaGen &ardaGen) {
    return {"Locations", "Generate locations first", [&]() {
              for (const auto &region : ardaGen.ardaRegions) {
                if (!region->locations.empty())
                  return true;
              }
              return false;
            }};
  }
  static Fwg::UI::Prerequisite strategicRegions(const ArdaGen &generator) {
    return {"Strategic Regions", "Generate strategic regions first",
            [&]() { return !generator.superRegions.empty(); }};
  }

  static Fwg::UI::Prerequisite countries(const ArdaGen &generator) {
    return {"Countries", "Generate countries first",
            [&]() { return !generator.countries.empty(); }};
  }
};

} // namespace Arda::UI