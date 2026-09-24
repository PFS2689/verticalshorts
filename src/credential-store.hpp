#pragma once

#include <QString>

namespace vsp {

struct CredentialStoreResult {
	bool ok = false;
	bool usedSecureStorage = false;
	QString error;
};

/* Target id format: "vsp/<destination-uuid>/<field>" e.g. key or password */
CredentialStoreResult SaveSecret(const QString &targetId, const QString &secret);
CredentialStoreResult LoadSecret(const QString &targetId, QString *outSecret);
CredentialStoreResult DeleteSecret(const QString &targetId);
bool SecureStorageAvailable();
QString SecureStorageDescription();

inline QString KeyTarget(const QString &destId)
{
	return QStringLiteral("vsp/%1/key").arg(destId);
}
inline QString PasswordTarget(const QString &destId)
{
	return QStringLiteral("vsp/%1/password").arg(destId);
}

} // namespace vsp
