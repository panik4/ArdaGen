#include "ArdaUI/ArdaUI.h"
#include <algorithm>

#include <filesystem>
#include <misc/cpp/imgui_stdlib.h>

namespace Arda {

ArdaUI::ArdaUI()
    : FwgUI(), languageGenerator(Fwg::Cfg::Values().resourcePath +
                                 "/names/languageGroups/") {}

Arda::ArdaGen &ArdaUI::currentGenerator() { return *standaloneArdaGenerator; }

std::string ArdaUI::windowTitle() const { return "ArdaGen 0.11.1"; }
std::string ArdaUI::frameTitle() const { return "ArdaGen"; }

void ArdaUI::renderSettingsTabs(Fwg::Cfg &cfg) {
  defaultTabs(cfg, currentGenerator());
  automapAreas();
  showCivilizationTab(cfg);
  overview(cfg);
}

int ArdaUI::shiny(std::shared_ptr<Arda::ArdaGen> &ardaGen) {
  standaloneArdaGenerator = ardaGen;
  if (!standaloneArdaGenerator)
    return -1;
  resetMapSelection();
  return runApplication();
}

void ArdaUI::setCivilisationDirty(bool dirty) {
  redoDevelopment = dirty;
  redoPopulation = dirty;
  redoTopography = dirty;
  redoCulture = dirty;
  redoLocations = dirty;
}

void ArdaUI::automapAreas() {
  if (uiContext.generationContext.modifiedAreas &&
      !uiContext.asyncContext.busy()) {
    currentGenerator().mapProvinces();
    currentGenerator().mapRegions();
    currentGenerator().mapContinents();
    uiContext.generationContext.modifiedAreas = false;
    setCivilisationDirty(true);
  }
}

bool ArdaUI::scenarioGenReady(bool printIssue) {
  bool ready = true;

  auto &generator = currentGenerator();

  if (!generator.terrainData.detailedHeightMap.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Detailed heightmap is missing.");
    ready = false;
  }

  if (!generator.climateMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Climate map not initialised.");
    ready = false;
  }

  if (!generator.provinceMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Province map not initialised.");
    ready = false;
  }

  if (!generator.regionMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Region map not initialised.");
    ready = false;
  }

  if (!generator.worldMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("World map not initialised.");
    ready = false;
  }

  if (!generator.areaData.provinces.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("No provinces defined.");
    ready = false;
  }

  if (!generator.areaData.regions.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("No regions defined.");
    ready = false;
  }

  return ready;
}

void ArdaUI::showCivilizationTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginMainTabItem(
          "Civilisation", redoCulture || redoDevelopment || redoPopulation)) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.resetTexture();
    }
    uiContext.helpContext.showHelpTextBox("Civilisation");

    ImGui::SeparatorText("Civilisation Parameters");

    {
      Fwg::UI::Elements::GridLayout grid(2, scaled(250.0f), scaled(12.0f));

      auto &civilizationConfig =
          currentGenerator().ardaConfig.civilizationConfig;
      grid.AddInputDouble("Development Influence",
                          &civilizationConfig.developmentInfluence, 0.0, 10.0);
      grid.AddInputDouble("Min Development",
                          &civilizationConfig.minimumDevelopment, 0.0, 10.0);
      grid.AddInputDouble("Max Development",
                          &civilizationConfig.maximumDevelopment, 0.0, 10.0);
      grid.AddInputDouble(
          "Urban Factor",
          &currentGenerator().ardaConfig.locationConfig.urbanFactor, 0.0, 10.0);
      grid.AddInputDouble(
          "Agriculture Factor",
          &currentGenerator().ardaConfig.locationConfig.agricultureFactor, 0.0,
          10.0);
    }

    ImGui::Spacing();

    // scoping the guard to ensure only the button is disabled, not the whole
    // tab, so the user can still edit parameters and go through tabs
    {
      auto guard = Fwg::UI::PrerequisiteChecker::require(
          {Fwg::UI::PrerequisiteChecker::climate(
               currentGenerator().climateData),
           Fwg::UI::PrerequisiteChecker::landforms(
               currentGenerator().terrainData),
           Fwg::UI::PrerequisiteChecker::habitability(
               currentGenerator().climateData),
           Fwg::UI::PrerequisiteChecker::superSegments(
               currentGenerator().areaData),
           Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
           Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
           Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
           Fwg::UI::PrerequisiteChecker::continents(
               currentGenerator().areaData)});
      if (guard.ready()) {
        if (Fwg::UI::Elements::AutomationStepButton(
                "Generate all civilisation data automatically")) {
          uiContext.asyncContext.runAsync(
              [this]() {
                currentGenerator().genCivilisationData();
                return true;
              },
              [this](Fwg::UI::TaskResult::Outcome outcome) {
                setCivilisationDirty(outcome ==
                                     Fwg::UI::TaskResult::Outcome::Failure);
              });
        }
      }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Manually edit civ data");

    if (Fwg::UI::Elements::BeginSubTabBar("Civilisation stuff")) {
      showTopographyTab(cfg);
      showDevelopmentTab(cfg);
      showPopulationTab(cfg);
      showCultureTab(cfg);
      showLocationTab(cfg);

      Fwg::UI::Elements::EndSubTabBar();
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showDevelopmentTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Development", redoDevelopment)) {
    if (uiContext.tabSwitchEvent(true)) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayDevelopment(
                 currentGenerator().ardaData.areaData.provinces));
      uiContext.imageContext.updateImage(1, currentGenerator().worldMap);
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});
    if (guard.ready()) {
      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        currentGenerator().loadDevelopment(cfg, uiContext.draggedFile);
        uiContext.imageContext.resetTexture();
        redoDevelopment = false;
      }

      ImGui::SeparatorText("Continent Development Modifiers");
      ImGui::TextWrapped("Base development affects the overall level of "
                         "development on a continent. It is still dependent on "
                         "climate and habitability.");

      {
        Fwg::UI::Elements::GridLayout grid(2, scaled(250.0f), scaled(12.0f));

        for (auto &continent :
             currentGenerator().ardaData.areaData.continents) {
          std::string label = "Continent " + std::to_string(continent->ID);
          grid.AddInputDouble(label.c_str(), &continent->developmentModifier,
                              0.0, 10.0);
        }
      }

      ImGui::Spacing();

      if (Fwg::UI::Elements::Button("Generate with Random Modifiers", false,
                                    ImVec2(250, 0))) {
        currentGenerator().ardaConfig.civilizationConfig.randomDevelopment =
            true;
        uiContext.asyncContext.runAsync(
            [&cfg, this]() { return currentGenerator().genDevelopment(cfg); },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoDevelopment =
                  outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::SameLine();

      if (Fwg::UI::Elements::ImportantStepButton(
              "Generate with Current Modifiers", ImVec2(250, 0))) {
        uiContext.asyncContext.runAsync(
            [&cfg, this]() {
              currentGenerator()
                  .ardaConfig.civilizationConfig.randomDevelopment = false;
              return currentGenerator().genDevelopment(cfg);
            },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoDevelopment =
                  outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Edit Selected Continent");

      // get the clicked state
      auto modifiableState = getSelectedRegion();
      if (modifiableState != nullptr && modifiableState->continentID >= 0 &&
          modifiableState->continentID <
              currentGenerator().ardaData.areaData.continents.size()) {
        ImGui::TextWrapped("Click a continent in the image and then modify the "
                           "base development modifiers.");

        auto continent =
            currentGenerator()
                .ardaData.areaData.continents[modifiableState->continentID];

        {
          Fwg::UI::Elements::GridLayout grid(2, scaled(250.0f), scaled(12.0f));

          grid.AddText("Selected Continent", "%d",
                       modifiableState->continentID);

          if (grid.AddInputDouble("Development Modifier",
                                  &continent->developmentModifier, 0.0, 10.0)) {
            uiContext.asyncContext.runAsync(
                [&cfg, this]() {
                  currentGenerator()
                      .ardaConfig.civilizationConfig.randomDevelopment = false;
                  return currentGenerator().genDevelopment(cfg);
                },
                [this](Fwg::UI::TaskResult::Outcome outcome) {
                  redoDevelopment =
                      outcome == Fwg::UI::TaskResult::Outcome::Failure;
                });
          }
        }
      } else {
        ImGui::TextColored(
            ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
            "No continent selected. Click on a continent to select it.");
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showPopulationTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Population", redoPopulation)) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayPopulation(
                 currentGenerator().ardaData.areaData.provinces));
      uiContext.imageContext.updateImage(1, currentGenerator().worldMap);
    }

    ImGui::SeparatorText("Population Configuration");

    {
      Fwg::UI::Elements::GridLayout grid(2, scaled(250.0f), scaled(12.0f));

      if (grid.AddInputDouble(
              "World Population Factor",
              &currentGenerator().ardaConfig.worldPopulationFactor, 0.0,
              10.0)) {
        currentGenerator().ardaConfig.calculateTargetWorldPopulation();
      }

      grid.AddText("Target Population", "%.1f Mio",
                   currentGenerator().ardaConfig.targetWorldPopulation /
                       1000'000.0);

      grid.AddText("Current Population", "%.1f Mio",
                   currentGenerator().ardaStats.totalWorldPopulation /
                       1000'000.0);
    }

    ImGui::Spacing();

    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});
    if (guard.ready()) {
      if (Fwg::UI::Elements::ImportantStepButton("Generate Population",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.runAsync(
            [&cfg, this]() { return currentGenerator().genPopulation(cfg); },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoPopulation = outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Population");
      ImGui::TextWrapped(
          "Drag and drop an image of the correct resolution to set "
          "population density. The red channel will be used where "
          "bright red = high population, black = no population.");

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        currentGenerator().loadPopulation(
            cfg, Fwg::IO::Reader::readGenericImage(uiContext.draggedFile,
                                                   cfg.imageReadContext()));
        uiContext.imageContext.resetTexture();
        redoPopulation = false;
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showTopographyTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Topography", redoTopography)) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayTopography(currentGenerator().ardaData.civLayer,
                                          currentGenerator().worldMap));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});
    if (guard.ready()) {
      ImGui::SeparatorText("Natural Features Generation");

      if (Fwg::UI::Elements::ImportantStepButton("Generate Natural Features",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.runAsync(
            [this]() {
              currentGenerator().genNaturalFeatures();
              return true;
            },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoTopography = outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Topography");
      ImGui::TextWrapped(
          "Drag and drop an image containing marshes and wasteland. "
          "Location colours are ignored; existing locations are preserved.");
      ImGui::TextWrapped("Development and population remain unchanged.");

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        const bool loaded = currentGenerator().loadNaturalFeatures(
            cfg, Fwg::IO::Reader::readGenericImage(uiContext.draggedFile,
                                                   cfg.imageReadContext()));
        uiContext.imageContext.resetTexture();
        if (loaded)
          redoTopography = false;
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showLocationTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Locations", redoLocations)) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayLocations(currentGenerator().areaData.regions,
                                         currentGenerator().worldMap));
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayConnections(currentGenerator().areaData.regions,
                                           currentGenerator().locationMap));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }

    ImGui::SeparatorText("Location Configuration");

    {
      Fwg::UI::Elements::GridLayout grid(2, scaled(250.0f), scaled(12.0f));

      grid.AddSliderInt(
          "Cities Per Region",
          &currentGenerator().ardaConfig.locationConfig.citiesPerRegion, 1, 10);
      grid.AddSliderInt(
          "Agriculture Per Region",
          &currentGenerator().ardaConfig.locationConfig.agriculturePerRegion, 1,
          10);
    }

    ImGui::Spacing();

    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});

    ImGui::SeparatorText("Location Management");

    if (guard.ready()) {
      if (Fwg::UI::Elements::Button("Clear All Locations", false,
                                    ImVec2(200, 0))) {
        uiContext.asyncContext.runAsync(
            [this]() {
              currentGenerator().clearLocations();
              return true;
            },
            [this](Fwg::UI::TaskResult::Outcome) { redoLocations = true; });
      }

      ImGui::SameLine();

      if (Fwg::UI::Elements::ImportantStepButton("Generate All Locations",
                                                 ImVec2(200, 0))) {
        uiContext.asyncContext.runAsync(
            [this]() {
              currentGenerator().genLocations();
              return true;
            },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoLocations = outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::Spacing();

      // Organized in a table
      if (ImGui::BeginTable("LocationGeneration", 1,
                            ImGuiTableFlags_SizingStretchSame)) {

        // --- GENERATION BUTTONS ---
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Generate Random");
        const std::vector<
            std::pair<const char *, Fwg::Civilization::LocationType>>
            genButtons = {
                {"Generate Cities", Fwg::Civilization::LocationType::City},
                {"Generate Farms", Fwg::Civilization::LocationType::Farm},
                {"Generate Ports", Fwg::Civilization::LocationType::Port},
                {"Generate Mines", Fwg::Civilization::LocationType::Mine},
                {"Generate Forests", Fwg::Civilization::LocationType::Forest}};

        for (auto &[label, type] : genButtons) {
          if (ImGui::Button(label, ImVec2(-1, 0))) {
            uiContext.asyncContext.runAsync(
                [this, type]() {
                  currentGenerator().genLocationType(type);
                  return true;
                },
                [this](Fwg::UI::TaskResult::Outcome outcome) {
                  redoLocations =
                      outcome == Fwg::UI::TaskResult::Outcome::Failure;
                });
          }
        }

        ImGui::EndTable();
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Locations");
      ImGui::TextWrapped("An image replaces all cities, farms, ports, mines "
                         "and forests. Natural topography is preserved.");

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        const bool loaded =
            currentGenerator().loadLocations(Fwg::IO::Reader::readGenericImage(
                uiContext.draggedFile, cfg.imageReadContext()));
        redoLocations = !loaded;
        uiContext.imageContext.resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showNavmeshTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Navmesh")) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayConnections(currentGenerator().areaData.regions,
                                           currentGenerator().locationMap));
      uiContext.imageContext.updateImage(1, currentGenerator().regionMap);
    }

    ImGui::SeparatorText("Navmesh Generation");

    if (currentGenerator().regionMap.initialised() &&
        currentGenerator().locationMap.initialised()) {
      // if (ImGui::Button("Generate Navmesh")) {
      //   uiContext.asyncContext.runAsync([this]() {
      //     currentGenerator().genNavmesh({}, {});
      //     uiContext.imageContext.resetTexture();
      //     return true;
      //   });
      // }

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
      }
    } else {
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
                         "Generate other maps first");
    }
    ImGui::EndTabItem();
  }
}

bool ShowDatasetPopup(const char *popupLabel,
                      const std::map<std::string, Dataset> &allDatasets,
                      std::unordered_set<std::string> &activeDatasets) {
  bool changed = false;

  if (ImGui::BeginPopupModal(popupLabel, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Select Datasets:");
    ImGui::Separator();

    for (const auto &dataset : allDatasets) {
      auto key = dataset.first;
      bool isActive = activeDatasets.contains(key);

      // Visual style
      if (isActive) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // green
      } else {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // grey
      }

      // Toggle on click
      if (ImGui::Selectable(key.c_str(), isActive,
                            ImGuiSelectableFlags_DontClosePopups)) {
        if (isActive) {
          activeDatasets.erase(key);
        } else {
          activeDatasets.insert(key);
        }
        changed = true;
      }

      ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return changed;
}

void ArdaUI::showCultureTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Culture", redoCulture)) {
    static std::string cultureGroupStatePath =
        Fwg::Cfg::Values().mapsPath + "world/cultureGroups.txt";
    static std::string cultureGroupStateStatus;
    if (uiContext.tabSwitchEvent(true)) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayCultureGroups(
                 currentGenerator().ardaData.areaData.provinces));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});
    if (guard.ready()) {
      if (Fwg::UI::Elements::ImportantStepButton("Generate Culture Data",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.runAsync(
            [this]() {
              currentGenerator().genEconomyData();
              currentGenerator().genCultureData();
              return true;
            },
            [this](Fwg::UI::TaskResult::Outcome outcome) {
              redoCulture = outcome == Fwg::UI::TaskResult::Outcome::Failure;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Selected Culture Details");

      if (currentGenerator().ardaData.areaData.regions.size()) {
        auto modifiableState = getSelectedRegion();
        if (modifiableState != nullptr && modifiableState->isLand()) {
          auto primaryCulture = modifiableState->getPrimaryCulture();
          if (primaryCulture != nullptr) {
            auto cultureGroup = primaryCulture->cultureGroup;
            auto languageGroup = cultureGroup->getLanguageGroup();
            if (languageGroup == nullptr) {
              ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
                                 "Language generator not initialized");
              ImGui::EndTabItem();
              return;
            }
            auto language = primaryCulture->language;

            // Culture info grid
            {
              Fwg::UI::Elements::GridLayout grid(2, scaled(180.0f),
                                                 scaled(12.0f));

              grid.AddText("Culture", "%s", primaryCulture->name.c_str());
              grid.AddText("Culture Group", "%s", cultureGroup->name.c_str());
              grid.AddText("Language", "%s", language->name.c_str());
              grid.AddText("Language Group", "%s", languageGroup->name.c_str());
            }

            ImGui::Spacing();

            static std::unordered_set<std::string> activeDatasets;
            if (resetSelection) {
              resetSelection = false;
              activeDatasets.clear();
              for (const auto &ds : languageGroup->mergedDataset.influences) {
                activeDatasets.insert(ds);
              }
            }

            if (Fwg::UI::Elements::Button("Edit Language", false,
                                          ImVec2(150, 0))) {
              ImGui::OpenPopup("DatasetPopup");
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Example Names");

            // generate a few random words to show in a table
            if (ImGui::BeginTable("ExampleNames", 5,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg)) {
              ImGui::TableSetupColumn("City Names");
              ImGui::TableSetupColumn("Male Names");
              ImGui::TableSetupColumn("Female Names");
              ImGui::TableSetupColumn("Surnames");
              ImGui::TableSetupColumn("Other Names");
              ImGui::TableHeadersRow();

              constexpr int rows = 10;

              for (int i = 0; i < rows; i++) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (i < language->cityNames.size())
                  ImGui::TextUnformatted(language->cityNames[i].c_str());

                ImGui::TableSetColumnIndex(1);
                if (i < language->maleNames.size())
                  ImGui::TextUnformatted(language->maleNames[i].c_str());

                ImGui::TableSetColumnIndex(2);
                if (i < language->femaleNames.size())
                  ImGui::TextUnformatted(language->femaleNames[i].c_str());

                ImGui::TableSetColumnIndex(3);
                if (i < language->surnames.size())
                  ImGui::TextUnformatted(language->surnames[i].c_str());

                ImGui::TableSetColumnIndex(4);
                if (i < language->names.size())
                  ImGui::TextUnformatted(language->names[i].c_str());
              }

              ImGui::EndTable();
            }

            if (ShowDatasetPopup("DatasetPopup",
                                 languageGenerator.datasetsByLanguage,
                                 activeDatasets)) {
              std::vector<std::string> datasetsToUse;
              for (const auto &ds : activeDatasets) {
                datasetsToUse.push_back(ds);
              }
              auto newLanguageGroup = std::make_shared<LanguageGroup>(
                  languageGenerator.generateLanguageGroup(
                      cultureGroup->getCultures().size(), datasetsToUse,
                      Fwg::Cfg::Values().mapSeed));
              cultureGroup->setLanguageGroup(newLanguageGroup);
            }
          } else {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
                               "Click a region to view culture details");
          }
        } else {
          ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
                             "Click a land region to view culture details");
        }
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Cultures");

      if (uiContext.triggeredDrag) {
        const auto inputImage = Fwg::IO::Reader::readGenericImage(
            uiContext.draggedFile, cfg.imageReadContext());
        if (currentGenerator().loadCultures(inputImage)) {
          uiContext.imageContext.updateImage(
              0, Arda::Gfx::displayCultureGroups(
                     currentGenerator().ardaData.areaData.provinces));
          uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
          redoCulture = false;
        }
        uiContext.triggeredDrag = false;
        uiContext.imageContext.resetTexture();
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Culture Group Sidecar");
      ImGui::SetNextItemWidth(scaled(360.0f));
      ImGui::InputText("##cultureGroupStatePath", &cultureGroupStatePath);
      ImGui::SameLine();
      if (ImGui::Button("Save State")) {
        cultureGroupStateStatus =
            currentGenerator().saveCultureGroupState(cultureGroupStatePath)
                ? "Culture-group state saved."
                : "Could not save culture-group state.";
      }
      ImGui::SameLine();
      if (ImGui::Button("Load State")) {
        if (currentGenerator().loadCultureGroupState(cultureGroupStatePath)) {
          uiContext.imageContext.updateImage(
              0, Arda::Gfx::displayCultureGroups(
                     currentGenerator().ardaData.areaData.provinces));
          uiContext.imageContext.resetTexture();
          resetSelection = true;
          cultureGroupStateStatus = "Culture-group state loaded.";
        } else {
          cultureGroupStateStatus = "Could not load state; check the file and "
                                    "current culture groups.";
        }
      }
      if (!cultureGroupStateStatus.empty())
        ImGui::TextUnformatted(cultureGroupStateStatus.c_str());
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showReligionTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Religion")) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayReligions(
                 currentGenerator().ardaData.areaData.provinces));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::landforms(
             currentGenerator().terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(
             currentGenerator().climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(
             currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::segments(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::provinces(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::regions(currentGenerator().areaData),
         Fwg::UI::PrerequisiteChecker::continents(
             currentGenerator().areaData)});
    if (guard.ready()) {

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        uiContext.imageContext.resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::overview(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginMainTabItem("Overview")) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Fwg::Gfx::displayWorldCivilisationMap(
                 currentGenerator().climateData, currentGenerator().provinceMap,
                 currentGenerator().worldMap, currentGenerator().regionMap, "",
                 cfg.visualizationContext()));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }

    ImGui::SeparatorText("Visual Layer Selection");
    ImGui::TextWrapped(
        "Toggle layers and adjust their weights to create custom "
        "visualizations.");
    ImGui::Spacing();

    if (showVisualLayerToggles(visualLayerInfos)) {
      if (Fwg::Gfx::isCurrentVisualizationDimensions(cfg.width, cfg.height,
                                                     cfg.processingArea)) {
        uiContext.imageContext.updateImage(
            0, UI::renderVisualLayers(
                   currentGenerator(), cfg.width, cfg.height, cfg,
                   currentGenerator().areaGenerationContext(cfg),
                   visualLayerInfos));
      } else {
        Fwg::Utils::Logging::logLine(
            "Skipping overview visualization: map dimensions are stale");
        uiContext.imageContext.updateImage(0, Fwg::Gfx::Image());
      }
    }

    ImGui::EndTabItem();
  }
}

bool ArdaUI::showVisualLayerToggles(UI::VisualLayerSettings &layerVisibility) {
  bool changed = false;
  for (auto &[type, info] : layerVisibility) {
    const auto label = info.composition == UI::VisualLayerComposition::Overwrite
                           ? info.name + " (Overlay)"
                           : info.name;
    if (ImGui::Checkbox(label.c_str(), &info.toggled)) {
      changed = true;
    }
    if (info.toggled && info.composition == UI::VisualLayerComposition::Blend &&
        ImGui::SliderFloat(("Weight##" + info.name).c_str(), &info.weight, 0.0f,
                           1.0f)) {
      changed = true;
    }
  }
  return changed;
}

std::optional<ArdaUI::MapSelection> ArdaUI::consumeMapSelection() {
  auto &events = uiContext.drawContext.clickEvents;
  if (uiContext.asyncContext.busy() || events.empty())
    return std::nullopt;

  const auto click = events.front();
  events.pop();
  auto &generator = currentGenerator();
  if (click.pixel < 0 ||
      static_cast<std::size_t>(click.pixel) >= generator.provinceMap.size() ||
      static_cast<std::size_t>(click.pixel) >=
          generator.provinceMap.imageData.size())
    return std::nullopt;

  const auto &colour = generator.provinceMap[click.pixel];
  if (!generator.areaData.provinceColourMap.contains(colour))
    return std::nullopt;
  const auto &province = generator.areaData.provinceColourMap.at(colour);
  if (!province)
    return std::nullopt;

  const auto region = std::ranges::find_if(
      generator.ardaData.areaData.regions, [&province](const auto &candidate) {
        return candidate && province->regionID >= 0 &&
               candidate->ID == province->regionID;
      });
  return MapSelection{
      click, province,
      region == generator.ardaData.areaData.regions.end() ? nullptr : *region};
}

void ArdaUI::resetMapSelection() {
  selectedRegionId = -1;
  resetSelection = true;
  uiContext.drawContext.clickEvents = {};
}

std::shared_ptr<Arda::ArdaRegion> ArdaUI::getSelectedRegion() {
  if (uiContext.asyncContext.busy())
    return nullptr;

  if (const auto selection = consumeMapSelection();
      selection && selection->region) {
    selectedRegionId = selection->region->ID;
    resetSelection = true;
  }

  const auto &regions = currentGenerator().ardaData.areaData.regions;
  auto selected = std::ranges::find_if(regions, [this](const auto &region) {
    return region && region->ID == selectedRegionId;
  });
  if (selected == regions.end()) {
    selected = std::ranges::find_if(
        regions, [](const auto &region) { return region != nullptr; });
    selectedRegionId = selected == regions.end() ? -1 : (*selected)->ID;
    resetSelection = true;
  }
  return selected == regions.end() ? nullptr : *selected;
}

} // namespace Arda
