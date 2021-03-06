/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2015 Synergy Si, Std.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file COPYING that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "WebClient.h"

#include "EditionType.h"
#include "QUtility.h"

#include <QProcess>
#include <QMessageBox>
#include <QCoreApplication>
#include <stdexcept>

int WebClient::getEdition(
		const QString& email,
		const QString& password,
		QMessageBox& message,
		QWidget* w)
{
	QString responseJson;
	int edition = Unknown;
	try {
		QStringList args("--login-auth");
		responseJson = request(email, password, args);
	}
	catch (std::exception& e)
	{
		message.critical(
			w, "Error",
			tr("An error occured while trying to sign in. "
			"Please contact the helpdesk, and provide the "
			"following details.\n\n%1").arg(e.what()));
		return edition;
	}

	QRegExp resultRegex(".*\"result\".*:.*(true|false).*");
	if (resultRegex.exactMatch(responseJson)) {
		QString boolString = resultRegex.cap(1);
		if (boolString == "true") {
			QRegExp editionRegex(".*\"edition\".*:.*\"([^\"]+)\".*");
			if (editionRegex.exactMatch(responseJson)) {
				QString e = editionRegex.cap(1);
				edition = e.toInt();
			}

			return edition;
		}
		else if (boolString == "false") {
			message.critical(
				w, "Error",
				tr("Login failed, invalid email or password."));

			return edition;
		}
	}
	else {
		QRegExp errorRegex(".*\"error\".*:.*\"([^\"]+)\".*");
		if (errorRegex.exactMatch(responseJson)) {

			// replace "\n" with real new lines.
			QString error = errorRegex.cap(1).replace("\\n", "\n");
			message.critical(
				w, "Error",
				tr("Login failed, an error occurred.\n\n%1").arg(error));

			return edition;
		}
	}

	message.critical(
		w, "Error",
		tr("Login failed, an error occurred.\n\nServer response:\n\n%1")
		.arg(responseJson));

	return edition;
}

void WebClient::queryPluginList()
{
	QString responseJson;
	try {
		QStringList args("--get-plugin-list");
		responseJson = request(m_Email, m_Password, args);
	}
	catch (std::exception& e)
	{
		m_Error = tr("An error occured while trying to query the "
			"plugin list. Please contact the help desk, and provide "
			"the following details.\n\n%1").arg(e.what());
		emit queryPluginDone();
		return;
	}

	QRegExp resultRegex(".*\"result\".*:.*(true|false).*");
	if (resultRegex.exactMatch(responseJson)) {
		QString boolString = resultRegex.cap(1);
		if (boolString == "true") {
			QRegExp editionRegex(".*\"plugins\".*:.*\"([^\"]+)\".*");
			if (editionRegex.exactMatch(responseJson)) {
				QString e = editionRegex.cap(1);
				m_PluginList = e.split(",");
				m_Error.clear();
				emit queryPluginDone();
				return;
			}
		}
		else if (boolString == "false") {
			m_Error = tr("Get plugin list failed, invalid user email "
						 "or password.");
			emit queryPluginDone();
			return;
		}
	}
	else {
		QRegExp errorRegex(".*\"error\".*:.*\"([^\"]+)\".*");
		if (errorRegex.exactMatch(responseJson)) {

			// replace "\n" with real new lines.
			QString error = errorRegex.cap(1).replace("\\n", "\n");
			m_Error = tr("Get plugin list failed, an error occurred."
						 "\n\n%1").arg(error);
			emit queryPluginDone();
			return;
		}
	}

	m_Error = tr("Get plugin list failed, an error occurred.\n\n"
				 "Server response:\n\n%1").arg(responseJson);
	emit queryPluginDone();
	return;
}

QString WebClient::request(
		const QString& email,
		const QString& password,
		QStringList& args)
{
	QString program(QCoreApplication::applicationDirPath() + "/syntool");

	QProcess process;
	process.setReadChannel(QProcess::StandardOutput);
	process.start(program, args);
	bool success = process.waitForStarted();

	QString out, error;
	if (success)
	{
		// hash password in case it contains interesting chars.
		QString credentials(email + ":" + hash(password) + "\n");
		process.write(credentials.toStdString().c_str());

		if (process.waitForFinished()) {
			out = process.readAllStandardOutput();
			error = process.readAllStandardError();
		}
	}

	out = out.trimmed();
	error = error.trimmed();

	if (out.isEmpty() ||
		!error.isEmpty() ||
		!success ||
		process.exitCode() != 0)
	{
		throw std::runtime_error(
			QString("Code: %1\nError: %2")
				.arg(process.exitCode())
				.arg(error.isEmpty() ? "Unknown" : error)
				.toStdString());
	}

	return out;
}
