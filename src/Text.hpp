 #pragma once
 #include <string>
 #include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
 #include <hyprland/src/render/pass/PassElement.hpp>
 #include <vector>
 #include "Globals.hpp"

 // Simple text & icon pass elements using stb_easy_font for workspace titles

 std::string buildWorkspaceTitle(int wsID);

 class CTextPassElement : public IPassElement {
 public:
     std::string text;
     CBox box; // position (x,y) baseline inside workspace box
     CHyprColor color;
     float scale = 1.f;

     CTextPassElement(const std::string& t, const CBox& b, CHyprColor c, float s = 1.f) : text(t), box(b), color(c), scale(s) {}
     virtual ~CTextPassElement() = default;

     virtual void draw(const CRegion& damage);
     virtual std::optional<CBox> boundingBox() { return box; }
     virtual bool needsLiveBlur() { return false; }
     virtual bool needsPrecomputeBlur() { return false; }
     virtual CRegion opaqueRegion() { return CRegion{}; }
     virtual const char* passName() { return "CTextPassElement"; }
 };

 class CIconPlusPassElement : public IPassElement {
 public:
     CBox box; // workspace box
     CHyprColor color;
     CIconPlusPassElement(const CBox& b, CHyprColor c) : box(b), color(c) {}
     virtual ~CIconPlusPassElement() = default;
     virtual void draw(const CRegion& damage);
     virtual std::optional<CBox> boundingBox() { return box; }
     virtual bool needsLiveBlur() { return false; }
     virtual bool needsPrecomputeBlur() { return false; }
     virtual CRegion opaqueRegion() { return CRegion{}; }
     virtual const char* passName() { return "CIconPlusPassElement"; }
 };
