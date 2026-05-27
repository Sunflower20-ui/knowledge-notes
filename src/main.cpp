#include <QApplication>
#include "app/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("KnowledgeNotes");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("KnowledgeNotes");

    MainWindow window;
    window.setWindowTitle("KnowledgeNotes");
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
