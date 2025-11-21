 #include "Text.hpp"
 #include <hyprland/src/Compositor.hpp>
 #include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
 #include <algorithm>
 #include <cstring>
 #include <cmath>

// Removed stb_easy_font usage; using simple rect glyphs.

 std::string buildWorkspaceTitle(int wsID) {
     auto ws = g_pCompositor->getWorkspaceByID(wsID);
     if (!ws) return std::to_string(wsID);
     // Focused window preferred
     std::string focusedTitle;
     if (auto fw = ws->getLastFocusedWindow()) {
         focusedTitle = fw->m_class + ":" + fw->m_title;
     }
     if (focusedTitle.empty()) {
         // fallback: enumerate first non-hidden mapped window
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

 // Draw text by adding small rect pass elements for each character (avoids immediate-mode GL)
 void CTextPassElement::draw(const CRegion& damage) {
     if (text.empty()) return;

     // simple monospace glyph approximation: each char is 8x12 scaled by `scale`
     const double glyphW = 8.0 * scale;
     const double glyphH = 12.0 * scale;
     double cx = box.x;
     double cy = box.y;

     for (size_t i = 0; i < text.size(); ++i) {
         const char c = text[i];
         if (c == '\n') {
             cy += glyphH;
             cx = box.x;
             continue;
         }
         if (c == ' ') { cx += glyphW + scale; continue; }

         CRectPassElement::SRectData rd;
         rd.box = CBox{cx, cy, glyphW - 1.0, glyphH - 2.0};
         rd.color = color;
         g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rd));

         cx += glyphW + scale;
     }
 }

 void CIconPlusPassElement::draw(const CRegion& damage) {
     // Draw a plus sign centered in workspace box using two thin rects
     double cx = box.x + box.w / 2.0;
     double cy = box.y + box.h / 2.0;
     double len = std::min(box.w, box.h) * 0.25;
     double thickness = std::max(2.0, std::min(4.0, box.w * 0.03));

     // horizontal
     CRectPassElement::SRectData h;
     h.box = CBox{cx - len, cy - thickness / 2.0, len * 2.0, thickness};
     h.color = color;
     g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(h));

     // vertical
     CRectPassElement::SRectData v;
     v.box = CBox{cx - thickness / 2.0, cy - len, thickness, len * 2.0};
     v.color = color;
     g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(v));
 }
