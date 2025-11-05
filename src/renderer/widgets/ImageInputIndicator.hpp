#pragma once

#include "IWidget.hpp"
#include "../../defines.hpp"
#include "../../helpers/Color.hpp"
#include "../../helpers/Math.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../core/Timer.hpp"
#include "Shadowable.hpp"
#include <string>
#include <filesystem>
#include <unordered_map>
#include <any>

struct SPreloadedAsset;
class COutput;

class CImageInputIndicator : public IWidget {
  public:
    CImageInputIndicator() = default;
    ~CImageInputIndicator();

    void         registerSelf(const ASP<CImageInputIndicator>& self);

    virtual void configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput);
    virtual bool draw(const SRenderData& data);
    virtual void onAssetUpdate(ResourceID id, ASP<CTexture> newAsset);

    virtual CBox getBoundingBoxWl() const;
    virtual void onClick(uint32_t button, bool down, const Vector2D& pos);
    virtual void onHover(const Vector2D& pos);

    void         reset();

    void         renderUpdate();
    void         selectImage();
    void         onTimerUpdate();
    void         plantTimer();

  private:
    AWP<CImageInputIndicator>                     m_self;

    CFramebuffer                    imageFB;

    int                             size     = 0;
    int                             rounding = 0;
    double                          border   = 0;
    double                          angle    = 0;
    CGradientValueData              color;
    Vector2D                        pos;
    Vector2D                        configPos;

    bool                            checkWaiting = false;
    bool                            displayFail  = false;
    size_t                          passwordLength = 0;

    std::string                     halign, valign, path, image_path;
    size_t                          num_images;

    bool                            firstRender = true;

    int                             reloadTime;
    std::string                     reloadCommand;
    std::string                     onclickCommand;

    std::filesystem::file_time_type modificationTime;
    size_t                          m_imageRevision = 0;

    ASP<CTimer>                     imageTimer;

    Vector2D                        viewport;
    std::string                     stringPort;

    ResourceID                      resourceID        = 0;
    bool                            m_pendingResource = false;

    ASP<CTexture>                   asset = nullptr;
    CShadowable                     shadow;
};
