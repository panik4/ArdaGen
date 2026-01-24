#include "ArdaUI/ArdaUI.h"
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);
namespace Arda {
// Data
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if
// dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your
// main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to
// your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from
// your application based on those two flags.
// LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    if (wParam == SIZE_MINIMIZED)
      return 0;
    g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
    g_ResizeHeight = (UINT)HIWORD(lParam);
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
// for state/country/strategic region editing
static bool drawBorders = false;

ArdaUI::ArdaUI()
    : FwgUI(), languageGenerator(Fwg::Cfg::Values().resourcePath +
                                 "/names/languageGroups/") {}

int ArdaUI::shiny(std::shared_ptr<Arda::ArdaGen> &ardaGen) {

  try {
    //  Create application window
    //  ImGui_ImplWin32_EnableDpiAwareness();
    auto wc = initializeWindowClass();

    HWND consoleWindow = GetConsoleWindow();

    ::RegisterClassExW(&wc);
    HWND hwnd =
        uiUtils->createAndConfigureWindow(wc, wc.lpszClassName, L"ArdaGen");
    initializeGraphics(hwnd);
    initializeImGui(hwnd);
    auto &io = ImGui::GetIO();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    auto &cfg = Fwg::Cfg::Values();
    // Main loop
    bool done = false;
    //--- prior to main loop:
    DragAcceptFiles(hwnd, TRUE);
    uiUtils->primaryTexture = nullptr;
    uiUtils->device = g_pd3dDevice;

    init(cfg, *ardaGen);

    while (!done) {
      try {
        initDraggingPoll(done);
        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
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
              ImGui::SeparatorText(
                  "Different Steps of the generation, usually go "
                  "from left to right");

              if (ImGui::BeginTabBar("Steps", ImGuiTabBarFlags_None)) {
                // Disable all inputs if computation is running
                if (computationRunning) {
                  ImGui::BeginDisabled();
                }

                defaultTabs(cfg, *ardaGen);
                overview(cfg);
                // showScenarioTab(cfg, ardaGen);
                // Re-enable inputs if computation is running
                if (computationRunning && !computationStarted) {
                  ImGui::EndDisabled();
                }
                // Check if the computation is done
                if (computationRunning &&
                    computationFutureBool.wait_for(std::chrono::seconds(0)) ==
                        std::future_status::ready) {
                  computationRunning = false;
                  uiUtils->resetTexture();
                }

                if (computationRunning) {
                  computationStarted = false;
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
              ImGui::GetWindowDrawList()->AddRect(childMin, childMax,
                                                  IM_COL32(100, 90, 180, 255),
                                                  0.0f, 0, 2.0f);
            }

            genericWrapper(cfg, *ardaGen);
            logWrapper();
          }
          ImGui::SameLine();
          imageWrapper(io);
          ImGui::End();
        }

        // Rendering
        uiUtils->renderImGui(g_pd3dDeviceContext, g_mainRenderTargetView,
                             clear_color, g_pSwapChain);
      } catch (std::exception e) {
        Fwg::Utils::Logging::logLine("Error in ArdaUI main loop: ", e.what());
      }
    }

    cleanup(hwnd, wc);
    return 0;
  } catch (std::exception e) {
    Fwg::Utils::Logging::logLine("Error in ArdaUI startup: ", e.what());
    return -1;
  }
}

void ArdaUI::automapAreas() {
  if (modifiedAreas && !computationRunning) {
    ardaGen->mapProvinces();
    ardaGen->mapRegions();
    ardaGen->mapContinents();
    modifiedAreas = false;
    redoDevelopment = true;
    redoPopulation = true;
    redoTopography = true;
    redoCulture = true;
    redoLocations = true;
    if (ardaGen->ardaProvinces.size() && ardaGen->ardaRegions.size() &&
        ardaGen->ardaContinents.size()) {
      configuredScenarioGen = true;
    }
  }
}

bool ArdaUI::scenarioGenReady(bool printIssue) {
  bool ready = true;

  auto &generator = ardaGen;
  auto &cfg = Fwg::Cfg::Values();

  if (redoProvinces) {
    if (printIssue)
      Fwg::Utils::Logging::logLine("Province redo is pending.");
    ready = false;
  }

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
    if (uiUtils->tabSwitchEvent()) {
      // force update so sub-selected tabs get updated
      uiUtils->setForceUpdate();
      uiUtils->resetTexture();
    }
    static bool scenGenReady = false;
    scenGenReady = scenarioGenReady(false);
    // if we detect a change in the previous tabs, we also reset
    // "configuredScenarioGen", to FORCE the user to remap areas
    if (!scenGenReady) {
      configuredScenarioGen = false;
    }

    // allow printing why the scenario generation is not ready
    if (scenGenReady) {
      uiUtils->showHelpTextBox("Civilisation");
      if (ardaGen->ardaProvinces.size() && ardaGen->ardaRegions.size() &&
          ardaGen->ardaContinents.size()) {

        ImGui::PushItemWidth(200.0f);
        ImGui::InputDouble(
            "<--Development influence on population and city size",
            &cfg.developmentInfluence, 0.1);
        ImGui::InputDouble("<--Minimum Development", &cfg.minimumDevelopment,
                           0.1);
        ImGui::InputDouble("<--Maximum Development", &cfg.maximumDevelopment,
                           0.1);

        ImGui::InputDouble("<--Pop influence on city size",
                           &cfg.populationInfluence, 0.1);
        ImGui::InputDouble("<--Urbanisation factor",
                           &ardaGen->ardaConfig.locationConfig.urbanFactor,
                           0.1);
        ImGui::InputDouble(
            "<--Agriculture factor",
            &ardaGen->ardaConfig.locationConfig.agricultureFactor, 0.05);
        ImGui::PopItemWidth();
        if (Fwg::UI::Elements::AutomationStepButton(
                "Generate all civilisation data automatically")) {
          computationFutureBool = runAsync([&cfg, this]() {
            // ardaGen->genCivData(cfg);
            ardaGen->genCivilisationData();
            uiUtils->resetTexture();
            redoTopography = false;
            redoDevelopment = false;
            redoPopulation = false;
            redoCulture = false;
            redoLocations = false;
            return true;
          });
        }
        ImGui::SeparatorText("Manually edit civ data");
        if (Fwg::UI::Elements::BeginSubTabBar("Civilisation stuff")) {
          showTopographyTab(cfg);
          showDevelopmentTab(cfg);
          showPopulationTab(cfg);
          showCultureTab(cfg);
          showLocationTab(cfg);

          Fwg::UI::Elements::EndSubTabBar();
        }
      } else {
        ImGui::Text("Generate other maps first");
      }
    } else {
      ImGui::Text("Generate required maps in the other tabs first");
    }

    ImGui::EndTabItem();
  }
}

void ArdaUI::showDevelopmentTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Development", redoDevelopment)) {
    if (uiUtils->tabSwitchEvent(true)) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayDevelopment(ardaGen->ardaProvinces));
      uiUtils->updateImage(1, ardaGen->worldMap);
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      if (triggeredDrag) {
        triggeredDrag = false;
        // load development map
        ardaGen->loadDevelopment(cfg, draggedFile);
        uiUtils->resetTexture();
        redoDevelopment = false;
      }
      ImGui::PushItemWidth(100.0f);
      ImGui::SeparatorText("Base development affects the overall level of "
                           "development on a continent.");
      ImGui::SeparatorText("It is still dependent on climate and habitability");
      for (auto &continent : ardaGen->ardaContinents) {
        std::string displayString = "Base development modifier for continent " +
                                    std::to_string(continent->ID);
        ImGui::InputDouble(displayString.c_str(),
                           &continent->developmentModifier);
      }
      if (ImGui::Button("Generate Development with new random base development "
                        "modifiers.")) {
        cfg.randomDevelopment = true;
        computationFutureBool = runAsync([&cfg, this]() {
          ardaGen->genDevelopment(cfg);
          uiUtils->resetTexture();
          redoDevelopment = false;
          return true;
        });
      }
      if (ImGui::Button("Generate Development with current base development "
                        "modifiers.")) {
        computationFutureBool = runAsync([&cfg, this]() {
          cfg.randomDevelopment = false;
          ardaGen->genDevelopment(cfg);
          uiUtils->resetTexture();
          redoDevelopment = false;
          return true;
        });
      }

      // get the clicked state
      auto modifiableState = getSelectedRegion();
      if (modifiableState != nullptr && modifiableState->continentID >= 0 &&
          modifiableState->continentID < ardaGen->ardaContinents.size()) {
        ImGui::SeparatorText(
            "You may click a continent in the image and then modify the "
            "base development modifiers");
        ImGui::Text("Selected Continent ID: %d", modifiableState->continentID);
        auto continent = ardaGen->ardaContinents[modifiableState->continentID];
        ImGui::Text("Continent Development Modifier, press enter to apply if "
                    "you manually enter a number.");
        if (ImGui::InputDouble("Continent Development Modifier",
                               &continent->developmentModifier, 0.1, 1.0,
                               "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
          computationFutureBool = runAsync([&cfg, this]() {
            cfg.randomDevelopment = false;
            ardaGen->genDevelopment(cfg);
            uiUtils->resetTexture();
            redoDevelopment = false;
            return true;
          });
        }
      } else {
        ImGui::Text(
            "No continent selected, click on a continent to select it.");
      }
      ImGui::PopItemWidth();

      // for drawing. Simpledraw not possible due to per-province edit
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (ardaGen->terrainData.landFormIds[pix.first.pixel].altitude > 0.0)
      //    {
      //      const auto &colour = ardaGen->provinceMap[pix.first.pixel];
      //      if (ardaGen->areaData.provinceColourMap.find(colour)) {
      //        const auto &prov = ardaGen->areaData.provinceColourMap[colour];
      //        // only allow drawing if enabled, otherwise take the click as
      //        a
      //        // select of the continent
      //        if (drawingMode) {
      //          if (pix.first.type == InteractionType::CLICK) {
      //            prov->averageDevelopment = pix.second;
      //          } else if (pix.first.type == InteractionType::RCLICK) {
      //            prov->averageDevelopment = 0.0;
      //          }
      //        } else {
      //          // sets the development modifier to the brush strength
      //          if (pix.first.type == InteractionType::CLICK) {
      //            for (auto &continent : ardaGen->areaData.continents) {
      //              if (continent.ID == prov->continentID) {
      //                continent.developmentModifier = pix.second;
      //              }
      //            }
      //          }
      //        }
      //      }
      //    }
      //  }
      //  // we had a click event, therefore setting the dev modifier. We want
      //  to
      //  // update, but not re-randomize the just changed development
      //  modifier,
      //  // so this must mean that we want to turn off random development
      //  if (!drawingMode) {
      //    cfg.randomDevelopment = false;
      //    ardaGen->genDevelopment(cfg);
      //  }
      //  uiUtils->resetTexture();
      //}
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showPopulationTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Population", redoPopulation)) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayPopulation(ardaGen->ardaProvinces));
      uiUtils->updateImage(1, ardaGen->worldMap);
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      ImGui::PushItemWidth(200.0f);
      if (ImGui::InputDouble("WorldPopulationFactor",
                             &ardaGen->ardaConfig.worldPopulationFactor, 0.1)) {
        ardaGen->ardaConfig.calculateTargetWorldPopulation();
      }
      ImGui::Text("Target world population: %f Mio",
                  ardaGen->ardaConfig.targetWorldPopulation / 1000'000.0);

      // display total world population
      ImGui::Text("Total world population: %f Mio",
                  ardaGen->ardaStats.totalWorldPopulation / 1000'000.0);
      if (ImGui::Button("Generate Population")) {
        computationFutureBool = runAsync([&cfg, this]() {
          ardaGen->genPopulation(cfg);
          uiUtils->resetTexture();
          redoPopulation = false;
          return true;
        });
      }
      ImGui::PopItemWidth();

      ImGui::SeparatorText(
          "Drag and drop in an image of the correct resolution to set the "
          "population density. The red channel of the input image will be "
          "used "
          "to set population, where bright red means high population, black "
          "means no population");
      if (triggeredDrag) {
        triggeredDrag = false;
        // load population map
        ardaGen->loadPopulation(
            cfg, Fwg::IO::Reader::readGenericImage(draggedFile, cfg));
        uiUtils->resetTexture();
        redoPopulation = false;
      }
      // for drawing
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (ardaGen->terrainData.landFormIds[pix.first.pixel].altitude > 0.0)
      //    {
      //      const auto &colour = ardaGen->provinceMap[pix.first.pixel];
      //      if (ardaGen->areaData.provinceColourMap.find(colour)) {
      //        const auto &prov = ardaGen->areaData.provinceColourMap[colour];
      //        if (pix.first.type == InteractionType::CLICK) {
      //          prov->populationDensity = pix.second;
      //        } else if (pix.first.type == InteractionType::RCLICK) {
      //          prov->populationDensity = 0.0;
      //        }
      //      }
      //    }
      //  }
      //  uiUtils->resetTexture();
      //}
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showTopographyTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Topography", redoTopography)) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayTopography(ardaGen->ardaData.civLayer,
                                          ardaGen->worldMap));
      uiUtils->updateImage(1, Fwg::Gfx::Image());
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      ImGui::PushItemWidth(200.0f);

      if (ImGui::Button("Generate Natural Features. Doesn't do much yet.")) {
        computationFutureBool = runAsync([&cfg, this]() {
          ardaGen->genNaturalFeatures();
          uiUtils->resetTexture();
          redoTopography = false;
          return true;
        });
      }
      ImGui::PopItemWidth();

      ImGui::SeparatorText(
          "Drag and drop in an image containing: marshes, "
          "wasteland, cities, farmland, mines, forestry, ports.");
      ImGui::SeparatorText(
          "Anything apart "
          "from marshes and wasteland will be used for location generation.");
      if (triggeredDrag) {
        triggeredDrag = false;
        ardaGen->loadNaturalFeatures(
            cfg, Fwg::IO::Reader::readGenericImage(draggedFile, cfg));
        uiUtils->resetTexture();
        redoTopography = false;
      }
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showLocationTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Locations", redoLocations)) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayLocations(ardaGen->areaData.regions,
                                         ardaGen->worldMap));
      uiUtils->updateImage(
          0, Arda::Gfx::displayConnections(ardaGen->areaData.regions,
                                           ardaGen->locationMap));
      uiUtils->updateImage(1, Fwg::Gfx::Image());
    }
    ImGui::SliderInt("Amount of separate cities per region",
                     &ardaGen->ardaConfig.locationConfig.citiesPerRegion, 1,
                     10);
    ImGui::SliderInt("Amount of separate farm areas per region",
                     &ardaGen->ardaConfig.locationConfig.agriculturePerRegion,
                     1, 10);

    if (ardaGen->ardaRegions.size()) {
      ImGui::SeparatorText("Location Management");

      if (ImGui::Button("Clear all Locations", ImVec2(-1, 0))) {
        computationFutureBool = runAsync([this]() {
          ardaGen->clearLocations();
          uiUtils->resetTexture();
          redoLocations = true;
          return true;
        });
      }
      if (ImGui::Button("Generate all Locations")) {
        computationFutureBool = runAsync([this]() {
          ardaGen->genLocations();
          uiUtils->resetTexture();
          redoLocations = false;
          return true;
        });
      }
      if (ImGui::Button("Generate Navmesh")) {
        computationFutureBool = runAsync([this]() {
          ardaGen->genNavmesh({}, {});
          uiUtils->resetTexture();
          return true;
        });
      }

      ImGui::Spacing();

      // Organized in a table (2 columns)
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
            computationFutureBool = runAsync([this, type]() {
              ardaGen->genLocationType(type);

              uiUtils->resetTexture();
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
            computationFutureBool = runAsync([this, type]() {
              ardaGen->detectLocationType(type);
              uiUtils->resetTexture();
              redoLocations = false;
              return true;
            });
          }
        }

        ImGui::EndTable();
      }
      if (triggeredDrag) {
        ardaGen->loadNaturalFeatures(
            cfg, Fwg::IO::Reader::readGenericImage(draggedFile, cfg));
        computationFutureBool = runAsync([this]() {
          ardaGen->detectLocationType(Fwg::Civilization::LocationType::City);
          ardaGen->detectLocationType(Fwg::Civilization::LocationType::Farm);
          ardaGen->detectLocationType(Fwg::Civilization::LocationType::Port);
          ardaGen->detectLocationType(Fwg::Civilization::LocationType::Mine);
          ardaGen->detectLocationType(Fwg::Civilization::LocationType::Forest);
          uiUtils->resetTexture();
          redoLocations = false;
          return true;
        });
        triggeredDrag = false;
      }

    } else {
      ImGui::Text("Generate areas first.");
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showNavmeshTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Navmesh")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayConnections(ardaGen->areaData.regions,
                                           ardaGen->locationMap));
      uiUtils->updateImage(1, ardaGen->regionMap);
    }
    if (ardaGen->regionMap.initialised() &&
        ardaGen->locationMap.initialised()) {
      if (ImGui::Button("Generate Navmesh")) {
        computationFutureBool = runAsync([this]() {
          ardaGen->genNavmesh({}, {});
          uiUtils->resetTexture();
          return true;
        });
      }

      if (triggeredDrag) {
        triggeredDrag = false;
      }
    } else {
      ImGui::Text("Generate other maps first");
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
    if (uiUtils->tabSwitchEvent(true)) {
      uiUtils->updateImage(
          0, Arda::Gfx::displayCultureGroups(ardaGen->ardaProvinces));
      uiUtils->updateImage(1, Fwg::Gfx::Image());
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      if (ImGui::Button("Generate Culture Data")) {
        computationFutureBool = runAsync([this]() {
          ardaGen->genEconomyData();
          ardaGen->genCultureData();
          uiUtils->resetTexture();
          redoCulture = false;
          return true;
        });
      }

      if (ardaGen->ardaRegions.size()) {
        auto modifiableState = getSelectedRegion();
        if (modifiableState != nullptr && modifiableState->isLand()) {
          auto primaryCulture = modifiableState->getPrimaryCulture();
          if (primaryCulture != nullptr) {
            auto cultureGroup = primaryCulture->cultureGroup;
            auto culture = modifiableState->getPrimaryCulture();
            auto languageGroup = cultureGroup->getLanguageGroup();
            auto language = culture->language;
            // show culture group and used dataset
            ImGui::Text(("Selected culture: " + culture->name).c_str());
            ImGui::Text(("Culture group: " + cultureGroup->name).c_str());
            ImGui::Text(("Language: " + language->name).c_str());
            ImGui::Text(("Language group: " + languageGroup->name).c_str());

            static std::unordered_set<std::string> activeDatasets;
            if (resetSelection) {
              resetSelection = false;
              activeDatasets.clear();
              for (const auto &ds : languageGroup->mergedDataset.influences) {
                activeDatasets.insert(ds);
              }
            }
            // Open on button click
            if (ImGui::Button("Edit Language")) {
              ImGui::OpenPopup("DatasetPopup");
            }
            // generate a few random words to show in a table of different
            // categories: City names, male names, female names, surnames
            if (ImGui::BeginTable("ExampleNames", 5,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg)) {
              // Set up column headers
              ImGui::TableSetupColumn("City Names");
              ImGui::TableSetupColumn("Male Names");
              ImGui::TableSetupColumn("Female Names");
              ImGui::TableSetupColumn("Surnames");
              ImGui::TableSetupColumn("Other Names");
              ImGui::TableHeadersRow();

              // Number of rows to display (10)
              constexpr int rows = 10;

              for (int i = 0; i < rows; i++) {
                ImGui::TableNextRow();

                // City names
                ImGui::TableSetColumnIndex(0);
                if (i < language->cityNames.size())
                  ImGui::TextUnformatted(language->cityNames[i].c_str());

                // Male names
                ImGui::TableSetColumnIndex(1);
                if (i < language->maleNames.size())
                  ImGui::TextUnformatted(language->maleNames[i].c_str());

                // Female names
                ImGui::TableSetColumnIndex(2);
                if (i < language->femaleNames.size())
                  ImGui::TextUnformatted(language->femaleNames[i].c_str());

                // Surnames
                ImGui::TableSetColumnIndex(3);
                if (i < language->surnames.size())
                  ImGui::TextUnformatted(language->surnames[i].c_str());

                // Other names
                ImGui::TableSetColumnIndex(4);
                if (i < language->names.size())
                  ImGui::TextUnformatted(language->names[i].c_str());
              }

              ImGui::EndTable();
            }
            if (ShowDatasetPopup("DatasetPopup",
                                 languageGenerator.datasetsByLanguage,
                                 activeDatasets)) {
              // gather the datasets
              std::vector<std::string> datasetsToUse;
              for (const auto &ds : activeDatasets) {
                datasetsToUse.push_back(ds);
                auto newLanguageGroup = std::make_shared<LanguageGroup>(
                    languageGenerator.generateLanguageGroup(
                        cultureGroup->getCultures().size(), datasetsToUse));
                cultureGroup->setLanguageGroup(newLanguageGroup);
              }
            }
          }
        }
      }
      if (triggeredDrag) {
        triggeredDrag = false;

        uiUtils->resetTexture();
        // TODO Add on loading redoCulture = false;
      }
    }
    ImGui::EndTabItem();
  }
}
void ArdaUI::showReligionTab(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginSubTabItem("Religion")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0,
                           Arda::Gfx::displayReligions(ardaGen->ardaProvinces));
      uiUtils->updateImage(1, Fwg::Gfx::Image());
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {

      if (triggeredDrag) {
        triggeredDrag = false;

        uiUtils->resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}
void ArdaUI::showLanguageTab(Fwg::Cfg &cfg) {}

void ArdaUI::overview(Fwg::Cfg &cfg) {
  if (Fwg::UI::Elements::BeginMainTabItem("Overview")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0, Fwg::Gfx::displayWorldCivilisationMap(
                                  ardaGen->climateData, ardaGen->provinceMap,
                                  ardaGen->worldMap, ardaGen->regionMap, ""));
      uiUtils->updateImage(1, Fwg::Gfx::Image());
    }

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
                {Fwg::Gfx::landMask(ardaGen->terrainData), info.weight});
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
      uiUtils->updateImage(
          0, Fwg::Gfx::mergeWeightedImages(cfg.width, cfg.height, layers));
      // show the layers
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

  auto &clickEvents = uiUtils->clickEvents;
  if (clickEvents.size()) {
    auto pix = clickEvents.front();
    clickEvents.pop();
    const auto colour = ardaGen->provinceMap[pix.pixel];
    if (ardaGen->areaData.provinceColourMap.find(colour)) {
      const auto &prov = ardaGen->areaData.provinceColourMap[colour];
      if (prov->regionID < ardaGen->ardaRegions.size()) {
        auto &state = ardaGen->ardaRegions[prov->regionID];
        selectedStateIndex = state->ID;
        resetSelection = true;
      }
    }
  }
  if (!computationRunning && selectedStateIndex >= 0 &&
      selectedStateIndex < ardaGen->ardaRegions.size()) {
    return ardaGen->ardaRegions[selectedStateIndex];
  }
  return nullptr;
}

} // namespace Arda