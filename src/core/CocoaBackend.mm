#include "FrameGeometry_p.h"
#include "PlatformBackend_p.h"

#import <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QPointer>
#include <QWindow>
#import <objc/runtime.h>
#include <vector>

namespace
{
char managedObjectKey;
char viewWindowContext;

// Each managed object gets a subclass of its actual Qt/AppKit class. Unmanaged
// windows retain their original class; the Qt NSWindow delegate is never replaced.
// Registered classes live for the process lifetime, just like the loaded module.
struct NativeClass
{
    Class installed = Nil;
    Class original             = Nil;
    IMP styleMask              = nullptr;
    IMP transparentTitleBar    = nullptr;
    IMP titleVisibility        = nullptr;
    IMP mouseDownCanMoveWindow = nullptr;
};
std::vector<NativeClass>& nativeClasses()
{
    static std::vector<NativeClass> classes;
    return classes;
}
NativeClass nativeClass(id object)
{
    // KVO or another library may have added a subclass above ours.
    for (Class type = object_getClass(object); type; type = class_getSuperclass(type))
        for (const auto& entry : nativeClasses())
            if (entry.installed == type)
                return entry;
    return {};
}
bool managed(id object)
{
    return objc_getAssociatedObject(object, &managedObjectKey) != nil;
}
void setStyleMask(id object, SEL selector, NSWindowStyleMask mask)
{
    const auto original = nativeClass(object).styleMask;
    if (managed(object))
        mask |= NSWindowStyleMaskFullSizeContentView;
    reinterpret_cast<void (*)(id, SEL, NSWindowStyleMask)>(original)(object, selector, mask);
}
void setTransparentTitleBar(id object, SEL selector, BOOL transparent)
{
    const auto original = nativeClass(object).transparentTitleBar;
    reinterpret_cast<void (*)(id, SEL, BOOL)>(original)(object, selector, managed(object) ? YES : transparent);
}
void setTitleVisibility(id object, SEL selector, NSWindowTitleVisibility visibility)
{
    const auto original = nativeClass(object).titleVisibility;
    if (managed(object))
    {
        NSWindow* window = object;
        visibility = (window.styleMask & NSWindowStyleMaskFullScreen) ? NSWindowTitleVisible : NSWindowTitleHidden;
    }
    reinterpret_cast<void (*)(id, SEL, NSWindowTitleVisibility)>(original)(object, selector, visibility);
}
BOOL mouseDownCanMoveWindow(id object, SEL selector)
{
    if (managed(object))
        return NO; // Only the registered title bar may initiate a system move.
    const auto original = nativeClass(object).mouseDownCanMoveWindow;
    return reinterpret_cast<BOOL (*)(id, SEL)>(original)(object, selector);
}
Class installNativeClass(id object, bool isView)
{
    const Class base = object_getClass(object);
    // Reuse a wrapper left underneath another owner's subclass. Wrapping it
    // again would make the saved IMP call our own function recursively.
    if (const auto existing = nativeClass(object); existing.installed)
    {
        objc_setAssociatedObject(object, &managedObjectKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return existing.installed;
    }
    for (const auto& entry : nativeClasses())
    {
        if (entry.original == base)
        {
            objc_setAssociatedObject(object, &managedObjectKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            object_setClass(object, entry.installed);
            return entry.installed;
        }
    }
    const QByteArray name = QByteArray("FramelessKit_") + class_getName(base);
    Class type            = objc_allocateClassPair(base, name.constData(), 0);
    if (!type)
        return Nil;
    NativeClass originals;
    originals.original   = base;
    const auto addMethod = [&](SEL selector, IMP replacement, IMP& previous) -> bool
    {
        Method method = class_getInstanceMethod(base, selector);
        if (!method)
            return false;
        previous = method_getImplementation(method);
        return class_addMethod(type, selector, replacement, method_getTypeEncoding(method)) != NO;
    };
    const bool success =
        isView ? addMethod(@selector(mouseDownCanMoveWindow), reinterpret_cast<IMP>(mouseDownCanMoveWindow),
                           originals.mouseDownCanMoveWindow)
               : addMethod(@selector(setStyleMask:), reinterpret_cast<IMP>(setStyleMask), originals.styleMask) &&
                     addMethod(@selector(setTitlebarAppearsTransparent:), reinterpret_cast<IMP>(setTransparentTitleBar),
                               originals.transparentTitleBar) &&
                     addMethod(@selector(setTitleVisibility:), reinterpret_cast<IMP>(setTitleVisibility),
                               originals.titleVisibility);
    if (!success)
    {
        objc_disposeClassPair(type);
        return Nil;
    }
    originals.installed = type;
    nativeClasses().push_back(originals);
    objc_registerClassPair(type);
    objc_setAssociatedObject(object, &managedObjectKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    object_setClass(object, type);
    return type;
}
void removeNativeClass(id object, Class installed)
{
    if (!object || !installed)
        return;
    objc_setAssociatedObject(object, &managedObjectKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    // Never undo a subclass installed by another owner after this binding.
    // In that case our methods remain harmless pass-throughs to their original IMPs.
    if (object_getClass(object) == installed)
        object_setClass(object, nativeClass(object).original);
}
} // namespace

@interface FKViewWindowObserver : NSObject
@property(nonatomic, copy) void (^changed)(void);
@end

@implementation FKViewWindowObserver
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context
{
    if (context == &viewWindowContext)
    {
        if (self.changed)
            self.changed();
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}
@end

namespace FramelessKit::Internal
{
namespace
{
class CocoaBackend final : public PlatformBackend
{
public:
    using PlatformBackend::PlatformBackend;
    ~CocoaBackend() override
    {
        releaseNative(true);
    }

protected:
    bool framelessHint() const override
    {
        return false;
    }
    bool clientResizeEnabled() const override
    {
        return false;
    }
    void showSystemMenu(QPointF) override
    {
        // Cocoa has no Windows-style title-bar system menu.
    }
    void titleBarDoubleClick() override
    {
        NSString* action = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleActionOnDoubleClick"];
        if ([action isEqualToString:@"None"])
            return;
        if ([action isEqualToString:@"Minimize"])
        {
            if (windowCapability(m_host, Qt::WindowMinimizeButtonHint))
                m_host.setStates(m_host.states() | Qt::WindowMinimized);
            return;
        }
        PlatformBackend::titleBarDoubleClick();
    }
    void synchronizeNative() override
    {
        QWindow* window = m_host.window();
        // On Cocoa winId() is the NSView. Never call it before Qt has a surface.
        NSView* view    = window && window->handle() ? (__bridge NSView*)reinterpret_cast<void*>(window->winId()) : nil;
        if (view != m_view)
        {
            releaseNative(true);
            if (!view)
                return;
            ++m_generation;
            m_view      = view;
            m_viewClass = installNativeClass(view, true);
            if (!m_viewClass)
            {
                m_diagnostic(QStringLiteral("Could not install the Cocoa view input adapter."));
                m_view = nil;
                return;
            }
            m_originalWantsLayer = view.wantsLayer;
            view.wantsLayer = YES;
            const QPointer<CocoaBackend> guard(this);
            const quint64 generation = m_generation;
            m_observer               = [FKViewWindowObserver new];
            m_observer.changed       = ^{
              if (guard && guard->m_generation == generation)
                  guard->scheduleSynchronize();
            };
            [m_view addObserver:m_observer forKeyPath:@"window" options:0 context:&viewWindowContext];
        }
        NSWindow* nativeWindow = m_view.window;
        if (nativeWindow != m_window)
        {
            detachWindow(true);
            if (!nativeWindow)
                return; // NSView.window may become available after SurfaceCreated.
            m_window                 = nativeWindow;
            m_originalFullSize       = bool(nativeWindow.styleMask & NSWindowStyleMaskFullSizeContentView);
            m_originalTransparent    = nativeWindow.titlebarAppearsTransparent;
            m_originalVisibility     = nativeWindow.titleVisibility;
            m_originalShadow         = nativeWindow.hasShadow;
            m_originalMovable        = nativeWindow.movable;
            m_originalBackgroundMove = nativeWindow.movableByWindowBackground;
            m_windowClass            = installNativeClass(nativeWindow, false);
            if (!m_windowClass)
            {
                m_window = nil;
                m_diagnostic(QStringLiteral("Could not install the Cocoa native frame adapter."));
                return;
            }
            m_notifications = [NSMutableArray new];
            const QPointer<CocoaBackend> guard(this);
            const quint64 generation = m_generation;
            for (NSNotificationName name in @[
                     NSWindowDidEnterFullScreenNotification, NSWindowDidExitFullScreenNotification,
                     NSWindowDidResizeNotification, NSWindowDidBecomeKeyNotification,
                     NSWindowDidDeminiaturizeNotification
                 ])
            {
                id token =
                    [[NSNotificationCenter defaultCenter] addObserverForName:name
                                                                      object:nativeWindow
                                                                       queue:nil
                                                                  usingBlock:^(NSNotification*) {
                                                                    if (guard && guard->m_generation == generation)
                                                                        guard->scheduleSynchronize();
                                                                  }];
                [m_notifications addObject:token];
            }
        }
        applyFrame();
    }
    void releaseNative(bool restore) override
    {
        ++m_generation;
        resetInput();
        if (m_observer)
        {
            m_observer.changed = nil;
            [m_view removeObserver:m_observer forKeyPath:@"window" context:&viewWindowContext];
            m_observer = nil;
        }
        detachWindow(restore);
        removeNativeClass(m_view, m_viewClass);
        if (restore && m_view.wantsLayer)
            m_view.wantsLayer = m_originalWantsLayer;
        m_viewClass = Nil;
        m_view      = nil;
    }

private:
    void applyFrame()
    {
        NSWindow* window = m_window;
        if (!window)
            return;
        if (!(window.styleMask & NSWindowStyleMaskFullSizeContentView))
            window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        const auto visibility =
            (window.styleMask & NSWindowStyleMaskFullScreen) ? NSWindowTitleVisible : NSWindowTitleHidden;
        if (window.titleVisibility != visibility)
            window.titleVisibility = visibility;
        if (!window.titlebarAppearsTransparent)
            window.titlebarAppearsTransparent = YES;
        if (!window.hasShadow)
            window.hasShadow = YES;
        if (!window.movable)
            window.movable = YES;
        if (window.movableByWindowBackground)
            window.movableByWindowBackground = NO;
        // AppKit owns the native buttons, resizing, rounding and fullscreen controls.
        // Do not force Resizable: Qt's flags and fixed-size constraints remain authoritative.
    }
    void detachWindow(bool restore)
    {
        for (id token in m_notifications)
            [[NSNotificationCenter defaultCenter] removeObserver:token];
        m_notifications  = nil;
        NSWindow* window = m_window;
        removeNativeClass(window, m_windowClass);
        m_windowClass = Nil;
        m_window      = nil;
        if (!restore || !window)
            return;
        // Restore only fields controlled by this binding, preserving other style bits.
        if (!m_originalFullSize && (window.styleMask & NSWindowStyleMaskFullSizeContentView))
            window.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
        if (window.titlebarAppearsTransparent)
            window.titlebarAppearsTransparent = m_originalTransparent;
        const auto appliedVisibility =
            (window.styleMask & NSWindowStyleMaskFullScreen) ? NSWindowTitleVisible : NSWindowTitleHidden;
        if (window.titleVisibility == appliedVisibility)
            window.titleVisibility = m_originalVisibility;
        if (window.hasShadow)
            window.hasShadow = m_originalShadow;
        if (window.movable)
            window.movable = m_originalMovable;
        if (!window.movableByWindowBackground)
            window.movableByWindowBackground = m_originalBackgroundMove;
    }

    NSView* __strong m_view                      = nil;
    NSWindow* __weak m_window                    = nil;
    FKViewWindowObserver* __strong m_observer    = nil;
    NSMutableArray* __strong m_notifications     = nil;
    Class m_viewClass                            = Nil;
    Class m_windowClass                          = Nil;
    BOOL m_originalWantsLayer = NO;
    bool m_originalFullSize                      = false;
    BOOL m_originalTransparent                   = NO;
    NSWindowTitleVisibility m_originalVisibility = NSWindowTitleVisible;
    BOOL m_originalShadow                        = YES;
    BOOL m_originalMovable                       = YES;
    BOOL m_originalBackgroundMove                = NO;
};
} // namespace

std::unique_ptr<PlatformBackend> createPlatformBackend(HostAdapter& host, const HitTestModel& hitTest,
                                                       PlatformBackend::Diagnostic diagnostic)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return createQtFallbackBackend(host, hitTest, std::move(diagnostic));
    auto backend = std::make_unique<CocoaBackend>(host, hitTest, std::move(diagnostic));
    backend->start();
    return backend;
}
} // namespace FramelessKit::Internal
