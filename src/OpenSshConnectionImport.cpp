#include "ConnectionCatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>
#include <utility>

namespace {

using Options = QHash<QString, QString>;

struct HostBlock
{
    QStringList patterns;
    Options directives;
};

bool configured(const QString &value)
{
    return !value.isEmpty() && value.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0;
}

bool matches(const QString &alias, const QStringList &patterns)
{
    bool allowed = false;
    for (QString pattern : patterns) {
        const bool excluded = pattern.startsWith(QLatin1Char('!'));
        if (excluded) {
            pattern.remove(0, 1);
        }
        const QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(pattern),
                                             QRegularExpression::CaseInsensitiveOption);
        if (expression.match(alias).hasMatch()) {
            if (excluded) {
                return false;
            }
            allowed = true;
        }
    }
    return allowed;
}

QString keyPath(QString file, const ConnectionProfile &profile, const QDir &directory)
{
    if (!configured(file)) {
        return {};
    }
    file.replace(QStringLiteral("%d"), QDir::homePath());
    file.replace(QStringLiteral("%h"), profile.host);
    file.replace(QStringLiteral("%r"), profile.username);
    if (file.startsWith(QStringLiteral("~/"))) {
        return QDir::home().filePath(file.mid(2));
    }
    return directory.absoluteFilePath(file);
}

} // namespace

QVector<ConnectionProfile> parseOpenSshConnections(const QString &content, const QDir &directory)
{
    static const QRegularExpression token(QStringLiteral(R"SSH("([^"]*)"|([^\s#=]+)|(#.*))SSH"));
    QVector<HostBlock> blocks{{{QStringLiteral("*")}, {}}};
    QStringList aliases;
    bool matchesDisabled = false;
    for (const QString &line : content.split(QLatin1Char('\n'))) {
        QStringList parts;
        auto tokens = token.globalMatch(line);
        while (tokens.hasNext()) {
            const auto match = tokens.next();
            if (!match.captured(3).isEmpty()) {
                break;
            }
            parts.append(match.capturedStart(1) >= 0 ? match.captured(1) : match.captured(2));
        }
        if (parts.size() < 2) {
            continue;
        }
        const QString directive = parts.takeFirst().toLower();
        if (directive == QStringLiteral("host")) {
            matchesDisabled = false;
            blocks.append({parts, {}});
            for (const QString &alias : parts) {
                if (!alias.contains(QLatin1Char('*')) && !alias.contains(QLatin1Char('?'))
                    && !alias.startsWith(QLatin1Char('!')) && !aliases.contains(alias, Qt::CaseInsensitive)) {
                    aliases.append(alias);
                }
            }
        } else if (directive == QStringLiteral("match")) {
            matchesDisabled = true;
        } else if (!matchesDisabled && !blocks.last().directives.contains(directive)) {
            blocks.last().directives.insert(directive, parts.first());
        }
    }

    QVector<ConnectionProfile> profiles;
    QHash<QString, int> byAlias;
    QVector<QString> jumps;
#ifdef Q_OS_WIN
    const QString localUser = qEnvironmentVariable("USERNAME");
#else
    const QString localUser = qEnvironmentVariable("USER");
#endif
    for (const QString &alias : std::as_const(aliases)) {
        Options options;
        for (const HostBlock &block : std::as_const(blocks)) {
            if (matches(alias, block.patterns)) {
                for (auto it = block.directives.cbegin(); it != block.directives.cend(); ++it) {
                    if (!options.contains(it.key())) {
                        options.insert(it.key(), it.value());
                    }
                }
            }
        }
        ConnectionProfile profile;
        const QString proxyCommand = options.value(QStringLiteral("proxycommand"));
        if (configured(proxyCommand)) {
            byAlias.insert(alias.toLower(), -1);
            continue;
        }
        profile.name = alias;
        profile.host = options.value(QStringLiteral("hostname"), alias);
        profile.host.replace(QStringLiteral("%h"), alias);
        profile.username = options.value(QStringLiteral("user"), localUser);
        const int port = options.value(QStringLiteral("port")).toInt();
        profile.port = port > 0 && port <= 65535 ? port : 22;
        profile.privateKeyPath = keyPath(options.value(QStringLiteral("identityfile")), profile, directory);
        profile.authType = profile.privateKeyPath.isEmpty() ? QStringLiteral("agent") : QStringLiteral("key");
        byAlias.insert(alias.toLower(), profiles.size());
        profiles.append(profile);
        jumps.append(options.value(QStringLiteral("proxyjump")));
    }

    for (qsizetype i = 0; i < profiles.size(); ++i) {
        const QString &jump = jumps.at(i);
        if (!configured(jump)) {
            continue;
        }
        const QUrl uri(jump.startsWith(QStringLiteral("ssh://")) ? jump : QStringLiteral("ssh://") + jump);
        if (!uri.isValid() || uri.host().isEmpty() || jump.contains(QLatin1Char(','))) {
            profiles[i].host.clear();
            continue;
        }
        auto &profile = profiles[i];
        const auto candidate = byAlias.constFind(uri.host().toLower());
        const int aliasIndex = candidate == byAlias.cend() ? -1 : candidate.value();
        if (candidate != byAlias.cend()
            && (aliasIndex < 0 || configured(jumps.at(aliasIndex)))) {
            profile.host.clear();
            continue;
        }
        const ConnectionProfile bastion = aliasIndex >= 0 ? profiles.at(aliasIndex) : ConnectionProfile{};
        profile.jumpHost = bastion.host.isEmpty() ? uri.host() : bastion.host;
        profile.jumpPort = uri.port(bastion.port);
        profile.jumpUsername = uri.userName().isEmpty()
            ? (bastion.username.isEmpty() ? localUser : bastion.username) : uri.userName();
        profile.jumpPrivateKeyPath = bastion.privateKeyPath;
        profile.jumpAuthType = bastion.privateKeyPath.isEmpty() ? QStringLiteral("agent") : QStringLiteral("key");
    }
    profiles.removeIf([](const ConnectionProfile &profile) { return profile.host.isEmpty(); });
    return profiles;
}
