#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <clocale> // 🌟

int main(int argc, char *argv[])
{
    // 🌟 强制使用标准 C 环境，避免 atof/sprintf 在德语/法语系统下解析逗号出错
    std::setlocale(LC_NUMERIC, "C");

    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "Array_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
