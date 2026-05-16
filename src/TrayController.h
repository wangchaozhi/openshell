#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QSystemTrayIcon>
#include <QVector>

#include "ConnectionCatalog.h"

class QAction;
class QActionGroup;
class QMenu;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    void setLanguage(const QString &language);
    void setConnections(const QVector<ConnectionProfile> &profiles);
    void showContextMenu();

signals:
    void showRequested();
    void hideRequested();
    void languageChanged(const QString &language);
    void connectionTriggered(const QString &id);
    void quitRequested();

private:
    void createMenu();
    void retranslate();

    QScopedPointer<QSystemTrayIcon> m_trayIcon;
    QMenu *m_trayMenu = nullptr;
    QMenu *m_connectionsMenu = nullptr;
    QMenu *m_languageMenu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_hideAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_systemLanguageAction = nullptr;
    QAction *m_englishLanguageAction = nullptr;
    QAction *m_chineseLanguageAction = nullptr;
    QActionGroup *m_languageActionGroup = nullptr;
    QVector<QAction *> m_connectionActions;
};
