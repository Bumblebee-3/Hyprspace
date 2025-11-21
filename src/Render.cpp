#include "Overview.hpp"
#include "Globals.hpp"
// Removed text pass elements; we'll draw text & icons directly via rect pass elements.
// Workspace title helper (previously in Text.cpp)
static std::string buildWorkspaceTitle(int wsID) {
    auto ws = g_pCompositor->getWorkspaceByID(wsID);
    if (!ws) return std::to_string(wsID);
    std::string focusedTitle;
    if (auto fw = ws->getLastFocusedWindow()) {
        focusedTitle = fw->m_class + ":" + fw->m_title;
    }
    if (focusedTitle.empty()) {
        for (auto& w : g_pCompositor->m_windows) {
            if (!w || !w->m_isMapped || w->isHidden()) continue;
            if (w->m_workspace == ws) {
                focusedTitle = w->m_class + ":" + w->m_title;
                break;
            }
        }
    }
    if (focusedTitle.empty()) focusedTitle = "(empty)";
    return std::to_string(wsID) + " " + focusedTitle;
}
#include "src/helpers/memory/Memory.hpp"
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

// What are we even doing...
class CFakeDamageElement : public IPassElement {
public:
    CBox       box;

    CFakeDamageElement(const CBox& box) {
        this->box = box;
    }
    virtual ~CFakeDamageElement() = default;

    virtual void                draw(const CRegion& damage) {
        return;
    }
    virtual bool                needsLiveBlur() {
        return true; // hack
    }
    virtual bool                needsPrecomputeBlur() {
        return false;
    }
    virtual std::optional<CBox> boundingBox() {
        return box.copy().scale(1.f / g_pHyprOpenGL->m_renderData.pMonitor->m_scale).round();
    }
    virtual CRegion             opaqueRegion() {
        return CRegion{};
    }
    virtual const char* passName() {
        return "CFakeDamageElement";
    }

};


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.round = Config::roundedCorners; // apply rounding
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    rectdata.round = Config::roundedCorners; // apply rounding
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, CGradientValueData gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = Config::roundedCorners; // rounded border
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

// --- Simple Monospace Text Drawing (placeholder) ---
// Renders each character as a small filled rectangle to avoid dependency on GL immediate mode or external font libs.
// This is intentionally minimal; replace with proper Pango/Cairo text if richer text is needed.
static void drawMonospaceText(const std::string& text, Vector2D start, CHyprColor color, float scale = 1.f) {
    const double glyphW = 8.0 * scale;
    const double glyphH = 12.0 * scale;
    double cx = start.x;
    double cy = start.y;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\n') { cy += glyphH; cx = start.x; continue; }
        if (c == ' ') { cx += glyphW + scale; continue; }
        CRectPassElement::SRectData rd;
        rd.box = CBox{cx, cy, glyphW - 1.0, glyphH - 2.0};
        rd.color = color;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rd));
        cx += glyphW + scale;
    }
}

static void drawPlusIcon(const CBox& box, CHyprColor color) {
    double cx = box.x + box.w / 2.0;
    double cy = box.y + box.h / 2.0;
    double len = std::min(box.w, box.h) * 0.25;
    double thickness = std::clamp(box.w * 0.03, 2.0, 4.0);

    // horizontal bar
    CRectPassElement::SRectData h;
    h.box = CBox{cx - len, cy - thickness / 2.0, len * 2.0, thickness};
    h.color = color;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(h));
    // vertical bar
    CRectPassElement::SRectData v;
    v.box = CBox{cx - thickness / 2.0, cy - len, thickness, len * 2.0};
    v.color = color;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(v));
}

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !time) return;

    // Enhanced validation for Hyprland 0.52
    if (!pWindow->m_isMapped || pWindow->isHidden() || !pWindow->m_workspace) return;
    
    const auto oRealPosition = pWindow->m_realPosition->value();
    const auto oSize = pWindow->m_realSize->value();
    if (oSize.x <= 0 || oSize.y <= 0) return;

    // Check if window has valid surface
    if (!pWindow->m_wlSurface || !pWindow->m_wlSurface->exists()) return;

    // Save original state
    const auto oFullscreen = pWindow->m_fullscreenState;
    const auto oPinned = pWindow->m_pinned;
    const auto oFloating = pWindow->m_isFloating;
    const auto oDraggedWindow = g_pInputManager->m_currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->m_dragMode;

    const float curScaling = rectOverride.w / (oSize.x * pMonitor->m_scale);

    // Set window state for rendering preview (without mutating workspace)
    pWindow->m_fullscreenState = SFullscreenState{FSMODE_NONE};
    pWindow->m_isFloating = false;
    pWindow->m_pinned = true;
    g_pInputManager->m_currentlyDraggedWindow = pWindow; // force INTERACTIVERESIZEINPROGRESS = true
    g_pInputManager->m_dragMode = MBIND_RESIZE;

    SRenderModifData renderModif;
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, (pMonitor->m_position * pMonitor->m_scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->m_scale)});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
    });

    g_pHyprRenderer->damageWindow(pWindow);

    // Render with RENDER_PASS_ALL - window keeps its own workspace context for correct texture lookup
    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_ALL, false, false);

    // Restore original state
    pWindow->m_fullscreenState = oFullscreen;
    pWindow->m_isFloating = oFloating;
    pWindow->m_pinned = oPinned;
    g_pInputManager->m_currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->m_dragMode = oDragMode;
}
// Special rendering function for focused windows that minimizes state changes
void renderFocusedWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    // Save minimal state - only what's absolutely necessary
    const auto oWorkspace = pWindow->m_workspace;
    const auto oFullscreen = pWindow->m_fullscreenState;
    const auto oPinned = pWindow->m_pinned;

    // Apply minimal state changes for preview rendering
    pWindow->m_workspace = pWorkspaceOverride;
    pWindow->m_fullscreenState = SFullscreenState{FSMODE_NONE};
    pWindow->m_pinned = true;

    // Use render modification for position and scale without touching window properties
    SRenderModifData renderModif;
    const auto oRealPosition = pWindow->m_realPosition->value();
    const auto oSize = pWindow->m_realSize->value();
    const float curScaling = rectOverride.w / (oSize.x * pMonitor->m_scale);

    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, 
                                 (pMonitor->m_position * pMonitor->m_scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->m_scale)});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));

    // Auto-cleanup render modif
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
    });

    // Render the window with minimal damage
    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_ALL, false, false);

    // Restore only the essential state
    pWindow->m_workspace = oWorkspace;
    pWindow->m_fullscreenState = oFullscreen;
    pWindow->m_pinned = oPinned;
}
void renderLayerStub(PHLLS pLayer, PHLMONITOR pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    if (!pLayer->m_mapped || pLayer->m_readyToDelete || !pLayer->m_layerSurface) return;

    Vector2D oRealPosition = pLayer->m_realPosition->value();
    Vector2D oSize = pLayer->m_realSize->value();
    
    if (oSize.x <= 0 || oSize.y <= 0) return;
    
    float oAlpha = pLayer->m_alpha->value(); // set to 1 to show hidden top layer
    const auto oFadingOut = pLayer->m_fadingOut;

    const float curScaling = rectOverride.w / (oSize.x);

    SRenderModifData renderModif;

    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, pMonitor->m_position + (rectOverride.pos() / curScaling) - oRealPosition});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;
    pLayer->m_alpha->setValue(1);
    pLayer->m_fadingOut = false;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    // remove modif as it goes out of scope (wtf is this blackmagic i need to relearn c++)
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        });

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->m_fadingOut = oFadingOut;
    pLayer->m_alpha->setValue(oAlpha);
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck? xd
void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset->isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFBShouldRender = true;

    int bottomInvert = 1;
    if (Config::onBottom) bottomInvert = -1;

    // Background box
    CBox widgetBox = {owner->m_position.x, owner->m_position.y + (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale}; //TODO: update size on monitor change

    // set widgetBox relative to current monitor for rendering panel
    widgetBox.x -= owner->m_position.x;
    widgetBox.y -= owner->m_position.y;

    g_pHyprOpenGL->m_renderData.clipBox = CBox({0, 0}, owner->m_transformedSize);

    if (!Config::disableBlur) {
        renderRectWithBlur(widgetBox, Config::panelBaseColor);
    }
    else {
        renderRect(widgetBox, Config::panelBaseColor);
    }

    // Panel Border
    if (Config::panelBorderWidth > 0) {
        // Border box
        CBox borderBox = {widgetBox.x, owner->m_position.y + (Config::onBottom * owner->m_transformedSize.y) + (Config::panelHeight + Config::reservedArea - curYOffset->value() * owner->m_scale) * bottomInvert, owner->m_transformedSize.x, static_cast<double>(Config::panelBorderWidth)};
        borderBox.y -= owner->m_position.y;

        renderRect(borderBox, Config::panelBorderColor);
    }


    // unscaled and relative to owner
    //CBox damageBox = {0, (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea)))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    //owner->addDamage(damageBox);
    g_pHyprRenderer->damageMonitor(owner);

    // add a fake element with needsliveblur = true and covers the entire monitor to ensure damage applies to the entire monitor
    // unoptimized atm but hey its working
    CFakeDamageElement fakeDamage = CFakeDamageElement(CBox({0, 0}, owner->m_transformedSize));
    g_pHyprRenderer->m_renderPass.add(makeUnique<CFakeDamageElement>(fakeDamage));

    // the list of workspaces to show
    std::vector<int> workspaces;

    if (Config::showSpecialWorkspace) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    // find the lowest and highest workspace id to determine which empty workspaces to insert
    int lowestID = INT_MAX;
    int highestID = 1;
    for (auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws) continue;
        // normal workspaces start from 1, special workspaces ends on -2
        if (ws->m_id < 1) continue;
        if (ws->m_monitor->m_id == ownerID) {
            workspaces.push_back(ws->m_id);
            if (highestID < ws->m_id) highestID = ws->m_id;
            if (lowestID > ws->m_id) lowestID = ws->m_id;
        }
    }

    // include empty workspaces that are between non-empty ones
    if (Config::showEmptyWorkspace) {
        int wsIDStart = 1;
        int wsIDEnd = highestID;

        // hyprsplit/split-monitor-workspaces compatibility
        if (numWorkspaces > 0) {
            wsIDStart = std::min<int>(numWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(numWorkspaces * ownerID + 1, highestID); // always show the initial workspace for current monitor
        }

        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            if (i == owner->activeSpecialWorkspaceID()) continue;
            const auto pWorkspace = g_pCompositor->getWorkspaceByID(i);
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    // add a new empty workspace at last
    if (Config::showNewWorkspace) {
        // get the lowest empty workspce id after the highest id of current workspace
        while (g_pCompositor->getWorkspaceByID(highestID) != nullptr) highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());

    // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = ((Config::panelHeight - 2 * Config::workspaceMargin) / (owner->m_transformedSize.y)) * owner->m_scale; // scale box with panel height
    double workspaceBoxW = owner->m_transformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->m_transformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + (Config::workspaceMargin * owner->m_scale) * (wsCount - 1);
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset->value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset->value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - curYOffset->value()) : (owner->m_transformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - workspaceBoxH + curYOffset->value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (Config::workspaceMargin * owner->m_scale), 0);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};

        // workspace background rect (NOT background layer) and border
        if (ws == owner->m_activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceActiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceActiveBackground); // cant really round it until I find a proper way to clip windows to a rounded rect
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceActiveBackground);
            }
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->m_scale);
                continue;
            }
        }
        else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceInactiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
        }

        // background and bottom layers
        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_layerSurfaceLayers[0]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_renderData.clipBox = CBox();
            }
            for (auto& ls : owner->m_layerSurfaceLayers[1]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_renderData.clipBox = CBox();
            }
        }

        // the mini panel to cover the awkward empty space reserved by the panel
        if (owner->m_activeWorkspace == ws && Config::affectStrut) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            if (Config::onBottom) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            if (!Config::disableBlur) {
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }
            else {
                // what
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }

        }

        if (ws != nullptr) {
            // draw tiled windows
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                // Skip invalid windows or windows without proper state
                if (!w->m_isMapped || w->isHidden()) continue;
                if (w->m_workspace == ws && !w->m_isFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    // CRITICAL: Use renderWindowStub WITHOUT workspace parameter - let window use its own texture buffer
                    renderWindowStub(w, owner, curWindowBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                // Skip invalid windows or windows without proper state
                if (!w->m_isMapped || w->isHidden()) continue;
                if (w->m_workspace == ws && w->m_isFloating && ws->getLastFocusedWindow() != w) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    // CRITICAL: Use renderWindowStub WITHOUT workspace parameter - let window use its own texture buffer
                    renderWindowStub(w, owner, curWindowBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_isFloating) {
                    const auto w = ws->getLastFocusedWindow();
                    // Skip invalid windows or windows without proper state
                    if (!w->m_isMapped || w->isHidden()) {
                        // Continue to next workspace instead of breaking the loop
                        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->m_scale;
                        continue;
                    }
                    double wX = curWorkspaceRectOffsetX + ((w->m_realPosition->value().x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_realPosition->value().y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_realSize->value().x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_realSize->value().y * monitorSizeScaleFactor * owner->m_scale;
                    if (wW > 0 && wH > 0) {
                        CBox curWindowBox = {wX, wY, wW, wH};
                        g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                        //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                        // CRITICAL: Use renderWindowStub WITHOUT workspace parameter - let window use its own texture buffer
                        renderWindowStub(w, owner, curWindowBox, &time);
                        g_pHyprOpenGL->m_renderData.clipBox = CBox();
                    }
                }
        }

        if (owner->m_activeWorkspace != ws || !Config::hideRealLayers) {
            // this layer is hidden for real workspace when panel is displayed
            if (!Config::hideTopLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_realPosition->value() - owner->m_position) * monitorSizeScaleFactor, ls->m_realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_renderData.clipBox = CBox();
                }
        }


        // Resets workspaceBox to scaled absolute coordinates for input detection.
        // While rendering is done in pixel coordinates, input detection is done in
        // scaled coordinates, taking into account monitor scaling.
        // Since the monitor position is already given in scaled coordinates,
        // we only have to scale all relative coordinates, then add them to the
        // monitor position to get a scaled absolute position.
        curWorkspaceBox.scale(1 / owner->m_scale);

        curWorkspaceBox.x += owner->m_position.x;
        curWorkspaceBox.y += owner->m_position.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        // set the current position to the next workspace box
        // Render workspace title if enabled (render after contents for overlay clarity)
        if (Config::showWorkspaceTitles) {
            std::string title = buildWorkspaceTitle(wsID);
            CHyprColor textColor = (ws && ws == owner->m_activeWorkspace) ? Config::workspaceActiveBorder : Config::workspaceInactiveBorder;
            // fallback if color fully transparent
            if (textColor.a < 0.05f) textColor = Config::workspaceActiveBorder.a > 0.05f ? Config::workspaceActiveBorder : Config::panelBorderColor;
            drawMonospaceText(title, {curWorkspaceRectOffsetX + 8.0, curWorkspaceRectOffsetY + 8.0}, textColor, 1.f);
        }

        if (Config::showNewWorkspaceIcon && ws == nullptr && wsID == workspaces.back()) {
            CHyprColor iconColor = Config::workspaceInactiveBorder;
            if (iconColor.a < 0.05f) iconColor = Config::workspaceActiveBorder.a > 0.05f ? Config::workspaceActiveBorder : Config::panelBorderColor;
            drawPlusIcon(curWorkspaceBox, iconColor);
        }

        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->m_scale;
    }
}
