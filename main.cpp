#include <QApplication>

#include "papermanager.h"


int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    PaperManager* pmw=new PaperManager;

    return app.exec();

}
