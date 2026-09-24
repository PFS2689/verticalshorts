#include "credential-store.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <sddl.h>
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Crypt32.lib")
#endif

namespace vsp {
namespace {

QString FallbackDir()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	const QString dir = QDir(base).filePath(QStringLiteral("VerticalShorts/credentials"));
	QDir().mkpath(dir);
#ifdef _WIN32
	/* Best-effort: restrict directory to the current user (DACL). */
	PSID userSid = nullptr;
	HANDLE token = nullptr;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		DWORD len = 0;
		GetTokenInformation(token, TokenUser, nullptr, 0, &len);
		QByteArray buf;
		buf.resize((int)len);
		if (GetTokenInformation(token, TokenUser, buf.data(), len, &len)) {
			userSid = reinterpret_cast<TOKEN_USER *>(buf.data())->User.Sid;
			EXPLICIT_ACCESSW ea{};
			ea.grfAccessPermissions = GENERIC_ALL;
			ea.grfAccessMode = SET_ACCESS;
			ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
			ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
			ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
			ea.Trustee.ptstrName = (LPWSTR)userSid;
			PACL acl = nullptr;
			if (SetEntriesInAclW(1, &ea, nullptr, &acl) == ERROR_SUCCESS) {
				SetNamedSecurityInfoW((LPWSTR)dir.utf16(), SE_FILE_OBJECT,
						      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
						      nullptr, nullptr, acl, nullptr);
				LocalFree(acl);
			}
		}
		CloseHandle(token);
	}
#endif
	return dir;
}

QString FallbackPath(const QString &targetId)
{
	/* Allow only a conservative character set to prevent path tricks. */
	QString safe;
	safe.reserve(targetId.size());
	for (QChar c : targetId) {
		if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_') || c == QLatin1Char('.'))
			safe.append(c);
		else
			safe.append(QLatin1Char('_'));
	}
	if (safe.isEmpty())
		safe = QStringLiteral("blank");
	if (safe.contains(QStringLiteral("..")))
		safe.replace(QStringLiteral(".."), QStringLiteral("_"));
	return QDir(FallbackDir()).filePath(safe + QStringLiteral(".bin"));
}

#ifdef _WIN32
QByteArray DpapiProtect(const QByteArray &plain)
{
	DATA_BLOB in{};
	DATA_BLOB out{};
	in.pbData = (BYTE *)plain.data();
	in.cbData = (DWORD)plain.size();
	if (!CryptProtectData(&in, L"VerticalShorts", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	QByteArray protectedBlob(reinterpret_cast<const char *>(out.pbData), (int)out.cbData);
	LocalFree(out.pbData);
	return protectedBlob;
}

QByteArray DpapiUnprotect(const QByteArray &protectedBlob)
{
	DATA_BLOB in{};
	DATA_BLOB out{};
	in.pbData = (BYTE *)protectedBlob.data();
	in.cbData = (DWORD)protectedBlob.size();
	if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	QByteArray plain(reinterpret_cast<const char *>(out.pbData), (int)out.cbData);
	LocalFree(out.pbData);
	return plain;
}
#endif

CredentialStoreResult SaveFallback(const QString &targetId, const QString &secret)
{
	CredentialStoreResult r;
	const QString path = FallbackPath(targetId);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		r.error = QStringLiteral("Could not write credential fallback file.");
		return r;
	}
	QByteArray data = secret.toUtf8();
#ifdef _WIN32
	const QByteArray protectedBlob = DpapiProtect(data);
	if (protectedBlob.isEmpty()) {
		f.close();
		QFile::remove(path);
		r.error = QStringLiteral("DPAPI protection failed for credential fallback.");
		return r;
	}
	/* Magic prefix so LoadFallback can detect DPAPI blobs vs legacy plaintext. */
	const QByteArray payload = QByteArrayLiteral("VSP1") + protectedBlob;
	data.fill('\0');
	if (f.write(payload) != payload.size()) {
		r.error = QStringLiteral("Failed writing credential fallback file.");
		return r;
	}
#else
	if (f.write(data) != data.size()) {
		r.error = QStringLiteral("Failed writing credential fallback file.");
		return r;
	}
#endif
	f.close();
#ifdef _WIN32
	SetFileAttributesW((LPCWSTR)path.utf16(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
#else
	QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
	r.ok = true;
	r.usedSecureStorage = false;
	return r;
}

CredentialStoreResult LoadFallback(const QString &targetId, QString *out)
{
	CredentialStoreResult r;
	QFile f(FallbackPath(targetId));
	if (!f.exists()) {
		r.ok = true;
		r.usedSecureStorage = false;
		if (out)
			out->clear();
		return r;
	}
	if (!f.open(QIODevice::ReadOnly)) {
		r.error = QStringLiteral("Could not read credential fallback file.");
		return r;
	}
	const QByteArray raw = f.readAll();
	f.close();
#ifdef _WIN32
	if (raw.startsWith("VSP1")) {
		const QByteArray plain = DpapiUnprotect(raw.mid(4));
		if (plain.isEmpty()) {
			r.error = QStringLiteral("Could not decrypt credential fallback file.");
			return r;
		}
		if (out)
			*out = QString::fromUtf8(plain);
	} else {
		/* Legacy plaintext fallback — load once; caller should re-save to DPAPI/CredMan. */
		if (out)
			*out = QString::fromUtf8(raw);
	}
#else
	if (out)
		*out = QString::fromUtf8(raw);
#endif
	r.ok = true;
	r.usedSecureStorage = false;
	return r;
}

CredentialStoreResult DeleteFallback(const QString &targetId)
{
	CredentialStoreResult r;
	QFile::remove(FallbackPath(targetId));
	r.ok = true;
	r.usedSecureStorage = false;
	return r;
}

#ifdef _WIN32
QString CredTargetName(const QString &targetId)
{
	return QStringLiteral("VerticalShorts/") + targetId;
}
#endif

} // namespace

bool SecureStorageAvailable()
{
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

QString SecureStorageDescription()
{
#ifdef _WIN32
	return QStringLiteral("Windows Credential Manager (DPAPI-protected file fallback if CredWrite fails)");
#else
	return QStringLiteral("Owner-only local fallback file (secure OS store unavailable on this platform)");
#endif
}

CredentialStoreResult SaveSecret(const QString &targetId, const QString &secret)
{
#ifdef _WIN32
	CredentialStoreResult r;
	const QString name = CredTargetName(targetId);
	QByteArray blob = secret.toUtf8();
	CREDENTIALW cred{};
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = (LPWSTR)name.utf16();
	cred.CredentialBlobSize = (DWORD)blob.size();
	cred.CredentialBlob = (LPBYTE)blob.data();
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.UserName = (LPWSTR)L"VerticalShorts";
	if (!CredWriteW(&cred, 0)) {
		r = SaveFallback(targetId, secret);
		if (r.ok) {
			r.error = QStringLiteral(
				"Windows Credential Manager write failed; used DPAPI-protected local fallback.");
		}
		r.usedSecureStorage = false;
		SecureZeroMemory(blob.data(), (size_t)blob.size());
		return r;
	}
	SecureZeroMemory(blob.data(), (size_t)blob.size());
	DeleteFallback(targetId); /* remove any prior fallback */
	r.ok = true;
	r.usedSecureStorage = true;
	return r;
#else
	return SaveFallback(targetId, secret);
#endif
}

CredentialStoreResult LoadSecret(const QString &targetId, QString *outSecret)
{
#ifdef _WIN32
	CredentialStoreResult r;
	const QString name = CredTargetName(targetId);
	PCREDENTIALW cred = nullptr;
	if (CredReadW((LPCWSTR)name.utf16(), CRED_TYPE_GENERIC, 0, &cred)) {
		if (outSecret && cred->CredentialBlob && cred->CredentialBlobSize > 0) {
			*outSecret = QString::fromUtf8(reinterpret_cast<const char *>(cred->CredentialBlob),
						       (int)cred->CredentialBlobSize);
		} else if (outSecret) {
			outSecret->clear();
		}
		if (cred->CredentialBlob && cred->CredentialBlobSize > 0)
			SecureZeroMemory(cred->CredentialBlob, cred->CredentialBlobSize);
		CredFree(cred);
		r.ok = true;
		r.usedSecureStorage = true;
		return r;
	}
	return LoadFallback(targetId, outSecret);
#else
	return LoadFallback(targetId, outSecret);
#endif
}

CredentialStoreResult DeleteSecret(const QString &targetId)
{
#ifdef _WIN32
	CredentialStoreResult r;
	const QString name = CredTargetName(targetId);
	CredDeleteW((LPCWSTR)name.utf16(), CRED_TYPE_GENERIC, 0);
	DeleteFallback(targetId);
	r.ok = true;
	r.usedSecureStorage = true;
	return r;
#else
	return DeleteFallback(targetId);
#endif
}

} // namespace vsp
