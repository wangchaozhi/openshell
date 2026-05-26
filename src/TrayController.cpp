#include "TrayController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QSignalBlocker>
#include <QStyle>

TrayController::TrayController(QObject *parent)
    : QObject(parent)
{
    createMenu();
}

TrayController::~TrayController()
{
    delete m_trayMenu;
}

void TrayController::setLanguage(const QString &language)
{
    QAction *target = m_systemLanguageAction;
    if (language == QStringLiteral("en")) target = m_englishLanguageAction;
    else if (language == QStringLiteral("zh_CN")) target = m_chineseLanguageAction;
    else if (language == QStringLiteral("ja_JP")) target = m_japaneseLanguageAction;
    else if (language == QStringLiteral("ko_KR")) target = m_koreanLanguageAction;
    else if (language == QStringLiteral("de_DE")) target = m_germanLanguageAction;

    if (target) {
        const QSignalBlocker blocker(m_languageActionGroup);
        target->setChecked(true);
    }
    retranslate();
}

void TrayController::setConnections(const QVector<ConnectionProfile> &profiles)
{
    if (!m_connectionsMenu) {
        return;
    }
    for (QAction *action : m_connectionActions) {
        m_connectionsMenu->removeAction(action);
        action->deleteLater();
    }
    m_connectionActions.clear();

    for (const ConnectionProfile &p : profiles) {
        const QString label = p.name.isEmpty()
                ? QStringLiteral("%1@%2").arg(p.username, p.host)
                : p.name;
        QAction *action = m_connectionsMenu->addAction(label);
        action->setData(p.id);
        connect(action, &QAction::triggered, this, [this, action]() {
            emit connectionTriggered(action->data().toString());
        });
        m_connectionActions.append(action);
    }

    if (m_connectionActions.isEmpty()) {
        QAction *empty = m_connectionsMenu->addAction(QString());
        empty->setEnabled(false);
        empty->setData(QStringLiteral("__empty__"));
        m_connectionActions.append(empty);
    }

    updateConnectionsMenuText();
}

void TrayController::showContextMenu()
{
    if (m_trayMenu) {
        m_trayMenu->popup(QCursor::pos());
    }
}

void TrayController::createMenu()
{
    const QIcon icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);

    m_trayMenu = new QMenu();

    m_showAction = m_trayMenu->addAction(QString());
    connect(m_showAction, &QAction::triggered, this, &TrayController::showRequested);

    m_hideAction = m_trayMenu->addAction(QString());
    connect(m_hideAction, &QAction::triggered, this, &TrayController::hideRequested);

    m_connectionsMenu = m_trayMenu->addMenu(QString());

    m_languageMenu = m_trayMenu->addMenu(QString());
    m_languageActionGroup = new QActionGroup(this);
    m_languageActionGroup->setExclusive(true);

    m_systemLanguageAction = m_languageMenu->addAction(QString());
    m_systemLanguageAction->setCheckable(true);
    m_systemLanguageAction->setData(QStringLiteral("system"));
    m_languageActionGroup->addAction(m_systemLanguageAction);

    m_englishLanguageAction = m_languageMenu->addAction(QString());
    m_englishLanguageAction->setCheckable(true);
    m_englishLanguageAction->setData(QStringLiteral("en"));
    m_languageActionGroup->addAction(m_englishLanguageAction);

    m_chineseLanguageAction = m_languageMenu->addAction(QString());
    m_chineseLanguageAction->setCheckable(true);
    m_chineseLanguageAction->setData(QStringLiteral("zh_CN"));
    m_languageActionGroup->addAction(m_chineseLanguageAction);

    m_japaneseLanguageAction = m_languageMenu->addAction(QString());
    m_japaneseLanguageAction->setCheckable(true);
    m_japaneseLanguageAction->setData(QStringLiteral("ja_JP"));
    m_languageActionGroup->addAction(m_japaneseLanguageAction);

    m_koreanLanguageAction = m_languageMenu->addAction(QString());
    m_koreanLanguageAction->setCheckable(true);
    m_koreanLanguageAction->setData(QStringLiteral("ko_KR"));
    m_languageActionGroup->addAction(m_koreanLanguageAction);

    m_germanLanguageAction = m_languageMenu->addAction(QString());
    m_germanLanguageAction->setCheckable(true);
    m_germanLanguageAction->setData(QStringLiteral("de_DE"));
    m_languageActionGroup->addAction(m_germanLanguageAction);

    connect(m_languageActionGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        emit languageChanged(action->data().toString());
    });

    m_trayMenu->addSeparator();

    m_quitAction = m_trayMenu->addAction(QString());
    connect(m_quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    m_trayIcon.reset(new QSystemTrayIcon(icon));
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon.get(), &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
                    emit showRequested();
                }
            });

    setConnections({});
    retranslate();
    m_trayIcon->show();
}

void TrayController::updateConnectionsMenuText()
{
    for (QAction *action : m_connectionActions) {
        if (action->data().toString() == QStringLiteral("__empty__")) {
            action->setText(tr("(no connections)"));
            break;
        }
    }
}

void TrayController::retranslate()
{
    m_showAction->setText(tr("Show OpenShell"));
    m_hideAction->setText(tr("Hide to Tray"));
    m_connectionsMenu->setTitle(tr("Connections"));
    m_languageMenu->setTitle(tr("Language"));
    m_systemLanguageAction->setText(tr("System"));
    m_englishLanguageAction->setText(tr("English"));
    m_chineseLanguageAction->setText(tr("Simplified Chinese"));
    m_japaneseLanguageAction->setText(tr("Japanese"));
    m_koreanLanguageAction->setText(tr("Korean"));
    m_germanLanguageAction->setText(tr("German"));
    m_quitAction->setText(tr("Quit"));
    m_trayIcon->setToolTip(tr("OpenShell"));
    updateConnectionsMenuText();
}
