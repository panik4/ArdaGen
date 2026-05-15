#include "ArdaUI/ArdaUI.h"

namespace Arda {
// Data

// for state/country/strategic region editing
static bool drawBorders = false;

ArdaUI::ArdaUI()
    : FwgUI(), languageGenerator(Fwg::Cfg::Values().resourcePath +
                                 "/names/languageGroups/") {}

int ArdaUI::shiny(std::shared_ptr<Arda::ArdaGen> &ardaGen) {

  Fwg::UI::Utils::CreateDeviceGL(window, "ArdaGen 0.10.2", 0, 0);

  Fwg::UI::Utils::setupImGuiContextAndStyle();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  glfwSetWindowUserPointer(window, this);
  glfwSetDropCallback(
      window, [](GLFWwindow *win, int count, const char **paths) {
        auto *fwgui = reinterpret_cast<ArdaUI *>(glfwGetWindowUserPointer(win));
        fwgui->uiContext.triggeredDrag = (count > 0);
        fwgui->uiContext.draggedFile = (count > 0) ? std::string(paths[count - 1]) : "";
      });

  auto &cfg = Fwg::Cfg::Values();
  auto &io = ImGui::GetIO();
  init(cfg, *ardaGen);

  while (!glfwWindowShouldClose(window)) {
    uiContext.triggeredDrag = false;
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    {
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y});
      ImGui::Begin("ArdaGen");

      ImGui::BeginChild("LeftContent",
                        ImVec2(ImGui::GetContentRegionAvail().x * 0.4f,
                               ImGui::GetContentRegionAvail().y * 1.0f),
                        false);
      {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(78, 90, 204, 40));
        // Create a child window for the left content
        ImGui::BeginChild("SettingsContent",
                          ImVec2(ImGui::GetContentRegionAvail().x * 1.0f,
                                 ImGui::GetContentRegionAvail().y * 0.8f),
                          false);
        {
          ImGui::SeparatorText("Different Steps of the generation, usually go "
                               "from left to right");

          if (ImGui::BeginTabBar("Steps", ImGuiTabBarFlags_None)) {
            // Disable all inputs if computation is running
            if (uiContext.asyncContext.computationRunning) {
              ImGui::BeginDisabled();
            }

            defaultTabs(cfg, *ardaGen);
            overview(cfg);
            // showScenarioTab(cfg, ardaGen);
            // Re-enable inputs if computation is running
            if (uiContext.asyncContext.computationRunning &&
                !uiContext.asyncContext.computationStarted) {
              ImGui::EndDisabled();
            }
            // Check if the computation is done
            if (uiContext.asyncContext.computationRunning &&
                uiContext.asyncContext.computationFutureBool.wait_for(
                    std::chrono::seconds(0)) == std::future_status::ready) {
              uiContext.asyncContext.computationRunning = false;
              uiContext.imageContext.resetTexture();
            }

            if (uiContext.asyncContext.computationRunning) {
              uiContext.asyncContext.computationStarted = false;
              ImGui::Text("Working, please be patient");
            } else {
              ImGui::Text("Ready!");
            }

            ImGui::EndTabBar();
          }

          ImGui::PopStyleColor();
          ImGui::EndChild();
          // Draw a frame around the child region
          ImVec2 childMin = ImGui::GetItemRectMin();
          ImVec2 childMax = ImGui::GetItemRectMax();
          ImGui::GetWindowDrawList()->AddRect(
              childMin, childMax, IM_COL32(100, 90, 180, 255), 0.0f, 0, 2.0f);
        }

        genericWrapper(cfg, *ardaGen);
        logWrapper();
      }
      ImGui::SameLine();
      imageWrapper(io);
      ImGui::End();
    }

    // Render
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

void ArdaUI::automapAreas() {
  if (uiContext.generationContext.modifiedAreas &&
      !uiContext.asyncContext.computationRunning) {
    ardaGen->mapProvinces();
    ardaGen->mapRegions();
    ardaGen->mapContinents();
    uiContext.generationContext.modifiedAreas = false;
    redoDevelopment = true;
    redoPopulation = true;
    redoTopography = true;
    redoCulture = true;
    redoLocations = true;
  }
}

bool ArdaUI::scenarioGenReady(bool printIssue) {
  bool ready = true;

  auto &generator = ardaGen;
  auto &cfg = Fwg::Cfg::Values();

  if (!generator->terrainData.detailedHeightMap.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Detailed heightmap is missing.");
    ready = false;
  }

  if (!generator->climateMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Climate map not initialised.");
    ready = false;
  }

  if (!generator->provinceMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Province map not initialised.");
    ready = false;
  }

  if (!generator->regionMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Region map not initialised.");
    ready = false;
  }

  if (!generator->worldMap.initialised()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("World map not initialised.");
    ready = false;
  }

  if (!generator->areaData.provinces.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("No provinces defined.");
    ready = false;
  }

  if (!generator->areaData.regions.size()) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("No regions defined.");
    ready = false;
  }

  return ready;
}

void ArdaUI::showCivilizationTab(Fwg::Cfg &cfg) {
  int retCode = 0;
  if (Fwg::UI::Elements::BeginMainTabItem(
          "Civilisation", redoCulture || redoDevelopment || redoPopulation)) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.resetTexture();
    }
    uiContext.helpContext.showHelpTextBox("Civilisation");

    ImGui::SeparatorText("Civilisation Parameters");

    {
      Fwg::UI::Elements::GridLayout grid(2, 250.0f, 12.0f);

      grid.AddInputDouble("Development Influence", &cfg.developmentInfluence,
                          0.0, 10.0);
      grid.AddInputDouble("Min Development", &cfg.minimumDevelopment, 0.0,
                          10.0);
      grid.AddInputDouble("Max Development", &cfg.maximumDevelopment, 0.0,
                          10.0);
      grid.AddInputDouble("Population Influence", &cfg.populationInfluence, 0.0,
                          10.0);
      grid.AddInputDouble("Urban Factor",
                          &ardaGen->ardaConfig.locationConfig.urbanFactor, 0.0,
                          10.0);
      grid.AddInputDouble("Agriculture Factor",
                          &ardaGen->ardaConfig.locationConfig.agricultureFactor,
                          0.0, 10.0);
    }

    ImGui::Spacing();

    // scoping the guard to ensure only the button is disabled, not the whole
    // tab, so the user can still edit parameters and go through tabs
    {
      auto guard = Fwg::UI::PrerequisiteChecker::require(
          {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
           Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
           Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
           Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
           Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
           Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
           Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
           Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
      if (guard.ready()) {
        if (Fwg::UI::Elements::AutomationStepButton(
                "Generate all civilisation data automatically")) {
          uiContext.asyncContext.computationFutureBool =
              uiContext.asyncContext.runAsync([&cfg, this]() {
                ardaGen->genCivilisationData();
                uiContext.imageContext.resetTexture();
                redoTopography = false;
                redoDevelopment = false;
                redoPopulation = false;
                redoCulture = false;
                redoLocations = false;
                return true;
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
          0, Arda::Gfx::displayDevelopment(ardaGen->ardaProvinces));
      uiContext.imageContext.updateImage(1, ardaGen->worldMap);
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
    if (guard.ready()) {
      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        ardaGen->loadDevelopment(cfg, uiContext.draggedFile);
        uiContext.imageContext.resetTexture();
        redoDevelopment = false;
      }

      ImGui::SeparatorText("Continent Development Modifiers");
      ImGui::TextWrapped("Base development affects the overall level of "
                         "development on a continent. It is still dependent on "
                         "climate and habitability.");

      {
        Fwg::UI::Elements::GridLayout grid(2, 250.0f, 12.0f);

        for (auto &continent : ardaGen->ardaContinents) {
          std::string label = "Continent " + std::to_string(continent->ID);
          grid.AddInputDouble(label.c_str(), &continent->developmentModifier,
                              0.0, 10.0);
        }
      }

      ImGui::Spacing();

      if (Fwg::UI::Elements::Button("Generate with Random Modifiers", false,
                                    ImVec2(250, 0))) {
        cfg.randomDevelopment = true;
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([&cfg, this]() {
              ardaGen->genDevelopment(cfg);
              uiContext.imageContext.resetTexture();
              redoDevelopment = false;
              return true;
            });
      }

      ImGui::SameLine();

      if (Fwg::UI::Elements::ImportantStepButton(
              "Generate with Current Modifiers", ImVec2(250, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([&cfg, this]() {
              cfg.randomDevelopment = false;
              ardaGen->genDevelopment(cfg);
              uiContext.imageContext.resetTexture();
              redoDevelopment = false;
              return true;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Edit Selected Continent");

      // get the clicked state
      auto modifiableState = getSelectedRegion();
      if (modifiableState != nullptr && modifiableState->continentID >= 0 &&
          modifiableState->continentID < ardaGen->ardaContinents.size()) {
        ImGui::TextWrapped("Click a continent in the image and then modify the "
                           "base development modifiers.");

        auto continent = ardaGen->ardaContinents[modifiableState->continentID];

        {
          Fwg::UI::Elements::GridLayout grid(2, 250.0f, 12.0f);

          grid.AddText("Selected Continent", "%d",
                       modifiableState->continentID);

          if (grid.AddInputDouble("Development Modifier",
                                  &continent->developmentModifier, 0.0, 10.0)) {
            uiContext.asyncContext.computationFutureBool =
                uiContext.asyncContext.runAsync([&cfg, this]() {
                  cfg.randomDevelopment = false;
                  ardaGen->genDevelopment(cfg);
                  uiContext.imageContext.resetTexture();
                  redoDevelopment = false;
                  return true;
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
          0, Arda::Gfx::displayPopulation(ardaGen->ardaProvinces));
      uiContext.imageContext.updateImage(1, ardaGen->worldMap);
    }

    ImGui::SeparatorText("Population Configuration");

    {
      Fwg::UI::Elements::GridLayout grid(2, 250.0f, 12.0f);

      if (grid.AddInputDouble("World Population Factor",
                              &ardaGen->ardaConfig.worldPopulationFactor, 0.0,
                              10.0)) {
        ardaGen->ardaConfig.calculateTargetWorldPopulation();
      }

      grid.AddText("Target Population", "%.1f Mio",
                   ardaGen->ardaConfig.targetWorldPopulation / 1000'000.0);

      grid.AddText("Current Population", "%.1f Mio",
                   ardaGen->ardaStats.totalWorldPopulation / 1000'000.0);
    }

    ImGui::Spacing();

    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
    if (guard.ready()) {
      if (Fwg::UI::Elements::ImportantStepButton("Generate Population",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([&cfg, this]() {
              ardaGen->genPopulation(cfg);
              uiContext.imageContext.resetTexture();
              redoPopulation = false;
              return true;
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
        ardaGen->loadPopulation(
            cfg, Fwg::IO::Reader::readGenericImage(uiContext.draggedFile, cfg));
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
          0, Arda::Gfx::displayTopography(ardaGen->ardaData.civLayer,
                                          ardaGen->worldMap));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
    if (guard.ready()) {
      ImGui::SeparatorText("Natural Features Generation");

      if (Fwg::UI::Elements::ImportantStepButton("Generate Natural Features",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([&cfg, this]() {
              ardaGen->genNaturalFeatures();
              uiContext.imageContext.resetTexture();
              redoTopography = false;
              return true;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Topography");
      ImGui::TextWrapped(
          "Drag and drop an image containing: marshes, wasteland, "
          "cities, farmland, mines, forestry, ports.");
      ImGui::TextWrapped(
          "Anything apart from marshes and wasteland will be used "
          "for location generation.");

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        ardaGen->loadNaturalFeatures(
            cfg, Fwg::IO::Reader::readGenericImage(uiContext.draggedFile, cfg));
        uiContext.imageContext.resetTexture();
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
          0, Arda::Gfx::displayLocations(ardaGen->areaData.regions,
                                         ardaGen->worldMap));
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayConnections(ardaGen->areaData.regions,
                                           ardaGen->locationMap));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }

    ImGui::SeparatorText("Location Configuration");

    {
      Fwg::UI::Elements::GridLayout grid(2, 250.0f, 12.0f);

      grid.AddSliderInt("Cities Per Region",
                        &ardaGen->ardaConfig.locationConfig.citiesPerRegion, 1,
                        10);
      grid.AddSliderInt(
          "Agriculture Per Region",
          &ardaGen->ardaConfig.locationConfig.agriculturePerRegion, 1, 10);
    }

    ImGui::Spacing();

    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});

    ImGui::SeparatorText("Location Management");

    if (guard.ready()) {
      if (Fwg::UI::Elements::Button("Clear All Locations", false,
                                    ImVec2(200, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([this]() {
              ardaGen->clearLocations();
              uiContext.imageContext.resetTexture();
              redoLocations = true;
              return true;
            });
      }

      ImGui::SameLine();

      if (Fwg::UI::Elements::ImportantStepButton("Generate All Locations",
                                                 ImVec2(200, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([this]() {
              ardaGen->genLocations();
              uiContext.imageContext.resetTexture();
              redoLocations = false;
              return true;
            });
      }

      ImGui::Spacing();

      // Organized in a table
      if (ImGui::BeginTable("LocationGeneration", 2,
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
            uiContext.asyncContext.computationFutureBool =
                uiContext.asyncContext.runAsync([this, type]() {
                  ardaGen->genLocationType(type);
                  uiContext.imageContext.resetTexture();
                  redoLocations = false;
                  return true;
                });
          }
        }

        // --- DETECTION BUTTONS ---
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Detect from Topography");
        const std::vector<
            std::pair<const char *, Fwg::Civilization::LocationType>>
            detectButtons = {
                {"Detect Cities", Fwg::Civilization::LocationType::City},
                {"Detect Farms", Fwg::Civilization::LocationType::Farm},
                {"Detect Ports", Fwg::Civilization::LocationType::Port},
                {"Detect Mines", Fwg::Civilization::LocationType::Mine},
                {"Detect Forests", Fwg::Civilization::LocationType::Forest},
            };

        for (auto &[label, type] : detectButtons) {
          if (ImGui::Button(label, ImVec2(-1, 0))) {
            uiContext.asyncContext.computationFutureBool =
                uiContext.asyncContext.runAsync([this, type]() {
                  ardaGen->detectLocationType(type);
                  uiContext.imageContext.resetTexture();
                  redoLocations = false;
                  return true;
                });
          }
        }

        ImGui::EndTable();
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Load Custom Locations");

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        redoLocations = false;
        ardaGen->loadLocations(
            Fwg::IO::Reader::readGenericImage(uiContext.draggedFile, cfg));
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
          0, Arda::Gfx::displayConnections(ardaGen->areaData.regions,
                                           ardaGen->locationMap));
      uiContext.imageContext.updateImage(1, ardaGen->regionMap);
    }

    ImGui::SeparatorText("Navmesh Generation");

    if (ardaGen->regionMap.initialised() &&
        ardaGen->locationMap.initialised()) {
      // if (ImGui::Button("Generate Navmesh")) {
      //   uiContext.asyncContext.computationFutureBool =
      //   uiContext.asyncContext.runAsync([this]() {
      //     ardaGen->genNavmesh({}, {});
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
    if (uiContext.tabSwitchEvent(true)) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayCultureGroups(ardaGen->ardaProvinces));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
    if (guard.ready()) {
      if (Fwg::UI::Elements::ImportantStepButton("Generate Culture Data",
                                                 ImVec2(220, 0))) {
        uiContext.asyncContext.computationFutureBool =
            uiContext.asyncContext.runAsync([this]() {
              ardaGen->genEconomyData();
              ardaGen->genCultureData();
              uiContext.imageContext.resetTexture();
              redoCulture = false;
              return true;
            });
      }

      ImGui::Spacing();
      ImGui::SeparatorText("Selected Culture Details");

      if (ardaGen->ardaRegions.size()) {
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
              Fwg::UI::Elements::GridLayout grid(2, 180.0f, 12.0f);

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
        uiContext.triggeredDrag = false;
        Fwg::Utils::Logging::logLine("Currently not possible to load cultures");
        uiContext.imageContext.resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showReligionTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Religion")) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Arda::Gfx::displayReligions(ardaGen->ardaProvinces));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }
    auto guard = Fwg::UI::PrerequisiteChecker::require(
        {Fwg::UI::PrerequisiteChecker::climate(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::landforms(ardaGen->terrainData),
         Fwg::UI::PrerequisiteChecker::habitability(ardaGen->climateData),
         Fwg::UI::PrerequisiteChecker::superSegments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::segments(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::provinces(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::regions(ardaGen->areaData),
         Fwg::UI::PrerequisiteChecker::continents(ardaGen->areaData)});
    if (guard.ready()) {

      if (uiContext.triggeredDrag) {
        uiContext.triggeredDrag = false;
        uiContext.imageContext.resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showLanguageTab(Fwg::Cfg &cfg) {}

void ArdaUI::overview(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginMainTabItem("Overview")) {
    if (uiContext.tabSwitchEvent()) {
      uiContext.imageContext.updateImage(
          0, Fwg::Gfx::displayWorldCivilisationMap(
                 ardaGen->climateData, ardaGen->provinceMap, ardaGen->worldMap,
                 ardaGen->regionMap, ""));
      uiContext.imageContext.updateImage(1, Fwg::Gfx::Image());
    }

    ImGui::SeparatorText("Visual Layer Selection");
    ImGui::TextWrapped(
        "Toggle layers and adjust their weights to create custom "
        "visualizations.");

    ImGui::Spacing();

    if (showVisualLayerToggles(visualLayerInfos)) {
      std::vector<Fwg::Gfx::WeightedImage> layers;
      for (const auto &[type, info] : visualLayerInfos) {
        if (info.toggled) {
          switch (type) {
          case VisualLayerType::HEIGHTMAP:
            layers.push_back({Fwg::Gfx::displayHeightMap(
                                  ardaGen->terrainData.detailedHeightMap),
                              info.weight});
            break;

          case VisualLayerType::LANDFORMS:
            layers.push_back(
                {Fwg::Gfx::landFormMap(ardaGen->terrainData), info.weight});
            break;

          case VisualLayerType::TOPOGRAPHY:
            layers.push_back(
                {Fwg::Gfx::topographyMap(ardaGen->terrainData.altitudes),
                 info.weight});
            break;

          case VisualLayerType::NORMALMAP:
            layers.push_back(
                {Fwg::Gfx::displaySobelMap(ardaGen->terrainData.sobelData),
                 info.weight});
            break;

          case VisualLayerType::INCLINATION:
            // TODO: Implement inclination map
            break;

          case VisualLayerType::RELSURROUNDINGS:
            // TODO: Implement relative surroundings map
            break;

          case VisualLayerType::HUMIDITY:
            layers.push_back(
                {Fwg::Gfx::Climate::displayHumidity(ardaGen->climateData),
                 info.weight});
            break;

          case VisualLayerType::TEMPERATURE:
            layers.push_back(
                {Fwg::Gfx::Climate::displayTemperature(ardaGen->climateData),
                 info.weight});
            break;

          case VisualLayerType::CLIMATE:
            layers.push_back(
                {Fwg::Gfx::Climate::displayClimate(ardaGen->climateData, false),
                 info.weight});
            break;

          case VisualLayerType::TREECLIMATE:
            layers.push_back(
                {Fwg::Gfx::Climate::displayClimate(ardaGen->climateData, true),
                 info.weight});
            break;

          case VisualLayerType::TREEDENSITY:
            layers.push_back(
                {Fwg::Gfx::Climate::displayTreeDensity(ardaGen->climateData),
                 info.weight});
            break;

          case VisualLayerType::HABITABILITY:
            layers.push_back({Fwg::Gfx::displayHabitability(
                                  ardaGen->climateData.habitabilities),
                              info.weight});
            break;

          case VisualLayerType::ARABLELAND:
            layers.push_back(
                {Fwg::Gfx::Climate::displayArableLand(ardaGen->climateData),
                 info.weight});
            break;

          case VisualLayerType::SUPERSEGMENTS:
            layers.push_back({Fwg::Gfx::Segments::displaySuperSegments(
                                  ardaGen->areaData.superSegments),
                              info.weight});
            break;

          case VisualLayerType::SEGMENTS:
            layers.push_back({Fwg::Gfx::Segments::displaySegments(
                                  ardaGen->areaData.segments),
                              info.weight});
            break;

          case VisualLayerType::PROVINCES:
            layers.push_back({ardaGen->provinceMap, info.weight});
            break;

          case VisualLayerType::REGIONS:
            layers.push_back({ardaGen->regionMap, info.weight});
            break;

          case VisualLayerType::REGIONSWITHPROVINCES:
            // TODO: Implement regions with province overlay
            break;

          case VisualLayerType::REGIONSWITHBORDERS:
            // TODO: Implement region borders overlay
            break;

          case VisualLayerType::SUPERREGIONS:
            layers.push_back({ardaGen->superRegionMap, info.weight});
            break;

          case VisualLayerType::CONTINENTS:
            layers.push_back(
                {Fwg::Gfx::simpleContinents(ardaGen->areaData.continents,
                                            ardaGen->areaData.seaBodies),
                 info.weight});
            break;

          case VisualLayerType::POPULATION:
            layers.push_back(
                {Arda::Gfx::displayPopulation(ardaGen->ardaProvinces),
                 info.weight});
            break;

          case VisualLayerType::DEVELOPMENT:
            layers.push_back(
                {Arda::Gfx::displayDevelopment(ardaGen->ardaProvinces),
                 info.weight});
            break;

          case VisualLayerType::LOCATIONS:
            layers.push_back({Arda::Gfx::displayLocations(
                                  ardaGen->areaData.regions, ardaGen->worldMap),
                              info.weight});
            break;

          case VisualLayerType::WORLD_MAP:
            layers.push_back({ardaGen->worldMap, info.weight});
            break;

          case VisualLayerType::CIVILISATION_MAP:
            layers.push_back({Fwg::Gfx::displayWorldCivilisationMap(
                                  ardaGen->climateData, ardaGen->provinceMap,
                                  ardaGen->worldMap, ardaGen->regionMap, ""),
                              info.weight});
            break;

          case VisualLayerType::COUNTRIES:
            layers.push_back({ardaGen->countryMap, info.weight});
            break;

          case VisualLayerType::CULTUREGROUPS:
            layers.push_back(
                {Arda::Gfx::displayCultureGroups(ardaGen->ardaProvinces),
                 info.weight});
            break;

          case VisualLayerType::CULTURES:
            layers.push_back(
                {Arda::Gfx::displayCultures(ardaGen->ardaProvinces),
                 info.weight});
            break;

          case VisualLayerType::RELIGIONS:
            // TODO: Implement religions visualization
            break;

          default:
            // TODO: Handle unexpected layer type
            break;
          }
        }
      }
      uiContext.imageContext.updateImage(
          0, Fwg::Gfx::mergeWeightedImages(cfg.width, cfg.height, layers));
    }

    ImGui::EndTabItem();
  }
}

bool ArdaUI::showVisualLayerToggles(
    std::map<VisualLayerType, VisualLayerInfo> &layerVisibility) {
  bool changed = false;

  for (auto &[type, info] : visualLayerInfos) {
    if (ImGui::Checkbox(info.name.c_str(), &info.toggled)) {
      changed = true;
    }
    // Optional: ImGui::SliderFloat for weight
    if (info.toggled) {
      if (ImGui::SliderFloat(("Weight##" + info.name).c_str(), &info.weight,
                             0.0f, 1.0f)) {
        changed = true;
      }
    }
  }

  return changed;
}

std::shared_ptr<Arda::ArdaRegion> ArdaUI::getSelectedRegion() {

  static int selectedStateIndex = 0;

  auto &clickEvents = uiContext.drawContext.clickEvents;
  if (clickEvents.size()) {
    auto pix = clickEvents.front();
    clickEvents.pop();
    const auto colour = ardaGen->provinceMap[pix.pixel];
    if (ardaGen->areaData.provinceColourMap.contains(colour)) {
      const auto &prov = ardaGen->areaData.provinceColourMap[colour];
      if (prov->regionID < ardaGen->ardaRegions.size()) {
        auto &state = ardaGen->ardaRegions[prov->regionID];
        selectedStateIndex = state->ID;
        resetSelection = true;
      }
    }
  }
  if (!uiContext.asyncContext.computationRunning && selectedStateIndex >= 0 &&
      selectedStateIndex < ardaGen->ardaRegions.size()) {
    return ardaGen->ardaRegions[selectedStateIndex];
  }
  return nullptr;
}

} // namespace Arda