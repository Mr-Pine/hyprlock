#include "ImageInputIndicator.hpp"

#include "Image.hpp"
#include "../Renderer.hpp"
#include "../AsyncResourceManager.hpp"
#include "../../core/hyprlock.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "src/auth/Auth.hpp"

#include <cmath>
#include <hyprlang.hpp>
#include <hyprutils/math/Vector2D.hpp>

CImageInputIndicator::~CImageInputIndicator() {
    reset();
}

void CImageInputIndicator::registerSelf(const ASP<CImageInputIndicator>& self) {
    m_self = self;
}

static void onTimer(AWP<CImageInputIndicator> ref) {
    if (auto PIMAGE = ref.lock(); PIMAGE) {
        PIMAGE->onTimerUpdate();
        PIMAGE->plantTimer();
    }
}

void CImageInputIndicator::selectImage() {
    if (m_pendingResource) {
        Debug::log(WARN, "Trying to select image, but a resource is still pending! Skipping update.");
        return;
    }

    std::string new_image_path;
    new_image_path.append(path);
    new_image_path.append("/");
    if (checkWaiting) {
        new_image_path.append("unlocked");
    } else if (displayFail) {
        new_image_path.append("wrong");
    } else {
        size_t image_number = passwordLength % num_images;
        new_image_path.append(std::format("{:02d}", image_number));
    }

    new_image_path.append(".png");

    if (new_image_path == image_path) {
        return;
    }

    Debug::log(INFO, "Selected new image: {}", new_image_path);
    image_path = new_image_path;

    m_pendingResource = true;

    if (resourceID == 0) {
        resourceID = g_asyncResourceManager->requestImage(image_path, m_imageRevision, nullptr);
        Debug::log(INFO, "New resourceID: {}", resourceID);
    } else {
        return;
        AWP<IWidget> widget(m_self);
        g_asyncResourceManager->requestImage(image_path, m_imageRevision, widget);
    }
}

void CImageInputIndicator::onTimerUpdate() {
    if (m_pendingResource) {
        Debug::log(WARN, "Trying to update image, but a resource is still pending! Skipping update.");
        return;
    }

    const std::string OLDPATH = path;

    if (!reloadCommand.empty()) {
        path = spawnSync(reloadCommand);

        if (path.ends_with('\n'))
            path.pop_back();

        if (path.starts_with("file://"))
            path = path.substr(7);

        if (path.empty())
            return;
    }

    try {
        const auto MTIME = std::filesystem::last_write_time(absolutePath(path, ""));
        if (OLDPATH == path && MTIME == modificationTime)
            return;

        modificationTime = MTIME;
        if (OLDPATH == path)
            m_imageRevision++;
        else
            m_imageRevision = 0;
    } catch (std::exception& e) {
        path = OLDPATH;
        Debug::log(ERR, "{}", e.what());
        return;
    }

    selectImage();
}

void CImageInputIndicator::plantTimer() {

    if (reloadTime == 0) {
        imageTimer = g_pHyprlock->addTimer(std::chrono::hours(1), [REF = m_self](auto, auto) { onTimer(REF); }, nullptr, true);
    } else if (reloadTime > 0)
        imageTimer = g_pHyprlock->addTimer(std::chrono::seconds(reloadTime), [REF = m_self](auto, auto) { onTimer(REF); }, nullptr, false);
}

void CImageInputIndicator::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    reset();

    viewport   = pOutput->getViewport();
    stringPort = pOutput->stringPort;

    shadow.configure(m_self, props, viewport);

    try {
        size      = std::any_cast<Hyprlang::INT>(props.at("size"));
        rounding  = std::any_cast<Hyprlang::INT>(props.at("rounding"));
        border    = std::any_cast<Hyprlang::INT>(props.at("border_size"));
        color     = *CGradientValueData::fromAnyPv(props.at("border_color"));
        configPos = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport);
        halign    = std::any_cast<Hyprlang::STRING>(props.at("halign"));
        valign    = std::any_cast<Hyprlang::STRING>(props.at("valign"));
        angle     = std::any_cast<Hyprlang::FLOAT>(props.at("rotate"));

        path           = std::any_cast<Hyprlang::STRING>(props.at("path"));
        num_images     = std::any_cast<Hyprlang::INT>(props.at("num_images"));
        reloadTime     = std::any_cast<Hyprlang::INT>(props.at("reload_time"));
        reloadCommand  = std::any_cast<Hyprlang::STRING>(props.at("reload_cmd"));
        onclickCommand = std::any_cast<Hyprlang::STRING>(props.at("onclick"));
    } catch (const std::bad_any_cast& e) {
        RASSERT(false, "Failed to construct CImageInputIndicator: {}", e.what()); //
    } catch (const std::out_of_range& e) {
        RASSERT(false, "Missing propperty for CImageInputIndicator: {}", e.what()); //
    }

    selectImage();
    angle      = angle * M_PI / 180.0;

    if (reloadTime > -1) {
        try {
            modificationTime = std::filesystem::last_write_time(absolutePath(path, ""));
        } catch (std::exception& e) { Debug::log(ERR, "{}", e.what()); }

        plantTimer();
    }
}

void CImageInputIndicator::reset() {
    if (imageTimer) {
        imageTimer->cancel();
        imageTimer.reset();
    }

    if (g_pHyprlock->m_bTerminate)
        return;

    imageFB.destroyBuffer();

    if (asset && reloadTime > -1) // Don't unload asset if it's a static image
        g_asyncResourceManager->unload(asset);

    asset             = nullptr;
    m_pendingResource = false;
    resourceID        = 0;
}

bool CImageInputIndicator::draw(const SRenderData& data) {
    passwordLength = g_pHyprlock->getPasswordBufferDisplayLen();
    checkWaiting   = g_pAuth->checkWaiting();
    displayFail    = g_pAuth->m_bDisplayFailText;

    selectImage();

    if (resourceID == 0)
        return false;

    if (!asset)
        asset = g_asyncResourceManager->getAssetByID(resourceID);

    if (!asset)
        return true;

    if (asset->m_iType == TEXTURE_INVALID) {
        g_asyncResourceManager->unload(asset);
        resourceID = 0;
        return false;
    }

    if (!imageFB.isAllocated()) {

        const Vector2D IMAGEPOS  = {border, border};
        const Vector2D BORDERPOS = {0.0, 0.0};
        const Vector2D TEXSIZE   = asset->m_vSize;
        const float    SCALEX    = size / TEXSIZE.x;
        const float    SCALEY    = size / TEXSIZE.y;

        // image with borders offset, with extra pixel for anti-aliasing when rotated
        CBox texbox = {angle == 0 ? IMAGEPOS : IMAGEPOS + Vector2D{1.0, 1.0}, TEXSIZE};

        texbox.w *= std::max(SCALEX, SCALEY);
        texbox.h *= std::max(SCALEX, SCALEY);

        // plus borders if any
        CBox borderBox = {angle == 0 ? BORDERPOS : BORDERPOS + Vector2D{1.0, 1.0}, texbox.size() + IMAGEPOS * 2.0};

        borderBox.round();

        const Vector2D FBSIZE      = angle == 0 ? borderBox.size() : borderBox.size() + Vector2D{2.0, 2.0};
        const int      ROUND       = roundingForBox(texbox, rounding);
        const int      BORDERROUND = roundingForBorderBox(borderBox, rounding, border);

        imageFB.alloc(FBSIZE.x, FBSIZE.y, true);
        g_pRenderer->pushFb(imageFB.m_iFb);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        if (border > 0)
            g_pRenderer->renderBorder(borderBox, color, border, BORDERROUND, 1.0);

        texbox.round();
        g_pRenderer->renderTexture(texbox, *asset, 1.0, ROUND, HYPRUTILS_TRANSFORM_NORMAL);
        g_pRenderer->popFb();
    }

    CTexture* tex    = &imageFB.m_cTex;
    CBox      texbox = {{}, tex->m_vSize};

    if (firstRender) {
        firstRender = false;
        shadow.markShadowDirty();
    }

    shadow.draw(data);

    pos = posFromHVAlign(viewport, tex->m_vSize, configPos, halign, valign, angle);

    texbox.x = pos.x;
    texbox.y = pos.y;

    texbox.round();
    texbox.rot = angle;
    g_pRenderer->renderTexture(texbox, *tex, data.opacity, 0, HYPRUTILS_TRANSFORM_FLIPPED_180);

    return data.opacity < 1.0;
}

void CImageInputIndicator::onAssetUpdate(ResourceID id, ASP<CTexture> newAsset) {
    m_pendingResource = false;

    if (!newAsset)
        Debug::log(ERR, "asset update failed, resourceID: {} not available on update!", id);
    else if (newAsset->m_iType == TEXTURE_INVALID) {
        g_asyncResourceManager->unload(newAsset);
        Debug::log(ERR, "New image asset has an invalid texture!");
    } else {
        g_asyncResourceManager->unload(asset);
        imageFB.destroyBuffer();

        asset       = newAsset;
        resourceID  = id;
        firstRender = true;
    }
}

CBox CImageInputIndicator::getBoundingBoxWl() const {
    if (!imageFB.isAllocated())
        return CBox{};

    return {
        Vector2D{pos.x, viewport.y - pos.y - imageFB.m_cTex.m_vSize.y},
        imageFB.m_cTex.m_vSize,
    };
}

void CImageInputIndicator::onClick(uint32_t button, bool down, const Vector2D& pos) {
    if (down && !onclickCommand.empty())
        spawnAsync(onclickCommand);
}

void CImageInputIndicator::onHover(const Vector2D& pos) {
    if (!onclickCommand.empty())
        g_pSeatManager->m_pCursorShape->setShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER);
}
