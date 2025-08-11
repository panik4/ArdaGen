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

ArdaUI::ArdaUI() : FwgUI() {}

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