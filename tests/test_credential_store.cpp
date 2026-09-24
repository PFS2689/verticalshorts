#include "credential-store.hpp"

#include <iostream>

static int failures = 0;

static void expect(bool cond, const char *msg)
{
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		++failures;
	} else {
		std::cout << "ok: " << msg << "\n";
	}
}

int main()
{
	const QString target = QStringLiteral("vsp/test-dest-unit/key");
	const QString secret = QStringLiteral("unit-test-stream-key-do-not-log");

	auto del = vsp::DeleteSecret(target);
	expect(del.ok, "delete preexisting ok");

	auto save = vsp::SaveSecret(target, secret);
	expect(save.ok, "save secret ok");
#ifndef _WIN32
	expect(!save.usedSecureStorage, "linux uses fallback file");
	expect(!vsp::SecureStorageAvailable(), "secure storage unavailable on non-windows");
#endif

	QString loaded;
	auto load = vsp::LoadSecret(target, &loaded);
	expect(load.ok, "load secret ok");
	expect(loaded == secret, "loaded secret matches");

	auto cleared = vsp::DeleteSecret(target);
	expect(cleared.ok, "clear secret ok");
	loaded = QStringLiteral("stale");
	auto load2 = vsp::LoadSecret(target, &loaded);
	expect(load2.ok, "load after delete ok");
	expect(loaded.isEmpty(), "secret cleared");

	expect(vsp::KeyTarget(QStringLiteral("abc")) == QStringLiteral("vsp/abc/key"), "key target format");
	expect(vsp::PasswordTarget(QStringLiteral("abc")) == QStringLiteral("vsp/abc/password"),
	       "password target format");
	expect(!vsp::SecureStorageDescription().isEmpty(), "storage description present");

	if (failures) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "all credential store tests passed\n";
	return 0;
}
