#include "TranslationManager.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>

TranslationManager::TranslationManager(QObject *parent)
    : QObject(parent)
{
}

QString TranslationManager::language() const
{
    return m_language;
}

bool TranslationManager::installLanguage(const QString &language)
{
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);

    m_language = language.isEmpty() ? QStringLiteral("system") : language;
    const QString localeName = localeNameForLanguage(m_language);

    if (localeName == QStringLiteral("en")) {
        return true;
    }

    const QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    const QString qtBaseLocale = localeName.section(QLatin1Char('_'), 0, 0);
    if (m_qtTranslator.load(QStringLiteral("qt_%1").arg(qtBaseLocale), translationsPath)) {
        QCoreApplication::installTranslator(&m_qtTranslator);
    }

    if (m_appTranslator.load(QStringLiteral(":/i18n/OpenShell_%1.qm").arg(localeName))) {
        QCoreApplication::installTranslator(&m_appTranslator);
        return true;
    }
    return false;
}

QString TranslationManager::localeNameForLanguage(const QString &language) const
{
    if (language == QStringLiteral("zh_CN")) return language;
    if (language == QStringLiteral("ja_JP")) return language;
    if (language == QStringLiteral("ko_KR")) return language;
    if (language == QStringLiteral("de_DE")) return language;
    if (language == QStringLiteral("en")) return language;

    const QString systemLocale = QLocale::system().name();
    if (systemLocale.startsWith(QStringLiteral("zh"))) return QStringLiteral("zh_CN");
    if (systemLocale.startsWith(QStringLiteral("ja"))) return QStringLiteral("ja_JP");
    if (systemLocale.startsWith(QStringLiteral("ko"))) return QStringLiteral("ko_KR");
    if (systemLocale.startsWith(QStringLiteral("de"))) return QStringLiteral("de_DE");
    return QStringLiteral("en");
}
