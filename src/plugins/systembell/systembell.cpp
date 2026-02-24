/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systembell.h"

#include "compositor.h"
#include "effect/effecthandler.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "platformsupport/scenes/vulkan/vulkancontext.h"
#include "platformsupport/scenes/vulkan/vulkanpipeline.h"
#include "platformsupport/scenes/vulkan/vulkanpipelinemanager.h"
#include "scene/workspacescene_vulkan.h"
#include "window.h"

#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QTimer>

#include <canberra.h>

Q_LOGGING_CATEGORY(KWIN_SYSTEMBELL, "kwin_effect_systembell", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(systembell);
}

namespace KWin
{

QTimer *SystemBellEffect::s_systemBellRemoveTimer = nullptr;
XdgSystemBellV1Interface *SystemBellEffect::s_systemBell = nullptr;

SystemBellEffect::SystemBellEffect()
    : m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
    , m_kdeglobals(QStringLiteral("kdeglobals"))
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/Effect/SystemBell1"),
                                                 QStringLiteral("org.kde.KWin.Effect.SystemBell1"),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);

    connect(effects, &EffectsHandler::windowClosed, this, &SystemBellEffect::slotWindowClosed);

    const QLatin1String groupName("Bell");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            m_bellConfig = group;
            reconfigure(ReconfigureAll);
        }
    });
    m_bellConfig = m_configWatcher->config()->group(groupName);
    reconfigure(ReconfigureAll);

    int ret = ca_context_create(&m_caContext);
    if (ret != CA_SUCCESS) {
        qCWarning(KWIN_SYSTEMBELL) << "Failed to initialize canberra context for audio notification:" << ca_strerror(ret);
        m_caContext = nullptr;
    } else {
        ret = ca_context_change_props(m_caContext,
                                      CA_PROP_APPLICATION_NAME,
                                      qApp->applicationDisplayName().toUtf8().constData(),
                                      CA_PROP_APPLICATION_ID,
                                      qApp->desktopFileName().toUtf8().constData(),
                                      nullptr);
        if (ret != CA_SUCCESS) {
            qCWarning(KWIN_SYSTEMBELL) << "Failed to set application properties on canberra context for audio notification:" << ca_strerror(ret);
        }
    }
}

SystemBellEffect::~SystemBellEffect()
{
    if (m_caContext) {
        ca_context_destroy(m_caContext);
    }
}

void SystemBellEffect::reconfigure(ReconfigureFlags flags)
{
    m_inited = false;
    m_color = m_bellConfig.readEntry<QColor>("VisibleBellColor", QColor(Qt::red));
    m_mode = m_bellConfig.readEntry<bool>("VisibleBellInvert", false) ? Invert : Color;
    m_duration = m_bellConfig.readEntry<int>("VisibleBellPause", 500);
    m_audibleBell = m_bellConfig.readEntry<bool>("SystemBell", true);
    m_customBell = m_bellConfig.readEntry<bool>("ArtsBell", false);
    m_customBellFile = m_bellConfig.readEntry<QString>("ArtsBellFile", QString());
    m_visibleBell = m_bellConfig.readEntry<bool>("VisibleBell", false);
}

bool SystemBellEffect::supported()
{
    // Support both OpenGL and Vulkan for offscreen rendering
    return effects->isOpenGLCompositing() || effects->isVulkanCompositing();
}

void SystemBellEffect::flash(EffectWindow *window)
{
    if (m_valid && !m_inited) {
        m_valid = loadData();
    }

    if (effects->isOpenGLCompositing() && m_shader) {
        redirect(window);
        setShader(window, m_shader.get());
    } else if (effects->isVulkanCompositing() && m_pipeline) {
        redirect(window);
        setPipeline(window, m_pipeline);
    }
}

void SystemBellEffect::unflash(EffectWindow *window)
{
    unredirect(window);
}

bool SystemBellEffect::loadData()
{
    ensureResources();
    m_inited = true;

    // Create shaders for OpenGL
    if (m_visibleBell && effects->isOpenGLCompositing()) {
        if (m_mode == Invert) {
            m_shader = GLShaderManager::instance()->generateShaderFromFile(GLShaderTrait::MapTexture, QString(), QStringLiteral(":/effects/systembell/shaders/invert.frag"));
        } else {
            m_shader = GLShaderManager::instance()->generateShaderFromFile(GLShaderTrait::MapTexture, QString(), QStringLiteral(":/effects/systembell/shaders/color.frag"));
            ShaderBinder binder(m_shader.get());
            m_shader->setUniform(GLShader::ColorUniform::Color, m_color);
        }

        if (!m_shader->isValid()) {
            qCCritical(KWIN_SYSTEMBELL) << "The shader failed to load!";
            return false;
        }
    } else if (m_visibleBell && effects->isVulkanCompositing()) {
        // For Vulkan, get the pipeline from the pipeline manager
        auto *scene = dynamic_cast<WorkspaceSceneVulkan *>(Compositor::self()->scene());
        if (!scene) {
            qCWarning(KWIN_SYSTEMBELL) << "Failed to get Vulkan scene for system bell";
            return false;
        }

        auto *context = scene->backend()->vulkanContext();
        if (!context) {
            qCWarning(KWIN_SYSTEMBELL) << "Failed to get Vulkan context for system bell";
            return false;
        }

        auto *pipelineManager = context->pipelineManager();
        if (!pipelineManager) {
            qCWarning(KWIN_SYSTEMBELL) << "Failed to get Vulkan pipeline manager for system bell";
            return false;
        }

        // Create pipeline with appropriate traits
        VulkanShaderTraits traits = VulkanShaderTrait::MapTexture
            | VulkanShaderTrait::Modulate
            | VulkanShaderTrait::AdjustSaturation
            | VulkanShaderTrait::TransformColorspace;

        if (m_mode == Invert) {
            traits |= VulkanShaderTrait::Invert;
        }

        m_pipeline = pipelineManager->pipeline(traits);
        if (!m_pipeline) {
            qCWarning(KWIN_SYSTEMBELL) << "Failed to create Vulkan pipeline for system bell";
            return false;
        }

        qCInfo(KWIN_SYSTEMBELL) << "Created Vulkan pipeline for visible bell with traits:" << static_cast<int>(traits);
    } else if (m_visibleBell) {
        // Visible bell not supported
        qCWarning(KWIN_SYSTEMBELL) << "Visible bell not supported, falling back to audio-only";
        m_visibleBell = false;
    }

    return true;
}

void SystemBellEffect::slotWindowClosed(EffectWindow *w)
{
    m_windows.removeOne(w);
}

void SystemBellEffect::triggerScreen()
{
    if (m_allWindows) {
        return;
    }

    if (m_audibleBell) {
        playAudibleBell();
    }

    if (m_visibleBell) {
        m_allWindows = true;

        const auto windows = effects->stackingOrder();
        for (EffectWindow *window : windows) {
            flash(window);
        }

        QTimer::singleShot(m_duration, this, [this] {
            const auto windows = effects->stackingOrder();
            for (EffectWindow *window : windows) {
                unflash(window);
            }
            m_allWindows = false;
            effects->addRepaintFull();
        });

        effects->addRepaintFull();
    }
}

void SystemBellEffect::triggerWindow()
{
    triggerWindow(effects->activeWindow());
}

void SystemBellEffect::triggerWindow(EffectWindow *window)
{
    if (!window || m_windows.contains(window)) {
        return;
    }

    if (m_audibleBell) {
        playAudibleBell();
    }

    if (m_visibleBell) {
        m_windows.append(window);
        flash(window);

        QTimer::singleShot(m_duration, this, [this, window] {
            // window may be closed by now
            if (m_windows.contains(window)) {
                unflash(window);
                m_windows.removeOne(window);
                window->addRepaintFull();
            }
        });

        window->addRepaintFull();
    }
}

bool SystemBellEffect::isActive() const
{
    return m_valid && (m_allWindows || !m_windows.isEmpty());
}

bool SystemBellEffect::provides(Feature f)
{
    return f == SystemBell;
}

bool SystemBellEffect::perform(Feature feature, const QVariantList &arguments)
{
    triggerScreen();
    return true;
}

void SystemBellEffect::playAudibleBell()
{
    if (m_customBell) {
        ca_context_play(m_caContext,
                        0,
                        CA_PROP_MEDIA_FILENAME,
                        QFile::encodeName(QUrl(m_customBellFile).toLocalFile()).constData(),
                        CA_PROP_MEDIA_ROLE,
                        "event",
                        nullptr);
    } else {
        const QString themeName = m_kdeglobals.group(QStringLiteral("Sounds")).readEntry("Theme", QStringLiteral("ocean"));
        ca_context_play(m_caContext,
                        0,
                        CA_PROP_EVENT_ID,
                        "bell",
                        CA_PROP_MEDIA_ROLE,
                        "event",
                        CA_PROP_CANBERRA_XDG_THEME_NAME,
                        themeName.toUtf8().constData(),
                        nullptr);
    }
}

} // namespace

#include "moc_systembell.cpp"
