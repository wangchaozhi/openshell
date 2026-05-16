#pragma once

#include <QObject>
#include <QTranslator>

class TranslationManager : public QObject
{
    Q_OBJECT

public:
    explicit TranslationManager(QObject *parent = nullptr);

    QString language() const;
    bool installLanguage(const QString &language);

private:
    QString localeNameForLanguage(const QString &language) const;

    QString m_language = QStringLiteral("system");
    QTranslator m_qtTranslator;
    QTranslator m_appTranslator;
};
