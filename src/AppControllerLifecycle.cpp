#include "AppController.h"

#include <QGuiApplication>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

void AppController::showWindow()
{
    emit showRequested();
}

void AppController::hideWindow()
{
    emit hideRequested();
}

void AppController::quit()
{
    QGuiApplication::quit();
}

bool AppController::moveAppToBackground()
{
#if defined(Q_OS_ANDROID)
    // 等效于按 home：把当前任务推到后台但保留进程和 QML window，下次
    // 用户从启动器/任务管理器拉回来 Qt 还在原状态。直接关闭 ApplicationWindow
    // 会让 QtActivity finish 但 QGuiApplication 仍在跑，下次 onCreate 时
    // 没有新的 window，整屏白。
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) {
        return false;
    }
    const jboolean result = activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", JNI_TRUE);
    return result == JNI_TRUE;
#else
    return false;
#endif
}
