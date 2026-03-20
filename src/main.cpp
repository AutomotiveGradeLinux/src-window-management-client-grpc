/*
 * Copyright 2023 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QtCore/QUrlQuery>
#include <QtCore/QObject>
#include <QObject>
#include <QQmlEngine>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtQml/qqml.h>
#include <QQuickWindow>
#include <QTimer>

#include "AglShellGrpcClient.h"

const QString myname = QString("window-management-client-rpc");

enum mode {
	NONE = 0,
	FLOAT = 1,
	FULLSCREEN = 2,
	FULLSCREEN_QT = 3,	//  rather than use rpc, use Qt API
	ON_OTHER_OUTPUTS = 4,
	SET_FLOAT_POS	= 5,
	SCALE	= 6,
	SPLIT	= 7,
};

static QWindow *
create_window(QQmlComponent *comp)
{
	QObject *obj = comp->create();
	obj->setParent(nullptr);

	QWindow *win = qobject_cast<QWindow *>(obj);

	return win;
}

int main(int argc, char *argv[])
{
	enum mode mmode = NONE;

	QGuiApplication app(argc, argv);
	QCommandLineParser parser;
	QCoreApplication::setApplicationName(myname);
	QCoreApplication::setApplicationVersion("1.0");
	QQmlApplicationEngine engine;

	// necessary to identify correctly by app_id
	app.setDesktopFileName(myname);

	parser.setApplicationDescription("Application to demonstrate "
					 "window-management functionality with gRPC. \nSpecifying a particular mode will make the application "
					 "talk with compositor through the gRPC proxy server about making the application in that mode.\nFor example "
					 "to start as floating specify -m float or if fullscreen, -m full");
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption outputOption(QStringList() << "o" << "output", "Specify output name [ex. HDMI-A-1].", "output");
	parser.addOption(outputOption);

	QCommandLineOption modeOption(QStringList() << "m" << "mode", "Specify window mode. [ex. full, full_qt, on_output, position, scale, split]", "mode");
	parser.addOption(modeOption);

	parser.process(app);

	if (parser.isSet(modeOption)) {
		QString mode = parser.value(modeOption).toLower();
		const char *output_name = NULL;

		if (mode == "float") {
			mmode = FLOAT;
		} else if (mode == "full") {
			mmode = FULLSCREEN;
		} else if (mode == "full_qt") {
			mmode = FULLSCREEN_QT;
		} else if (mode == "on_output") {
			mmode = ON_OTHER_OUTPUTS;
		} else if (mode == "position") {
			mmode = SET_FLOAT_POS;
		} else if (mode == "scale") {
			mmode = SCALE;
		} else if (mode == "split") {
			mmode = SPLIT;
		} else {
			assert(!"Invalid mode");
		}

		if (mmode != FLOAT && mmode != FULLSCREEN && 
		    mmode != ON_OTHER_OUTPUTS && mmode != SET_FLOAT_POS &&
		    mmode != SCALE && mmode != SPLIT) {
			fprintf(stderr, "Will not use rpc\n");
			goto skip;
		}

		if (mmode == ON_OTHER_OUTPUTS ||
		    mmode == SPLIT) {
			QString q_output_name = parser.value(outputOption).toLower();
			output_name = q_output_name.toUtf8().toStdString().c_str();
		}

		// start grpc connection
		GrpcClient *client = new GrpcClient();

		// note that these are setting up the window state by using
		// another communication channel (rpc), and need to happen
		// before the client does the initial commit (without a buffer
		// attached)
		switch (mmode) {
		case FLOAT:
			fprintf(stderr, "Setting the application as float\n");
			client->SetAppFloat(myname.toStdString(), 40, 300);
			break;
		case FULLSCREEN:
			fprintf(stderr, "Setting the application as fullscreen\n");
			client->SetAppFullscreen(myname.toStdString());
			break;
		case ON_OTHER_OUTPUTS:
			fprintf(stderr, "Setting application '%s' on output '%s'\n", 
					myname.toStdString().c_str(), output_name);
			if (!output_name) {
				fprintf(stderr, "Output name is not set!\n");
				exit(EXIT_FAILURE);
			}
			client->SetAppOnOutput(myname.toStdString(),
					       std::string(output_name));
			break;
		case SET_FLOAT_POS:
			// this assumes the window is already running and
			// floating; uses the same application so this needs
			// to be first started as float.
			client->SetAppPosition(myname.toStdString(), 550, 550);
			exit(EXIT_SUCCESS);
			break;
		case SCALE:
			// this assumes the window is already running and
			// floating; uses the same application so this needs
			// to be first started as float.
			client->SetAppScale(myname.toStdString(), 200, 200);
			exit(EXIT_SUCCESS);
			break;
		case SPLIT:
			// put it on the bottom
			client->SetAppSplit(myname.toStdString(), 4, 0, 0,
					std::string(output_name));
			break;
		default:
			break;
		}
	}

skip:
	QQmlComponent main_comp(&engine, QUrl("qrc:/Main.qml"));
	QWindow *win = create_window(&main_comp);
	(void) win;

	// alternatively do the load directly
	//engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));

	return app.exec();
}
