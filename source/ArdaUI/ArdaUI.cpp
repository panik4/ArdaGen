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
                overview(ardaGen, cfg);
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

  // if (generator->civLayer.urbanisation.size() != cfg.bitmapSize ||
  //     generator->civLayer.agriculture.size() != cfg.bitmapSize) {
  //   if (printIssue)
  //     Fwg::Utils::Logging::logLine(
  //         "Civilisation tab data missing or wrong size.");
  //   ready = false;
  // }

  // if (!generator->civLayer.agriculture.size() ||
  //     !generator->locationMap.size()) {
  //   if (printIssue)
  //     Fwg::Utils::Logging::logLine("Locations tab data is missing.");
  //   ready = false;
  // }

  return ready;
}
void ArdaUI::showCivilizationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  int retCode = 0;
  if (UI::Elements::BeginMainTabItem("Civilisation")) {
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
    // if (UI::Elements::BeginMainTabItem("Scenario")) {
    //   if (uiUtils->tabSwitchEvent()) {
    //     // uiUtils->updateImage(
    //     //     0, Fwg::Gfx::displayWorldCivilisationMap(
    //     //            activeGenerator->climateData,
    //     //            activeGenerator->provinceMap,
    //     activeGenerator->worldMap,
    //     //            activeGenerator->civLayer, activeGenerator->regionMap,
    //     //            ""));
    //     uiUtils->updateImage(0, Fwg::Gfx::Bitmap());
    //     uiUtils->updateImage(1, Fwg::Gfx::Bitmap());
    //     scenGenReady = scenarioGenReady(true);
    //     // if we detect a change in the previous tabs, we also reset
    //     // "configuredScenarioGen", to FORCE the user to remap areas
    //     if (!scenGenReady) {
    //       configuredScenarioGen = false;
    //     }
    //   }

    // allow printing why the scenario generation is not ready
    if (scenGenReady) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.2f, 0.6f, 0.9f, 1.0f)); // background
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.3f, 0.7f, 1.0f, 1.0f)); // hover
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.1f, 0.4f, 0.8f, 1.0f)); // clicked
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                          6.0f); // rounded corners
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f); // add border
      if (ImGui::Button("Remap areas every single time you change something in "
                        "the previous tabs on the left.",
                        ImVec2(ImGui::GetContentRegionAvail().x * 0.8f, 50))) {
        // if (!ardaGen->createPaths()) {
        //   Fwg::Utils::Logging::logLine("ERROR: Couldn't create paths");
        //   retCode = -1;
        // }
        //  start with the generic stuff in the Scenario Generator
        //  activeGenerator->wrapup(cfg);
        ardaGen->mapProvinces();
        ardaGen->mapRegions();
        // ardaGen->mapTerrain();
        ardaGen->mapContinents();

        configuredScenarioGen = true;
      }
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(3);

      uiUtils->showHelpTextBox("Civilisation");
      if (fwg.areaData.provinces.size() && fwg.areaData.regions.size() &&
          fwg.areaData.continents.size()) {

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
        ImGui::InputDouble("<--Urbanisation factor", &cfg.urbanFactor, 0.1);
        ImGui::InputDouble("<--Agriculture factor", &cfg.agricultureFactor,
                           0.05);
        ImGui::PopItemWidth();
        ImGui::SeparatorText("Generate all civ data automatically");
        if (UI::Elements::AutomationStepButton(
                "Generate all civilisation data automatically")) {
          computationFutureBool = runAsync([&fwg, &cfg, this]() {
            fwg.genCivData(cfg);
            Arda::Civilization::generateWorldCivilizations(
                ardaGen->ardaRegions, ardaGen->ardaProvinces, ardaGen->civData,
                ardaGen->ardaContinents, ardaGen->superRegions);
            uiUtils->resetTexture();
            return true;
          });
        }
        ImGui::SeparatorText("Manually edit civ data");
        if (UI::Elements::BeginSubTabBar("Civilisation stuff")) {
          showDevelopmentTab(cfg, fwg);
          showPopulationTab(cfg, fwg);
          showCultureTab(cfg);
          showLocationTab(cfg, fwg);

          UI::Elements::EndSubTabBar();
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

void ArdaUI::showDevelopmentTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  static bool drawingMode = false;
  if (UI::Elements::BeginSubTabItem("Development")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Fwg::Gfx::displayDevelopment(fwg.areaData.provinces));
      uiUtils->updateImage(1, fwg.worldMap);
    }
    if (!fwg.areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!fwg.areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      if (triggeredDrag) {
        triggeredDrag = false;
        // load development map
        fwg.loadDevelopment(cfg, draggedFile);
        uiUtils->resetTexture();
      }
      ImGui::PushItemWidth(200.0f);
      for (auto &continent : fwg.areaData.continents) {
        std::string displayString =
            "Continent development modifier for continent" +
            std::to_string(continent->ID);
        ImGui::InputDouble(displayString.c_str(),
                           &continent->developmentModifier);
      }
      ImGui::Checkbox("Random Development", &cfg.randomDevelopment);
      if (ImGui::Button("Generate Development")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genDevelopment(cfg);
          uiUtils->resetTexture();
          return true;
        });
      }
      // ImGui::Checkbox("Draw mode", &drawingMode);
      ImGui::PopItemWidth();
      // for drawing. Simpledraw not possible due to per-province edit
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (fwg.terrainData.landForms[pix.first.pixel].altitude > 0.0) {
      //      const auto &colour = fwg.provinceMap[pix.first.pixel];
      //      if (fwg.areaData.provinceColourMap.find(colour)) {
      //        const auto &prov = fwg.areaData.provinceColourMap[colour];
      //        // only allow drawing if enabled, otherwise take the click as
      //        a
      //        // select of the continent
      //        if (drawingMode) {
      //          if (pix.first.type == InteractionType::CLICK) {
      //            prov->developmentFactor = pix.second;
      //          } else if (pix.first.type == InteractionType::RCLICK) {
      //            prov->developmentFactor = 0.0;
      //          }
      //        } else {
      //          // sets the development modifier to the brush strength
      //          if (pix.first.type == InteractionType::CLICK) {
      //            for (auto &continent : fwg.areaData.continents) {
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
      //    fwg.genDevelopment(cfg);
      //  }
      //  uiUtils->resetTexture();
      //}
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showPopulationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  if (UI::Elements::BeginSubTabItem("Population")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0,
                           Fwg::Gfx::displayPopulation(fwg.areaData.provinces));
      uiUtils->updateImage(1, fwg.worldMap);
    }
    if (!fwg.areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!fwg.areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      if (ImGui::Button("Generate Population")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genPopulation(cfg);
          uiUtils->resetTexture();
          return true;
        });
      }

      ImGui::SeparatorText(
          "Drag and drop in an image of the correct resolution to set the "
          "population density. The red channel of the input image will be "
          "used "
          "to set population, where bright red means high population, black "
          "means no population");
      if (triggeredDrag) {
        triggeredDrag = false;
        // load population map
        fwg.loadPopulation(cfg,
                           Fwg::IO::Reader::readGenericImage(draggedFile, cfg));
        uiUtils->resetTexture();
      }
      // for drawing
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (fwg.terrainData.landForms[pix.first.pixel].altitude > 0.0) {
      //      const auto &colour = fwg.provinceMap[pix.first.pixel];
      //      if (fwg.areaData.provinceColourMap.find(colour)) {
      //        const auto &prov = fwg.areaData.provinceColourMap[colour];
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
void ArdaUI::showLocationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {

  if (UI::Elements::BeginSubTabItem("Locations")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Fwg::Gfx::displayLocations(fwg.areaData.regions, fwg.worldMap));
      uiUtils->updateImage(1, fwg.regionMap);
    }
    ImGui::SliderInt("Amount of separate cities per region",
                     &cfg.citiesPerRegion, 1, 10);
    ImGui::SliderInt("Amount of separate farm areas per region",
                     &cfg.agriculturePerRegion, 1, 10);

    if (fwg.areaData.regions.size()) {
      if (ImGui::Button("Generate Locations")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genLocations(cfg);
          uiUtils->resetTexture();
          return true;
        });
      }

      if (triggeredDrag) {
        triggeredDrag = false;
      }

    } else {
      ImGui::Text("Generate regions first.");
    }
    ImGui::EndTabItem();
  }
}

void ArdaUI::showNavmeshTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  if (UI::Elements::BeginSubTabItem("Navmesh")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0, Fwg::Gfx::displayConnections(fwg.areaData.regions,
                                                           fwg.locationMap));
      uiUtils->updateImage(1, fwg.regionMap);
    }
    if (fwg.regionMap.initialised() && fwg.locationMap.initialised()) {
      if (ImGui::Button("Generate Navmesh")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genNavmesh(cfg);
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

void ArdaUI::showUrbanisationTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  if (UI::Elements::BeginSubTabItem("Cities")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0,
                           Fwg::Gfx::displayUrban(fwg.worldMap, fwg.civLayer));
      uiUtils->updateImage(1, fwg.regionMap);
    }
    if (fwg.provinceMap.initialised()) {

      if (ImGui::Button("Generate Cities")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genUrbanisation(cfg);
          uiUtils->resetTexture();
          return true;
        });
      }
      ImGui::SeparatorText(
          "Drag and drop in an image of the correct resolution to set the "
          "city density. The shade of red of the input image will be used "
          "to set cities, where bright red means dense cities, black "
          "means no cities. Ensure the colour is pure red!");
      ImGui::SeparatorText(
          "It is recommended to take an image such as the worldmap.png and "
          "draw into it with red, and then drag it in. This way, you have a "
          "reference for drawing cities");
      ImGui::SeparatorText(
          "You can also draw in this image, making use of the brush settings "
          "at the top. Zoom with Ctrl + Mousewheel. Drag with Ctrl + left "
          "mouse");
      if (triggeredDrag) {
        // load city map
        fwg.loadUrbanisation(cfg, draggedFile);
        triggeredDrag = false;
        uiUtils->resetTexture();
      }

      // for drawing
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (fwg.terrainData.landForms[pix.first.pixel].altitude > 0.0) {
      //      if (pix.first.type == InteractionType::CLICK) {
      //        fwg.civLayer.urbanisation[pix.first.pixel] =
      //            static_cast<unsigned char>(pix.second * 255.0f);
      //      } else if (pix.first.type == InteractionType::RCLICK) {
      //        fwg.civLayer.urbanisation[pix.first.pixel] = 0;
      //      }
      //    }
      //  }

      //  displayImage = Fwg::Gfx::displayUrban(fwg.worldMap, fwg.civLayer);
      //  uiUtils->resetTexture();
      //}

    } else {
      ImGui::Text("Generate other maps first");
    }
    ImGui::EndTabItem();
  }
}
void ArdaUI::showAgricultureTab(Fwg::Cfg &cfg, Fwg::FastWorldGenerator &fwg) {
  if (UI::Elements::BeginSubTabItem("Agriculture")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(
          0, Fwg::Gfx::displayAgriculture(fwg.worldMap, fwg.civLayer));
      uiUtils->updateImage(1, Fwg::Gfx::Bitmap());
    }
    if (fwg.provinceMap.initialised()) {
      if (ImGui::Button("Generate Agriculture")) {
        computationFutureBool = runAsync([&fwg, &cfg, this]() {
          fwg.genAgriculture(cfg);
          uiUtils->resetTexture();
          return true;
        });
      }
      ImGui::SeparatorText(
          "Drag and drop in an image of the correct resolution to set the "
          "agriculture density. The shade of red of the input image will be "
          "used "
          "to set agriculture, where bright red means dense agriculture, "
          "black "
          "means no agriculture. Ensure the colour is pure red!");
      ImGui::SeparatorText(
          "It is recommended to take an image such as the worldmap.png and "
          "draw into it with red, and then drag it in. This way, you have a "
          "reference for drawing agriculture");
      ImGui::SeparatorText(
          "You can also draw in this image, making use of the brush settings "
          "at the top. Zoom with Ctrl + Mousewheel. Drag with Ctrl + left "
          "mouse");
      if (triggeredDrag) {
        // load city map
        fwg.loadAgriculture(cfg, draggedFile);
        triggeredDrag = false;
        uiUtils->resetTexture();
      }

      // for drawing
      // auto affected = uiUtils->getLatestAffectedPixels();
      // if (affected.size() > 0) {
      //  for (auto &pix : affected) {
      //    if (fwg.terrainData.landForms[pix.first.pixel].altitude > 0.0) {
      //      if (pix.first.type == InteractionType::CLICK) {
      //        fwg.civLayer.agriculture[pix.first.pixel] =
      //            static_cast<unsigned char>(pix.second * 255.0f);
      //      } else if (pix.first.type == InteractionType::RCLICK) {
      //        fwg.civLayer.agriculture[pix.first.pixel] = 0;
      //      }
      //    }
      //  }

      //  displayImage = Fwg::Gfx::displayAgriculture(fwg.worldMap,
      //  fwg.civLayer); uiUtils->resetTexture();
      //}

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
  if (UI::Elements::BeginSubTabItem("Culture")) {
    if (uiUtils->tabSwitchEvent(true)) {
      uiUtils->updateImage(0,
                           Arda::Gfx::displayCultureGroups(ardaGen->civData));
      uiUtils->updateImage(1, Fwg::Gfx::Bitmap());
    }
    if (!ardaGen->areaData.provinces.size()) {
      ImGui::Text("Provinces are missing, make sure there were no errors in "
                  "the province generation.");
    } else if (!ardaGen->areaData.regions.size()) {
      ImGui::Text("Regions are missing, make sure there were no errors in the "
                  "region generation.");
    } else {
      static int selectedStateIndex = 0;
      static bool resetSelection = false;
      static std::string drawCountryTag;
      if (!drawBorders) {
        drawCountryTag = "";
      }
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
      if (ardaGen->ardaRegions.size()) {
        auto &modifiableState = ardaGen->ardaRegions[selectedStateIndex];
        if (modifiableState->isLand()) {

          auto &cultureGroup =
              modifiableState->getPrimaryCulture()->cultureGroup;
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
      if (triggeredDrag) {
        triggeredDrag = false;

        uiUtils->resetTexture();
      }
    }
    ImGui::EndTabItem();
  }
}
void ArdaUI::showReligionTab(Fwg::Cfg &cfg) {
  if (UI::Elements::BeginSubTabItem("Religion")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0,
                           Arda::Gfx::displayReligions(ardaGen->ardaRegions));
      uiUtils->updateImage(1, Fwg::Gfx::Bitmap());
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

void ArdaUI::overview(std::shared_ptr<Arda::ArdaGen> &ardaGen, Fwg::Cfg &cfg) {
  if (UI::Elements::BeginMainTabItem("Overview")) {
    if (uiUtils->tabSwitchEvent()) {
      uiUtils->updateImage(0, Fwg::Gfx::displayWorldCivilisationMap(
                                  ardaGen->climateData, ardaGen->provinceMap,
                                  ardaGen->worldMap, ardaGen->civLayer,
                                  ardaGen->regionMap, ""));
      uiUtils->updateImage(1, Fwg::Gfx::Bitmap());
    }

    if (showVisualLayerToggles(visualLayerInfos)) {
      std::vector<Fwg::Gfx::WeightedBitmap> layers;
      for (const auto &[type, info] : visualLayerInfos) {
        if (info.toggled) {
          switch (type) {
          case VisualLayerType::HEIGHTMAP:
            layers.push_back({Fwg::Gfx::displayHeightMap(
                                  ardaGen->terrainData.detailedHeightMap),
                              info.weight});
            break;

          case VisualLayerType::ELEVATIONTYPES:
            layers.push_back(
                {Fwg::Gfx::landMap(ardaGen->terrainData), info.weight});
            break;

          case VisualLayerType::TOPOGRAPHY:
            layers.push_back(
                {Fwg::Gfx::topographyMap(ardaGen->terrainData.landForms),
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
                {Fwg::Gfx::displayPopulation(ardaGen->areaData.provinces),
                 info.weight});
            break;

          case VisualLayerType::DEVELOPMENT:
            layers.push_back(
                {Fwg::Gfx::displayDevelopment(ardaGen->areaData.provinces),
                 info.weight});
            break;

          case VisualLayerType::LOCATIONS:
            layers.push_back({Fwg::Gfx::displayLocations(
                                  ardaGen->areaData.regions, ardaGen->worldMap),
                              info.weight});
            break;

          case VisualLayerType::WORLD_MAP:
            layers.push_back({ardaGen->worldMap, info.weight});
            break;

          case VisualLayerType::CIVILISATION_MAP:
            layers.push_back({Fwg::Gfx::displayWorldCivilisationMap(
                                  ardaGen->climateData, ardaGen->provinceMap,
                                  ardaGen->worldMap, ardaGen->civLayer,
                                  ardaGen->regionMap, ""),
                              info.weight});
            break;

          case VisualLayerType::COUNTRIES:
            layers.push_back({ardaGen->countryMap, info.weight});
            break;

          case VisualLayerType::CULTUREGROUPS:
            layers.push_back({Arda::Gfx::displayCultureGroups(ardaGen->civData),
                              info.weight});
            break;

          case VisualLayerType::CULTURES:
            layers.push_back({Arda::Gfx::displayCultures(ardaGen->ardaRegions),
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
          0, Fwg::Gfx::mergeWeightedBitmaps(cfg.width, cfg.height, layers));
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

// int ArdaUI::showScenarioTab(Fwg::Cfg &cfg,
//                          std::shared_ptr<Rpx::GenericModule> activeModule)
//                          {
//   int retCode = 0;
//   if (ImGui::BeginTabItem("Scenario")) {
//
//     ImGui::EndTabItem();
//   }
//   return retCode;
// }

// void ArdaUI::countryEdit(std::shared_ptr<Arda::ArdaGen> generator) {
//   static int selectedStateIndex = 0;
//   static std::string drawCountryTag;
//   if (!drawBorders) {
//     drawCountryTag = "";
//   }
//   auto &clickEvents = uiUtils->clickEvents;
//   if (clickEvents.size()) {
//     auto pix = clickEvents.front();
//     clickEvents.pop();
//     const auto &colour = generator->provinceMap[pix.pixel];
//     if (generator->areaData.provinceColourMap.find(colour)) {
//       const auto &prov = generator->areaData.provinceColourMap[colour];
//       if (prov->regionID < generator->ardaRegions.size()) {
//         auto &state = generator->ardaRegions[prov->regionID];
//         selectedStateIndex = state->ID;
//       }
//     }
//   }
//   if (generator->ardaRegions.size()) {
//     auto &modifiableState = generator->ardaRegions[selectedStateIndex];
//
//     if ((modifiableState->owner &&
//              generator->countries.find(modifiableState->owner->tag) !=
//                  generator->countries.end() ||
//          (drawCountryTag.size()) &&
//          generator->countries.find(drawCountryTag)
//          !=
//                                         generator->countries.end())) {
//       std::shared_ptr selectedCountry =
//           modifiableState->owner ? modifiableState->owner
//                                  : generator->countries.at(drawCountryTag);
//       if (!drawBorders) {
//         drawCountryTag = selectedCountry->tag;
//       }
//       std::string tempTag = selectedCountry->tag;
//       static std::string bufferChangedTag = "";
//
//       Elements::borderChild("CountryEdit", [&]() {
//         if (ImGui::InputText("Country tag", &tempTag)) {
//           bufferChangedTag = tempTag;
//         }
//         if (ImGui::Button("update tag")) {
//           if (bufferChangedTag.size() != 3) {
//             Fwg::Utils::Logging::logLine("Tag must be 3 characters long");
//           } else {
//             std::string &oldTag = selectedCountry->tag;
//             if (oldTag == bufferChangedTag) {
//               Fwg::Utils::Logging::logLine("Tag is the same as the old
//               one");
//             } else {
//               generator->countries.erase(oldTag);
//               selectedCountry->tag = bufferChangedTag;
//               // add country under different tag
//               generator->countries.insert(
//                   {selectedCountry->tag, selectedCountry});
//               for (auto &region : selectedCountry->ownedRegions) {
//                 region->owner = selectedCountry;
//               }
//             }
//             requireCountryDetails = true;
//             generator->visualiseCountries(generator->countryMap);
//           }
//         }
//         ImGui::InputText("Country name", &selectedCountry->name);
//         ImGui::InputText("Country adjective", &selectedCountry->adjective);
//         ImVec4 color =
//             ImVec4(((float)selectedCountry->colour.getRed()) / 255.0f,
//                    ((float)selectedCountry->colour.getGreen()) / 255.0f,
//                    ((float)selectedCountry->colour.getBlue()) /
//                    255.0f, 1.0f);
//
//         if (ImGui::ColorEdit3("Country Colour", (float *)&color,
//                               ImGuiColorEditFlags_NoInputs |
//                                   ImGuiColorEditFlags_NoLabel |
//                                   ImGuiColorEditFlags_HDR)) {
//           selectedCountry->colour = Fwg::Gfx::Colour(
//               color.x * 255.0, color.y * 255.0, color.z * 255.0);
//           generator->visualiseCountries(generator->countryMap);
//           uiUtils->resetTexture();
//         }
//       });
//
//       if (drawBorders && drawCountryTag.size()) {
//         if (generator->countries.find(drawCountryTag) !=
//             generator->countries.end()) {
//           selectedCountry = generator->countries.at(drawCountryTag);
//         }
//         if (selectedCountry != nullptr && !modifiableState->isSea() &&
//             modifiableState->owner != selectedCountry) {
//           modifiableState->owner->removeRegion(modifiableState);
//           modifiableState->owner = selectedCountry;
//           selectedCountry->addRegion(modifiableState);
//           requireCountryDetails = true;
//           generator->visualiseCountries(generator->countryMap,
//                                         modifiableState->ID);
//           uiUtils->updateImage(0, generator->countryMap);
//         }
//       }
//     }
//
//     Elements::borderChild("StateEdit", [&]() {
//       if (ImGui::InputText("State name", &modifiableState->name)) {
//         requireCountryDetails = true;
//       }
//       if (modifiableState->owner) {
//         ImGui::Text("State owner", &modifiableState->owner->tag);
//       }
//       if (ImGui::InputInt("Population", &modifiableState->totalPopulation))
//       {
//         requireCountryDetails = true;
//       }
//     });
//   }
// }
//
// int ArdaUI::showCountryTab(Fwg::Cfg &cfg) {
//   if (ImGui::BeginTabItem("Countries")) {
//     if (uiUtils->tabSwitchEvent(true)) {
//       uiUtils->updateImage(
//           0, generator->visualiseCountries(generator->countryMap));
//       uiUtils->updateImage(1, generator->worldMap);
//     }
//
//     ImGui::Text(
//         "Use auto generated country map or drop it in. You may also first "
//         "generate a country map, then edit it in the Maps folder, and then
//         " "drop it in again.");
//     ImGui::Text("You can also drag in a list of countries (in a .txt file)
//     "
//                 "with the following format: #r;g;b;tag;name;adjective. See
//                 " "inputs//countryMappings.txt as an example. If no file is
//                 " "dragged in, this example file is used.");
//     ImGui::PushItemWidth(300.0f);
//     uiUtils->showHelpTextBox("Countries");
//     ImGui::InputInt("Number of countries", &generator->numCountries);
//     // ImGui::InputText("Path to country list: ",
//     // &generator->countryMappingPath); ImGui::InputText("Path to state
//     list:
//     // ", &generator->regionMappingPath);
//     ImGui::Checkbox("Draw-borders", &drawBorders);
//
//
//
//     ImGui::EndTabItem();
//     ImGui::PopItemWidth();
//   }
//   return 0;
// }
} // namespace Arda